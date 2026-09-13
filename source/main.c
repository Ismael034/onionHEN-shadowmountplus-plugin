#include <onion/client.h>
#include <onion/plugin.h>
#include <onion/status.h>
#include <onion/transport.h>
#include <onion/ui.h>

#include <ps5/kernel.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "plugin_config.h"
#include "plugin_ui.h"
#include "shadowmount_paths.h"
#include "shadowmount_service.h"

#define CONNECT_ATTEMPTS 30
#define CONNECT_RETRY_US (250 * 1000)
#define EVENT_POLL_US (100 * 1000)
#define PLUGIN_AUTH_ID UINT64_C(0x4800000000000006)

extern const onion_plugin_descriptor_v1 onion_plugin_descriptor;

typedef struct plugin_app {
    onion_transport transport;
    onion_socket_transport socket;
    onion_client client;
    onion_host_services_v1 services;
    onion_ui_document *document;
    onion_ui_handle ui_handle;
    shadowmount_service scanner;
    FILE *log_file;
    int transport_connected;
    int client_initialized;
    int scanner_initialized;
} plugin_app;

static volatile sig_atomic_t running = 1;

static void request_stop(int signal_number) {
    (void)signal_number;
    running = 0;
}

static void log_message(plugin_app *app, const char *format, ...) {
    va_list arguments;
    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
    fflush(stdout);

    if (!app->log_file) return;
    va_start(arguments, format);
    vfprintf(app->log_file, format, arguments);
    va_end(arguments);
    fflush(app->log_file);
}

static onion_status connect_to_daemon(plugin_app *app) {
    for (int attempt = 1; attempt <= CONNECT_ATTEMPTS && running; ++attempt) {
        onion_status status = onion_socket_transport_connect(
            &app->transport, &app->socket, ONION_PLUGIN_IPC_SOCKET_PATH);
        if (status == ONION_OK) {
            app->transport_connected = 1;
            return ONION_OK;
        }
        if (attempt < CONNECT_ATTEMPTS) usleep(CONNECT_RETRY_US);
    }
    return ONION_E_IO;
}

static onion_status start_plugin(plugin_app *app) {
    if (kernel_set_ucred_authid(getpid(), PLUGIN_AUTH_ID) != 0) {
        return ONION_E_PERMISSION;
    }

    onion_status status = connect_to_daemon(app);
    if (status != ONION_OK) return status;
    status = onion_client_init(&app->client, &app->transport);
    if (status != ONION_OK) return status;
    app->client_initialized = 1;

    status = onion_client_open_session(&app->client, &onion_plugin_descriptor);
    if (status == ONION_OK) {
        status = onion_client_make_services(&app->client, &app->services);
    }
    if (status == ONION_OK) status = plugin_ui_create(&app->document);
    if (status == ONION_OK) {
        status = onion_ui_register(
            &app->services, app->document, &app->ui_handle);
    }
    if (status != ONION_OK) return status;

    if (!shadowmount_service_init(&app->scanner)) return ONION_E_IO;
    app->scanner_initialized = 1;
    if (!shadowmount_service_start(&app->scanner)) return ONION_E_IO;
    return ONION_OK;
}

static void stop_plugin(plugin_app *app) {
    if (app->ui_handle != 0) {
        (void)onion_ui_unregister(&app->services, app->ui_handle);
        app->ui_handle = 0;
    }
    onion_ui_document_destroy(app->document);
    app->document = NULL;
    if (app->scanner_initialized) {
        shadowmount_service_destroy(&app->scanner);
        app->scanner_initialized = 0;
    }
    if (app->client_initialized) {
        onion_client_deinit(&app->client);
        app->client_initialized = 0;
    }
    if (app->transport_connected) {
        onion_socket_transport_deinit(&app->transport);
        app->transport_connected = 0;
    }
}

static onion_status apply_action(plugin_app *app,
                                 const plugin_ui_action *action) {
    switch (action->kind) {
    case PLUGIN_UI_ACTION_SCAN_NOW:
        shadowmount_service_request_scan(&app->scanner);
        return ONION_OK;

    case PLUGIN_UI_ACTION_SET_SCAN_PATH: {
        const bool accepted = shadowmount_paths_set_custom(
            action->scan_path_slot, action->scan_path_value);
        if (!accepted) {
            log_message(app, "[%s] rejected scan path slot %d: %s\n",
                        PLUGIN_ID, action->scan_path_slot,
                        action->scan_path_value);
        }
        char slots[SHADOWMOUNT_UI_SCAN_SLOTS][SHADOWMOUNT_UI_SCAN_PATH_MAX + 1];
        (void)shadowmount_paths_load_custom(slots);
        (void)plugin_ui_set_scan_path(&app->services, app->ui_handle,
                                      action->scan_path_slot,
                                      slots[action->scan_path_slot]);
        if (accepted) shadowmount_service_request_scan(&app->scanner);
        return accepted ? ONION_OK : ONION_E_INVALID_ARGUMENT;
    }

    case PLUGIN_UI_ACTION_NONE:
        return ONION_E_NOT_FOUND;
    }
    return ONION_E_NOT_FOUND;
}

static int run_event_loop(plugin_app *app) {
    while (running) {
        if (!shadowmount_service_running(&app->scanner)) {
            log_message(app, "[%s] scanner stopped unexpectedly\n", PLUGIN_ID);
            return 1;
        }

        onion_ui_event_v1 event;
        onion_status status = onion_client_poll_ui_event(&app->client, &event);
        if (status == ONION_E_NOT_FOUND) {
            usleep(EVENT_POLL_US);
            continue;
        }
        if (status != ONION_OK) {
            log_message(app, "[%s] UI event poll failed: %s\n", PLUGIN_ID,
                        onion_status_string(status));
            return 1;
        }

        plugin_ui_action action;
        status = plugin_ui_decode_action(app->ui_handle, &event, &action);
        if (status == ONION_OK) status = apply_action(app, &action);
        if (status != ONION_OK && status != ONION_E_NOT_FOUND) {
            log_message(app, "[%s] action %s failed: %s\n", PLUGIN_ID,
                        event.node_id, onion_status_string(status));
        }
    }
    return 0;
}

int main(void) {
    plugin_app app = {0};
    app.log_file = fopen(PLUGIN_LOG_PATH, "a");

    signal(SIGINT, request_stop);
    signal(SIGTERM, request_stop);
    signal(SIGPIPE, SIG_IGN);
    (void)syscall(SYS_thr_set_name, -1, "shadowmountplus.elf");
    log_message(&app, "[%s] starting %s %s\n", PLUGIN_ID, PLUGIN_NAME,
                PLUGIN_VERSION);

    const onion_status status = start_plugin(&app);
    int exit_code = 1;
    if (status == ONION_OK) {
        log_message(&app, "[%s] scanner ready; UI handle=%llu\n", PLUGIN_ID,
                    (unsigned long long)app.ui_handle);
        exit_code = run_event_loop(&app);
    } else {
        log_message(&app, "[%s] startup failed: %s\n", PLUGIN_ID,
                    onion_status_string(status));
    }

    stop_plugin(&app);
    log_message(&app, "[%s] stopped with exit_code=%d\n", PLUGIN_ID, exit_code);
    if (app.log_file) fclose(app.log_file);
    return exit_code;
}

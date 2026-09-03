#include <onion/client.h>
#include <onion/plugin.h>
#include <onion/status.h>
#include <onion/transport.h>
#include <onion/ui.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>

#include "plugin_config.h"
#include "plugin_ui.h"

#define CONNECT_ATTEMPTS 30
#define CONNECT_RETRY_US (250 * 1000)
#define EVENT_POLL_US (100 * 1000)

extern const onion_plugin_descriptor_v1 onion_plugin_descriptor;

typedef struct plugin_app {
    onion_transport transport;
    onion_socket_transport socket;
    onion_client client;
    onion_host_services_v1 services;
    onion_ui_document *document;
    onion_ui_handle ui_handle;
    plugin_ui_state ui_state;
    FILE *log_file;
    int transport_connected;
    int client_initialized;
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
    onion_status status = connect_to_daemon(app);
    if (status != ONION_OK) return status;

    status = onion_client_init(&app->client, &app->transport);
    if (status != ONION_OK) return status;
    app->client_initialized = 1;

    status = onion_client_open_session(&app->client, &onion_plugin_descriptor);
    if (status == ONION_OK) {
        status = onion_client_make_services(&app->client, &app->services);
    }
    if (status == ONION_OK) {
        status = plugin_ui_create(&app->ui_state, &app->document);
    }
    if (status == ONION_OK) {
        status = onion_ui_register(
            &app->services, app->document, &app->ui_handle);
    }
    return status;
}

static void stop_plugin(plugin_app *app) {
    if (app->ui_handle != 0) {
        (void)onion_ui_unregister(&app->services, app->ui_handle);
        app->ui_handle = 0;
    }
    onion_ui_document_destroy(app->document);
    app->document = NULL;
    if (app->client_initialized) {
        onion_client_deinit(&app->client);
        app->client_initialized = 0;
    }
    if (app->transport_connected) {
        onion_socket_transport_deinit(&app->transport);
        app->transport_connected = 0;
    }
}

static int run_event_loop(plugin_app *app) {
    while (running) {
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

        status = plugin_ui_handle_action(
            &app->ui_state, &app->services, app->ui_handle, &event);
        if (status != ONION_OK && status != ONION_E_NOT_FOUND) {
            log_message(app, "[%s] rejected action %s: %s\n", PLUGIN_ID,
                        event.node_id, onion_status_string(status));
        }
    }
    return 0;
}

int main(void) {
    plugin_app app = {0};
    plugin_ui_state_init(&app.ui_state);
    app.log_file = fopen(PLUGIN_LOG_PATH, "a");

    signal(SIGINT, request_stop);
    signal(SIGTERM, request_stop);
    log_message(&app, "[%s] starting %s %s\n", PLUGIN_ID, PLUGIN_NAME,
                PLUGIN_VERSION);

    const onion_status status = start_plugin(&app);
    int exit_code = 1;
    if (status == ONION_OK) {
        log_message(&app, "[%s] UI registered with handle=%llu\n", PLUGIN_ID,
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

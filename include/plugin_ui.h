#pragma once

#include <onion/services.h>
#include <onion/status.h>
#include <onion/ui.h>

typedef struct plugin_ui_state {
    int enabled;
    char mode[16];
    int port;
} plugin_ui_state;

void plugin_ui_state_init(plugin_ui_state *state);
onion_status plugin_ui_create(const plugin_ui_state *state,
                              onion_ui_document **out_document);
onion_status plugin_ui_handle_action(plugin_ui_state *state,
                                     const onion_host_services_v1 *services,
                                     onion_ui_handle handle,
                                     const onion_ui_event_v1 *event);

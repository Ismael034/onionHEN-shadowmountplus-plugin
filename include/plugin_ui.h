#pragma once

#include <onion/services.h>
#include <onion/status.h>
#include <onion/ui.h>

#include "shadowmount_paths.h"

typedef enum plugin_ui_action_kind {
    PLUGIN_UI_ACTION_NONE = 0,
    PLUGIN_UI_ACTION_SCAN_NOW,
    PLUGIN_UI_ACTION_SET_SCAN_PATH
} plugin_ui_action_kind;

typedef struct plugin_ui_action {
    plugin_ui_action_kind kind;
    int scan_path_slot;
    char scan_path_value[SHADOWMOUNT_UI_SCAN_PATH_MAX + 1];
} plugin_ui_action;

onion_status plugin_ui_create(onion_ui_document **out_document);
onion_status plugin_ui_decode_action(onion_ui_handle handle,
                                     const onion_ui_event_v1 *event,
                                     plugin_ui_action *out_action);
onion_status plugin_ui_set_scan_path(const onion_host_services_v1 *services,
                                     onion_ui_handle handle, int slot,
                                     const char *value);

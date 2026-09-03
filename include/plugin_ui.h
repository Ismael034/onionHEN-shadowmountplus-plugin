#pragma once

#include <onion/status.h>
#include <onion/ui.h>

typedef enum plugin_ui_action_kind {
    PLUGIN_UI_ACTION_NONE = 0,
    PLUGIN_UI_ACTION_SCAN_NOW
} plugin_ui_action_kind;

typedef struct plugin_ui_action {
    plugin_ui_action_kind kind;
} plugin_ui_action;

onion_status plugin_ui_create(onion_ui_document **out_document);
onion_status plugin_ui_decode_action(onion_ui_handle handle,
                                     const onion_ui_event_v1 *event,
                                     plugin_ui_action *out_action);

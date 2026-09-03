#include "plugin_ui.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugin_config.h"

#define CONTRIBUTION_ID "settings"

static onion_ui_node_desc_v1 make_node(uint32_t kind, const char *id,
                                       const char *parent_id,
                                       const char *title) {
    onion_ui_node_desc_v1 node;
    memset(&node, 0, sizeof(node));
    node.struct_size = sizeof(node);
    node.abi_version = ONION_UI_ABI_VERSION;
    node.kind = kind;
    snprintf(node.id, sizeof(node.id), "%s", id);
    snprintf(node.parent_id, sizeof(node.parent_id), "%s",
             parent_id ? parent_id : "");
    snprintf(node.title, sizeof(node.title), "%s", title);
    return node;
}

static onion_status add_node(onion_ui_document *document,
                             const onion_ui_node_desc_v1 *node) {
    return onion_ui_document_add_node(document, node);
}

static onion_status add_document_nodes(onion_ui_document *document,
                                       const plugin_ui_state *state) {
    onion_ui_node_desc_v1 node =
        make_node(ONION_UI_NODE_PAGE, "main", NULL, PLUGIN_NAME);
    onion_status status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_PAGE, "settings_page", NULL, "Settings");
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_LABEL, "about", "main", "Plugin ready");
    snprintf(node.description, sizeof(node.description),
             "%s %s is connected to OnionHEN", PLUGIN_NAME, PLUGIN_VERSION);
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_MENU, "open_settings", "main", "Settings");
    snprintf(node.description, sizeof(node.description),
             "Configure this example plugin");
    snprintf(node.target_id, sizeof(node.target_id), "settings_page");
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_GROUP, "general", "settings_page", "General");
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_TOGGLE, "enabled", "general", "Enabled");
    node.value_type = ONION_UI_VALUE_BOOL;
    node.binding = ONION_UI_BINDING_EVENT;
    snprintf(node.binding_key, sizeof(node.binding_key), "enabled_changed");
    snprintf(node.value, sizeof(node.value), "%s",
             state->enabled ? "true" : "false");
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_LIST, "mode", "general", "Mode");
    node.value_type = ONION_UI_VALUE_STRING;
    node.binding = ONION_UI_BINDING_EVENT;
    snprintf(node.binding_key, sizeof(node.binding_key), "mode_changed");
    snprintf(node.value, sizeof(node.value), "%s", state->mode);
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_LIST_ITEM, "mode_safe", "mode", "Safe");
    node.value_type = ONION_UI_VALUE_STRING;
    snprintf(node.value, sizeof(node.value), "safe");
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_LIST_ITEM, "mode_fast", "mode", "Fast");
    node.value_type = ONION_UI_VALUE_STRING;
    snprintf(node.value, sizeof(node.value), "fast");
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_INPUT, "port", "general", "Port");
    node.value_type = ONION_UI_VALUE_INT;
    node.binding = ONION_UI_BINDING_EVENT;
    node.min_value = 1;
    node.max_value = 65535;
    node.min_length = 1;
    node.max_length = 5;
    snprintf(node.binding_key, sizeof(node.binding_key), "port_changed");
    snprintf(node.value, sizeof(node.value), "%d", state->port);
    status = add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_ACTION, "reset", "general", "Reset settings");
    node.flags = ONION_UI_NODE_FLAG_CONFIRM;
    node.binding = ONION_UI_BINDING_EVENT;
    snprintf(node.binding_key, sizeof(node.binding_key), "reset_requested");
    return add_node(document, &node);
}

void plugin_ui_state_init(plugin_ui_state *state) {
    if (!state) return;
    state->enabled = 1;
    snprintf(state->mode, sizeof(state->mode), "safe");
    state->port = 1337;
}

onion_status plugin_ui_create(const plugin_ui_state *state,
                              onion_ui_document **out_document) {
    if (!state || !out_document) return ONION_E_INVALID_ARGUMENT;

    onion_ui_document_desc_v1 description;
    memset(&description, 0, sizeof(description));
    description.struct_size = sizeof(description);
    description.abi_version = ONION_UI_ABI_VERSION;
    description.priority = 100;
    snprintf(description.plugin_id, sizeof(description.plugin_id), "%s",
             PLUGIN_ID);
    snprintf(description.contribution_id,
             sizeof(description.contribution_id), "%s", CONTRIBUTION_ID);
    snprintf(description.title, sizeof(description.title), "%s", PLUGIN_NAME);
    snprintf(description.description, sizeof(description.description),
             "Dynamic settings provided by %s", PLUGIN_NAME);
    snprintf(description.root_page_id, sizeof(description.root_page_id), "main");

    onion_status status = onion_ui_document_create(&description, out_document);
    if (status != ONION_OK) return status;

    status = add_document_nodes(*out_document, state);
    if (status == ONION_OK) status = onion_ui_document_validate(*out_document);
    if (status != ONION_OK) {
        onion_ui_document_destroy(*out_document);
        *out_document = NULL;
    }
    return status;
}

static onion_status parse_port(const char *value, int *out_port) {
    char *end = NULL;
    errno = 0;
    const long port = strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || port < 1 ||
        port > 65535) {
        return ONION_E_INVALID_ARGUMENT;
    }
    *out_port = (int)port;
    return ONION_OK;
}

static onion_status reset_ui(plugin_ui_state *state,
                             const onion_host_services_v1 *services,
                             onion_ui_handle handle) {
    plugin_ui_state_init(state);
    onion_status status = onion_ui_set_value(
        services, handle, "enabled", ONION_UI_VALUE_BOOL, "true");
    if (status != ONION_OK) return status;
    status = onion_ui_set_value(
        services, handle, "mode", ONION_UI_VALUE_STRING, state->mode);
    if (status != ONION_OK) return status;
    return onion_ui_set_value(
        services, handle, "port", ONION_UI_VALUE_INT, "1337");
}

onion_status plugin_ui_handle_action(plugin_ui_state *state,
                                     const onion_host_services_v1 *services,
                                     onion_ui_handle handle,
                                     const onion_ui_event_v1 *event) {
    if (!state || !services || handle == 0 || !event) {
        return ONION_E_INVALID_ARGUMENT;
    }
    if (event->handle != handle ||
        strcmp(event->contribution_id, CONTRIBUTION_ID) != 0) {
        return ONION_E_NOT_FOUND;
    }

    if (strcmp(event->node_id, "enabled") == 0) {
        if (event->value_type != ONION_UI_VALUE_BOOL) {
            return ONION_E_INVALID_ARGUMENT;
        }
        if (strcmp(event->value, "true") == 0 ||
            strcmp(event->value, "1") == 0) {
            state->enabled = 1;
            return ONION_OK;
        }
        if (strcmp(event->value, "false") == 0 ||
            strcmp(event->value, "0") == 0) {
            state->enabled = 0;
            return ONION_OK;
        }
        return ONION_E_INVALID_ARGUMENT;
    }

    if (strcmp(event->node_id, "mode") == 0) {
        if (event->value_type != ONION_UI_VALUE_STRING ||
            (strcmp(event->value, "safe") != 0 &&
             strcmp(event->value, "fast") != 0)) {
            return ONION_E_INVALID_ARGUMENT;
        }
        snprintf(state->mode, sizeof(state->mode), "%s", event->value);
        return ONION_OK;
    }

    if (strcmp(event->node_id, "port") == 0) {
        if (event->value_type != ONION_UI_VALUE_INT) {
            return ONION_E_INVALID_ARGUMENT;
        }
        return parse_port(event->value, &state->port);
    }

    if (strcmp(event->node_id, "reset") == 0) {
        if (event->value_type != ONION_UI_VALUE_NONE) {
            return ONION_E_INVALID_ARGUMENT;
        }
        return reset_ui(state, services, handle);
    }

    return ONION_E_NOT_FOUND;
}

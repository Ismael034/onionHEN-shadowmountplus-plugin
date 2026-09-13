#include "plugin_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugin_config.h"
#include "shadowmount_paths.h"

#define CONTRIBUTION_ID "scanner"
#define SCAN_PATH_NODE_PREFIX "scan_path_"

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

static void scan_path_node_id(int slot, char *out, size_t out_size) {
    snprintf(out, out_size, "%s%d", SCAN_PATH_NODE_PREFIX, slot + 1);
}

static onion_status add_scan_path_nodes(onion_ui_document *document) {
    char custom_values[SHADOWMOUNT_UI_SCAN_SLOTS][SHADOWMOUNT_UI_SCAN_PATH_MAX + 1];
    (void)shadowmount_paths_load_custom(custom_values);

    onion_ui_node_desc_v1 node =
        make_node(ONION_UI_NODE_GROUP, "paths", "scanner", "Scan Paths");
    onion_status status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    for (int slot = 0; slot < SHADOWMOUNT_UI_SCAN_SLOTS; slot++) {
        char node_id[ONION_UI_NODE_ID_MAX];
        scan_path_node_id(slot, node_id, sizeof(node_id));
        char title[ONION_UI_TITLE_MAX];
        snprintf(title, sizeof(title), "Custom path %d", slot + 1);

        node = make_node(ONION_UI_NODE_INPUT, node_id, "paths", title);
        node.value_type = ONION_UI_VALUE_STRING;
        node.binding = ONION_UI_BINDING_EVENT;
        node.min_length = 0;
        node.max_length = SHADOWMOUNT_UI_SCAN_PATH_MAX;
        snprintf(node.binding_key, sizeof(node.binding_key), "%s_changed",
                 node_id);
        snprintf(node.value, sizeof(node.value), "%s", custom_values[slot]);
        status = onion_ui_document_add_node(document, &node);
        if (status != ONION_OK) return status;
    }

    return ONION_OK;
}

static onion_status add_nodes(onion_ui_document *document) {
    onion_ui_node_desc_v1 node =
        make_node(ONION_UI_NODE_PAGE, "main", NULL, PLUGIN_NAME);
    onion_status status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_LABEL, "status", "main",
                     "Background scanner active");
    snprintf(node.description, sizeof(node.description),
             "ShadowMount+ %s", SHADOWMOUNT_UPSTREAM_VERSION);
    status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_GROUP, "scanner", "main", "Scanner");
    status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_ACTION, "scan_now", "scanner", "Scan now");
    node.binding = ONION_UI_BINDING_EVENT;
    snprintf(node.binding_key, sizeof(node.binding_key), "scan_requested");
    snprintf(node.description, sizeof(node.description),
             "Scans all configured paths now");
    status = onion_ui_document_add_node(document, &node);
    if (status != ONION_OK) return status;

    status = add_scan_path_nodes(document);
    if (status != ONION_OK) return status;

    node = make_node(ONION_UI_NODE_LABEL, "configuration", "main",
                     "Configuration");
    snprintf(node.description, sizeof(node.description),
             "/data/shadowmount/config.ini");
    return onion_ui_document_add_node(document, &node);
}

onion_status plugin_ui_create(onion_ui_document **out_document) {
    if (!out_document) return ONION_E_INVALID_ARGUMENT;

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
             "Automatic game image scanner and mounter");
    snprintf(description.root_page_id, sizeof(description.root_page_id), "main");

    onion_status status = onion_ui_document_create(&description, out_document);
    if (status != ONION_OK) return status;
    status = add_nodes(*out_document);
    if (status == ONION_OK) status = onion_ui_document_validate(*out_document);
    if (status != ONION_OK) {
        onion_ui_document_destroy(*out_document);
        *out_document = NULL;
    }
    return status;
}

static int decode_scan_path_slot(const char *node_id) {
    const size_t prefix_len = strlen(SCAN_PATH_NODE_PREFIX);
    if (strncmp(node_id, SCAN_PATH_NODE_PREFIX, prefix_len) != 0) return -1;
    const char *digits = node_id + prefix_len;
    if (digits[0] == '\0') return -1;
    char *end = NULL;
    long slot_number = strtol(digits, &end, 10);
    if (!end || *end != '\0' || slot_number < 1 ||
        slot_number > SHADOWMOUNT_UI_SCAN_SLOTS) {
        return -1;
    }
    return (int)slot_number - 1;
}

onion_status plugin_ui_decode_action(onion_ui_handle handle,
                                     const onion_ui_event_v1 *event,
                                     plugin_ui_action *out_action) {
    if (handle == 0 || !event || !out_action) return ONION_E_INVALID_ARGUMENT;
    memset(out_action, 0, sizeof(*out_action));
    if (event->handle != handle ||
        strcmp(event->contribution_id, CONTRIBUTION_ID) != 0) {
        return ONION_E_NOT_FOUND;
    }

    if (strcmp(event->node_id, "scan_now") == 0) {
        if (event->value_type != ONION_UI_VALUE_NONE) {
            return ONION_E_INVALID_ARGUMENT;
        }
        out_action->kind = PLUGIN_UI_ACTION_SCAN_NOW;
        return ONION_OK;
    }

    const int slot = decode_scan_path_slot(event->node_id);
    if (slot >= 0) {
        if (event->value_type != ONION_UI_VALUE_STRING) {
            return ONION_E_INVALID_ARGUMENT;
        }
        out_action->kind = PLUGIN_UI_ACTION_SET_SCAN_PATH;
        out_action->scan_path_slot = slot;
        snprintf(out_action->scan_path_value, sizeof(out_action->scan_path_value),
                 "%s", event->value);
        return ONION_OK;
    }

    return ONION_E_NOT_FOUND;
}

onion_status plugin_ui_set_scan_path(const onion_host_services_v1 *services,
                                     onion_ui_handle handle, int slot,
                                     const char *value) {
    if (slot < 0 || slot >= SHADOWMOUNT_UI_SCAN_SLOTS) {
        return ONION_E_INVALID_ARGUMENT;
    }
    char node_id[ONION_UI_NODE_ID_MAX];
    scan_path_node_id(slot, node_id, sizeof(node_id));
    return onion_ui_set_value(services, handle, node_id, ONION_UI_VALUE_STRING,
                              value ? value : "");
}


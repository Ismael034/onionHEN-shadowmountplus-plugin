#include "plugin_ui.h"

#include <stdio.h>
#include <string.h>

#include "plugin_config.h"

#define CONTRIBUTION_ID "scanner"

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
             "Wake the scanner and process configured paths");
    status = onion_ui_document_add_node(document, &node);
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

onion_status plugin_ui_decode_action(onion_ui_handle handle,
                                     const onion_ui_event_v1 *event,
                                     plugin_ui_action *out_action) {
    if (handle == 0 || !event || !out_action) return ONION_E_INVALID_ARGUMENT;
    memset(out_action, 0, sizeof(*out_action));
    if (event->handle != handle ||
        strcmp(event->contribution_id, CONTRIBUTION_ID) != 0) {
        return ONION_E_NOT_FOUND;
    }
    if (strcmp(event->node_id, "scan_now") != 0) return ONION_E_NOT_FOUND;
    if (event->value_type != ONION_UI_VALUE_NONE) {
        return ONION_E_INVALID_ARGUMENT;
    }
    out_action->kind = PLUGIN_UI_ACTION_SCAN_NOW;
    return ONION_OK;
}

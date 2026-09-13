#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHADOWMOUNT_UI_SCAN_SLOTS 6
#define SHADOWMOUNT_UI_SCAN_PATH_MAX 255

int shadowmount_paths_builtin_count(void);
int shadowmount_paths_effective_count(void);
int shadowmount_paths_load_custom(
    char out[SHADOWMOUNT_UI_SCAN_SLOTS][SHADOWMOUNT_UI_SCAN_PATH_MAX + 1]);
bool shadowmount_paths_set_custom(int slot, const char *value);

#ifdef __cplusplus
}
#endif

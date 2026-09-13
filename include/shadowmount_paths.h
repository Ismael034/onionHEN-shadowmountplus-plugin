#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of custom scan-path slots exposed by the plugin UI. This is a UI
 * limit only; config.ini itself accepts far more scanpath= lines (see
 * MAX_SCAN_PATHS in third_party/ShadowMountPlus). */
#define SHADOWMOUNT_UI_SCAN_SLOTS 6
#define SHADOWMOUNT_UI_SCAN_PATH_MAX 255

/* Number of compile-time default scan roots (SM_DEFAULT_SCAN_PATHS_INITIALIZER). */
int shadowmount_paths_builtin_count(void);

/* Number of scan roots actually active right now (defaults/managed/custom,
 * however config.ini currently resolves them). Requires the runtime config
 * to have been loaded at least once. */
int shadowmount_paths_effective_count(void);

/* Reads the custom scanpath= lines currently in config.ini, in file order,
 * into out[0..SHADOWMOUNT_UI_SCAN_SLOTS). Unused slots are set to an empty
 * string. Returns the total number of scanpath= lines found, which may
 * exceed SHADOWMOUNT_UI_SCAN_SLOTS if the file has more than the UI shows
 * (e.g. hand-edited). */
int shadowmount_paths_load_custom(
    char out[SHADOWMOUNT_UI_SCAN_SLOTS][SHADOWMOUNT_UI_SCAN_PATH_MAX + 1]);

/* Sets custom scan-path slot `slot` (0-based, < SHADOWMOUNT_UI_SCAN_SLOTS) to
 * `value` by rewriting /data/shadowmount/config.ini in place:
 *   - An empty `value` clears/removes that slot's scanpath= line, if any.
 *   - A non-empty `value` must start with '/'; it replaces the slot's
 *     existing line, or appends a new one if the slot was empty.
 * Whenever the resulting custom scan-path count is greater than zero, this
 * also ensures scan_include_defaults=1 is set, so paths added from the UI
 * are additive to the built-in list rather than replacing it (matching what
 * a user adding "one more folder" from a settings screen expects).
 * Returns true on success; the file is left unmodified on failure. */
bool shadowmount_paths_set_custom(int slot, const char *value);

#ifdef __cplusplus
}
#endif

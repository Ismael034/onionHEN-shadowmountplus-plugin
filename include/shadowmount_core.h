#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void shadowmount_core_prepare(void);
int shadowmount_core_run(void);
void shadowmount_core_request_stop(void);
void shadowmount_core_request_scan(void);
/* Creates /data/shadowmount/config.ini from the bundled template if it does
 * not exist yet. Safe to call any time; a no-op once the file is present. */
void shadowmount_core_ensure_config_file(void);

#ifdef __cplusplus
}
#endif

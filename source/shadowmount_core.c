#include "shadowmount_core.h"

#include "sm_platform.h"

#include <pthread.h>
#include <stdatomic.h>
#include <sys/sysctl.h>

#include "sm_runtime.h"
#include "sm_types.h"
#include "sm_log.h"
#include "sm_shellcore_flags.h"
#include "sm_config_mount.h"
#include "sm_game_lifecycle.h"
#include "sm_kstuff.h"
#include "sm_mount_device.h"
#include "sm_filesystem.h"
#include "sm_image.h"
#include "sm_path_utils.h"
#include "sm_scan.h"
#include "sm_scanner.h"
#include "sm_time.h"
#include "sm_install.h"
#include "sm_appdb.h"
#include "sm_limits.h"
#include "sm_mdbg.h"
#include "sm_paths.h"

#ifndef SHADOWMOUNT_VERSION
#define SHADOWMOUNT_VERSION "unknown"
#endif

#define BACKPORK_PROCESS_NAME "backpork.elf"
#define BACKPORK_PROCESS_NAME_ALT "ps5-backpork.elf"
#define STOP_FILE_POLL_INTERVAL_US 3000000ull
#define KINFO_PID_OFFSET 72
#define KINFO_TDNAME_OFFSET 447

static volatile sig_atomic_t g_stop_requested = 0;
static atomic_bool g_shutdown_on_going_stop_requested = false;
static atomic_bool g_runtime_sleep_mode_active = false;
static _Atomic(uintptr_t) g_shutdown_stop_reason_bits = 0;
static atomic_uint_fast64_t g_next_stop_file_poll_us = 0;
static pthread_mutex_t g_runtime_mount_state_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  pthread_mutex_t reason_mutex;
  char reason[128];
} immediate_scan_request_t;

static immediate_scan_request_t g_scan_now = {
    .reason_mutex = PTHREAD_MUTEX_INITIALIZER,
    .reason = {0},
};

#ifndef SHADOWMOUNT_CONFIG_TEMPLATE
#define SHADOWMOUNT_CONFIG_TEMPLATE "config.ini.example"
#endif
#ifndef SHADOWMOUNT_NOTIFICATION_ICON
#define SHADOWMOUNT_NOTIFICATION_ICON "smp_icon.png"
#endif

extern const unsigned char config_ini_example[];
extern const unsigned int config_ini_example_len;
__asm__(".section .rodata\n"
        ".global config_ini_example\n"
        ".type config_ini_example, @object\n"
        ".align 16\n"
        "config_ini_example:\n"
        ".incbin \"" SHADOWMOUNT_CONFIG_TEMPLATE "\"\n"
        "config_ini_example_end:\n"
        ".global config_ini_example_len\n"
        ".type config_ini_example_len, @object\n"
        ".align 4\n"
        "config_ini_example_len:\n"
        ".int config_ini_example_end - config_ini_example\n");

extern const unsigned char smp_icon_png[];
extern const unsigned int smp_icon_png_len;
__asm__(".section .rodata\n"
        ".global smp_icon_png\n"
        ".type smp_icon_png, @object\n"
        ".align 16\n"
        "smp_icon_png:\n"
        ".incbin \"" SHADOWMOUNT_NOTIFICATION_ICON "\"\n"
        "smp_icon_png_end:\n"
        ".global smp_icon_png_len\n"
        ".type smp_icon_png_len, @object\n"
        ".align 4\n"
        "smp_icon_png_len:\n"
        ".int smp_icon_png_end - smp_icon_png\n");

/* Optional mdbg support uses a kernel export absent from older SDK stubs. */
__asm__(".weak C49jelxiaVE\n"
        ".set C49jelxiaVE, shadowmount_dbg_log_buffer_size_stub\n");
uint64_t shadowmount_dbg_log_buffer_size_stub(void) { return 0; }

bool should_stop_requested(void) {
  if (g_stop_requested)
    return true;

  uint64_t now_us = monotonic_time_us();
  if (now_us != 0) {
    uint64_t next_poll_us =
        atomic_load_explicit(&g_next_stop_file_poll_us, memory_order_acquire);
    if (next_poll_us != 0 && now_us < next_poll_us)
      return false;
    atomic_store_explicit(&g_next_stop_file_poll_us,
                          now_us + STOP_FILE_POLL_INTERVAL_US,
                          memory_order_release);
  }

  if (remove(KILL_FILE) == 0) {
    g_stop_requested = 1;
    return true;
  }
  return false;
}

void request_shutdown_stop(const char *reason) {
  const char *resolved_reason =
      (reason && reason[0] != '\0') ? reason : "unknown shutdown source";
  static char g_shutdown_stop_reason[128];
  bool already_requested =
      atomic_exchange_explicit(&g_shutdown_on_going_stop_requested, true,
                               memory_order_acq_rel);
  if (!already_requested) {
    (void)strlcpy(g_shutdown_stop_reason, resolved_reason,
                  sizeof(g_shutdown_stop_reason));
    atomic_store_explicit(&g_shutdown_stop_reason_bits,
                          (uintptr_t)g_shutdown_stop_reason,
                          memory_order_release);
    log_debug("[SHUTDOWN] requested by %s", g_shutdown_stop_reason);
  }
  g_stop_requested = 1;
  sm_scanner_wake();
  wake_game_lifecycle_watcher();
}

bool runtime_sleep_mode_active(void) {
  return atomic_load_explicit(&g_runtime_sleep_mode_active,
                              memory_order_acquire);
}

static void clear_scan_now_request(void) {
  pthread_mutex_lock(&g_scan_now.reason_mutex);
  g_scan_now.reason[0] = '\0';
  pthread_mutex_unlock(&g_scan_now.reason_mutex);
}

bool request_runtime_sleep_mode(bool active, const char *reason) {
  bool previous = atomic_exchange_explicit(&g_runtime_sleep_mode_active, active,
                                           memory_order_acq_rel);
  if (previous == active)
    return false;

  if (active)
    clear_scan_now_request();

  const char *resolved_reason =
      (reason && reason[0] != '\0') ? reason : "unknown sleep source";
  log_debug("[SLEEP] %s by %s", active ? "entered" : "left",
            resolved_reason);
  sm_scanner_wake();
  wake_game_lifecycle_watcher();
  return true;
}

void runtime_mount_state_lock(void) {
  pthread_mutex_lock(&g_runtime_mount_state_mutex);
}

void runtime_mount_state_unlock(void) {
  pthread_mutex_unlock(&g_runtime_mount_state_mutex);
}

void request_scan_now(const char *reason) {
  const char *resolved_reason =
      (reason && reason[0] != '\0') ? reason : "unknown scan source";
  bool resume_scan =
      strcmp(resolved_reason, "SceSystemStateMgrInfo=WORKING") == 0;
  if (runtime_sleep_mode_active() && !resume_scan)
    return;

  char log_reason[sizeof(g_scan_now.reason)];
  bool should_log = !resume_scan;

  pthread_mutex_lock(&g_scan_now.reason_mutex);
  if (g_scan_now.reason[0] == '\0') {
    (void)strlcpy(g_scan_now.reason, resolved_reason, sizeof(g_scan_now.reason));
    (void)strlcpy(log_reason, g_scan_now.reason, sizeof(log_reason));
  } else {
    should_log = false;
  }
  pthread_mutex_unlock(&g_scan_now.reason_mutex);

  if (should_log)
    log_debug("[SCAN] immediate scan requested by %s", log_reason);
  sm_scanner_wake();
}

bool consume_scan_now_request(char *reason_out, size_t reason_out_size) {
  if (reason_out && reason_out_size > 0)
    reason_out[0] = '\0';
  pthread_mutex_lock(&g_scan_now.reason_mutex);
  if (g_scan_now.reason[0] == '\0') {
    pthread_mutex_unlock(&g_scan_now.reason_mutex);
    return false;
  }
  if (reason_out && reason_out_size > 0)
    (void)strlcpy(reason_out, g_scan_now.reason, reason_out_size);
  g_scan_now.reason[0] = '\0';
  pthread_mutex_unlock(&g_scan_now.reason_mutex);
  return true;
}

bool sleep_with_stop_check(unsigned int total_us) {
  const unsigned int chunk_us = 200000;
  unsigned int slept = 0;
  while (slept < total_us) {
    if (should_stop_requested())
      return true;
    unsigned int remain = total_us - slept;
    unsigned int step = remain < chunk_us ? remain : chunk_us;
    sceKernelUsleep(step);
    slept += step;
  }
  return should_stop_requested();
}

static void get_firmware_version_string(char out[32]) {
  uint32_t fw = kernel_get_fw_version();
  uint32_t major_bcd = (fw >> 24) & 0xFFu;
  uint32_t minor_bcd = (fw >> 16) & 0xFFu;
  uint32_t major =
      ((major_bcd >> 4) & 0xFu) * 10u + (major_bcd & 0xFu);
  uint32_t minor =
      ((minor_bcd >> 4) & 0xFu) * 10u + (minor_bcd & 0xFu);

  if (major == 0 && minor == 0) {
    (void)strlcpy(out, "unknown", 32);
    return;
  }

  snprintf(out, 32, "%u.%02u", major, minor);
}

pid_t find_pid_by_name(const char *name, bool exclude_self) {
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  size_t buf_size = 0;
  if (sysctl(mib, 4, NULL, &buf_size, NULL, 0) != 0)
    return -1;
  if (buf_size == 0)
    return 0;

  uint8_t *buf = malloc(buf_size);
  if (!buf)
    return -1;

  if (sysctl(mib, 4, buf, &buf_size, NULL, 0) != 0) {
    free(buf);
    return -1;
  }

  pid_t mypid = exclude_self ? getpid() : -1;
  pid_t found_pid = 0;
  uint8_t *ptr = buf;
  uint8_t *end = buf + buf_size;
  while (ptr < end) {
    int ki_structsize = *(int *)ptr;
    pid_t ki_pid = *(pid_t *)&ptr[KINFO_PID_OFFSET];
    const char *ki_tdname = (const char *)&ptr[KINFO_TDNAME_OFFSET];
    ptr += ki_structsize;
    if ((!exclude_self || ki_pid != mypid) && strcmp(ki_tdname, name) == 0) {
      found_pid = ki_pid;
      break;
    }
  }

  free(buf);
  return found_pid;
}

static void log_non_empty_scan_paths(void) {
  for (int i = 0; i < get_scan_path_count(); i++) {
    const char *scan_path = get_scan_path(i);
    DIR *d = opendir(scan_path);
    if (!d)
      continue;

    bool non_empty = false;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
      if ((entry->d_name[0] == '.' && entry->d_name[1] == '\0') ||
          (entry->d_name[0] == '.' && entry->d_name[1] == '.' &&
           entry->d_name[2] == '\0')) {
        continue;
      }
      non_empty = true;
      break;
    }
    closedir(d);

    if (non_empty)
      log_fs_stats("SCAN", scan_path, NULL);
  }
}

static void ensure_kstuff_noautomount_file(void) {
  if (path_exists(KSTUFF_NOAUTOMOUNT_FILE))
    return;

  int fd = open(KSTUFF_NOAUTOMOUNT_FILE, O_RDONLY | O_CREAT, 0666);
  if (fd >= 0) {
    close(fd);
    printf("[KSTUFF] Created startup sentinel: %s\n",
           KSTUFF_NOAUTOMOUNT_FILE);
    return;
  }

  printf("[KSTUFF] Failed to create %s: %s\n", KSTUFF_NOAUTOMOUNT_FILE,
         strerror(errno));
}

static bool write_buffer_to_fd(int fd, const unsigned char *buf, size_t size) {
  size_t offset = 0;
  while (offset < size) {
    ssize_t written = write(fd, buf + offset, size - offset);
    if (written < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    if (written == 0) {
      errno = EIO;
      return false;
    }
    offset += (size_t)written;
  }
  return true;
}

static void ensure_runtime_config_file(void) {
  int fd = open(CONFIG_FILE, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (fd < 0) {
    if (errno == EEXIST)
      return;
    printf("[CFG] Failed to create %s: %s\n", CONFIG_FILE, strerror(errno));
    return;
  }

  size_t template_size = (size_t)config_ini_example_len;
  int saved_errno = 0;
  if (!write_buffer_to_fd(fd, config_ini_example, template_size))
    saved_errno = errno;
  if (close(fd) != 0 && saved_errno == 0)
    saved_errno = errno;

  if (saved_errno != 0) {
    errno = saved_errno;
    printf("[CFG] Failed to write %s: %s\n", CONFIG_FILE, strerror(errno));
    (void)unlink(CONFIG_FILE);
    return;
  }

  printf("[CFG] Created default config from template: %s\n", CONFIG_FILE);
}

static void cleanup_kstuff_noautomount_files(void) {
  if (unlink(KSTUFF_NOAUTOMOUNT_FILE) == 0) {
    log_debug("[KSTUFF] removed shutdown sentinel: %s",
              KSTUFF_NOAUTOMOUNT_FILE);
  } else if (errno != ENOENT) {
    log_debug("[KSTUFF] failed to remove %s: %s", KSTUFF_NOAUTOMOUNT_FILE,
              strerror(errno));
  }
}

static void stop_conflicting_backpork(void) {
  if (!runtime_config()->backport_fakelib_enabled)
    return;

  const char *names[] = {BACKPORK_PROCESS_NAME, BACKPORK_PROCESS_NAME_ALT};
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    while (true) {
      pid_t pid = find_pid_by_name(names[i], false);
      if (pid <= 0)
        break;

      if (kill(pid, SIGKILL) != 0) {
        if (errno != ESRCH) {
          log_debug("  [FAKELIB] failed to stop %s pid=%ld: %s", names[i],
                    (long)pid, strerror(errno));
        }
        break;
      }

      log_debug("  [FAKELIB] stopped conflicting %s pid=%ld", names[i],
                (long)pid);
      sceKernelUsleep(100000);
    }
  }
}

void shadowmount_core_prepare(void) {
  g_stop_requested = 0;
  atomic_store_explicit(&g_shutdown_on_going_stop_requested, false,
                        memory_order_release);
  atomic_store_explicit(&g_runtime_sleep_mode_active, false,
                        memory_order_release);
  atomic_store_explicit(&g_shutdown_stop_reason_bits, 0, memory_order_release);
  atomic_store_explicit(&g_next_stop_file_poll_us, 0, memory_order_release);
  clear_scan_now_request();
}

int shadowmount_core_run(void) {
  sceUserServiceInitialize(0);
  sceAppInstUtilInitialize();

  mkdir(LOG_DIR, 0777);
  ensure_runtime_config_file();
  ensure_kstuff_noautomount_file();
  syscall(SYS_thr_set_name, -1, "shadowmount-core");

  if (remove(KILL_FILE) == 0) {
    printf("[STOP] Cleared stale stop flag at startup: %s\n", KILL_FILE);
  } else if (errno != ENOENT) {
    printf("[STOP] Could not clear %s: %s\n", KILL_FILE, strerror(errno));
  }

  (void)unlink(LOG_FILE_PREV);
  (void)rename(LOG_FILE, LOG_FILE_PREV);
  if (!sm_scanner_init())
    log_debug("  [SCAN] scanner service init incomplete; steady-state scanner will stop if initialization cannot be completed");

  char firmware_version[32];
  get_firmware_version_string(firmware_version);
  log_debug(
      "ShadowMount+ v%s exFAT/UFS/PFS/LVD/MD. "
      "FW: %s. "
      "Build: %s %s. "
      "Thx to VoidWhisper/Gezine/Earthonion/EchoStretch/Drakmor",
      SHADOWMOUNT_VERSION, firmware_version, __DATE__, __TIME__);
  load_runtime_config();
  sm_notifications_init();
  stop_conflicting_backpork();
  if (!sm_shellcore_flags_start())
    log_debug("  [SHELLFLAG] monitor unavailable");
  sm_mdbg_init();
  sm_kstuff_init();
  if (!refresh_game_lifecycle_watcher())
    log_debug("  [GAME] lifecycle watcher unavailable");

  if (mkdir("/system_ex/app", 0777) != 0 && errno != EEXIST) {
    log_debug("  [MOUNT] failed to create /system_ex/app: %s", strerror(errno));
  }
  if (remount_system_ex() != 0) {
    log_debug("  [MOUNT] remount_system_ex failed: %s", strerror(errno));
  }

  notify_system("ShadowMount+ v%s exFAT/UFS/PFS", SHADOWMOUNT_VERSION);
  log_non_empty_scan_paths();

  if (runtime_config()->legacy_recursive_scan_forced) {
    notify_system_info("ShadowMount+: recursive_scan=1 deprecated, using scan_depth=2.");
  } else if (runtime_config()->scan_depth > 1u) {
    notify_system_info("ShadowMount+: scan depth %u enabled.",
                       runtime_config()->scan_depth);
  }

  cleanup_mount_dirs();
  if (!wait_for_lvd_release()) {
    log_debug("[SHUTDOWN] stop requested while waiting /dev/lvd2 release");
    goto shutdown;
  }

  log_debug("[STARTUP] cleanup_staged_mount_links begin");
  cleanup_staged_mount_links();
  log_debug("[STARTUP] cleanup_duplicate_title_mounts begin");
  cleanup_duplicate_title_mounts();
  if (!app_db_run_startup_maintenance())
    log_debug("  [DB] startup snd0info maintenance unavailable");
  log_debug("[STARTUP] scanner startup sync begin");
  if (!sm_scanner_run_startup_sync()) {
    log_debug("[STARTUP] scanner startup sync aborted");
    goto shutdown;
  }
  log_debug("[STARTUP] scanner startup sync done");
  sm_scanner_run_loop();

shutdown:
  sm_shellcore_flags_stop();
  stop_game_lifecycle_watcher();
  sm_scanner_shutdown();
  sm_kstuff_shutdown();
  sm_mdbg_shutdown();
  cleanup_kstuff_noautomount_files();
  shutdown_title_mounts();
  if (!shutdown_image_mounts()) {
    log_debug("[SHUTDOWN] some image mounts or devices were not fully released");
  }
  shutdown_app_db();

  if (atomic_load_explicit(&g_shutdown_on_going_stop_requested,
                           memory_order_acquire)) {
    const char *shutdown_reason =
        (const char *)atomic_load_explicit(&g_shutdown_stop_reason_bits,
                                           memory_order_acquire);
    log_debug("[SHUTDOWN] cleanup complete for %s",
              shutdown_reason ? shutdown_reason : "unknown shutdown source");
  }

  sm_log_shutdown();
  sceUserServiceTerminate();
  return 0;
}

void shadowmount_core_request_stop(void) {
  request_shutdown_stop("OnionHEN plugin lifecycle");
}

void shadowmount_core_request_scan(void) {
  request_scan_now("OnionHEN plugin UI");
}

#include "shadowmount_paths.h"

#include "shadowmount_core.h"

#include "sm_config_mount.h"
#include "sm_paths.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static const char *const k_builtin_scan_paths[] = SM_DEFAULT_SCAN_PATHS_INITIALIZER;

/* ---- small line-oriented editor for /data/shadowmount/config.ini ---- */

typedef struct {
    char **lines;
    int count;
    int capacity;
} line_list_t;

static char *dup_str(const char *text) {
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy) memcpy(copy, text, length);
    return copy;
}

static bool line_list_reserve(line_list_t *list) {
    if (list->count < list->capacity) return true;
    int capacity = list->capacity ? list->capacity * 2 : 64;
    char **grown = realloc(list->lines, (size_t)capacity * sizeof(char *));
    if (!grown) return false;
    list->lines = grown;
    list->capacity = capacity;
    return true;
}

static bool line_list_push(line_list_t *list, const char *text) {
    if (!line_list_reserve(list)) return false;
    char *copy = dup_str(text);
    if (!copy) return false;
    list->lines[list->count++] = copy;
    return true;
}

static bool line_list_insert_at(line_list_t *list, int index, const char *text) {
    if (!line_list_reserve(list)) return false;
    char *copy = dup_str(text);
    if (!copy) return false;
    memmove(&list->lines[index + 1], &list->lines[index],
            (size_t)(list->count - index) * sizeof(char *));
    list->lines[index] = copy;
    list->count++;
    return true;
}

static void line_list_remove_at(line_list_t *list, int index) {
    free(list->lines[index]);
    memmove(&list->lines[index], &list->lines[index + 1],
            (size_t)(list->count - index - 1) * sizeof(char *));
    list->count--;
}

static void line_list_free(line_list_t *list) {
    for (int i = 0; i < list->count; i++) free(list->lines[i]);
    free(list->lines);
    list->lines = NULL;
    list->count = 0;
    list->capacity = 0;
}

static bool read_config_lines(line_list_t *out) {
    memset(out, 0, sizeof(*out));
    FILE *f = fopen(CONFIG_FILE, "r");
    if (!f) return errno == ENOENT;

    char line[600];
    bool ok = true;
    while (ok && fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        ok = line_list_push(out, line);
    }
    fclose(f);
    if (!ok) line_list_free(out);
    return ok;
}

static bool write_all(int fd, const char *buf, size_t size) {
    size_t offset = 0;
    while (offset < size) {
        ssize_t written = write(fd, buf + offset, size - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
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

static bool write_config_lines(const line_list_t *list) {
    char tmp_path[600];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", CONFIG_FILE);

    int fd = open(tmp_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) return false;

    bool ok = true;
    for (int i = 0; ok && i < list->count; i++) {
        ok = write_all(fd, list->lines[i], strlen(list->lines[i])) &&
             write_all(fd, "\n", 1);
    }
    if (close(fd) != 0) ok = false;
    if (!ok) {
        (void)unlink(tmp_path);
        return false;
    }
    if (rename(tmp_path, CONFIG_FILE) != 0) {
        (void)unlink(tmp_path);
        return false;
    }
    return true;
}

/* ---- config.ini directive parsing (independent of the vendored parser,
 * which keeps only its own merged runtime state, not raw line positions) ---- */

static char *trim_inplace(char *text) {
    while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
        text++;
    size_t len = strlen(text);
    while (len > 0) {
        char c = text[len - 1];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
        text[--len] = '\0';
    }
    return text;
}

static bool split_active_directive(const char *raw_line, char *key_out,
                                   size_t key_out_size, char *value_out,
                                   size_t value_out_size) {
    char buf[600];
    snprintf(buf, sizeof(buf), "%s", raw_line);
    char *trimmed = trim_inplace(buf);
    if (trimmed[0] == '\0' || trimmed[0] == '#' || trimmed[0] == ';' ||
        trimmed[0] == '[') {
        return false;
    }
    char *eq = strchr(trimmed, '=');
    if (!eq) return false;
    *eq = '\0';
    char *key = trim_inplace(trimmed);
    char *value = trim_inplace(eq + 1);
    if (key[0] == '\0') return false;
    snprintf(key_out, key_out_size, "%s", key);
    snprintf(value_out, value_out_size, "%s", value);
    return true;
}

static bool line_is_scanpath(const char *raw_line, char *value_out,
                             size_t value_out_size) {
    char key[64];
    char value[600];
    if (!split_active_directive(raw_line, key, sizeof(key), value, sizeof(value)))
        return false;
    if (strcasecmp(key, "scanpath") != 0 || value[0] == '\0') return false;
    snprintf(value_out, value_out_size, "%s", value);
    return true;
}

static bool line_is_scan_include_defaults(const char *raw_line, bool *enabled_out) {
    char key[64];
    char value[64];
    if (!split_active_directive(raw_line, key, sizeof(key), value, sizeof(value)))
        return false;
    if (strcasecmp(key, "scan_include_defaults") != 0) return false;
    *enabled_out = strcasecmp(value, "1") == 0 || strcasecmp(value, "true") == 0 ||
                   strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0;
    return true;
}

static bool normalize_path_value(const char *input, char *out, size_t out_size) {
    char buf[SHADOWMOUNT_UI_SCAN_PATH_MAX + 16];
    snprintf(buf, sizeof(buf), "%s", input);
    char *trimmed = trim_inplace(buf);
    size_t len = strlen(trimmed);
    if (len == 0 || len > SHADOWMOUNT_UI_SCAN_PATH_MAX || trimmed[0] != '/')
        return false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)trimmed[i];
        if (c < 0x20 || c == 0x7f) return false;
    }
    while (len > 1 && trimmed[len - 1] == '/') trimmed[--len] = '\0';
    snprintf(out, out_size, "%s", trimmed);
    return true;
}

/* ---- public API ---- */

int shadowmount_paths_builtin_count(void) {
    int count = 0;
    while (k_builtin_scan_paths[count] != NULL) count++;
    return count;
}

int shadowmount_paths_effective_count(void) {
    return get_scan_path_count();
}

int shadowmount_paths_load_custom(
    char out[SHADOWMOUNT_UI_SCAN_SLOTS][SHADOWMOUNT_UI_SCAN_PATH_MAX + 1]) {
    for (int i = 0; i < SHADOWMOUNT_UI_SCAN_SLOTS; i++) out[i][0] = '\0';

    line_list_t lines;
    if (!read_config_lines(&lines)) return 0;

    int found = 0;
    for (int i = 0; i < lines.count; i++) {
        char value[600];
        if (!line_is_scanpath(lines.lines[i], value, sizeof(value))) continue;
        if (found < SHADOWMOUNT_UI_SCAN_SLOTS)
            snprintf(out[found], SHADOWMOUNT_UI_SCAN_PATH_MAX + 1, "%s", value);
        found++;
    }
    line_list_free(&lines);
    return found;
}

bool shadowmount_paths_set_custom(int slot, const char *value) {
    if (slot < 0 || slot >= SHADOWMOUNT_UI_SCAN_SLOTS) return false;

    const bool clearing = (value == NULL || value[0] == '\0');
    char normalized[SHADOWMOUNT_UI_SCAN_PATH_MAX + 1];
    if (!clearing && !normalize_path_value(value, normalized, sizeof(normalized)))
        return false;

    shadowmount_core_ensure_config_file();

    line_list_t lines;
    if (!read_config_lines(&lines)) return false;

    int scanpath_line_index[SHADOWMOUNT_UI_SCAN_SLOTS];
    for (int i = 0; i < SHADOWMOUNT_UI_SCAN_SLOTS; i++) scanpath_line_index[i] = -1;
    int scanpath_total = 0;
    int last_scanpath_line = -1;
    for (int i = 0; i < lines.count; i++) {
        char existing_value[600];
        if (!line_is_scanpath(lines.lines[i], existing_value, sizeof(existing_value)))
            continue;
        if (scanpath_total < SHADOWMOUNT_UI_SCAN_SLOTS)
            scanpath_line_index[scanpath_total] = i;
        scanpath_total++;
        last_scanpath_line = i;
    }

    const int target_line = (slot < scanpath_total) ? scanpath_line_index[slot] : -1;
    bool ok = true;

    if (clearing) {
        if (target_line != -1) {
            line_list_remove_at(&lines, target_line);
            scanpath_total--;
        }
    } else {
        char new_line[600];
        snprintf(new_line, sizeof(new_line), "scanpath=%s", normalized);
        if (target_line != -1) {
            free(lines.lines[target_line]);
            lines.lines[target_line] = dup_str(new_line);
            ok = lines.lines[target_line] != NULL;
        } else {
            const int insert_at =
                (last_scanpath_line != -1) ? last_scanpath_line + 1 : lines.count;
            ok = line_list_insert_at(&lines, insert_at, new_line);
            if (ok) scanpath_total++;
        }
    }

    if (ok && scanpath_total > 0) {
        bool found_flag = false;
        for (int i = 0; i < lines.count && ok; i++) {
            bool enabled = false;
            if (!line_is_scan_include_defaults(lines.lines[i], &enabled)) continue;
            found_flag = true;
            if (!enabled) {
                free(lines.lines[i]);
                lines.lines[i] = dup_str("scan_include_defaults=1");
                ok = lines.lines[i] != NULL;
            }
            break;
        }
        if (ok && !found_flag)
            ok = line_list_push(&lines, "scan_include_defaults=1");
    }

    if (ok) ok = write_config_lines(&lines);
    line_list_free(&lines);
    return ok;
}

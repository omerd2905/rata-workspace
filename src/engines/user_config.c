#include "engines/user_config.h"

#include <stdio.h>
#include <string.h>

static size_t trimmed_dir_len(const char *s) {
    size_t n = strlen(s);
    while (n > 1u && (s[n - 1u] == '/' || s[n - 1u] == '\\')) {
        n--;
    }
    return n;
}

RataStatus user_config_resolve_dir(bool is_windows,
                                   const char *xdg_config_home,
                                   const char *home_dir,
                                   const char *appdata_dir,
                                   char *dir_out,
                                   size_t dir_cap) {
    if (dir_out == NULL || dir_cap < 32u) {
        return RATA_ERR_INVALID_ARG;
    }
    dir_out[0] = '\0';

    if (is_windows) {
        if (appdata_dir != NULL && appdata_dir[0] != '\0') {
            size_t base_len = trimmed_dir_len(appdata_dir);
            int n = snprintf(dir_out, dir_cap, "%.*s\\RataWorkspace",
                             (int)base_len, appdata_dir);
            return (n > 0 && (size_t)n < dir_cap) ? RATA_OK : RATA_ERR_RANGE;
        }
        return RATA_ERR_IO;
    }

    if (xdg_config_home != NULL && xdg_config_home[0] != '\0') {
        size_t base_len = trimmed_dir_len(xdg_config_home);
        int n = snprintf(dir_out, dir_cap, "%.*s/rata-workspace",
                         (int)base_len, xdg_config_home);
        return (n > 0 && (size_t)n < dir_cap) ? RATA_OK : RATA_ERR_RANGE;
    }

    if (home_dir != NULL && home_dir[0] != '\0') {
        size_t base_len = trimmed_dir_len(home_dir);
        int n = snprintf(dir_out, dir_cap, "%.*s/.config/rata-workspace",
                         (int)base_len, home_dir);
        return (n > 0 && (size_t)n < dir_cap) ? RATA_OK : RATA_ERR_RANGE;
    }

    return RATA_ERR_IO;
}

RataStatus user_config_join_path(bool is_windows,
                                 const char *dir,
                                 const char *filename,
                                 char *path_out,
                                 size_t path_cap) {
    if (dir == NULL || dir[0] == '\0' || filename == NULL || filename[0] == '\0' ||
        path_out == NULL || path_cap < 8u) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t base_len = trimmed_dir_len(dir);
    char sep = is_windows ? '\\' : '/';
    int n = snprintf(path_out, path_cap, "%.*s%c%s", (int)base_len, dir, sep, filename);
    if (n <= 0 || (size_t)n >= path_cap) {
        path_out[0] = '\0';
        return RATA_ERR_RANGE;
    }
    return RATA_OK;
}

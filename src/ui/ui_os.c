#include "ui/ui_os.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "engines/user_config.h"

#ifdef _WIN32
#include <direct.h>
#define RATA_IS_WINDOWS true
static int make_one_dir(const char *p) {
    return _mkdir(p);
}
#else
#include <sys/stat.h>
#include <sys/types.h>
#define RATA_IS_WINDOWS false
static int make_one_dir(const char *p) {
    return mkdir(p, 0755);
}
#endif

RataStatus ui_os_config_dir(char *dir_out, size_t dir_cap) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");
    const char *appdata = getenv("APPDATA");
    return user_config_resolve_dir(RATA_IS_WINDOWS, xdg, home, appdata, dir_out, dir_cap);
}

RataStatus ui_os_config_file(const char *filename, char *path_out, size_t path_cap) {
    char dir[RATA_PATH_MAX];
    RataStatus st = ui_os_config_dir(dir, sizeof(dir));
    if (st != RATA_OK) {
        return st;
    }
    return user_config_join_path(RATA_IS_WINDOWS, dir, filename, path_out, path_cap);
}

RataStatus ui_os_mkdir_p(const char *dir_path) {
    if (dir_path == NULL || dir_path[0] == '\0') {
        return RATA_ERR_INVALID_ARG;
    }
    size_t len = strlen(dir_path);
    if (len >= RATA_PATH_MAX) {
        return RATA_ERR_RANGE;
    }
    char tmp[RATA_PATH_MAX];
    memcpy(tmp, dir_path, len + 1u);

    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            /* Skip Windows drive root like "C:\". */
            if (i == 2u && tmp[1] == ':') {
                continue;
            }
            char saved = tmp[i];
            tmp[i] = '\0';
            if (make_one_dir(tmp) != 0 && errno != EEXIST) {
                return RATA_ERR_IO;
            }
            tmp[i] = saved;
        }
    }
    if (make_one_dir(tmp) != 0 && errno != EEXIST) {
        return RATA_ERR_IO;
    }
    return RATA_OK;
}

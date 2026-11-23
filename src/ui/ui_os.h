#ifndef RATA_UI_OS_H
#define RATA_UI_OS_H

#include <stddef.h>

#include "core/status.h"

/* Resolves per-user config directory using OS environment variables
 * (%APPDATA%\RataWorkspace on Windows; $XDG_CONFIG_HOME/rata-workspace or
 * $HOME/.config/rata-workspace on Linux). Never resolves next to the executable. */
RataStatus ui_os_config_dir(char *dir_out, size_t dir_cap);

/* Resolves a file path inside the per-user config directory. */
RataStatus ui_os_config_file(const char *filename, char *path_out, size_t path_cap);

/* Creates directory and missing parent directories if needed (idempotent). */
RataStatus ui_os_mkdir_p(const char *dir_path);

#endif /* RATA_UI_OS_H */

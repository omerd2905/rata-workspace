#ifndef RATA_ENGINES_USER_CONFIG_H
#define RATA_ENGINES_USER_CONFIG_H

#include <stdbool.h>
#include <stddef.h>

#include "core/status.h"

#define RATA_PATH_MAX 256u

/* Pure C11 per-user path resolver (zero OS headers; caller supplies getenv values).
 * On Windows:  %APPDATA%\RataWorkspace
 * On Linux:    $XDG_CONFIG_HOME/rata-workspace  (or $HOME/.config/rata-workspace)
 * Never resolves next to the executable. */
RataStatus user_config_resolve_dir(bool is_windows,
                                   const char *xdg_config_home,
                                   const char *home_dir,
                                   const char *appdata_dir,
                                   char *dir_out,
                                   size_t dir_cap);

/* Joins `dir` and `filename` with platform separator into caller-owned `path_out`. */
RataStatus user_config_join_path(bool is_windows,
                                 const char *dir,
                                 const char *filename,
                                 char *path_out,
                                 size_t path_cap);

#endif /* RATA_ENGINES_USER_CONFIG_H */

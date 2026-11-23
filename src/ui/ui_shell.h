#ifndef RATA_UI_SHELL_H
#define RATA_UI_SHELL_H

#include <stdbool.h>

#include "core/arena.h"
#include "engines/user_config.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"
#include "ui/ui_workspace.h"

#ifndef RATA_VERSION
#define RATA_VERSION "0.1.0-dev"
#endif

/* Top-level shell state (sidebar switcher, project file bar, About dialog, toast banner).
 * Owned by caller (no hidden global state). */
typedef struct RataShell {
    RataTheme theme;
    RataWorkspaceRegistry registry;
    RataWorkspaceId active_ws;
    UiInputFocus focus;
    char config_dir[RATA_PATH_MAX];
    char project_path[RATA_PATH_MAX];
    char toast_msg[200];
    RataBannerLevel toast_level;
    double toast_timer;
    bool show_about;
} RataShell;

void ui_shell_init(RataShell *shell);

/* Loads/saves lightweight user preferences inside the per-user config directory.
 * Missing settings file on first run is silently ignored (zero setup required). */
void ui_shell_load_settings(RataShell *shell, RataArena *scratch);
void ui_shell_save_settings(const RataShell *shell, RataArena *scratch);

void ui_shell_update_and_draw(RataShell *shell, RataArena *frame_arena, double dt);

#endif /* RATA_UI_SHELL_H */

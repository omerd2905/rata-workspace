#ifndef RATA_UI_SHELL_H
#define RATA_UI_SHELL_H

#include "core/arena.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"
#include "ui/ui_workspace.h"

/* Top-level shell state (sidebar switcher, project file bar, toast banner).
 * Owned by caller (no hidden global state). */
typedef struct RataShell {
    RataTheme theme;
    RataWorkspaceRegistry registry;
    RataWorkspaceId active_ws;
    UiInputFocus focus;
    char project_path[128];
    char toast_msg[160];
    RataBannerLevel toast_level;
    double toast_timer;
} RataShell;

void ui_shell_init(RataShell *shell);
void ui_shell_update_and_draw(RataShell *shell, RataArena *frame_arena, double dt);

#endif /* RATA_UI_SHELL_H */

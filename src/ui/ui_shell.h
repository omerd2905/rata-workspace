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

typedef enum RataUpdateUiState {
    RATA_UPD_IDLE = 0,
    RATA_UPD_CHECKING = 1,
    RATA_UPD_UP_TO_DATE = 2,
    RATA_UPD_READY = 3,
    RATA_UPD_ERROR = 4
} RataUpdateUiState;

typedef struct RataUpdateUiStatus {
    bool supported;       /* True on Windows; false on Linux/Nix */
    bool is_nix_store;    /* True when running from /nix/store */
    bool from_manual;     /* True when last result came from explicit user check */
    RataUpdateUiState state;
    char ready_version[32];
    char message[192];
} RataUpdateUiStatus;

typedef void (*RataUpdatePollFn)(void *user_data, RataUpdateUiStatus *out);
typedef void (*RataUpdateTriggerFn)(void *user_data);

/* Top-level shell state (sidebar switcher, project file bar, About/Updates dialog,
 * non-blocking update banner, toast banner). Owned by caller (no hidden global state). */
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

    /* Auto-update UI state & callbacks (decoupled from src/app/ to preserve layering). */
    bool auto_update;
    bool update_banner_dismissed;
    bool show_restart_update_modal;
    bool request_restart_for_update;
    RataUpdateUiState last_seen_upd_state;
    RataUpdateUiStatus update_status;
    void *updater_ctx;
    RataUpdatePollFn updater_poll_fn;
    RataUpdateTriggerFn updater_check_fn;
} RataShell;

void ui_shell_init(RataShell *shell);

/* Connects app-layer updater polling/trigger callbacks without violating UI->App layering. */
void ui_shell_set_updater(RataShell *shell,
                          void *ctx,
                          RataUpdatePollFn poll_fn,
                          RataUpdateTriggerFn check_fn);

/* Loads/saves lightweight user preferences inside the per-user config directory.
 * Missing settings file on first run is silently ignored (zero setup required). */
void ui_shell_load_settings(RataShell *shell, RataArena *scratch);
void ui_shell_save_settings(const RataShell *shell, RataArena *scratch);

void ui_shell_update_and_draw(RataShell *shell, RataArena *frame_arena, double dt);

#endif /* RATA_UI_SHELL_H */

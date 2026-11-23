#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_updater.h"
#include "core/arena.h"
#include "raylib.h"
#include "ui/ui_ratawave.h"
#include "ui/ui_shell.h"
#include "ui/ui_theme.h"

static void shell_poll_updater(void *user_data, RataUpdateUiStatus *out) {
    AppUpdater *u = (AppUpdater *)user_data;
    if (u == NULL || out == NULL) {
        return;
    }
    AppUpdateSnapshot snap;
    app_updater_get_snapshot(u, &snap);
    out->supported = snap.supported;
    out->is_nix_store = snap.is_nix_store;
    out->from_manual = snap.from_manual;
    out->state = (RataUpdateUiState)snap.state;
    snprintf(out->ready_version, sizeof(out->ready_version), "%s", snap.ready_version);
    snprintf(out->message, sizeof(out->message), "%s", snap.message);
}

static void shell_trigger_update_check(void *user_data) {
    AppUpdater *u = (AppUpdater *)user_data;
    app_updater_start_background_check(u, true);
}

int main(int argc, char **argv) {
    const char *argv0 = (argc > 0 && argv != NULL) ? argv[0] : "";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            app_console_attach_for_cli(false);
            printf("Rata Workspace %s\n", RATA_VERSION);
            fflush(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            app_console_attach_for_cli(false);
            printf("Rata Workspace %s — Electrical Engineering Toolkit\n"
                   "Usage:\n"
                   "  rata-workspace              Launch GUI application\n"
                   "  rata-workspace --version    Print application version\n"
                   "  rata-workspace --help       Print this help message\n"
                   "  rata-workspace --update     Check, verify, and install update (Windows)\n"
                   "  rata-workspace --set-token  Store GitHub read-only PAT (Windows)\n",
                   RATA_VERSION);
            fflush(stdout);
            return 0;
        }
        if (strcmp(argv[i], "--set-token") == 0) {
            app_console_attach_for_cli(true);
            AppUpdater *u = app_updater_create(argv0);
            int rc = app_updater_cli_set_token(u, argc, argv);
            app_updater_destroy(u);
            return rc;
        }
        if (strcmp(argv[i], "--update") == 0) {
            app_console_attach_for_cli(false);
            AppUpdater *u = app_updater_create(argv0);
            int rc = app_updater_cli_update(u);
            app_updater_destroy(u);
            return rc;
        }
    }

    AppUpdater *updater = app_updater_create(argv0);

    /* Apply any previously downloaded, signature-verified newer update BEFORE
     * opening the GUI window; exit immediately so the installer can relaunch us. */
    if (updater != NULL && app_updater_apply_pending_on_startup(updater)) {
        app_updater_destroy(updater);
        return 0;
    }

    /* 4 MiB per-frame scratch arena for DSP computation, plots, and formulas. */
    RataArena frame_arena;
    if (rata_arena_init_heap(&frame_arena, 4u * 1024u * 1024u) != RATA_OK) {
        fprintf(stderr, "rata-workspace: failed to allocate frame arena.\n");
        app_updater_destroy(updater);
        return 1;
    }

    RataShell *shell = (RataShell *)calloc(1u, sizeof(RataShell));
    RataWaveState *ratawave = (RataWaveState *)calloc(1u, sizeof(RataWaveState));
    if (shell == NULL || ratawave == NULL) {
        fprintf(stderr, "rata-workspace: failed to allocate application state.\n");
        free(ratawave);
        free(shell);
        rata_arena_destroy(&frame_arena);
        app_updater_destroy(updater);
        return 1;
    }

    ui_shell_init(shell);
    ui_workspace_register(&shell->registry,
                          WS_RATAWAVE,
                          ratawave,
                          ui_ratawave_init,
                          ui_ratawave_draw,
                          ui_ratawave_save,
                          ui_ratawave_load);
    ui_shell_load_settings(shell, &frame_arena);
    if (updater != NULL) {
        ui_shell_set_updater(shell, updater, shell_poll_updater, shell_trigger_update_check);
        if (shell->auto_update) {
            app_updater_start_background_check(updater, false);
        }
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 800, "Rata Workspace — Electrical Engineering Toolkit");
    if (!IsWindowReady()) {
        fprintf(stderr, "rata-workspace: failed to initialize OpenGL/GLFW window.\n");
        free(ratawave);
        free(shell);
        rata_arena_destroy(&frame_arena);
        app_updater_destroy(updater);
        return 1;
    }
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    ui_theme_load_font(&shell->theme);
    ui_theme_apply_raygui(&shell->theme);

    while (!WindowShouldClose() && !shell->request_restart_for_update) {
        double dt = (double)GetFrameTime();
        rata_arena_reset(&frame_arena);

        BeginDrawing();
        ui_shell_update_and_draw(shell, &frame_arena, dt);
        EndDrawing();
    }

    bool restart_for_update = shell->request_restart_for_update;
    ui_shell_save_settings(shell, &frame_arena);
    ui_theme_unload_font(&shell->theme);
    CloseWindow();

    if (restart_for_update && updater != NULL) {
        (void)app_updater_launch_pending(updater);
    }

    free(ratawave);
    free(shell);
    rata_arena_destroy(&frame_arena);
    app_updater_destroy(updater);
    return 0;
}

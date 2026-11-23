#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/arena.h"
#include "raylib.h"
#include "ui/ui_ratawave.h"
#include "ui/ui_shell.h"
#include "ui/ui_theme.h"

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("Rata Workspace %s\n", RATA_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Rata Workspace %s — Electrical Engineering Toolkit\n"
                   "Usage: rata-workspace [--version | --help]\n", RATA_VERSION);
            return 0;
        }
    }

    /* 4 MiB per-frame scratch arena for DSP computation, plots, and formulas. */
    RataArena frame_arena;
    if (rata_arena_init_heap(&frame_arena, 4u * 1024u * 1024u) != RATA_OK) {
        fprintf(stderr, "rata-workspace: failed to allocate frame arena.\n");
        return 1;
    }

    RataShell *shell = (RataShell *)calloc(1u, sizeof(RataShell));
    RataWaveState *ratawave = (RataWaveState *)calloc(1u, sizeof(RataWaveState));
    if (shell == NULL || ratawave == NULL) {
        fprintf(stderr, "rata-workspace: failed to allocate application state.\n");
        free(ratawave);
        free(shell);
        rata_arena_destroy(&frame_arena);
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

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1280, 800, "Rata Workspace — Electrical Engineering Toolkit");
    SetWindowMinSize(960, 600);
    SetTargetFPS(60);

    ui_theme_load_font(&shell->theme);
    ui_theme_apply_raygui(&shell->theme);

    while (!WindowShouldClose()) {
        double dt = (double)GetFrameTime();
        rata_arena_reset(&frame_arena);

        BeginDrawing();
        ui_shell_update_and_draw(shell, &frame_arena, dt);
        EndDrawing();
    }

    ui_shell_save_settings(shell, &frame_arena);
    ui_theme_unload_font(&shell->theme);
    CloseWindow();
    free(ratawave);
    free(shell);
    rata_arena_destroy(&frame_arena);
    return 0;
}

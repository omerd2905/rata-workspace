#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/arena.h"
#include "raylib.h"
#include "ui/ui_ratawave.h"
#include "ui/ui_shell.h"
#include "ui/ui_theme.h"

int main(int argc, char **argv) {
    const char *shot_dir = NULL;
    for (int i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], "--screenshot-dir") == 0) {
            shot_dir = argv[i + 1];
        }
    }

    RataArena arena;
    if (rata_arena_init_heap(&arena, 4u * 1024u * 1024u) != RATA_OK) {
        fprintf(stderr, "ui_smoke: arena init failed\n");
        return 1;
    }

    RataShell *shell = (RataShell *)calloc(1u, sizeof(RataShell));
    RataWaveState *rw = (RataWaveState *)calloc(1u, sizeof(RataWaveState));
    if (shell == NULL || rw == NULL) {
        free(rw);
        free(shell);
        rata_arena_destroy(&arena);
        return 1;
    }

    ui_shell_init(shell);
    ui_workspace_register(&shell->registry,
                          WS_RATAWAVE,
                          rw,
                          ui_ratawave_init,
                          ui_ratawave_draw,
                          ui_ratawave_save,
                          ui_ratawave_load);

    SetTraceLogLevel(LOG_WARNING);
    InitWindow(1280, 800, "Rata Workspace UI Smoke");
    SetTargetFPS(60);

    ui_theme_load_font(&shell->theme);
    ui_theme_apply_raygui(&shell->theme);

    if (!shell->theme.has_custom_font) {
        fprintf(stderr, "ui_smoke: FAIL embedded JetBrains Mono font failed to load\n");
        CloseWindow();
        free(rw);
        free(shell);
        rata_arena_destroy(&arena);
        return 1;
    }

    /* Verify Greek codepoints (pi=0x03C0, omega=0x03C9, Omega=0x03A9) exist in loaded atlas. */
    const int greek_check[3] = {0x03C0, 0x03C9, 0x03A9};
    for (int i = 0; i < 3; ++i) {
        int gi = GetGlyphIndex(shell->theme.font, greek_check[i]);
        if (gi < 0 || shell->theme.font.glyphs[gi].value != greek_check[i]) {
            fprintf(stderr, "ui_smoke: FAIL missing Greek glyph U+%04X\n", greek_check[i]);
            ui_theme_unload_font(&shell->theme);
            CloseWindow();
            free(rw);
            free(shell);
            rata_arena_destroy(&arena);
            return 1;
        }
    }

    char shot_path[512];
    for (int tab = 0; tab < RW_TAB_COUNT; ++tab) {
        shell->active_ws = WS_RATAWAVE;
        shell->show_about = false;
        rw->active_tab = tab;
        for (int f = 0; f < 3; ++f) {
            rata_arena_reset(&arena);
            BeginDrawing();
            ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
            EndDrawing();
        }
        if (shot_dir != NULL) {
            snprintf(shot_path, sizeof(shot_path), "%s/tab_%d.png", shot_dir, tab);
            Image img = LoadImageFromScreen();
            ExportImage(img, shot_path);
            UnloadImage(img);
        }
    }

    /* Exercise unit-circle pole clamping banner in LTI tab. */
    rw->active_tab = RW_TAB_LTI;
    snprintf(rw->lti_b_str, sizeof(rw->lti_b_str), "1");
    snprintf(rw->lti_a_str, sizeof(rw->lti_a_str), "1, 0, 1");
    for (int f = 0; f < 2; ++f) {
        rata_arena_reset(&arena);
        BeginDrawing();
        ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
        EndDrawing();
    }
    if (shot_dir != NULL) {
        snprintf(shot_path, sizeof(shot_path), "%s/lti_clamped.png", shot_dir);
        Image img = LoadImageFromScreen();
        ExportImage(img, shot_path);
        UnloadImage(img);
    }

    /* Exercise Coming Soon workspace and About modal. */
    shell->active_ws = WS_RATALOOP;
    shell->show_about = true;
    for (int f = 0; f < 2; ++f) {
        rata_arena_reset(&arena);
        BeginDrawing();
        ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
        EndDrawing();
    }
    if (shot_dir != NULL) {
        snprintf(shot_path, sizeof(shot_path), "%s/about.png", shot_dir);
        Image img = LoadImageFromScreen();
        ExportImage(img, shot_path);
        UnloadImage(img);
    }

    ui_theme_unload_font(&shell->theme);
    CloseWindow();
    free(rw);
    free(shell);
    rata_arena_destroy(&arena);
    printf("ui_smoke: PASSED (embedded JetBrains Mono + Greek glyphs, 6 RataWave tabs, pole-clamped LTI, Coming Soon, About).\n");
    return 0;
}

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
    if (!IsWindowReady()) {
        fprintf(stderr, "ui_smoke: FAIL OpenGL/GLFW window failed to initialize\n");
        free(rw);
        free(shell);
        rata_arena_destroy(&arena);
        return 1;
    }
    SetTargetFPS(60);

    ui_theme_load_font(&shell->theme);
    ui_theme_apply_raygui(&shell->theme);

    if (!shell->theme.has_custom_font) {
        fprintf(stderr, "ui_smoke: FAIL embedded Lora + JetBrains Mono font failed to load\n");
        CloseWindow();
        free(rw);
        free(shell);
        rata_arena_destroy(&arena);
        return 1;
    }

    /* Verify Lora + fallback Greek/math codepoints (pi=0x03C0, omega=0x03C9, Omega=0x03A9, arrow=0x2192). */
    const int glyph_check[4] = {0x03C0, 0x03C9, 0x03A9, 0x2192};
    for (int i = 0; i < 4; ++i) {
        int gi = GetGlyphIndex(shell->theme.font, glyph_check[i]);
        if (gi < 0 ||
            shell->theme.font.glyphs[gi].value != glyph_check[i] ||
            shell->theme.font.recs[gi].width <= 0.0f ||
            shell->theme.font.recs[gi].height <= 0.0f) {
            fprintf(stderr, "ui_smoke: FAIL missing or empty glyph U+%04X\n", glyph_check[i]);
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
        shell->open_menu = RATA_MENU_NONE;
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

    /* Exercise Convolution bidirectional solve modes: Solve h[n], Solve x[n], and Verify all 3. */
    rw->active_tab = RW_TAB_CONV;
    for (int sm = CONV_SOLVE_H; sm <= CONV_VERIFY_ALL; ++sm) {
        rw->conv_solve_mode = sm;
        ui_plot_view_reset(&rw->view_conv_top);
        ui_plot_view_reset(&rw->view_conv_bot);
        for (int f = 0; f < 2; ++f) {
            rata_arena_reset(&arena);
            BeginDrawing();
            ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
            EndDrawing();
        }
        if (shot_dir != NULL) {
            snprintf(shot_path, sizeof(shot_path), "%s/conv_mode_%d.png", shot_dir, sm);
            Image img = LoadImageFromScreen();
            ExportImage(img, shot_path);
            UnloadImage(img);
        }
    }
    rw->conv_solve_mode = CONV_SOLVE_Y;

    /* Exercise top-bar View menu dropdown and 135% UI zoom scaling. */
    rw->active_tab = RW_TAB_SIGNAL;
    shell->open_menu = RATA_MENU_VIEW;
    for (int f = 0; f < 2; ++f) {
        rata_arena_reset(&arena);
        BeginDrawing();
        ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
        EndDrawing();
    }
    if (shot_dir != NULL) {
        snprintf(shot_path, sizeof(shot_path), "%s/view_menu.png", shot_dir);
        Image img = LoadImageFromScreen();
        ExportImage(img, shot_path);
        UnloadImage(img);
    }

    shell->open_menu = RATA_MENU_NONE;
    ui_shell_set_scale(shell, 1.35f);
    for (int f = 0; f < 2; ++f) {
        rata_arena_reset(&arena);
        BeginDrawing();
        ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
        EndDrawing();
    }
    if (shot_dir != NULL) {
        snprintf(shot_path, sizeof(shot_path), "%s/scaled_135.png", shot_dir);
        Image img = LoadImageFromScreen();
        ExportImage(img, shot_path);
        UnloadImage(img);
    }
    ui_shell_set_scale(shell, 1.0f);

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

    /* Exercise Coming Soon workspace, update-ready banner, restart modal, and About modal. */
    shell->active_ws = WS_RATALOOP;
    shell->update_status.supported = true;
    shell->update_status.state = RATA_UPD_READY;
    snprintf(shell->update_status.ready_version, sizeof(shell->update_status.ready_version), "0.2.0");
    snprintf(shell->update_status.message, sizeof(shell->update_status.message),
             "v0.2.0 ready \xe2\x80\x94 restart to apply.");
    for (int f = 0; f < 2; ++f) {
        rata_arena_reset(&arena);
        BeginDrawing();
        ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
        EndDrawing();
    }
    if (shot_dir != NULL) {
        snprintf(shot_path, sizeof(shot_path), "%s/coming_soon.png", shot_dir);
        Image img = LoadImageFromScreen();
        ExportImage(img, shot_path);
        UnloadImage(img);
    }

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

    shell->show_about = false;
    shell->show_restart_update_modal = true;
    for (int f = 0; f < 2; ++f) {
        rata_arena_reset(&arena);
        BeginDrawing();
        ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
        EndDrawing();
    }

    shell->show_restart_update_modal = false;
    shell->update_status.supported = false;
    shell->update_status.is_nix_store = true;
    shell->update_status.state = RATA_UPD_IDLE;
    shell->show_about = true;
    for (int f = 0; f < 2; ++f) {
        rata_arena_reset(&arena);
        BeginDrawing();
        ui_shell_update_and_draw(shell, &arena, 1.0 / 60.0);
        EndDrawing();
    }

    ui_theme_unload_font(&shell->theme);
    CloseWindow();
    free(rw);
    free(shell);
    rata_arena_destroy(&arena);
    printf("ui_smoke: PASSED (embedded Lora + Greek/math glyphs, top menu bar, UI scaling, 6 RataWave tabs, pole-clamped LTI, Coming Soon, About & Update UI).\n");
    return 0;
}

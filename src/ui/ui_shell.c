#include "ui/ui_shell.h"

#include <stdio.h>
#include <string.h>

#include "engines/project_file.h"

void ui_shell_init(RataShell *shell) {
    if (shell == NULL) {
        return;
    }
    memset(shell, 0, sizeof(*shell));
    ui_theme_init_default(&shell->theme);
    ui_workspace_registry_init(&shell->registry);
    shell->active_ws = WS_RATAWAVE;
    snprintf(shell->project_path, sizeof(shell->project_path), "rata_project.rata");
    snprintf(shell->toast_msg, sizeof(shell->toast_msg),
             "Rata Workspace ready. Shortcuts: Alt+1..6 switch workspaces.");
    shell->toast_level = BANNER_INFO;
    shell->toast_timer = 5.0;
}

static void set_toast(RataShell *shell, const char *msg, RataBannerLevel level) {
    snprintf(shell->toast_msg, sizeof(shell->toast_msg), "%s", msg);
    shell->toast_level = level;
    shell->toast_timer = 5.0;
}

static void save_current_project(RataShell *shell, RataArena *arena) {
    size_t mark = rata_arena_mark(arena);
    RataProjectDoc doc;
    RataStatus st = project_doc_init(&doc, RATA_PROJECT_MAX_ENTRIES, arena);
    if (st != RATA_OK) {
        set_toast(shell, "Save failed: arena out of memory.", BANNER_ERROR);
        rata_arena_restore(arena, mark);
        return;
    }

    const RataWorkspaceEntry *active = &shell->registry.entries[shell->active_ws];
    (void)project_doc_set(&doc, "app", "active_workspace", active->id_key);

    for (int i = 0; i < WS_COUNT; ++i) {
        const RataWorkspaceEntry *e = &shell->registry.entries[i];
        if (e->implemented && e->save_fn != NULL) {
            st = e->save_fn(e->state, &doc);
            if (st != RATA_OK) {
                set_toast(shell, "Save failed while serializing workspace state.", BANNER_ERROR);
                rata_arena_restore(arena, mark);
                return;
            }
        }
    }

    st = project_file_save_path(&doc, shell->project_path);
    rata_arena_restore(arena, mark);
    if (st == RATA_OK) {
        char msg[160];
        snprintf(msg, sizeof(msg), "Saved project to %.110s", shell->project_path);
        set_toast(shell, msg, BANNER_OK);
    } else {
        char msg[160];
        snprintf(msg, sizeof(msg), "Could not save '%s': %s",
                 shell->project_path, rata_status_str(st));
        set_toast(shell, msg, BANNER_ERROR);
    }
}

static void load_current_project(RataShell *shell, RataArena *arena) {
    size_t mark = rata_arena_mark(arena);
    RataProjectDoc doc;
    RataStatus st = project_file_load_path(shell->project_path, &doc, arena);
    if (st != RATA_OK) {
        char msg[160];
        snprintf(msg, sizeof(msg), "Could not load '%s': %s",
                 shell->project_path, rata_status_str(st));
        set_toast(shell, msg, BANNER_ERROR);
        rata_arena_restore(arena, mark);
        return;
    }

    const char *ws_key = project_doc_get(&doc, "app", "active_workspace");
    if (ws_key != NULL) {
        for (int i = 0; i < WS_COUNT; ++i) {
            if (strcmp(shell->registry.entries[i].id_key, ws_key) == 0) {
                shell->active_ws = (RataWorkspaceId)i;
                break;
            }
        }
    }

    for (int i = 0; i < WS_COUNT; ++i) {
        RataWorkspaceEntry *e = &shell->registry.entries[i];
        if (e->implemented && e->load_fn != NULL) {
            (void)e->load_fn(e->state, &doc);
        }
    }
    rata_arena_restore(arena, mark);

    char msg[160];
    snprintf(msg, sizeof(msg), "Loaded project from %.110s", shell->project_path);
    set_toast(shell, msg, BANNER_OK);
}

void ui_shell_update_and_draw(RataShell *shell, RataArena *frame_arena, double dt) {
    if (shell == NULL || frame_arena == NULL) {
        return;
    }
    if (shell->toast_timer > 0.0) {
        shell->toast_timer -= dt;
    }

    /* Keyboard shortcuts Alt+1 .. Alt+6 to switch workspaces. */
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) {
        for (int i = 0; i < WS_COUNT; ++i) {
            if (IsKeyPressed(KEY_ONE + i)) {
                shell->active_ws = (RataWorkspaceId)i;
                shell->focus.active_id = 0;
            }
        }
    }

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    const float sidebar_w = 192.0f;
    const float topbar_h = 44.0f;

    ClearBackground(shell->theme.bg_app);

    /* Left sidebar: branding + all 6 registered workspaces. */
    Rectangle sidebar = {0.0f, 0.0f, sidebar_w, sh};
    DrawRectangleRec(sidebar, shell->theme.bg_sidebar);
    DrawLine((int)sidebar_w, 0, (int)sidebar_w, (int)sh, shell->theme.border);

    DrawRectangle(0, 0, (int)sidebar_w, (int)topbar_h, (Color){14, 19, 28, 255});
    DrawText("RATA", 16, 10, 20, shell->theme.accent);
    DrawText("WORKSPACE", 78, 15, 13, shell->theme.text_primary);
    DrawLine(0, (int)topbar_h, (int)sidebar_w, (int)topbar_h, shell->theme.border);

    DrawText("WORKSPACES", 14, (int)topbar_h + 14, 10, shell->theme.text_muted);

    float item_y = topbar_h + 32.0f;
    for (int i = 0; i < WS_COUNT; ++i) {
        const RataWorkspaceEntry *e = &shell->registry.entries[i];
        Rectangle btn = {10.0f, item_y, sidebar_w - 20.0f, 52.0f};
        bool active = (shell->active_ws == (RataWorkspaceId)i);
        bool hovered = CheckCollisionPointRec(GetMousePosition(), btn);

        Color bg = active ? shell->theme.bg_button_active
                          : (hovered ? (Color){30, 40, 56, 255} : shell->theme.bg_panel);
        Color bdr = active ? shell->theme.accent : shell->theme.border;
        DrawRectangleRec(btn, bg);
        if (active) {
            DrawRectangle((int)btn.x, (int)btn.y, 3, (int)btn.height, shell->theme.accent);
        }
        DrawRectangleLinesEx(btn, 1.0f, bdr);

        DrawText(e->name, (int)btn.x + 12, (int)btn.y + 8, 15,
                 active ? shell->theme.text_primary : shell->theme.text_secondary);
        DrawText(e->subtitle, (int)btn.x + 12, (int)btn.y + 28, 11, shell->theme.text_muted);

        if (e->implemented) {
            DrawCircle((int)(btn.x + btn.width - 14.0f), (int)(btn.y + 15.0f),
                       4.0f, shell->theme.success);
        } else {
            DrawText("SOON", (int)(btn.x + btn.width - 36.0f), (int)btn.y + 10,
                     10, shell->theme.text_muted);
        }

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            shell->active_ws = (RataWorkspaceId)i;
            shell->focus.active_id = 0;
        }
        item_y += 58.0f;
    }

    /* Footer note in sidebar. */
    DrawText("Pure C11 | Double Precision", 14, (int)sh - 34, 10, shell->theme.text_muted);
    DrawText("Alt+1..6 switch workspace", 14, (int)sh - 20, 10, shell->theme.text_muted);

    /* Top bar: active workspace title + project file path + Save / Load buttons. */
    Rectangle topbar = {sidebar_w, 0.0f, sw - sidebar_w, topbar_h};
    DrawRectangleRec(topbar, shell->theme.bg_sidebar);
    DrawLine((int)sidebar_w, (int)topbar_h, (int)sw, (int)topbar_h, shell->theme.border);

    RataWorkspaceEntry *cur = &shell->registry.entries[shell->active_ws];
    DrawText(cur->name, (int)sidebar_w + 16, 8, 18, shell->theme.text_primary);
    DrawText(cur->subtitle, (int)sidebar_w + 16, 27, 11, shell->theme.accent);

    /* Right side of topbar: project file path + Save/Load. */
    float load_x = sw - 74.0f;
    float save_x = load_x - 68.0f;
    float path_w = 190.0f;
    float path_x = save_x - path_w - 8.0f;
    if (path_x > sidebar_w + 210.0f) {
        (void)ui_text_field((Rectangle){path_x, 9.0f, path_w, 26.0f},
                            NULL, shell->project_path, (int)sizeof(shell->project_path),
                            9001, &shell->focus, &shell->theme);
        if (ui_button((Rectangle){save_x, 9.0f, 62.0f, 26.0f}, "Save", false, &shell->theme)) {
            save_current_project(shell, frame_arena);
        }
        if (ui_button((Rectangle){load_x, 9.0f, 62.0f, 26.0f}, "Load", false, &shell->theme)) {
            load_current_project(shell, frame_arena);
        }
    }

    /* Main workspace viewport. */
    float toast_h = (shell->toast_timer > 0.0) ? 26.0f : 0.0f;
    Rectangle viewport = {
        sidebar_w + 8.0f,
        topbar_h + 8.0f,
        sw - sidebar_w - 16.0f,
        sh - topbar_h - 16.0f - toast_h
    };

    if (cur->implemented && cur->draw_fn != NULL) {
        RataWorkspaceCtx ctx;
        ctx.theme = &shell->theme;
        ctx.frame_arena = frame_arena;
        ctx.focus = &shell->focus;
        ctx.dt = dt;
        cur->draw_fn(cur->state, viewport, &ctx);
    } else {
        ui_workspace_draw_coming_soon(cur, viewport, &shell->theme);
    }

    if (toast_h > 0.0f) {
        Rectangle toast_r = {sidebar_w + 8.0f, sh - toast_h - 4.0f,
                             sw - sidebar_w - 16.0f, 22.0f};
        ui_draw_banner(toast_r, shell->toast_msg, shell->toast_level, &shell->theme);
    }
}

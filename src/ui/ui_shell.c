#include "ui/ui_shell.h"

#include <stdio.h>
#include <string.h>

#include "engines/project_file.h"
#include "ui/ui_os.h"

void ui_shell_init(RataShell *shell) {
    if (shell == NULL) {
        return;
    }
    memset(shell, 0, sizeof(*shell));
    ui_theme_init_default(&shell->theme);
    ui_workspace_registry_init(&shell->registry);
    shell->active_ws = WS_RATAWAVE;

    if (ui_os_config_dir(shell->config_dir, sizeof(shell->config_dir)) != RATA_OK) {
        snprintf(shell->config_dir, sizeof(shell->config_dir), "(unavailable)");
    }
    if (ui_os_config_file("project.rata", shell->project_path,
                          sizeof(shell->project_path)) != RATA_OK) {
        snprintf(shell->project_path, sizeof(shell->project_path), "project.rata");
    }
    snprintf(shell->toast_msg, sizeof(shell->toast_msg),
             "Rata Workspace %s ready. Alt+1..6 switch workspaces.", RATA_VERSION);
    shell->toast_level = BANNER_INFO;
    shell->toast_timer = 5.0;
}

static void set_toast(RataShell *shell, const char *msg, RataBannerLevel level) {
    snprintf(shell->toast_msg, sizeof(shell->toast_msg), "%s", msg);
    shell->toast_level = level;
    shell->toast_timer = 6.0;
}

void ui_shell_load_settings(RataShell *shell, RataArena *scratch) {
    if (shell == NULL || scratch == NULL) {
        return;
    }
    char path[RATA_PATH_MAX];
    if (ui_os_config_file("settings.rata", path, sizeof(path)) != RATA_OK) {
        return;
    }
    size_t mark = rata_arena_mark(scratch);
    RataProjectDoc doc;
    if (project_file_load_path(path, &doc, scratch) == RATA_OK) {
        const char *ws = project_doc_get(&doc, "settings", "active_workspace");
        if (ws != NULL) {
            for (int i = 0; i < WS_COUNT; ++i) {
                if (strcmp(shell->registry.entries[i].id_key, ws) == 0) {
                    shell->active_ws = (RataWorkspaceId)i;
                    break;
                }
            }
        }
        const char *lp = project_doc_get(&doc, "settings", "last_project");
        if (lp != NULL && lp[0] != '\0') {
            snprintf(shell->project_path, sizeof(shell->project_path), "%s", lp);
        }
    }
    rata_arena_restore(scratch, mark);
}

void ui_shell_save_settings(const RataShell *shell, RataArena *scratch) {
    if (shell == NULL || scratch == NULL) {
        return;
    }
    char dir[RATA_PATH_MAX];
    char path[RATA_PATH_MAX];
    if (ui_os_config_dir(dir, sizeof(dir)) != RATA_OK ||
        ui_os_config_file("settings.rata", path, sizeof(path)) != RATA_OK) {
        return;
    }
    if (ui_os_mkdir_p(dir) != RATA_OK) {
        return;
    }

    size_t mark = rata_arena_mark(scratch);
    RataProjectDoc doc;
    if (project_doc_init(&doc, 16u, scratch) == RATA_OK) {
        const RataWorkspaceEntry *active = &shell->registry.entries[shell->active_ws];
        (void)project_doc_set(&doc, "settings", "active_workspace", active->id_key);
        (void)project_doc_set(&doc, "settings", "last_project", shell->project_path);
        (void)project_file_save_path(&doc, path);
    }
    rata_arena_restore(scratch, mark);
}

static void save_current_project(RataShell *shell, RataArena *arena) {
    if (shell->config_dir[0] != '\0' && shell->config_dir[0] != '(') {
        (void)ui_os_mkdir_p(shell->config_dir);
    }

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
        ui_shell_save_settings(shell, arena);
        char msg[200];
        snprintf(msg, sizeof(msg), "Saved project to %.150s", shell->project_path);
        set_toast(shell, msg, BANNER_OK);
    } else {
        char msg[200];
        snprintf(msg, sizeof(msg), "Could not save '%.110s': %s",
                 shell->project_path, rata_status_str(st));
        set_toast(shell, msg, BANNER_ERROR);
    }
}

static void load_current_project(RataShell *shell, RataArena *arena) {
    size_t mark = rata_arena_mark(arena);
    RataProjectDoc doc;
    RataStatus st = project_file_load_path(shell->project_path, &doc, arena);
    if (st == RATA_ERR_NEWER_VERSION) {
        char msg[200];
        snprintf(msg, sizeof(msg),
                 "Project file is format v%u, newer than Rata Workspace %s supports (max v%u). Please update.",
                 (unsigned)doc.version, RATA_VERSION, (unsigned)RATA_PROJECT_FORMAT_VERSION);
        set_toast(shell, msg, BANNER_ERROR);
        rata_arena_restore(arena, mark);
        return;
    }
    if (st != RATA_OK) {
        char msg[200];
        snprintf(msg, sizeof(msg), "Could not load '%.110s': %s",
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
    ui_shell_save_settings(shell, arena);

    char msg[200];
    snprintf(msg, sizeof(msg), "Loaded project from %.150s", shell->project_path);
    set_toast(shell, msg, BANNER_OK);
}

static void draw_about_modal(RataShell *shell, float sw, float sh) {
    const RataTheme *t = &shell->theme;
    DrawRectangle(0, 0, (int)sw, (int)sh, (Color){5, 8, 14, 180});

    float mw = 520.0f;
    float mh = 290.0f;
    Rectangle card = {(sw - mw) * 0.5f, (sh - mh) * 0.5f, mw, mh};
    ui_draw_panel(card, t);
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, 4, t->accent);

    float x = card.x + 22.0f;
    float y = card.y + 18.0f;
    ui_draw_text(t, "Rata Workspace", x, y, 24.0f, t->text_primary);
    char ver_line[96];
    snprintf(ver_line, sizeof(ver_line), "Version %s  (Project Format v%u)",
             RATA_VERSION, (unsigned)RATA_PROJECT_FORMAT_VERSION);
    ui_draw_text(t, ver_line, x, y + 32.0f, 14.0f, t->accent);

    ui_draw_text(t, "Native C11 multi-workspace toolkit for electrical-engineering students.",
                 x, y + 62.0f, 13.0f, t->text_secondary);
    ui_draw_text(t, "Double-precision in-house solvers (zero scripting runtimes).",
                 x, y + 82.0f, 13.0f, t->text_secondary);

    ui_draw_text(t, "Per-user config & data directory:", x, y + 114.0f, 12.0f, t->text_muted);
    ui_draw_text(t, shell->config_dir, x, y + 132.0f, 12.0f, t->text_primary);

    ui_draw_text(t, "Third-party components:", x, y + 162.0f, 12.0f, t->text_muted);
    ui_draw_text(t, "raylib (zlib)  |  raygui 4.0 (zlib)  |  JetBrains Mono NL (SIL OFL 1.1)",
                 x, y + 180.0f, 12.0f, t->text_secondary);

    Rectangle close_btn = {card.x + card.width - 96.0f, card.y + card.height - 42.0f, 76.0f, 28.0f};
    if (ui_button(close_btn, "Close", true, t) || IsKeyPressed(KEY_ESCAPE)) {
        shell->show_about = false;
    }
}

void ui_shell_update_and_draw(RataShell *shell, RataArena *frame_arena, double dt) {
    if (shell == NULL || frame_arena == NULL) {
        return;
    }
    const RataTheme *t = &shell->theme;
    if (shell->toast_timer > 0.0) {
        shell->toast_timer -= dt;
    }

    if (!shell->show_about && (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))) {
        for (int i = 0; i < WS_COUNT; ++i) {
            if (IsKeyPressed(KEY_ONE + i)) {
                shell->active_ws = (RataWorkspaceId)i;
                shell->focus.active_id = 0;
            }
        }
    }

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    const float sidebar_w = 196.0f;
    const float topbar_h = 44.0f;

    ClearBackground(t->bg_app);

    /* Left sidebar: branding + all 6 registered workspaces. */
    Rectangle sidebar = {0.0f, 0.0f, sidebar_w, sh};
    DrawRectangleRec(sidebar, t->bg_sidebar);
    DrawLine((int)sidebar_w, 0, (int)sidebar_w, (int)sh, t->border);

    DrawRectangle(0, 0, (int)sidebar_w, (int)topbar_h, (Color){14, 19, 28, 255});
    ui_draw_text(t, "RATA", 14.0f, 11.0f, 20.0f, t->accent);
    ui_draw_text(t, "WORKSPACE", 72.0f, 15.0f, 14.0f, t->text_primary);
    DrawLine(0, (int)topbar_h, (int)sidebar_w, (int)topbar_h, t->border);

    ui_draw_text(t, "WORKSPACES", 14.0f, topbar_h + 13.0f, 11.0f, t->text_muted);

    float item_y = topbar_h + 32.0f;
    for (int i = 0; i < WS_COUNT; ++i) {
        const RataWorkspaceEntry *e = &shell->registry.entries[i];
        Rectangle btn = {10.0f, item_y, sidebar_w - 20.0f, 52.0f};
        bool active = (shell->active_ws == (RataWorkspaceId)i);
        bool hovered = !shell->show_about && CheckCollisionPointRec(GetMousePosition(), btn);

        Color bg = active ? t->bg_button_active
                          : (hovered ? (Color){30, 40, 56, 255} : t->bg_panel);
        Color bdr = active ? t->accent : t->border;
        DrawRectangleRec(btn, bg);
        if (active) {
            DrawRectangle((int)btn.x, (int)btn.y, 3, (int)btn.height, t->accent);
        }
        DrawRectangleLinesEx(btn, 1.0f, bdr);

        ui_draw_text(t, e->name, btn.x + 12.0f, btn.y + 8.0f, 15.0f,
                     active ? t->text_primary : t->text_secondary);
        ui_draw_text(t, e->subtitle, btn.x + 12.0f, btn.y + 28.0f, 12.0f, t->text_muted);

        if (e->implemented) {
            DrawCircle((int)(btn.x + btn.width - 14.0f), (int)(btn.y + 15.0f), 4.0f, t->success);
        } else {
            ui_draw_text(t, "SOON", btn.x + btn.width - 40.0f, btn.y + 10.0f, 11.0f, t->text_muted);
        }

        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            shell->active_ws = (RataWorkspaceId)i;
            shell->focus.active_id = 0;
        }
        item_y += 58.0f;
    }

    /* Footer: About button + version tag. */
    Rectangle about_btn = {10.0f, sh - 66.0f, sidebar_w - 20.0f, 26.0f};
    if (!shell->show_about && ui_button(about_btn, "About Rata Workspace", false, t)) {
        shell->show_about = true;
        shell->focus.active_id = 0;
    }
    char vfoot[48];
    snprintf(vfoot, sizeof(vfoot), "v%s | Pure C11", RATA_VERSION);
    ui_draw_text(t, vfoot, 14.0f, sh - 34.0f, 11.0f, t->text_muted);
    ui_draw_text(t, "Alt+1..6 switch workspace", 14.0f, sh - 20.0f, 11.0f, t->text_muted);

    /* Top bar: active workspace title + project file path + Save / Load buttons. */
    Rectangle topbar = {sidebar_w, 0.0f, sw - sidebar_w, topbar_h};
    DrawRectangleRec(topbar, t->bg_sidebar);
    DrawLine((int)sidebar_w, (int)topbar_h, (int)sw, (int)topbar_h, t->border);

    RataWorkspaceEntry *cur = &shell->registry.entries[shell->active_ws];
    ui_draw_text(t, cur->name, sidebar_w + 16.0f, 7.0f, 18.0f, t->text_primary);
    ui_draw_text(t, cur->subtitle, sidebar_w + 16.0f, 26.0f, 12.0f, t->accent);

    float load_x = sw - 74.0f;
    float save_x = load_x - 68.0f;
    float path_w = 240.0f;
    float path_x = save_x - path_w - 8.0f;
    if (!shell->show_about && path_x > sidebar_w + 210.0f) {
        (void)ui_text_field((Rectangle){path_x, 9.0f, path_w, 26.0f},
                            NULL, shell->project_path, (int)sizeof(shell->project_path),
                            9001, &shell->focus, t);
        if (ui_button((Rectangle){save_x, 9.0f, 62.0f, 26.0f}, "Save", false, t)) {
            save_current_project(shell, frame_arena);
        }
        if (ui_button((Rectangle){load_x, 9.0f, 62.0f, 26.0f}, "Load", false, t)) {
            load_current_project(shell, frame_arena);
        }
    }

    float toast_h = (shell->toast_timer > 0.0) ? 26.0f : 0.0f;
    Rectangle viewport = {
        sidebar_w + 8.0f,
        topbar_h + 8.0f,
        sw - sidebar_w - 16.0f,
        sh - topbar_h - 16.0f - toast_h
    };

    if (cur->implemented && cur->draw_fn != NULL) {
        RataWorkspaceCtx ctx;
        ctx.theme = t;
        ctx.frame_arena = frame_arena;
        ctx.focus = &shell->focus;
        ctx.dt = dt;
        cur->draw_fn(cur->state, viewport, &ctx);
    } else {
        ui_workspace_draw_coming_soon(cur, viewport, t);
    }

    if (toast_h > 0.0f) {
        Rectangle toast_r = {sidebar_w + 8.0f, sh - toast_h - 4.0f,
                             sw - sidebar_w - 16.0f, 22.0f};
        ui_draw_banner(toast_r, shell->toast_msg, shell->toast_level, t);
    }

    if (shell->show_about) {
        draw_about_modal(shell, sw, sh);
    }
}

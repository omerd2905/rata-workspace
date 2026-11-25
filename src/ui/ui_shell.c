#include "ui/ui_shell.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engines/project_file.h"
#include "raygui.h"
#include "ui/ui_os.h"

static float clamp_scale(float s) {
    if (s < 0.65f) {
        return 0.65f;
    }
    if (s > 2.50f) {
        return 2.50f;
    }
    return s;
}

void ui_shell_set_scale(RataShell *shell, float scale) {
    if (shell == NULL) {
        return;
    }
    shell->ui_scale = clamp_scale(scale);
}

void ui_shell_init(RataShell *shell) {
    if (shell == NULL) {
        return;
    }
    memset(shell, 0, sizeof(*shell));
    ui_theme_init_default(&shell->theme);
    ui_workspace_registry_init(&shell->registry);
    shell->active_ws = WS_RATAWAVE;
    shell->ui_scale = 1.0f;
    shell->dpi_auto = true;
    shell->show_sidebar = true;
    shell->open_menu = RATA_MENU_NONE;
    shell->auto_update = true;
    shell->toast_timer = 0.0;

    if (ui_os_config_dir(shell->config_dir, sizeof(shell->config_dir)) != RATA_OK) {
        snprintf(shell->config_dir, sizeof(shell->config_dir), "(unavailable)");
    }
    if (ui_os_config_file("project.rata", shell->project_path,
                          sizeof(shell->project_path)) != RATA_OK) {
        snprintf(shell->project_path, sizeof(shell->project_path), "project.rata");
    }
}

void ui_shell_set_updater(RataShell *shell,
                          void *ctx,
                          RataUpdatePollFn poll_fn,
                          RataUpdateTriggerFn check_fn) {
    if (shell == NULL) {
        return;
    }
    shell->updater_ctx = ctx;
    shell->updater_poll_fn = poll_fn;
    shell->updater_check_fn = check_fn;
    if (poll_fn != NULL) {
        poll_fn(ctx, &shell->update_status);
    }
}

static void set_toast(RataShell *shell, const char *msg, RataBannerLevel level) {
    snprintf(shell->toast_msg, sizeof(shell->toast_msg), "%s", msg);
    shell->toast_level = level;
    shell->toast_timer = 4.5;
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
        const char *au = project_doc_get(&doc, "settings", "auto_update");
        if (au != NULL) {
            shell->auto_update = (strcmp(au, "0") != 0 && strcmp(au, "false") != 0);
        }
        double dv = 1.0;
        if (project_doc_get_double(&doc, "settings", "ui_scale", &dv)) {
            ui_shell_set_scale(shell, (float)dv);
        }
        const char *da = project_doc_get(&doc, "settings", "dpi_auto");
        if (da != NULL) {
            shell->dpi_auto = (strcmp(da, "0") != 0 && strcmp(da, "false") != 0);
        }
        const char *sb = project_doc_get(&doc, "settings", "show_sidebar");
        if (sb != NULL) {
            shell->show_sidebar = (strcmp(sb, "0") != 0 && strcmp(sb, "false") != 0);
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
        char scale_buf[32];
        snprintf(scale_buf, sizeof(scale_buf), "%.3f", (double)shell->ui_scale);
        const RataWorkspaceEntry *active = &shell->registry.entries[shell->active_ws];
        (void)project_doc_set(&doc, "settings", "active_workspace", active->id_key);
        (void)project_doc_set(&doc, "settings", "last_project", shell->project_path);
        (void)project_doc_set(&doc, "settings", "auto_update", shell->auto_update ? "1" : "0");
        (void)project_doc_set(&doc, "settings", "ui_scale", scale_buf);
        (void)project_doc_set(&doc, "settings", "dpi_auto", shell->dpi_auto ? "1" : "0");
        (void)project_doc_set(&doc, "settings", "show_sidebar", shell->show_sidebar ? "1" : "0");
        (void)project_file_save_path(&doc, path);
    }
    rata_arena_restore(scratch, mark);
}

static bool save_current_project(RataShell *shell, RataArena *arena) {
    if (shell->config_dir[0] != '\0' && shell->config_dir[0] != '(') {
        (void)ui_os_mkdir_p(shell->config_dir);
    }

    size_t mark = rata_arena_mark(arena);
    RataProjectDoc doc;
    RataStatus st = project_doc_init(&doc, RATA_PROJECT_MAX_ENTRIES, arena);
    if (st != RATA_OK) {
        set_toast(shell, "Save failed: arena out of memory.", BANNER_ERROR);
        rata_arena_restore(arena, mark);
        return false;
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
                return false;
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
        return true;
    }
    char msg[200];
    snprintf(msg, sizeof(msg), "Could not save '%.110s': %s",
             shell->project_path, rata_status_str(st));
    set_toast(shell, msg, BANNER_ERROR);
    return false;
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

static void draw_modal_backdrop(float sw, float sh) {
    DrawRectangle(0, 0, (int)sw + 2, (int)sh + 2, (Color){20, 16, 12, 168});
}

static void draw_path_modal(RataShell *shell, RataArena *frame_arena, float sw, float sh) {
    const RataTheme *t = &shell->theme;
    draw_modal_backdrop(sw, sh);

    float mw = (sw > 580.0f) ? 540.0f : (sw - 32.0f);
    float mh = 188.0f;
    Rectangle card = {(sw - mw) * 0.5f, (sh - mh) * 0.5f, mw, mh};
    ui_draw_panel(card, t);
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, 28, t->bg_topbar);
    DrawLine((int)card.x, (int)card.y + 28, (int)(card.x + card.width), (int)card.y + 28, t->border);
    ui_draw_text(t, "Project File Path", card.x + 16.0f, card.y + 6.0f, 14.5f, t->text_primary);

    float x = card.x + 18.0f;
    float w = card.width - 36.0f;
    (void)ui_text_field((Rectangle){x, card.y + 44.0f, w, 46.0f},
                        "Active .rata project file:",
                        shell->project_path, (int)sizeof(shell->project_path),
                        9001, &shell->focus, t);

    ui_draw_text(t, "Stored outside the installation folder so updates never touch your files.",
                 x, card.y + 98.0f, 12.0f, t->text_muted);

    float by = card.y + card.height - 42.0f;
    if (ui_button((Rectangle){x, by, 104.0f, 28.0f}, "Open File", false, t)) {
        load_current_project(shell, frame_arena);
        shell->show_path_modal = false;
        shell->focus.active_id = 0;
    }
    if (ui_button((Rectangle){x + 112.0f, by, 104.0f, 28.0f}, "Save File", true, t)) {
        (void)save_current_project(shell, frame_arena);
        shell->show_path_modal = false;
        shell->focus.active_id = 0;
    }
    if (ui_button((Rectangle){card.x + card.width - 96.0f, by, 78.0f, 28.0f}, "Close", false, t) ||
        IsKeyPressed(KEY_ESCAPE)) {
        ui_shell_save_settings(shell, frame_arena);
        shell->show_path_modal = false;
        shell->focus.active_id = 0;
    }
}

static void draw_about_modal(RataShell *shell, RataArena *frame_arena, float sw, float sh) {
    const RataTheme *t = &shell->theme;
    draw_modal_backdrop(sw, sh);

    float mw = (sw > 660.0f) ? 620.0f : (sw - 32.0f);
    float mh = 392.0f;
    Rectangle card = {(sw - mw) * 0.5f, (sh - mh) * 0.5f, mw, mh};
    ui_draw_panel(card, t);
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, 30, t->bg_topbar);
    DrawLine((int)card.x, (int)card.y + 30, (int)(card.x + card.width), (int)card.y + 30, t->border);
    ui_draw_text(t, "About Rata Workspace", card.x + 18.0f, card.y + 7.0f, 14.5f, t->text_primary);

    float x = card.x + 22.0f;
    float y = card.y + 44.0f;
    ui_draw_text(t, "Rata Workspace", x, y, 24.0f, t->text_primary);
    char ver_line[96];
    snprintf(ver_line, sizeof(ver_line), "Version %s  (Project Format v%u)",
             RATA_VERSION, (unsigned)RATA_PROJECT_FORMAT_VERSION);
    ui_draw_text(t, ver_line, x, y + 30.0f, 13.5f, t->text_secondary);

    ui_draw_text(t, "Native C11 multi-workspace toolkit for electrical-engineering students.",
                 x, y + 56.0f, 13.5f, t->text_secondary);
    ui_draw_text(t, "Double-precision in-house solvers with zero scripting runtimes.",
                 x, y + 75.0f, 13.5f, t->text_muted);

    ui_draw_text(t, "Per-user configuration directory:", x, y + 102.0f, 12.0f, t->text_muted);
    ui_draw_text(t, shell->config_dir, x, y + 119.0f, 12.5f, t->text_primary);

    ui_draw_text(t, "License: MIT  |  Third-party components:", x, y + 144.0f, 12.0f, t->text_muted);
    ui_draw_text(t, "raylib (zlib) | raygui 4.0 (zlib) | Lora & JetBrains Mono NL (OFL 1.1) | Monocypher (BSD/CC0)",
                 x, y + 161.0f, 12.0f, t->text_secondary);

    float uy = y + 188.0f;
    DrawLine((int)x, (int)uy, (int)(card.x + card.width - 22.0f), (int)uy, t->border);
    uy += 10.0f;
    ui_draw_text(t, "Updates", x, uy, 14.0f, t->text_primary);
    uy += 24.0f;

    const RataUpdateUiStatus *us = &shell->update_status;
    if (us->supported) {
        const char *auto_lbl = shell->auto_update
                                   ? "[x] Update automatically"
                                   : "[ ] Update automatically";
        if (ui_toggle_chip((Rectangle){x, uy - 2.0f, 214.0f, 26.0f},
                           auto_lbl, &shell->auto_update, t)) {
            ui_shell_save_settings(shell, frame_arena);
        }

        bool checking = (us->state == RATA_UPD_CHECKING);
        Rectangle check_btn = {x + 224.0f, uy - 2.0f, 182.0f, 26.0f};
        if (ui_button(check_btn, checking ? "Checking..." : "Check for updates now",
                      false, t) &&
            !checking && shell->updater_check_fn != NULL) {
            shell->updater_check_fn(shell->updater_ctx);
        }

        if (us->state == RATA_UPD_READY) {
            Rectangle rst_btn = {x + 416.0f, uy - 2.0f, 144.0f, 26.0f};
            if (ui_button(rst_btn, "Restart & update", true, t)) {
                shell->show_about = false;
                shell->show_restart_update_modal = true;
            }
        }

        uy += 34.0f;
        if (us->message[0] != '\0') {
            Color mc = t->text_secondary;
            if (us->state == RATA_UPD_READY || us->state == RATA_UPD_UP_TO_DATE) {
                mc = t->success;
            } else if (us->state == RATA_UPD_ERROR) {
                mc = t->danger;
            }
            ui_draw_text(t, us->message, x, uy, 12.5f, mc);
        } else {
            ui_draw_text(t, "Ed25519-signed updates checked over HTTPS from GitHub Releases.",
                         x, uy, 12.0f, t->text_muted);
        }
    } else if (us->is_nix_store) {
        ui_draw_text(t, "Installed from /nix/store (self-update is disabled on Nix builds).",
                     x, uy, 13.0f, t->text_secondary);
        ui_draw_text(t, "Update via:  nix profile upgrade rata-workspace",
                     x, uy + 22.0f, 13.0f, t->text_primary);
    } else {
        ui_draw_text(t, "Self-update is disabled on Linux builds.",
                     x, uy, 13.0f, t->text_secondary);
        ui_draw_text(t, "Update via Nix (nix profile upgrade rata-workspace) or a release archive.",
                     x, uy + 22.0f, 12.5f, t->text_muted);
    }

    Rectangle close_btn = {card.x + card.width - 96.0f, card.y + card.height - 40.0f, 76.0f, 28.0f};
    if (ui_button(close_btn, "Close", true, t) || IsKeyPressed(KEY_ESCAPE)) {
        shell->show_about = false;
    }
}

static void draw_restart_update_modal(RataShell *shell, RataArena *frame_arena, float sw, float sh) {
    const RataTheme *t = &shell->theme;
    draw_modal_backdrop(sw, sh);

    float mw = 510.0f;
    float mh = 204.0f;
    Rectangle card = {(sw - mw) * 0.5f, (sh - mh) * 0.5f, mw, mh};
    ui_draw_panel(card, t);
    DrawRectangle((int)card.x, (int)card.y, (int)card.width, 28, t->bg_topbar);
    DrawLine((int)card.x, (int)card.y + 28, (int)(card.x + card.width), (int)card.y + 28, t->border);

    float x = card.x + 22.0f;
    char title[96];
    snprintf(title, sizeof(title), "Restart & Update to v%s", shell->update_status.ready_version);
    ui_draw_text(t, title, x, card.y + 6.0f, 14.5f, t->text_primary);

    float y = card.y + 42.0f;
    ui_draw_text(t, "The update has been verified (Ed25519 signature + SHA-256).",
                 x, y, 13.5f, t->success);
    ui_draw_text(t, "Save your current project before restarting to install?",
                 x, y + 24.0f, 13.5f, t->text_secondary);
    ui_draw_text(t, shell->project_path, x, y + 48.0f, 12.0f, t->text_muted);

    float by = card.y + card.height - 44.0f;
    if (ui_button((Rectangle){x, by, 140.0f, 28.0f}, "Save & Restart", true, t)) {
        if (save_current_project(shell, frame_arena)) {
            shell->request_restart_for_update = true;
            shell->show_restart_update_modal = false;
        }
    }
    if (ui_button((Rectangle){x + 150.0f, by, 180.0f, 28.0f}, "Restart Without Saving", false, t)) {
        shell->request_restart_for_update = true;
        shell->show_restart_update_modal = false;
    }
    if (ui_button((Rectangle){card.x + card.width - 102.0f, by, 80.0f, 28.0f}, "Cancel", false, t) ||
        IsKeyPressed(KEY_ESCAPE)) {
        shell->show_restart_update_modal = false;
    }
}

static bool menu_row(Rectangle r,
                     const char *label,
                     const char *shortcut,
                     bool checked,
                     const RataTheme *t) {
    Vector2 mp = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mp, r);
    if (hovered) {
        DrawRectangleRec(r, (Color){62, 49, 39, 255});
    }
    if (checked) {
        DrawCircle((int)(r.x + 11.0f), (int)(r.y + r.height * 0.5f), 3.0f, t->accent);
    }
    ui_draw_text(t, label, r.x + 22.0f, r.y + (r.height - 13.5f) * 0.5f,
                 13.5f, hovered ? t->text_primary : t->text_secondary);
    if (shortcut != NULL && shortcut[0] != '\0') {
        int sw = ui_measure_text(t, shortcut, 12.0f);
        ui_draw_text(t, shortcut, r.x + r.width - (float)sw - 12.0f,
                     r.y + (r.height - 12.0f) * 0.5f, 12.0f, t->text_muted);
    }
    return hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static void menu_divider(float x, float y, float w, const RataTheme *t) {
    DrawLine((int)(x + 8.0f), (int)(y + 3.0f), (int)(x + w - 8.0f), (int)(y + 3.0f), t->border);
}

static void draw_topbar_and_menus(RataShell *shell,
                                  RataArena *frame_arena,
                                  float sw,
                                  float topbar_h,
                                  float dpi_scale,
                                  bool modal_open) {
    const RataTheme *t = &shell->theme;

    /* Coffee Bean top menu bar */
    DrawRectangle(0, 0, (int)sw + 2, (int)topbar_h, t->bg_topbar);
    DrawLine(0, (int)topbar_h - 1, (int)sw + 2, (int)topbar_h - 1, (Color){66, 55, 44, 255});

    static const char *menu_titles[4] = {"File", "View", "Workspace", "Help"};
    const RataShellMenuId menu_ids[4] = {
        RATA_MENU_FILE, RATA_MENU_VIEW, RATA_MENU_WORKSPACE, RATA_MENU_HELP
    };
    Rectangle menu_btns[4];
    float mx = 8.0f;
    Vector2 mp = GetMousePosition();
    bool clicked_menu_bar = false;

    for (int i = 0; i < 4; ++i) {
        int tw = ui_measure_text(t, menu_titles[i], 14.0f);
        Rectangle mr = {mx, 2.0f, (float)tw + 22.0f, topbar_h - 4.0f};
        menu_btns[i] = mr;
        bool open = (shell->open_menu == menu_ids[i]);
        bool hov = !modal_open && CheckCollisionPointRec(mp, mr);

        if (open || hov) {
            DrawRectangleRec(mr, open ? (Color){66, 52, 41, 255} : (Color){52, 40, 32, 255});
        }
        ui_draw_text(t, menu_titles[i], mr.x + 11.0f, (topbar_h - 14.0f) * 0.5f,
                     14.0f, (open || hov) ? t->text_primary : t->text_secondary);

        if (!modal_open) {
            if (hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                shell->open_menu = open ? RATA_MENU_NONE : menu_ids[i];
                shell->focus.active_id = 0;
                clicked_menu_bar = true;
            } else if (hov && shell->open_menu != RATA_MENU_NONE && !open) {
                shell->open_menu = menu_ids[i];
            }
        }
        mx += mr.width + 2.0f;
    }

    /* Right side of Coffee Bean top bar: subtle workspace indicator + clickable View Scale pill */
    char scale_lbl[32];
    int pct = (int)(shell->ui_scale * 100.0f + 0.5f);
    snprintf(scale_lbl, sizeof(scale_lbl), "View %d%%", pct);
    int slw = ui_measure_text(t, scale_lbl, 12.5f);
    Rectangle scale_pill = {sw - (float)slw - 24.0f, 4.0f, (float)slw + 16.0f, topbar_h - 8.0f};
    bool sp_hov = !modal_open && CheckCollisionPointRec(mp, scale_pill);
    DrawRectangleRec(scale_pill, (sp_hov || shell->open_menu == RATA_MENU_VIEW)
                                     ? (Color){62, 49, 39, 255}
                                     : (Color){48, 36, 29, 255});
    DrawRectangleLinesEx(scale_pill, 1.0f, (Color){76, 62, 50, 255});
    ui_draw_text(t, scale_lbl, scale_pill.x + 8.0f, (topbar_h - 12.5f) * 0.5f,
                 12.5f, t->text_secondary);
    if (sp_hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        shell->open_menu = (shell->open_menu == RATA_MENU_VIEW) ? RATA_MENU_NONE : RATA_MENU_VIEW;
        shell->focus.active_id = 0;
        clicked_menu_bar = true;
    }

    const RataWorkspaceEntry *cur = &shell->registry.entries[shell->active_ws];
    char ws_hdr[96];
    snprintf(ws_hdr, sizeof(ws_hdr), "%s  \xe2\x80\x94  %s", cur->name, cur->subtitle);
    int whw = ui_measure_text(t, ws_hdr, 13.0f);
    float whx = scale_pill.x - (float)whw - 16.0f;
    if (whx > mx + 16.0f) {
        ui_draw_text(t, ws_hdr, whx, (topbar_h - 13.0f) * 0.5f, 13.0f, t->text_muted);
    }

    if (shell->open_menu == RATA_MENU_NONE || modal_open) {
        return;
    }

    /* Dropdown menu panel */
    Rectangle panel = {0.0f, topbar_h, 248.0f, 100.0f};
    if (shell->open_menu == RATA_MENU_FILE) {
        panel.x = menu_btns[0].x;
        panel.width = 246.0f;
        panel.height = 158.0f;
    } else if (shell->open_menu == RATA_MENU_VIEW) {
        panel.x = menu_btns[1].x;
        panel.width = 278.0f;
        panel.height = 374.0f;
    } else if (shell->open_menu == RATA_MENU_WORKSPACE) {
        panel.x = menu_btns[2].x;
        panel.width = 276.0f;
        panel.height = (float)WS_COUNT * 26.0f + 10.0f;
    } else if (shell->open_menu == RATA_MENU_HELP) {
        panel.x = menu_btns[3].x;
        panel.width = 256.0f;
        panel.height = shell->update_status.supported ? 96.0f : 36.0f;
    }
    if (panel.x + panel.width > sw - 6.0f) {
        panel.x = sw - panel.width - 6.0f;
    }
    if (panel.x < 4.0f) {
        panel.x = 4.0f;
    }

    /* Subtle drop shadow + Coffee Bean menu surface */
    DrawRectangle((int)panel.x + 3, (int)panel.y + 3, (int)panel.width, (int)panel.height,
                  (Color){14, 11, 8, 120});
    DrawRectangleRec(panel, (Color){36, 27, 21, 252});
    DrawRectangleLinesEx(panel, 1.0f, t->border_strong);

    float ry = panel.y + 5.0f;
    const float rh = 25.0f;

    if (shell->open_menu == RATA_MENU_FILE) {
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Open Project", "Ctrl+O", false, t)) {
            load_current_project(shell, frame_arena);
            shell->open_menu = RATA_MENU_NONE;
        }
        ry += rh;
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Save Project", "Ctrl+S", false, t)) {
            (void)save_current_project(shell, frame_arena);
            shell->open_menu = RATA_MENU_NONE;
        }
        ry += rh;
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Project File Path...", "", false, t)) {
            shell->show_path_modal = true;
            shell->open_menu = RATA_MENU_NONE;
        }
        ry += rh;
        menu_divider(panel.x, ry, panel.width, t);
        ry += 7.0f;
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Reset Active Workspace", "", false, t)) {
            RataWorkspaceEntry *e = &shell->registry.entries[shell->active_ws];
            if (e->implemented && e->init_fn != NULL) {
                e->init_fn(e->state);
                set_toast(shell, "Workspace reset to defaults.", BANNER_INFO);
            }
            shell->open_menu = RATA_MENU_NONE;
        }
        ry += rh;
        menu_divider(panel.x, ry, panel.width, t);
        ry += 7.0f;
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Exit", "", false, t)) {
            shell->request_exit = true;
            shell->open_menu = RATA_MENU_NONE;
        }
    } else if (shell->open_menu == RATA_MENU_VIEW) {
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Zoom In (+10%)", "Ctrl+=", false, t)) {
            ui_shell_set_scale(shell, shell->ui_scale + 0.10f);
            ui_shell_save_settings(shell, frame_arena);
        }
        ry += rh;
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Zoom Out (-10%)", "Ctrl+-", false, t)) {
            ui_shell_set_scale(shell, shell->ui_scale - 0.10f);
            ui_shell_save_settings(shell, frame_arena);
        }
        ry += rh;
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Reset Scale (100%)", "Ctrl+0", false, t)) {
            ui_shell_set_scale(shell, 1.0f);
            ui_shell_save_settings(shell, frame_arena);
            shell->open_menu = RATA_MENU_NONE;
        }
        ry += rh;
        menu_divider(panel.x, ry, panel.width, t);
        ry += 7.0f;

        static const float presets[8] = {0.75f, 0.90f, 1.00f, 1.15f, 1.25f, 1.50f, 1.75f, 2.00f};
        static const char *preset_lbls[8] = {
            "Scale: 75%", "Scale: 90%", "Scale: 100%", "Scale: 115%",
            "Scale: 125%", "Scale: 150%", "Scale: 175%", "Scale: 200%"
        };
        for (int i = 0; i < 8; ++i) {
            bool match = (fabsf(shell->ui_scale - presets[i]) < 0.03f);
            if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                         preset_lbls[i], (i == 2) ? "Default" : "", match, t)) {
                ui_shell_set_scale(shell, presets[i]);
                ui_shell_save_settings(shell, frame_arena);
                shell->open_menu = RATA_MENU_NONE;
            }
            ry += rh;
        }

        menu_divider(panel.x, ry, panel.width, t);
        ry += 7.0f;
        char dpi_tag[24];
        snprintf(dpi_tag, sizeof(dpi_tag), "%.2gx OS", (double)dpi_scale);
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Auto-Scale for Display DPI", dpi_tag, shell->dpi_auto, t)) {
            shell->dpi_auto = !shell->dpi_auto;
            ui_shell_save_settings(shell, frame_arena);
        }
        ry += rh;
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "Show Workspace Sidebar", "Ctrl+B", shell->show_sidebar, t)) {
            shell->show_sidebar = !shell->show_sidebar;
            ui_shell_save_settings(shell, frame_arena);
            shell->open_menu = RATA_MENU_NONE;
        }
    } else if (shell->open_menu == RATA_MENU_WORKSPACE) {
        for (int i = 0; i < WS_COUNT; ++i) {
            const RataWorkspaceEntry *e = &shell->registry.entries[i];
            char row_lbl[96];
            char sc_lbl[16];
            snprintf(row_lbl, sizeof(row_lbl), "%s  \xe2\x80\x94  %s", e->name, e->subtitle);
            snprintf(sc_lbl, sizeof(sc_lbl), "Alt+%d", i + 1);
            if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, 26.0f},
                         row_lbl, sc_lbl, shell->active_ws == (RataWorkspaceId)i, t)) {
                shell->active_ws = (RataWorkspaceId)i;
                shell->focus.active_id = 0;
                ui_shell_save_settings(shell, frame_arena);
                shell->open_menu = RATA_MENU_NONE;
            }
            ry += 26.0f;
        }
    } else if (shell->open_menu == RATA_MENU_HELP) {
        if (shell->update_status.supported) {
            bool ready = (shell->update_status.state == RATA_UPD_READY);
            const char *ulbl = ready ? "Restart & Install Update..." : "Check for Updates";
            if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                         ulbl, "", ready, t)) {
                if (ready) {
                    shell->show_restart_update_modal = true;
                } else if (shell->updater_check_fn != NULL) {
                    shell->updater_check_fn(shell->updater_ctx);
                }
                shell->open_menu = RATA_MENU_NONE;
            }
            ry += rh;
            if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                         "Update Automatically", "", shell->auto_update, t)) {
                shell->auto_update = !shell->auto_update;
                ui_shell_save_settings(shell, frame_arena);
            }
            ry += rh;
            menu_divider(panel.x, ry, panel.width, t);
            ry += 7.0f;
        }
        if (menu_row((Rectangle){panel.x + 2.0f, ry, panel.width - 4.0f, rh},
                     "About Rata Workspace", "", false, t)) {
            shell->show_about = true;
            shell->open_menu = RATA_MENU_NONE;
        }
    }

    if (!clicked_menu_bar && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        !CheckCollisionPointRec(mp, panel)) {
        shell->open_menu = RATA_MENU_NONE;
    }
}

void ui_shell_update_and_draw(RataShell *shell, RataArena *frame_arena, double dt) {
    if (shell == NULL || frame_arena == NULL) {
        return;
    }
    if (shell->ui_scale < 0.65f || shell->ui_scale > 2.50f) {
        shell->ui_scale = 1.0f;
    }
    if (shell->toast_timer > 0.0) {
        shell->toast_timer -= dt;
    }

    if (shell->updater_poll_fn != NULL) {
        shell->updater_poll_fn(shell->updater_ctx, &shell->update_status);
        if (shell->update_status.state != shell->last_seen_upd_state) {
            if (shell->update_status.from_manual &&
                shell->update_status.state == RATA_UPD_UP_TO_DATE &&
                shell->update_status.message[0] != '\0') {
                set_toast(shell, shell->update_status.message, BANNER_OK);
            } else if (shell->update_status.from_manual &&
                       shell->update_status.state == RATA_UPD_ERROR &&
                       shell->update_status.message[0] != '\0') {
                set_toast(shell, shell->update_status.message, BANNER_ERROR);
            } else if (shell->update_status.state == RATA_UPD_READY) {
                shell->update_banner_dismissed = false;
            }
            shell->last_seen_upd_state = shell->update_status.state;
        }
    }

    /* Compute High-DPI + user zoom effective scale */
    float phys_w = (float)GetScreenWidth();
    float phys_h = (float)GetScreenHeight();
    if (phys_w < 320.0f) { phys_w = 320.0f; }
    if (phys_h < 240.0f) { phys_h = 240.0f; }

    Vector2 dpi = GetWindowScaleDPI();
    float dpi_s = (dpi.x >= 1.0f && dpi.x <= 4.0f) ? dpi.x : 1.0f;
    float eff_scale = (shell->dpi_auto ? dpi_s : 1.0f) * shell->ui_scale;
    if (eff_scale < 0.65f) {
        eff_scale = 0.65f;
    }
    if (phys_w >= 820.0f && phys_h >= 500.0f) {
        float max_sw = phys_w / 820.0f;
        float max_sh = phys_h / 500.0f;
        float max_s = (max_sw < max_sh) ? max_sw : max_sh;
        if (eff_scale > max_s) {
            eff_scale = max_s;
        }
    }
    shell->theme.render_scale = eff_scale;

    float sw = phys_w / eff_scale;
    float sh = phys_h / eff_scale;
    SetMouseOffset(0, 0);
    SetMouseScale(1.0f / eff_scale, 1.0f / eff_scale);

    const RataTheme *t = &shell->theme;
    bool modal_open = shell->show_about || shell->show_path_modal || shell->show_restart_update_modal;
    if (modal_open) {
        shell->open_menu = RATA_MENU_NONE;
    }

    /* Global keyboard shortcuts */
    if (!modal_open) {
        if (shell->open_menu != RATA_MENU_NONE && IsKeyPressed(KEY_ESCAPE)) {
            shell->open_menu = RATA_MENU_NONE;
        }
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool alt = IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT);
        if (ctrl) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
                ui_shell_set_scale(shell, shell->ui_scale + 0.10f);
                ui_shell_save_settings(shell, frame_arena);
            } else if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                ui_shell_set_scale(shell, shell->ui_scale - 0.10f);
                ui_shell_save_settings(shell, frame_arena);
            } else if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0)) {
                ui_shell_set_scale(shell, 1.0f);
                ui_shell_save_settings(shell, frame_arena);
            } else if (IsKeyPressed(KEY_B)) {
                shell->show_sidebar = !shell->show_sidebar;
                ui_shell_save_settings(shell, frame_arena);
            } else if (IsKeyPressed(KEY_S)) {
                (void)save_current_project(shell, frame_arena);
            } else if (IsKeyPressed(KEY_O)) {
                load_current_project(shell, frame_arena);
            }
        } else if (alt) {
            for (int i = 0; i < WS_COUNT; ++i) {
                if (IsKeyPressed(KEY_ONE + i)) {
                    shell->active_ws = (RataWorkspaceId)i;
                    shell->focus.active_id = 0;
                    ui_shell_save_settings(shell, frame_arena);
                }
            }
        }
    }

    Camera2D cam;
    memset(&cam, 0, sizeof(cam));
    cam.zoom = eff_scale;

    ClearBackground(t->bg_app);
    BeginMode2D(cam);

    const float topbar_h = 30.0f;
    const float sidebar_w = shell->show_sidebar ? 174.0f : 0.0f;
    bool block_bg_input = modal_open || (shell->open_menu != RATA_MENU_NONE);
    if (block_bg_input) {
        GuiLock();
        SetMouseOffset(100000, 100000);
    }

    /* Minimalist left workspace sidebar */
    if (shell->show_sidebar) {
        Rectangle sidebar = {0.0f, topbar_h, sidebar_w, sh - topbar_h};
        DrawRectangleRec(sidebar, t->bg_sidebar);
        DrawLine((int)sidebar_w, (int)topbar_h, (int)sidebar_w, (int)sh, t->border);

        float item_y = topbar_h + 10.0f;
        Vector2 mp = GetMousePosition();
        for (int i = 0; i < WS_COUNT; ++i) {
            const RataWorkspaceEntry *e = &shell->registry.entries[i];
            Rectangle row = {6.0f, item_y, sidebar_w - 12.0f, 44.0f};
            bool active = (shell->active_ws == (RataWorkspaceId)i);
            bool hovered = CheckCollisionPointRec(mp, row);

            if (active) {
                DrawRectangleRec(row, t->bg_panel);
                DrawRectangleLinesEx(row, 1.0f, t->border);
                DrawRectangle((int)row.x, (int)row.y + 6, 2, (int)row.height - 12, t->accent);
            } else if (hovered) {
                DrawRectangleRec(row, (Color){56, 53, 41, 255});
            }

            Color name_col = active ? t->text_primary
                                    : (e->implemented ? t->text_secondary : t->text_muted);
            Color sub_col = active ? t->text_secondary
                                   : (e->implemented ? t->text_muted : (Color){122, 115, 100, 255});
            ui_draw_text(t, e->name, row.x + 12.0f, row.y + 6.0f, 14.5f, name_col);
            ui_draw_text(t, e->subtitle, row.x + 12.0f, row.y + 24.0f, 11.5f, sub_col);

            if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !active) {
                shell->active_ws = (RataWorkspaceId)i;
                shell->focus.active_id = 0;
                ui_shell_save_settings(shell, frame_arena);
            }
            item_y += 47.0f;
        }

        char vfoot[32];
        snprintf(vfoot, sizeof(vfoot), "v%s", RATA_VERSION);
        ui_draw_text(t, vfoot, 16.0f, sh - 22.0f, 11.5f, t->text_muted);
    }

    /* Non-blocking update-ready banner when a verified update has finished downloading. */
    float content_x = sidebar_w + 10.0f;
    float content_w = sw - sidebar_w - 20.0f;
    float upd_banner_h = 0.0f;
    if (shell->update_status.state == RATA_UPD_READY && !shell->update_banner_dismissed) {
        upd_banner_h = 34.0f;
        Rectangle bar = {content_x, topbar_h + 8.0f, content_w, 28.0f};
        DrawRectangleRec(bar, t->bg_panel);
        DrawRectangleLinesEx(bar, 1.0f, t->border_strong);
        DrawRectangle((int)bar.x, (int)bar.y, 3, (int)bar.height, t->success);

        char bmsg[128];
        snprintf(bmsg, sizeof(bmsg), "Update v%s verified and ready to install.",
                 shell->update_status.ready_version);
        ui_draw_text(t, bmsg, bar.x + 14.0f, bar.y + 7.0f, 13.5f, t->text_primary);

        Rectangle later_btn = {bar.x + bar.width - 74.0f, bar.y + 3.0f, 66.0f, 22.0f};
        Rectangle rst_btn = {later_btn.x - 144.0f, bar.y + 3.0f, 136.0f, 22.0f};
        if (ui_button(rst_btn, "Restart & update", true, t)) {
            shell->show_restart_update_modal = true;
            shell->focus.active_id = 0;
        }
        if (ui_button(later_btn, "Later", false, t)) {
            shell->update_banner_dismissed = true;
        }
    }

    float toast_h = (shell->toast_timer > 0.0) ? 26.0f : 0.0f;
    Rectangle viewport = {
        content_x,
        topbar_h + 8.0f + upd_banner_h,
        content_w,
        sh - topbar_h - 16.0f - upd_banner_h - toast_h
    };

    RataWorkspaceEntry *cur = &shell->registry.entries[shell->active_ws];
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
        Rectangle toast_r = {content_x, sh - toast_h - 4.0f, content_w, 22.0f};
        ui_draw_banner(toast_r, shell->toast_msg, shell->toast_level, t);
    }

    if (block_bg_input) {
        SetMouseOffset(0, 0);
        GuiUnlock();
    }

    /* Top menu bar & open dropdown rendered above workspace content */
    draw_topbar_and_menus(shell, frame_arena, sw, topbar_h, dpi_s, modal_open);

    if (shell->show_about) {
        draw_about_modal(shell, frame_arena, sw, sh);
    } else if (shell->show_path_modal) {
        draw_path_modal(shell, frame_arena, sw, sh);
    } else if (shell->show_restart_update_modal) {
        draw_restart_update_modal(shell, frame_arena, sw, sh);
    }

    EndMode2D();
    SetMouseOffset(0, 0);
}

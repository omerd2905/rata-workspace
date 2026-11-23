#include "ui/ui_theme.h"

#include <stddef.h>

#include "raygui.h"

static int color_to_hex(Color c) {
    unsigned int r = (unsigned int)c.r;
    unsigned int g = (unsigned int)c.g;
    unsigned int b = (unsigned int)c.b;
    unsigned int a = (unsigned int)c.a;
    return (int)((r << 24u) | (g << 16u) | (b << 8u) | a);
}

void ui_theme_init_default(RataTheme *t) {
    if (t == NULL) {
        return;
    }
    t->bg_app           = (Color){15, 20, 29, 255};
    t->bg_sidebar       = (Color){19, 25, 36, 255};
    t->bg_panel         = (Color){24, 32, 46, 255};
    t->bg_plot          = (Color){13, 18, 26, 255};
    t->bg_input         = (Color){16, 22, 33, 255};
    t->bg_button        = (Color){34, 45, 64, 255};
    t->bg_button_active = (Color){26, 98, 148, 255};
    t->border           = (Color){46, 60, 84, 255};
    t->border_strong    = (Color){78, 102, 138, 255};
    t->grid_major       = (Color){36, 48, 68, 255};
    t->grid_minor       = (Color){24, 33, 47, 255};
    t->axis_zero        = (Color){92, 114, 148, 255};
    t->text_primary     = (Color){232, 238, 248, 255};
    t->text_secondary   = (Color){176, 190, 212, 255};
    t->text_muted       = (Color){120, 136, 162, 255};
    t->accent           = (Color){56, 189, 248, 255};
    t->success          = (Color){52, 211, 153, 255};
    t->warning          = (Color){251, 191, 36, 255};
    t->danger           = (Color){251, 113, 133, 255};

    /* High-contrast, colorblind-conscious oscilloscope trace palette. */
    t->traces[0] = (Color){56, 189, 248, 255};  /* Cyan */
    t->traces[1] = (Color){251, 191, 36, 255};  /* Amber */
    t->traces[2] = (Color){52, 211, 153, 255};  /* Emerald */
    t->traces[3] = (Color){251, 113, 133, 255}; /* Coral */
    t->traces[4] = (Color){167, 139, 250, 255}; /* Violet */
    t->traces[5] = (Color){250, 204, 21, 255};  /* Yellow */
}

void ui_theme_apply_raygui(const RataTheme *t) {
    if (t == NULL) {
        return;
    }
    GuiSetStyle(DEFAULT, TEXT_SIZE, 13);
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, color_to_hex(t->bg_panel));
    GuiSetStyle(DEFAULT, LINE_COLOR, color_to_hex(t->border));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL, color_to_hex(t->bg_button));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, color_to_hex(t->bg_button_active));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, color_to_hex(t->accent));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, color_to_hex(t->border));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, color_to_hex(t->accent));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, color_to_hex(t->accent));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, color_to_hex(t->text_primary));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, color_to_hex(t->text_primary));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, color_to_hex(t->bg_app));

    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL, color_to_hex(t->bg_input));
    GuiSetStyle(TEXTBOX, BASE_COLOR_FOCUSED, color_to_hex(t->bg_input));
    GuiSetStyle(TEXTBOX, BASE_COLOR_PRESSED, color_to_hex(t->bg_input));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_PRESSED, color_to_hex(t->text_primary));

    GuiSetStyle(SLIDER, BASE_COLOR_NORMAL, color_to_hex(t->bg_input));
    GuiSetStyle(SLIDER, BASE_COLOR_PRESSED, color_to_hex(t->accent));
    GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL, color_to_hex(t->text_secondary));
}

void ui_draw_panel(Rectangle r, const RataTheme *theme) {
    if (theme == NULL) {
        return;
    }
    DrawRectangleRec(r, theme->bg_panel);
    DrawRectangleLinesEx(r, 1.0f, theme->border);
}

void ui_draw_section_title(Rectangle r, const char *title, const RataTheme *theme) {
    if (theme == NULL || title == NULL) {
        return;
    }
    DrawText(title, (int)r.x + 10, (int)r.y + 6, 13, theme->accent);
    DrawLine((int)r.x + 8, (int)(r.y + r.height - 1.0f),
             (int)(r.x + r.width - 8.0f), (int)(r.y + r.height - 1.0f), theme->border);
}

void ui_draw_banner(Rectangle r, const char *text, RataBannerLevel level, const RataTheme *theme) {
    if (theme == NULL || text == NULL) {
        return;
    }
    Color bar = theme->accent;
    Color fill = (Color){22, 42, 62, 255};
    if (level == BANNER_OK) {
        bar = theme->success;
        fill = (Color){18, 46, 42, 255};
    } else if (level == BANNER_WARN) {
        bar = theme->warning;
        fill = (Color){54, 44, 20, 255};
    } else if (level == BANNER_ERROR) {
        bar = theme->danger;
        fill = (Color){58, 22, 30, 255};
    }
    DrawRectangleRec(r, fill);
    DrawRectangle((int)r.x, (int)r.y, 4, (int)r.height, bar);
    DrawRectangleLinesEx(r, 1.0f, bar);

    BeginScissorMode((int)r.x + 8, (int)r.y, (int)r.width - 12, (int)r.height);
    DrawText(text, (int)r.x + 10, (int)(r.y + (r.height - 12.0f) * 0.5f), 12, theme->text_primary);
    EndScissorMode();
}

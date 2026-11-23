#include "ui/ui_theme.h"

#include <stddef.h>
#include <string.h>

#include "raygui.h"
#include "ui/ui_font_data.h"

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
    memset(t, 0, sizeof(*t));
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

void ui_theme_load_font(RataTheme *t) {
    if (t == NULL) {
        return;
    }
    if (t->has_custom_font) {
        return;
    }

    /* Build codepoint table: printable ASCII + Greek & Coptic (0x0370..0x03FF) + math symbols. */
    int codepoints[280];
    int count = 0;
    for (int cp = 0x0020; cp <= 0x007E; ++cp) {
        codepoints[count++] = cp;
    }
    static const int extra_cps[] = {
        0x00B0, 0x00B1, 0x00B5, 0x00D7, 0x00F7, /* deg, pm, mu, times, div */
        0x2013, 0x2014,                         /* en-dash, em-dash */
        0x2192, 0x2211, 0x2212, 0x221A, 0x221E, /* arrow, sum, minus, sqrt, infty */
        0x2220, 0x222B, 0x2248, 0x2260, 0x2264, 0x2265 /* angle, int, approx, ne, le, ge */
    };
    size_t nextra = sizeof(extra_cps) / sizeof(extra_cps[0]);
    for (size_t i = 0u; i < nextra; ++i) {
        codepoints[count++] = extra_cps[i];
    }
    for (int cp = 0x0370; cp <= 0x03FF; ++cp) {
        codepoints[count++] = cp;
    }

    const uint8_t *data = ui_font_mono_data();
    size_t data_len = ui_font_mono_size();
    Font loaded = LoadFontFromMemory(".ttf", data, (int)data_len, 32, codepoints, count);
    if (loaded.texture.id != 0u) {
        SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
        t->font = loaded;
        t->has_custom_font = true;
    } else {
        t->font = GetFontDefault();
        t->has_custom_font = false;
    }
}

void ui_theme_unload_font(RataTheme *t) {
    if (t == NULL) {
        return;
    }
    if (t->has_custom_font && t->font.texture.id != 0u) {
        UnloadFont(t->font);
    }
    memset(&t->font, 0, sizeof(t->font));
    t->has_custom_font = false;
}

void ui_theme_apply_raygui(const RataTheme *t) {
    if (t == NULL) {
        return;
    }
    if (t->has_custom_font) {
        GuiSetFont(t->font);
    }
    GuiSetStyle(DEFAULT, TEXT_SIZE, 14);
    GuiSetStyle(DEFAULT, TEXT_SPACING, 0);
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

void ui_draw_text(const RataTheme *theme, const char *text, float x, float y, float size, Color color) {
    if (text == NULL) {
        return;
    }
    if (theme != NULL && theme->has_custom_font) {
        DrawTextEx(theme->font, text, (Vector2){x, y}, size, 0.0f, color);
    } else {
        DrawText(text, (int)x, (int)y, (int)size, color);
    }
}

int ui_measure_text(const RataTheme *theme, const char *text, float size) {
    if (text == NULL) {
        return 0;
    }
    if (theme != NULL && theme->has_custom_font) {
        Vector2 v = MeasureTextEx(theme->font, text, size, 0.0f);
        return (int)(v.x + 0.5f);
    }
    return MeasureText(text, (int)size);
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
    ui_draw_text(theme, title, r.x + 10.0f, r.y + 6.0f, 14.0f, theme->accent);
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
    ui_draw_text(theme, text, r.x + 10.0f, r.y + (r.height - 13.0f) * 0.5f,
                 13.0f, theme->text_primary);
    EndScissorMode();
}

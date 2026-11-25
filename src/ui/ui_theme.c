#include "ui/ui_theme.h"

#include <stddef.h>
#include <string.h>

#include "raygui.h"
#include "ui/ui_font_data.h"
#include "ui/ui_font_lora.h"

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
    t->render_scale     = 1.0f;

    /* Minimalist warm palette: Coffee Bean top bar, Dark Khaki surfaces, Pale Oak text. */
    t->bg_topbar        = (Color){38, 28, 22, 255};    /* Coffee Bean (#261C16) */
    t->bg_app           = (Color){63, 60, 46, 255};    /* Dark Khaki (#3F3C2E) */
    t->bg_sidebar       = (Color){48, 45, 35, 255};    /* Deep Khaki (#302D23) */
    t->bg_panel         = (Color){55, 52, 40, 255};    /* Warm Khaki Panel (#373428) */
    t->bg_plot          = (Color){42, 40, 31, 255};    /* Recessed Plot Well (#2A281F) */
    t->bg_input         = (Color){40, 38, 29, 255};    /* Recessed Input Well (#28261D) */
    t->bg_button        = (Color){68, 64, 50, 255};    /* Tactile Button (#444032) */
    t->bg_button_hover  = (Color){79, 74, 58, 255};    /* Hover Lift (#4F4A3A) */
    t->bg_button_active = (Color){94, 86, 66, 255};    /* Active Selection (#5E5642) */
    t->border           = (Color){82, 77, 61, 255};    /* Hairline Border (#524D3D) */
    t->border_strong    = (Color){118, 110, 89, 255};  /* Focused Hairline (#766E59) */
    t->grid_major       = (Color){56, 53, 42, 255};
    t->grid_minor       = (Color){47, 45, 35, 255};
    t->axis_zero        = (Color){108, 101, 82, 255};

    t->text_primary     = (Color){232, 221, 203, 255}; /* Pale Oak (#E8DDCB) */
    t->text_secondary   = (Color){198, 188, 170, 255}; /* Soft Pale Oak (#C6BCAA) */
    t->text_muted       = (Color){154, 145, 128, 255}; /* Muted Taupe-Oak (#9A9180) */
    t->accent           = (Color){218, 202, 176, 255}; /* Warm Pale Oak Accent (#DACAB0) */
    t->success          = (Color){158, 176, 134, 255}; /* Muted Sage */
    t->warning          = (Color){208, 176, 118, 255}; /* Muted Ochre */
    t->danger           = (Color){196, 124, 110, 255}; /* Muted Terracotta */

    /* Restrained, low-saturation trace palette anchored by Pale Oak. */
    t->traces[0] = (Color){232, 221, 203, 255}; /* Pale Oak */
    t->traces[1] = (Color){194, 156, 108, 255}; /* Warm Ochre */
    t->traces[2] = (Color){154, 174, 134, 255}; /* Muted Sage */
    t->traces[3] = (Color){190, 130, 114, 255}; /* Muted Terracotta */
    t->traces[4] = (Color){152, 166, 170, 255}; /* Warm Slate */
    t->traces[5] = (Color){208, 188, 142, 255}; /* Dry Wheat */
}

static bool glyph_matches_notdef(const GlyphInfo *g, const GlyphInfo *notdef) {
    if (g == NULL) {
        return true;
    }
    if (g->image.data == NULL || g->image.width <= 0 || g->image.height <= 0) {
        return true;
    }
    if (notdef == NULL || notdef->image.data == NULL) {
        return false;
    }
    if (g->image.width != notdef->image.width ||
        g->image.height != notdef->image.height ||
        g->image.format != notdef->image.format ||
        g->offsetX != notdef->offsetX ||
        g->offsetY != notdef->offsetY ||
        g->advanceX != notdef->advanceX) {
        return false;
    }
    int bytes = GetPixelDataSize(g->image.width, g->image.height, g->image.format);
    if (bytes <= 0) {
        return false;
    }
    return memcmp(g->image.data, notdef->image.data, (size_t)bytes) == 0;
}

static GlyphInfo *load_font_glyphs(const uint8_t *bytes,
                                   size_t len,
                                   int font_size,
                                   int *codepoints,
                                   int req_count,
                                   int *out_count) {
#if (RAYLIB_VERSION_MAJOR > 5) || (RAYLIB_VERSION_MAJOR == 5 && RAYLIB_VERSION_MINOR >= 6)
    int actual = 0;
    GlyphInfo *g = LoadFontData(bytes, (int)len, font_size, codepoints, req_count,
                                FONT_DEFAULT, &actual);
    if (out_count != NULL) {
        *out_count = (g != NULL) ? actual : 0;
    }
    return g;
#else
    GlyphInfo *g = LoadFontData(bytes, (int)len, font_size, codepoints, req_count, FONT_DEFAULT);
    if (out_count != NULL) {
        *out_count = (g != NULL) ? req_count : 0;
    }
    return g;
#endif
}

void ui_theme_load_font(RataTheme *t) {
    if (t == NULL || t->has_custom_font) {
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
        0x2220, 0x2248, 0x2260, 0x2264, 0x2265, /* angle, approx, ne, le, ge */
        0x222B
    };
    size_t nextra = sizeof(extra_cps) / sizeof(extra_cps[0]);
    for (size_t i = 0u; i < nextra; ++i) {
        codepoints[count++] = extra_cps[i];
    }
    for (int cp = 0x0370; cp <= 0x03FF; ++cp) {
        codepoints[count++] = cp;
    }

    const int base_size = 48;
    const uint8_t *lora_bytes = ui_font_lora_data();
    size_t lora_len = ui_font_lora_size();
    const uint8_t *mono_bytes = ui_font_mono_data();
    size_t mono_len = ui_font_mono_size();

    int probe_cp = 0xFFFE;
    int notdef_count = 0;
    int lora_count = 0;
    int merged_count = 0;
    GlyphInfo *notdef = load_font_glyphs(lora_bytes, lora_len, base_size,
                                         &probe_cp, 1, &notdef_count);
    GlyphInfo *lora_glyphs = load_font_glyphs(lora_bytes, lora_len, base_size,
                                              codepoints, count, &lora_count);
    GlyphInfo *merged = load_font_glyphs(mono_bytes, mono_len, base_size,
                                         codepoints, count, &merged_count);

    if (merged != NULL && merged_count > 0) {
        const GlyphInfo *nd_ref = (notdef != NULL && notdef_count > 0) ? &notdef[0] : NULL;
        if (lora_glyphs != NULL && lora_count > 0) {
            for (int i = 0; i < merged_count; ++i) {
                int cp = merged[i].value;
                for (int j = 0; j < lora_count; ++j) {
                    if (lora_glyphs[j].value == cp) {
                        bool take_lora = (cp == 0x0020) ||
                                         !glyph_matches_notdef(&lora_glyphs[j], nd_ref);
                        if (take_lora) {
                            UnloadImage(merged[i].image);
                            merged[i] = lora_glyphs[j];
                            lora_glyphs[j].image = (Image){0};
                        }
                        break;
                    }
                }
            }
            UnloadFontData(lora_glyphs, lora_count);
        }
        if (notdef != NULL) {
            UnloadFontData(notdef, notdef_count);
        }

        Font loaded;
        memset(&loaded, 0, sizeof(loaded));
        loaded.baseSize = base_size;
        loaded.glyphCount = merged_count;
        loaded.glyphPadding = 4;
        Image atlas = GenImageFontAtlas(merged, &loaded.recs, merged_count, base_size, 4, 0);
        if (atlas.data != NULL && atlas.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA &&
            atlas.width > 0 && atlas.height > 0) {
            /* Ensure transparent padding texels carry white luminance (255) so GPU mipmap
             * downsampling averages alpha coverage cleanly without dark halo fringes. */
            uint8_t *px = (uint8_t *)atlas.data;
            size_t npix = (size_t)atlas.width * (size_t)atlas.height;
            for (size_t k = 0u; k < npix; ++k) {
                px[k * 2u] = 255u;
            }
        }
        loaded.texture = LoadTextureFromImage(atlas);
        UnloadImage(atlas);
        loaded.glyphs = merged;

        if (loaded.texture.id != 0u) {
            GenTextureMipmaps(&loaded.texture);
            SetTextureFilter(loaded.texture,
                             (loaded.texture.mipmaps > 1) ? TEXTURE_FILTER_TRILINEAR
                                                          : TEXTURE_FILTER_BILINEAR);
            t->font = loaded;
            t->has_custom_font = true;
            return;
        }
        UnloadFont(loaded);
    } else {
        if (merged != NULL) {
            UnloadFontData(merged, merged_count);
        }
        if (lora_glyphs != NULL) {
            UnloadFontData(lora_glyphs, lora_count);
        }
        if (notdef != NULL) {
            UnloadFontData(notdef, notdef_count);
        }
    }

    t->font = GetFontDefault();
    t->has_custom_font = false;
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
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED, color_to_hex(t->bg_button_hover));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED, color_to_hex(t->bg_button_active));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, color_to_hex(t->border));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, color_to_hex(t->border_strong));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, color_to_hex(t->accent));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, color_to_hex(t->text_primary));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED, color_to_hex(t->text_primary));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED, color_to_hex(t->text_primary));

    GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL, color_to_hex(t->bg_input));
    GuiSetStyle(TEXTBOX, BASE_COLOR_FOCUSED, color_to_hex(t->bg_input));
    GuiSetStyle(TEXTBOX, BASE_COLOR_PRESSED, color_to_hex(t->bg_input));
    GuiSetStyle(TEXTBOX, BORDER_COLOR_FOCUSED, color_to_hex(t->border_strong));
    GuiSetStyle(TEXTBOX, BORDER_COLOR_PRESSED, color_to_hex(t->accent));
    GuiSetStyle(TEXTBOX, TEXT_COLOR_PRESSED, color_to_hex(t->text_primary));

    GuiSetStyle(SLIDER, BASE_COLOR_NORMAL, color_to_hex(t->bg_input));
    GuiSetStyle(SLIDER, BASE_COLOR_FOCUSED, color_to_hex(t->bg_input));
    GuiSetStyle(SLIDER, BASE_COLOR_PRESSED, color_to_hex((Color){132, 122, 98, 255}));
    GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL, color_to_hex(t->text_secondary));
    GuiSetStyle(SLIDER, TEXT_COLOR_FOCUSED, color_to_hex((Color){164, 152, 126, 255}));
    GuiSetStyle(SLIDER, TEXT_COLOR_PRESSED, color_to_hex(t->accent));
    GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL, color_to_hex(t->border));
    GuiSetStyle(SLIDER, BORDER_COLOR_FOCUSED, color_to_hex(t->border_strong));
    GuiSetStyle(SLIDER, BORDER_COLOR_PRESSED, color_to_hex(t->accent));
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

void ui_begin_scissor(const RataTheme *theme, float x, float y, float width, float height) {
    float s = (theme != NULL && theme->render_scale > 0.01f) ? theme->render_scale : 1.0f;
    int sx = (int)(x * s);
    int sy = (int)(y * s);
    int sw = (int)(width * s + 0.5f);
    int sh = (int)(height * s + 0.5f);
    if (sw < 1) {
        sw = 1;
    }
    if (sh < 1) {
        sh = 1;
    }
    BeginScissorMode(sx, sy, sw, sh);
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
    ui_draw_text(theme, title, r.x + 10.0f, r.y + 6.0f, 14.5f, theme->text_primary);
    DrawLine((int)r.x + 10, (int)(r.y + r.height - 1.0f),
             (int)(r.x + r.width - 10.0f), (int)(r.y + r.height - 1.0f), theme->border);
}

void ui_draw_banner(Rectangle r, const char *text, RataBannerLevel level, const RataTheme *theme) {
    if (theme == NULL || text == NULL) {
        return;
    }
    Color bar = theme->border_strong;
    Color fill = theme->bg_panel;
    Color text_col = theme->text_secondary;
    if (level == BANNER_OK) {
        bar = theme->success;
        text_col = theme->text_primary;
    } else if (level == BANNER_WARN) {
        bar = theme->warning;
        fill = (Color){62, 54, 38, 255};
        text_col = theme->text_primary;
    } else if (level == BANNER_ERROR) {
        bar = theme->danger;
        fill = (Color){64, 42, 38, 255};
        text_col = theme->text_primary;
    }
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, 1.0f, theme->border);
    DrawRectangle((int)r.x, (int)r.y, 3, (int)r.height, bar);

    ui_begin_scissor(theme, r.x + 8.0f, r.y, r.width - 12.0f, r.height);
    ui_draw_text(theme, text, r.x + 10.0f, r.y + (r.height - 13.0f) * 0.5f,
                 13.0f, text_col);
    EndScissorMode();
}

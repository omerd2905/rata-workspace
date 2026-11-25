#ifndef RATA_UI_THEME_H
#define RATA_UI_THEME_H

#include <stdbool.h>

#include "raylib.h"

#define RATA_TRACE_PALETTE_COUNT 6

/* Shared minimalist warm studio palette (Pale Oak / Coffee Bean / Dark Khaki)
 * and loaded UI font. Passed explicitly (no hidden globals). */
typedef struct RataTheme {
    Color bg_topbar;
    Color bg_app;
    Color bg_sidebar;
    Color bg_panel;
    Color bg_plot;
    Color bg_input;
    Color bg_button;
    Color bg_button_hover;
    Color bg_button_active;
    Color border;
    Color border_strong;
    Color grid_major;
    Color grid_minor;
    Color axis_zero;
    Color text_primary;
    Color text_secondary;
    Color text_muted;
    Color accent;
    Color success;
    Color warning;
    Color danger;
    Color traces[RATA_TRACE_PALETTE_COUNT];
    Font font;
    bool has_custom_font;
    float render_scale;
} RataTheme;

typedef enum RataBannerLevel {
    BANNER_INFO = 0,
    BANNER_OK = 1,
    BANNER_WARN = 2,
    BANNER_ERROR = 3
} RataBannerLevel;

void ui_theme_init_default(RataTheme *theme);

/* Loads embedded OFL Lora Regular font (with JetBrains Mono fallback for math/Greek symbols)
 * into GPU memory. Must be called after InitWindow(). Caller must call ui_theme_unload_font()
 * before CloseWindow(). */
void ui_theme_load_font(RataTheme *theme);
void ui_theme_unload_font(RataTheme *theme);

void ui_theme_apply_raygui(const RataTheme *theme);

/* Crisp text drawing/measurement using loaded Lora font (fallback to default font). */
void ui_draw_text(const RataTheme *theme, const char *text, float x, float y, float size, Color color);
int ui_measure_text(const RataTheme *theme, const char *text, float size);

/* DPI/zoom-aware scissor rectangle matching active Camera2D render_scale. */
void ui_begin_scissor(const RataTheme *theme, float x, float y, float width, float height);

void ui_draw_panel(Rectangle r, const RataTheme *theme);
void ui_draw_section_title(Rectangle r, const char *title, const RataTheme *theme);
void ui_draw_banner(Rectangle r, const char *text, RataBannerLevel level, const RataTheme *theme);

#endif /* RATA_UI_THEME_H */

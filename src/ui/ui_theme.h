#ifndef RATA_UI_THEME_H
#define RATA_UI_THEME_H

#include <stdbool.h>

#include "raylib.h"

#define RATA_TRACE_PALETTE_COUNT 6

/* Shared dark engineering workstation palette and loaded UI font.
 * Passed explicitly (no hidden globals). */
typedef struct RataTheme {
    Color bg_app;
    Color bg_sidebar;
    Color bg_panel;
    Color bg_plot;
    Color bg_input;
    Color bg_button;
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
} RataTheme;

typedef enum RataBannerLevel {
    BANNER_INFO = 0,
    BANNER_OK = 1,
    BANNER_WARN = 2,
    BANNER_ERROR = 3
} RataBannerLevel;

void ui_theme_init_default(RataTheme *theme);

/* Loads embedded OFL JetBrains Mono font (ASCII + Greek + math codepoints) into GPU memory.
 * Must be called after InitWindow(). Caller must call ui_theme_unload_font() before CloseWindow(). */
void ui_theme_load_font(RataTheme *theme);
void ui_theme_unload_font(RataTheme *theme);

void ui_theme_apply_raygui(const RataTheme *theme);

/* Crisp text drawing/measurement using loaded JetBrains Mono font (fallback to default font). */
void ui_draw_text(const RataTheme *theme, const char *text, float x, float y, float size, Color color);
int ui_measure_text(const RataTheme *theme, const char *text, float size);

void ui_draw_panel(Rectangle r, const RataTheme *theme);
void ui_draw_section_title(Rectangle r, const char *title, const RataTheme *theme);
void ui_draw_banner(Rectangle r, const char *text, RataBannerLevel level, const RataTheme *theme);

#endif /* RATA_UI_THEME_H */

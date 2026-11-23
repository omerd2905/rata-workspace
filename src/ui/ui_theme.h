#ifndef RATA_UI_THEME_H
#define RATA_UI_THEME_H

#include "raylib.h"

#define RATA_TRACE_PALETTE_COUNT 6

/* Shared dark engineering workstation palette. Passed explicitly (no hidden globals). */
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
} RataTheme;

typedef enum RataBannerLevel {
    BANNER_INFO = 0,
    BANNER_OK = 1,
    BANNER_WARN = 2,
    BANNER_ERROR = 3
} RataBannerLevel;

void ui_theme_init_default(RataTheme *theme);
void ui_theme_apply_raygui(const RataTheme *theme);

void ui_draw_panel(Rectangle r, const RataTheme *theme);
void ui_draw_section_title(Rectangle r, const char *title, const RataTheme *theme);
void ui_draw_banner(Rectangle r, const char *text, RataBannerLevel level, const RataTheme *theme);

#endif /* RATA_UI_THEME_H */

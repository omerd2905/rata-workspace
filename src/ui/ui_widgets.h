#ifndef RATA_UI_WIDGETS_H
#define RATA_UI_WIDGETS_H

#include <stdbool.h>
#include <stdint.h>

#include "raylib.h"
#include "ui/ui_theme.h"

/* Explicit focus tracker for immediate-mode raygui text inputs (zero globals). */
typedef struct UiInputFocus {
    int32_t active_id;
} UiInputFocus;

/* Labeled single-line text input. Returns true when user presses Enter or clicks away after edit. */
bool ui_text_field(Rectangle bounds,
                   const char *label,
                   char *buf,
                   int buf_size,
                   int32_t field_id,
                   UiInputFocus *focus,
                   const RataTheme *theme);

/* Labeled horizontal slider with live value + units display. Returns true when value changes. */
bool ui_slider_row(Rectangle bounds,
                   const char *label,
                   const char *unit,
                   double *val,
                   double min_v,
                   double max_v,
                   const RataTheme *theme);

/* Horizontal segmented button bar. Updates `*selected` and returns true when changed. */
bool ui_segmented_tabs(Rectangle bounds,
                       const char *const *labels,
                       int count,
                       int *selected,
                       const RataTheme *theme);

/* Minimalist underline tab bar for workspace sub-views. Updates `*selected` and returns true when changed. */
bool ui_tab_bar(Rectangle bounds,
                const char *const *labels,
                int count,
                int *selected,
                const RataTheme *theme);

/* Crisp themed button (supports active/toggled highlight). Returns true on click. */
bool ui_button(Rectangle bounds, const char *label, bool active, const RataTheme *theme);

/* Small toggle pill (e.g. dB / Linear, Log X, Shift). Returns true when toggled. */
bool ui_toggle_chip(Rectangle bounds, const char *label, bool *state, const RataTheme *theme);

#endif /* RATA_UI_WIDGETS_H */

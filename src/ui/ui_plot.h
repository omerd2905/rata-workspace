#ifndef RATA_UI_PLOT_H
#define RATA_UI_PLOT_H

#include <stdbool.h>
#include <stddef.h>

#include "raylib.h"
#include "ui/ui_theme.h"

typedef enum PlotTraceStyle {
    PLOT_LINE = 0,
    PLOT_STEM = 1,
    PLOT_SCATTER = 2,
    PLOT_STEP = 3
} PlotTraceStyle;

typedef struct PlotTrace {
    const double *xs;
    const double *ys;
    size_t count;
    const char *label;
    Color color;
    PlotTraceStyle style;
    float thickness;
} PlotTrace;

typedef enum PlotMarkerKind {
    PLOT_MARKER_POLE = 0,   /* 'X' at (x, y) */
    PLOT_MARKER_ZERO = 1,   /* 'O' at (x, y) */
    PLOT_MARKER_VLINE = 2,  /* Vertical reference line at x */
    PLOT_MARKER_HLINE = 3   /* Horizontal reference line at y */
} PlotMarkerKind;

typedef struct PlotMarker {
    PlotMarkerKind kind;
    double x;
    double y;
    const char *label;
    Color color;
} PlotMarker;

/* Persistent interactive view state owned by the caller (no hidden globals). */
typedef struct PlotView {
    double x_min;
    double x_max;
    double y_min;
    double y_max;
    bool log_x;
    bool log_y;
    bool equal_aspect;
    bool initialized;
    bool is_panning;
    Vector2 pan_mouse_start;
    double pan_x0;
    double pan_x1;
    double pan_y0;
    double pan_y1;
    double last_click_time;
} PlotView;

typedef struct PlotConfig {
    const char *title;
    const char *x_label;
    const char *y_label;
    bool show_grid;
    bool show_legend;
    bool show_zero_axes;
    bool show_unit_circle;
} PlotConfig;

/* Resets PlotView to defaults (next draw will auto-fit bounds). */
void ui_plot_view_reset(PlotView *view);

/* Computes tight padded bounds from traces and markers (respecting log_x / log_y / unit circle). */
void ui_plot_autofit(PlotView *view,
                     const PlotTrace *traces,
                     size_t trace_count,
                     const PlotMarker *markers,
                     size_t marker_count,
                     bool include_unit_circle);

/* Draws interactive 2D scientific plot inside `bounds` and updates `view` on zoom/pan/fit. */
void ui_plot_draw(Rectangle bounds,
                  const PlotConfig *cfg,
                  PlotView *view,
                  const PlotTrace *traces,
                  size_t trace_count,
                  const PlotMarker *markers,
                  size_t marker_count,
                  const RataTheme *theme);

#endif /* RATA_UI_PLOT_H */

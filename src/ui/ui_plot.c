#include "ui/ui_plot.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/status.h"
#include "ui/ui_widgets.h"

static double nice_step(double span, int target_ticks) {
    if (!isfinite(span) || span <= 0.0 || target_ticks <= 0) {
        return 1.0;
    }
    double raw = span / (double)target_ticks;
    double exp10 = floor(log10(raw));
    double base = pow(10.0, exp10);
    double frac = raw / base;
    double nice = 1.0;
    if (frac <= 1.5) {
        nice = 1.0;
    } else if (frac <= 3.0) {
        nice = 2.0;
    } else if (frac <= 7.0) {
        nice = 5.0;
    } else {
        nice = 10.0;
    }
    return nice * base;
}

static void format_tick(double v, char *buf, size_t cap) {
    if (fabs(v) < 1.0e-12) {
        v = 0.0;
    }
    if (fabs(v) >= 1.0e4 || (fabs(v) > 0.0 && fabs(v) < 1.0e-3)) {
        snprintf(buf, cap, "%.2e", v);
    } else {
        snprintf(buf, cap, "%.4g", v);
    }
}

void ui_plot_view_reset(PlotView *view) {
    if (view == NULL) {
        return;
    }
    bool lx = view->log_x;
    bool ly = view->log_y;
    bool eq = view->equal_aspect;
    memset(view, 0, sizeof(*view));
    view->log_x = lx;
    view->log_y = ly;
    view->equal_aspect = eq;
}

void ui_plot_autofit(PlotView *view,
                     const PlotTrace *traces,
                     size_t trace_count,
                     const PlotMarker *markers,
                     size_t marker_count,
                     bool include_unit_circle) {
    if (view == NULL) {
        return;
    }
    double xmin = 1.0e300, xmax = -1.0e300;
    double ymin = 1.0e300, ymax = -1.0e300;
    bool has_x = false, has_y = false;

    if (include_unit_circle && !view->log_x && !view->log_y) {
        xmin = -1.25;
        xmax = 1.25;
        ymin = -1.25;
        ymax = 1.25;
        has_x = true;
        has_y = true;
    }

    for (size_t t = 0u; t < trace_count; ++t) {
        const PlotTrace *tr = &traces[t];
        if (tr->xs == NULL || tr->ys == NULL) {
            continue;
        }
        for (size_t i = 0u; i < tr->count; ++i) {
            double x = tr->xs[i];
            double y = tr->ys[i];
            if (!isfinite(x) || !isfinite(y)) {
                continue;
            }
            if (view->log_x && x <= 0.0) {
                continue;
            }
            if (view->log_y && y <= 0.0) {
                continue;
            }
            if (x < xmin) { xmin = x; }
            if (x > xmax) { xmax = x; }
            if (y < ymin) { ymin = y; }
            if (y > ymax) { ymax = y; }
            if (tr->style == PLOT_STEM && !view->log_y) {
                if (0.0 < ymin) { ymin = 0.0; }
                if (0.0 > ymax) { ymax = 0.0; }
            }
            has_x = true;
            has_y = true;
        }
    }

    for (size_t m = 0u; m < marker_count; ++m) {
        const PlotMarker *mk = &markers[m];
        if (mk->kind == PLOT_MARKER_POLE || mk->kind == PLOT_MARKER_ZERO) {
            if (isfinite(mk->x) && (!view->log_x || mk->x > 0.0)) {
                if (mk->x < xmin) { xmin = mk->x; }
                if (mk->x > xmax) { xmax = mk->x; }
                has_x = true;
            }
            if (isfinite(mk->y) && (!view->log_y || mk->y > 0.0)) {
                if (mk->y < ymin) { ymin = mk->y; }
                if (mk->y > ymax) { ymax = mk->y; }
                has_y = true;
            }
        }
    }

    if (!has_x) {
        xmin = view->log_x ? 1.0 : -1.0;
        xmax = view->log_x ? 1000.0 : 1.0;
    }
    if (!has_y) {
        ymin = view->log_y ? 1.0e-3 : -1.0;
        ymax = view->log_y ? 10.0 : 1.0;
    }

    if (view->log_x) {
        if (xmin <= 0.0) { xmin = 1.0e-3; }
        if (xmax <= xmin) { xmax = xmin * 10.0; }
        double lx0 = log10(xmin);
        double lx1 = log10(xmax);
        double pad = (lx1 - lx0) * 0.05;
        view->x_min = pow(10.0, lx0 - pad);
        view->x_max = pow(10.0, lx1 + pad);
    } else {
        double dx = xmax - xmin;
        double pad = (dx < 1.0e-9) ? 1.0 : (dx * 0.06);
        view->x_min = xmin - pad;
        view->x_max = xmax + pad;
    }

    if (view->log_y) {
        if (ymin <= 0.0) { ymin = 1.0e-4; }
        if (ymax <= ymin) { ymax = ymin * 10.0; }
        double ly0 = log10(ymin);
        double ly1 = log10(ymax);
        double pad = (ly1 - ly0) * 0.08;
        view->y_min = pow(10.0, ly0 - pad);
        view->y_max = pow(10.0, ly1 + pad);
    } else {
        double dy = ymax - ymin;
        double pad = (dy < 1.0e-9) ? 1.0 : (dy * 0.10);
        view->y_min = ymin - pad;
        view->y_max = ymax + pad;
    }
    view->initialized = true;
}

/* Effective axis bounds in mapped coordinates (linear value, or log10 value when log axis). */
typedef struct PlotMapper {
    Rectangle area;
    double ux0, ux1;
    double uy0, uy1;
    bool log_x;
    bool log_y;
} PlotMapper;

static PlotMapper make_mapper(Rectangle area, const PlotView *view) {
    PlotMapper mp;
    mp.area = area;
    mp.log_x = view->log_x;
    mp.log_y = view->log_y;

    double xmin = view->x_min;
    double xmax = view->x_max;
    double ymin = view->y_min;
    double ymax = view->y_max;
    if (mp.log_x) {
        if (xmin <= 0.0) { xmin = 1.0e-3; }
        if (xmax <= xmin) { xmax = xmin * 10.0; }
        mp.ux0 = log10(xmin);
        mp.ux1 = log10(xmax);
    } else {
        if (xmax <= xmin) { xmax = xmin + 1.0; }
        mp.ux0 = xmin;
        mp.ux1 = xmax;
    }
    if (mp.log_y) {
        if (ymin <= 0.0) { ymin = 1.0e-4; }
        if (ymax <= ymin) { ymax = ymin * 10.0; }
        mp.uy0 = log10(ymin);
        mp.uy1 = log10(ymax);
    } else {
        if (ymax <= ymin) { ymax = ymin + 1.0; }
        mp.uy0 = ymin;
        mp.uy1 = ymax;
    }

    if (view->equal_aspect && !mp.log_x && !mp.log_y && area.width > 10.0f && area.height > 10.0f) {
        double sx = (mp.ux1 - mp.ux0) / (double)area.width;
        double sy = (mp.uy1 - mp.uy0) / (double)area.height;
        double cx = 0.5 * (mp.ux0 + mp.ux1);
        double cy = 0.5 * (mp.uy0 + mp.uy1);
        if (sx > sy) {
            double half_y = 0.5 * sx * (double)area.height;
            mp.uy0 = cy - half_y;
            mp.uy1 = cy + half_y;
        } else {
            double half_x = 0.5 * sy * (double)area.width;
            mp.ux0 = cx - half_x;
            mp.ux1 = cx + half_x;
        }
    }
    return mp;
}

static bool map_to_px(const PlotMapper *mp, double x, double y, Vector2 *out) {
    if (!isfinite(x) || !isfinite(y)) {
        return false;
    }
    if (mp->log_x && x <= 0.0) {
        return false;
    }
    if (mp->log_y && y <= 0.0) {
        return false;
    }
    double ux = mp->log_x ? log10(x) : x;
    double uy = mp->log_y ? log10(y) : y;
    double tx = (ux - mp->ux0) / (mp->ux1 - mp->ux0);
    double ty = (uy - mp->uy0) / (mp->uy1 - mp->uy0);
    out->x = mp->area.x + (float)(tx * (double)mp->area.width);
    out->y = mp->area.y + mp->area.height - (float)(ty * (double)mp->area.height);
    return isfinite(out->x) && isfinite(out->y);
}

static void px_to_data(const PlotMapper *mp, Vector2 p, double *x_out, double *y_out) {
    double tx = ((double)(p.x - mp->area.x)) / (double)mp->area.width;
    double ty = ((double)(mp->area.y + mp->area.height - p.y)) / (double)mp->area.height;
    double ux = mp->ux0 + tx * (mp->ux1 - mp->ux0);
    double uy = mp->uy0 + ty * (mp->uy1 - mp->uy0);
    *x_out = mp->log_x ? pow(10.0, ux) : ux;
    *y_out = mp->log_y ? pow(10.0, uy) : uy;
}

static void handle_interaction(Rectangle area,
                               PlotView *view,
                               const PlotMapper *mp,
                               const PlotTrace *traces,
                               size_t trace_count,
                               const PlotMarker *markers,
                               size_t marker_count,
                               bool include_unit_circle) {
    Vector2 mouse = GetMousePosition();
    bool inside = CheckCollisionPointRec(mouse, area);

    /* Double-click to auto-fit. */
    if (inside && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        double now = GetTime();
        if (now - view->last_click_time < 0.30) {
            ui_plot_autofit(view, traces, trace_count, markers, marker_count, include_unit_circle);
            view->last_click_time = 0.0;
            view->is_panning = false;
            return;
        }
        view->last_click_time = now;
        view->is_panning = true;
        view->pan_mouse_start = mouse;
        view->pan_x0 = mp->ux0;
        view->pan_x1 = mp->ux1;
        view->pan_y0 = mp->uy0;
        view->pan_y1 = mp->uy1;
    }

    if (view->is_panning) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            double dx_u = -((double)(mouse.x - view->pan_mouse_start.x) / (double)area.width) *
                          (view->pan_x1 - view->pan_x0);
            double dy_u = ((double)(mouse.y - view->pan_mouse_start.y) / (double)area.height) *
                          (view->pan_y1 - view->pan_y0);
            double nx0 = view->pan_x0 + dx_u;
            double nx1 = view->pan_x1 + dx_u;
            double ny0 = view->pan_y0 + dy_u;
            double ny1 = view->pan_y1 + dy_u;
            view->x_min = view->log_x ? pow(10.0, nx0) : nx0;
            view->x_max = view->log_x ? pow(10.0, nx1) : nx1;
            view->y_min = view->log_y ? pow(10.0, ny0) : ny0;
            view->y_max = view->log_y ? pow(10.0, ny1) : ny1;
        } else {
            view->is_panning = false;
        }
    }

    /* Scroll wheel zoom anchored at cursor. */
    if (inside) {
        float wheel = GetMouseWheelMove();
        if (fabsf(wheel) > 0.01f) {
            double factor = (wheel > 0.0f) ? 0.85 : (1.0 / 0.85);
            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            bool zoom_x = !ctrl || shift;
            bool zoom_y = !shift || ctrl;

            double tx = ((double)(mouse.x - area.x)) / (double)area.width;
            double ty = ((double)(area.y + area.height - mouse.y)) / (double)area.height;
            double anchor_x = mp->ux0 + tx * (mp->ux1 - mp->ux0);
            double anchor_y = mp->uy0 + ty * (mp->uy1 - mp->uy0);

            if (zoom_x) {
                double nx0 = anchor_x - (anchor_x - mp->ux0) * factor;
                double nx1 = anchor_x + (mp->ux1 - anchor_x) * factor;
                if (nx1 - nx0 > 1.0e-9 && nx1 - nx0 < 1.0e12) {
                    view->x_min = view->log_x ? pow(10.0, nx0) : nx0;
                    view->x_max = view->log_x ? pow(10.0, nx1) : nx1;
                }
            }
            if (zoom_y) {
                double ny0 = anchor_y - (anchor_y - mp->uy0) * factor;
                double ny1 = anchor_y + (mp->uy1 - anchor_y) * factor;
                if (ny1 - ny0 > 1.0e-9 && ny1 - ny0 < 1.0e12) {
                    view->y_min = view->log_y ? pow(10.0, ny0) : ny0;
                    view->y_max = view->log_y ? pow(10.0, ny1) : ny1;
                }
            }
        }
    }
}

static void draw_grid_and_axes(const PlotMapper *mp,
                               const PlotConfig *cfg,
                               const RataTheme *theme) {
    char lbl[32];

    /* X ticks and vertical grid lines. */
    if (mp->log_x) {
        int d0 = (int)floor(mp->ux0);
        int d1 = (int)ceil(mp->ux1);
        for (int d = d0; d <= d1; ++d) {
            double v = pow(10.0, (double)d);
            Vector2 p;
            if (map_to_px(mp, v, mp->log_y ? pow(10.0, mp->uy0) : mp->uy0, &p)) {
                if (p.x >= mp->area.x - 1.0f && p.x <= mp->area.x + mp->area.width + 1.0f) {
                    if (cfg->show_grid) {
                        DrawLine((int)p.x, (int)mp->area.y,
                                 (int)p.x, (int)(mp->area.y + mp->area.height), theme->grid_major);
                    }
                    snprintf(lbl, sizeof(lbl), "1e%d", d);
                    int tw = MeasureText(lbl, 10);
                    DrawText(lbl, (int)p.x - tw / 2, (int)(mp->area.y + mp->area.height + 4.0f),
                             10, theme->text_muted);
                }
            }
            if (cfg->show_grid && d1 - d0 <= 6) {
                for (int m = 2; m <= 9; ++m) {
                    Vector2 pm;
                    if (map_to_px(mp, v * (double)m, mp->log_y ? pow(10.0, mp->uy0) : mp->uy0, &pm)) {
                        if (pm.x > mp->area.x && pm.x < mp->area.x + mp->area.width) {
                            DrawLine((int)pm.x, (int)mp->area.y,
                                     (int)pm.x, (int)(mp->area.y + mp->area.height), theme->grid_minor);
                        }
                    }
                }
            }
        }
    } else {
        double step = nice_step(mp->ux1 - mp->ux0, (int)(mp->area.width / 78.0f));
        double first = ceil(mp->ux0 / step) * step;
        for (double v = first; v <= mp->ux1 + step * 0.01; v += step) {
            Vector2 p;
            if (map_to_px(mp, v, mp->log_y ? pow(10.0, mp->uy0) : mp->uy0, &p)) {
                if (cfg->show_grid) {
                    Color gc = (cfg->show_zero_axes && fabs(v) < step * 1.0e-4)
                                   ? theme->axis_zero
                                   : theme->grid_major;
                    DrawLine((int)p.x, (int)mp->area.y,
                             (int)p.x, (int)(mp->area.y + mp->area.height), gc);
                }
                format_tick(v, lbl, sizeof(lbl));
                int tw = MeasureText(lbl, 10);
                DrawText(lbl, (int)p.x - tw / 2, (int)(mp->area.y + mp->area.height + 4.0f),
                         10, theme->text_muted);
            }
        }
    }

    /* Y ticks and horizontal grid lines. */
    if (mp->log_y) {
        int d0 = (int)floor(mp->uy0);
        int d1 = (int)ceil(mp->uy1);
        for (int d = d0; d <= d1; ++d) {
            double v = pow(10.0, (double)d);
            Vector2 p;
            if (map_to_px(mp, mp->log_x ? pow(10.0, mp->ux0) : mp->ux0, v, &p)) {
                if (p.y >= mp->area.y - 1.0f && p.y <= mp->area.y + mp->area.height + 1.0f) {
                    if (cfg->show_grid) {
                        DrawLine((int)mp->area.x, (int)p.y,
                                 (int)(mp->area.x + mp->area.width), (int)p.y, theme->grid_major);
                    }
                    snprintf(lbl, sizeof(lbl), "1e%d", d);
                    int tw = MeasureText(lbl, 10);
                    DrawText(lbl, (int)mp->area.x - tw - 6, (int)p.y - 5, 10, theme->text_muted);
                }
            }
        }
    } else {
        double step = nice_step(mp->uy1 - mp->uy0, (int)(mp->area.height / 44.0f));
        double first = ceil(mp->uy0 / step) * step;
        for (double v = first; v <= mp->uy1 + step * 0.01; v += step) {
            Vector2 p;
            if (map_to_px(mp, mp->log_x ? pow(10.0, mp->ux0) : mp->ux0, v, &p)) {
                if (cfg->show_grid) {
                    Color gc = (cfg->show_zero_axes && fabs(v) < step * 1.0e-4)
                                   ? theme->axis_zero
                                   : theme->grid_major;
                    DrawLine((int)mp->area.x, (int)p.y,
                             (int)(mp->area.x + mp->area.width), (int)p.y, gc);
                }
                format_tick(v, lbl, sizeof(lbl));
                int tw = MeasureText(lbl, 10);
                DrawText(lbl, (int)mp->area.x - tw - 6, (int)p.y - 5, 10, theme->text_muted);
            }
        }
    }
}

static void draw_traces(const PlotMapper *mp,
                        const PlotTrace *traces,
                        size_t trace_count) {
    for (size_t t = 0u; t < trace_count; ++t) {
        const PlotTrace *tr = &traces[t];
        if (tr->xs == NULL || tr->ys == NULL || tr->count == 0u) {
            continue;
        }
        float thick = (tr->thickness > 0.0f) ? tr->thickness : 1.8f;

        if (tr->style == PLOT_LINE || tr->style == PLOT_STEP) {
            bool have_prev = false;
            Vector2 prev = {0.0f, 0.0f};
            for (size_t i = 0u; i < tr->count; ++i) {
                Vector2 cur;
                if (!map_to_px(mp, tr->xs[i], tr->ys[i], &cur)) {
                    have_prev = false;
                    continue;
                }
                if (have_prev) {
                    if (tr->style == PLOT_STEP) {
                        Vector2 corner = {cur.x, prev.y};
                        DrawLineEx(prev, corner, thick, tr->color);
                        DrawLineEx(corner, cur, thick, tr->color);
                    } else {
                        DrawLineEx(prev, cur, thick, tr->color);
                    }
                }
                prev = cur;
                have_prev = true;
            }
        } else if (tr->style == PLOT_STEM) {
            double base_y = mp->log_y ? pow(10.0, mp->uy0) : 0.0;
            for (size_t i = 0u; i < tr->count; ++i) {
                Vector2 top, bot;
                if (!map_to_px(mp, tr->xs[i], tr->ys[i], &top)) {
                    continue;
                }
                if (!map_to_px(mp, tr->xs[i], base_y, &bot)) {
                    continue;
                }
                DrawLineEx(bot, top, thick, tr->color);
                DrawCircleV(top, 3.6f, tr->color);
            }
        } else if (tr->style == PLOT_SCATTER) {
            for (size_t i = 0u; i < tr->count; ++i) {
                Vector2 pt;
                if (map_to_px(mp, tr->xs[i], tr->ys[i], &pt)) {
                    DrawCircleV(pt, 3.8f, tr->color);
                }
            }
        }
    }
}

static void draw_markers(const PlotMapper *mp,
                         const PlotMarker *markers,
                         size_t marker_count) {
    for (size_t i = 0u; i < marker_count; ++i) {
        const PlotMarker *mk = &markers[i];
        if (mk->kind == PLOT_MARKER_POLE) {
            Vector2 p;
            if (map_to_px(mp, mk->x, mk->y, &p)) {
                float r = 6.0f;
                DrawLineEx((Vector2){p.x - r, p.y - r}, (Vector2){p.x + r, p.y + r}, 2.2f, mk->color);
                DrawLineEx((Vector2){p.x - r, p.y + r}, (Vector2){p.x + r, p.y - r}, 2.2f, mk->color);
            }
        } else if (mk->kind == PLOT_MARKER_ZERO) {
            Vector2 p;
            if (map_to_px(mp, mk->x, mk->y, &p)) {
                DrawCircleLines((int)p.x, (int)p.y, 6.5f, mk->color);
                DrawCircleLines((int)p.x, (int)p.y, 5.5f, mk->color);
            }
        } else if (mk->kind == PLOT_MARKER_VLINE) {
            Vector2 p;
            if (map_to_px(mp, mk->x, mp->log_y ? pow(10.0, mp->uy0) : 0.0, &p)) {
                DrawLineEx((Vector2){p.x, mp->area.y},
                           (Vector2){p.x, mp->area.y + mp->area.height}, 1.4f, mk->color);
                if (mk->label != NULL) {
                    DrawText(mk->label, (int)p.x + 4, (int)mp->area.y + 4, 10, mk->color);
                }
            }
        } else if (mk->kind == PLOT_MARKER_HLINE) {
            Vector2 p;
            if (map_to_px(mp, mp->log_x ? pow(10.0, mp->ux0) : 0.0, mk->y, &p)) {
                DrawLineEx((Vector2){mp->area.x, p.y},
                           (Vector2){mp->area.x + mp->area.width, p.y}, 1.4f, mk->color);
                if (mk->label != NULL) {
                    DrawText(mk->label, (int)mp->area.x + 6, (int)p.y - 13, 10, mk->color);
                }
            }
        }
    }
}

void ui_plot_draw(Rectangle bounds,
                  const PlotConfig *cfg,
                  PlotView *view,
                  const PlotTrace *traces,
                  size_t trace_count,
                  const PlotMarker *markers,
                  size_t marker_count,
                  const RataTheme *theme) {
    if (cfg == NULL || view == NULL || theme == NULL) {
        return;
    }
    if (!view->initialized) {
        ui_plot_autofit(view, traces, trace_count, markers, marker_count, cfg->show_unit_circle);
    }

    DrawRectangleRec(bounds, theme->bg_plot);
    DrawRectangleLinesEx(bounds, 1.0f, theme->border);

    float pad_l = 58.0f;
    float pad_r = 14.0f;
    float pad_t = (cfg->title != NULL && cfg->title[0] != '\0') ? 30.0f : 14.0f;
    float pad_b = (cfg->x_label != NULL && cfg->x_label[0] != '\0') ? 32.0f : 20.0f;

    Rectangle area = {
        bounds.x + pad_l,
        bounds.y + pad_t,
        bounds.width - pad_l - pad_r,
        bounds.height - pad_t - pad_b
    };
    if (area.width < 20.0f || area.height < 20.0f) {
        return;
    }

    if (cfg->title != NULL && cfg->title[0] != '\0') {
        BeginScissorMode((int)area.x, (int)bounds.y, (int)(area.width - 44.0f), (int)pad_t);
        DrawText(cfg->title, (int)area.x, (int)bounds.y + 8, 13, theme->text_primary);
        EndScissorMode();
    }

    /* Fit button in top-right header corner. */
    Rectangle fit_btn = {bounds.x + bounds.width - 42.0f, bounds.y + 4.0f, 34.0f, 18.0f};
    if (ui_button(fit_btn, "Fit", false, theme)) {
        ui_plot_autofit(view, traces, trace_count, markers, marker_count, cfg->show_unit_circle);
    }

    PlotMapper mp = make_mapper(area, view);
    handle_interaction(area, view, &mp, traces, trace_count, markers, marker_count,
                       cfg->show_unit_circle);
    mp = make_mapper(area, view);

    draw_grid_and_axes(&mp, cfg, theme);

    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);

    /* Optional unit circle for z-plane pole-zero diagrams. */
    if (cfg->show_unit_circle && !mp.log_x && !mp.log_y) {
        const int segs = 120;
        Vector2 prev = {0.0f, 0.0f};
        for (int i = 0; i <= segs; ++i) {
            double ang = RATA_TWO_PI * (double)i / (double)segs;
            Vector2 cur;
            if (map_to_px(&mp, cos(ang), sin(ang), &cur)) {
                if (i > 0) {
                    DrawLineEx(prev, cur, 1.4f, theme->border_strong);
                }
                prev = cur;
            }
        }
    }

    draw_traces(&mp, traces, trace_count);
    draw_markers(&mp, markers, marker_count);

    /* Hover crosshair and coordinate readout. */
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, area)) {
        Color ch = (Color){120, 145, 180, 120};
        DrawLine((int)mouse.x, (int)area.y, (int)mouse.x, (int)(area.y + area.height), ch);
        DrawLine((int)area.x, (int)mouse.y, (int)(area.x + area.width), (int)mouse.y, ch);

        double dx = 0.0, dy = 0.0;
        px_to_data(&mp, mouse, &dx, &dy);
        char coord[64];
        snprintf(coord, sizeof(coord), "(%.4g, %.4g)", dx, dy);
        int cw = MeasureText(coord, 11);
        Rectangle cbox = {area.x + area.width - (float)cw - 12.0f,
                          area.y + area.height - 20.0f, (float)cw + 8.0f, 16.0f};
        DrawRectangleRec(cbox, (Color){15, 20, 29, 210});
        DrawText(coord, (int)cbox.x + 4, (int)cbox.y + 3, 11, theme->accent);
    }

    /* Top-right legend inside plot area. */
    if (cfg->show_legend && trace_count > 0u) {
        float ly = area.y + 6.0f;
        for (size_t i = 0u; i < trace_count; ++i) {
            if (traces[i].label == NULL || traces[i].label[0] == '\0') {
                continue;
            }
            int lw = MeasureText(traces[i].label, 11);
            float lx = area.x + area.width - (float)lw - 24.0f;
            DrawRectangle((int)lx - 4, (int)ly - 1, lw + 22, 14, (Color){15, 20, 29, 200});
            DrawRectangle((int)lx, (int)ly + 3, 10, 6, traces[i].color);
            DrawText(traces[i].label, (int)lx + 14, (int)ly + 1, 11, theme->text_primary);
            ly += 16.0f;
        }
    }

    EndScissorMode();

    DrawRectangleLinesEx(area, 1.0f, theme->border_strong);

    if (cfg->x_label != NULL && cfg->x_label[0] != '\0') {
        int tw = MeasureText(cfg->x_label, 11);
        DrawText(cfg->x_label, (int)(area.x + (area.width - (float)tw) * 0.5f),
                 (int)(bounds.y + bounds.height - 14.0f), 11, theme->text_secondary);
    }
    if (cfg->y_label != NULL && cfg->y_label[0] != '\0') {
        int ylw = MeasureText(cfg->y_label, 10);
        int ylx = (int)area.x - ylw - 6;
        if (ylx < (int)bounds.x + 4) {
            ylx = (int)bounds.x + 4;
        }
        BeginScissorMode((int)bounds.x + 2, (int)bounds.y + 2, (int)pad_l - 6, (int)pad_t);
        DrawText(cfg->y_label, ylx, (int)area.y - 14, 10, theme->accent);
        EndScissorMode();
    }
}

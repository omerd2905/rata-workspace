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
                    int tw = ui_measure_text(theme, lbl, 12.0f);
                    ui_draw_text(theme, lbl, p.x - (float)tw * 0.5f,
                                 mp->area.y + mp->area.height + 4.0f, 12.0f, theme->text_muted);
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
        double step = nice_step(mp->ux1 - mp->ux0, (int)(mp->area.width / 80.0f));
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
                int tw = ui_measure_text(theme, lbl, 12.0f);
                ui_draw_text(theme, lbl, p.x - (float)tw * 0.5f,
                             mp->area.y + mp->area.height + 4.0f, 12.0f, theme->text_muted);
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
                    int tw = ui_measure_text(theme, lbl, 12.0f);
                    ui_draw_text(theme, lbl, mp->area.x - (float)tw - 6.0f,
                                 p.y - 6.0f, 12.0f, theme->text_muted);
                }
            }
        }
    } else {
        double step = nice_step(mp->uy1 - mp->uy0, (int)(mp->area.height / 46.0f));
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
                int tw = ui_measure_text(theme, lbl, 12.0f);
                ui_draw_text(theme, lbl, mp->area.x - (float)tw - 6.0f,
                             p.y - 6.0f, 12.0f, theme->text_muted);
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
                         size_t marker_count,
                         const RataTheme *theme) {
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
                    ui_draw_text(theme, mk->label, p.x + 4.0f, mp->area.y + 4.0f, 11.0f, mk->color);
                }
            }
        } else if (mk->kind == PLOT_MARKER_HLINE) {
            Vector2 p;
            if (map_to_px(mp, mp->log_x ? pow(10.0, mp->ux0) : 0.0, mk->y, &p)) {
                DrawLineEx((Vector2){mp->area.x, p.y},
                           (Vector2){mp->area.x + mp->area.width, p.y}, 1.4f, mk->color);
                if (mk->label != NULL) {
                    ui_draw_text(theme, mk->label, mp->area.x + 6.0f, p.y - 14.0f, 11.0f, mk->color);
                }
            }
        }
    }
}

static void draw_hover_probe(const PlotMapper *mp,
                             const PlotConfig *cfg,
                             Vector2 mouse,
                             const PlotTrace *traces,
                             size_t trace_count,
                             const PlotMarker *markers,
                             size_t marker_count,
                             const RataTheme *theme) {
    Color guide_col = (Color){198, 188, 170, 88};
    char coord[96];
    coord[0] = '\0';

    if (trace_count > 0u && traces != NULL) {
        bool all_discrete = true;
        for (size_t t = 0u; t < trace_count; ++t) {
            if (traces[t].style != PLOT_STEM && traces[t].style != PLOT_SCATTER) {
                all_discrete = false;
                break;
            }
        }

        bool found = false;
        float best_score = 1.0e30f;
        size_t best_t = 0u;
        double best_x = 0.0;
        double best_y = 0.0;
        Vector2 best_px = {0.0f, 0.0f};

        for (size_t t = 0u; t < trace_count; ++t) {
            const PlotTrace *tr = &traces[t];
            if (tr->xs == NULL || tr->ys == NULL || tr->count == 0u) {
                continue;
            }
            bool tr_found = false;
            float min_dx = 1.0e30f;
            float min_dy = 1.0e30f;
            double cand_x = 0.0;
            double cand_y = 0.0;
            Vector2 cand_px = {0.0f, 0.0f};

            for (size_t i = 0u; i < tr->count; ++i) {
                Vector2 pt;
                if (!map_to_px(mp, tr->xs[i], tr->ys[i], &pt)) {
                    continue;
                }
                if (pt.x < mp->area.x - 8.0f || pt.x > mp->area.x + mp->area.width + 8.0f) {
                    continue;
                }
                float dx = fabsf(pt.x - mouse.x);
                float dy = fabsf(pt.y - mouse.y);
                if (!tr_found || dx < min_dx - 0.25f ||
                    (fabsf(dx - min_dx) <= 0.25f && dy < min_dy)) {
                    tr_found = true;
                    min_dx = dx;
                    min_dy = dy;
                    cand_x = tr->xs[i];
                    cand_y = tr->ys[i];
                    cand_px = pt;
                }
            }

            if (!tr_found) {
                continue;
            }
            bool discrete = (tr->style == PLOT_STEM || tr->style == PLOT_SCATTER);
            if (trace_count > 1u && !all_discrete && discrete && min_dx > 12.0f) {
                continue;
            }
            float bonus = (discrete && min_dx <= 9.0f) ? -14.0f : 0.0f;
            if (trace_count > 1u && fabs(cand_y) > 1.0e-12 && min_dy > 8.0f) {
                /* Prefer active non-zero stems over zero-padded baseline dots when hovering a column. */
                bonus -= 24.0f;
            }
            float score = 2.5f * min_dx + 0.35f * min_dy + bonus;
            if (!found || score < best_score) {
                found = true;
                best_score = score;
                best_t = t;
                best_x = cand_x;
                best_y = cand_y;
                best_px = cand_px;
            }
        }

        if (found) {
            const PlotTrace *wtr = &traces[best_t];
            DrawLine((int)(best_px.x + 0.5f), (int)mp->area.y,
                     (int)(best_px.x + 0.5f), (int)(mp->area.y + mp->area.height), guide_col);
            if (best_px.y >= mp->area.y && best_px.y <= mp->area.y + mp->area.height) {
                DrawLine((int)mp->area.x, (int)(best_px.y + 0.5f),
                         (int)(mp->area.x + mp->area.width), (int)(best_px.y + 0.5f), guide_col);
                DrawCircleV(best_px, 4.5f, wtr->color);
                DrawCircleLines((int)(best_px.x + 0.5f), (int)(best_px.y + 0.5f),
                                6.5f, theme->text_primary);
            }

            const char *lbl = (wtr->label != NULL && wtr->label[0] != '\0') ? wtr->label : NULL;
            bool int_index = (wtr->style == PLOT_STEM || wtr->style == PLOT_SCATTER) &&
                             (fabs(best_x - round(best_x)) < 1.0e-6) &&
                             (fabs(best_x) < 1.0e8);
            if (int_index) {
                int n_idx = (int)round(best_x);
                if (lbl != NULL) {
                    snprintf(coord, sizeof(coord), "n = %d   %s = %.5g", n_idx, lbl, best_y);
                } else {
                    snprintf(coord, sizeof(coord), "n = %d   val = %.5g", n_idx, best_y);
                }
            } else if (lbl != NULL) {
                snprintf(coord, sizeof(coord), "%s: (%.5g, %.5g)", lbl, best_x, best_y);
            } else {
                snprintf(coord, sizeof(coord), "(%.5g, %.5g)", best_x, best_y);
            }
        }
    } else {
        /* Marker-only / Z-plane plot: snap to nearest pole or zero within 32 px. */
        bool snapped_mk = false;
        float best_d = 32.0f;
        const PlotMarker *win_mk = NULL;
        Vector2 win_px = {0.0f, 0.0f};
        for (size_t i = 0u; i < marker_count && markers != NULL; ++i) {
            if (markers[i].kind != PLOT_MARKER_POLE && markers[i].kind != PLOT_MARKER_ZERO) {
                continue;
            }
            Vector2 p;
            if (map_to_px(mp, markers[i].x, markers[i].y, &p)) {
                float d = hypotf(p.x - mouse.x, p.y - mouse.y);
                if (d <= best_d) {
                    best_d = d;
                    snapped_mk = true;
                    win_mk = &markers[i];
                    win_px = p;
                }
            }
        }
        if (snapped_mk && win_mk != NULL) {
            DrawLine((int)(win_px.x + 0.5f), (int)mp->area.y,
                     (int)(win_px.x + 0.5f), (int)(mp->area.y + mp->area.height), guide_col);
            DrawLine((int)mp->area.x, (int)(win_px.y + 0.5f),
                     (int)(mp->area.x + mp->area.width), (int)(win_px.y + 0.5f), guide_col);
            DrawCircleLines((int)(win_px.x + 0.5f), (int)(win_px.y + 0.5f),
                            9.0f, theme->text_primary);
            double mag = hypot(win_mk->x, win_mk->y);
            snprintf(coord, sizeof(coord), "%s: %.4g %+.4gj  (|z| = %.4g)",
                     (win_mk->kind == PLOT_MARKER_POLE) ? "Pole" : "Zero",
                     win_mk->x, win_mk->y, mag);
        } else {
            DrawLine((int)mouse.x, (int)mp->area.y,
                     (int)mouse.x, (int)(mp->area.y + mp->area.height), guide_col);
            DrawLine((int)mp->area.x, (int)mouse.y,
                     (int)(mp->area.x + mp->area.width), (int)mouse.y, guide_col);
            double dx = 0.0, dy = 0.0;
            px_to_data(mp, mouse, &dx, &dy);
            if (cfg->show_unit_circle) {
                snprintf(coord, sizeof(coord), "z = %.4g %+.4gj  (|z| = %.4g)",
                         dx, dy, hypot(dx, dy));
            } else {
                snprintf(coord, sizeof(coord), "(%.4g, %.4g)", dx, dy);
            }
        }
    }

    if (coord[0] != '\0') {
        int cw = ui_measure_text(theme, coord, 12.5f);
        Rectangle cbox = {mp->area.x + mp->area.width - (float)cw - 14.0f,
                          mp->area.y + mp->area.height - 22.0f, (float)cw + 10.0f, 18.0f};
        DrawRectangleRec(cbox, (Color){38, 28, 22, 232});
        DrawRectangleLinesEx(cbox, 1.0f, theme->border);
        ui_draw_text(theme, coord, cbox.x + 5.0f, cbox.y + 2.5f, 12.5f, theme->text_primary);
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

    float pad_l = 62.0f;
    float pad_r = 14.0f;
    float pad_t = (cfg->title != NULL && cfg->title[0] != '\0') ? 30.0f : 14.0f;
    float pad_b = (cfg->x_label != NULL && cfg->x_label[0] != '\0') ? 34.0f : 22.0f;

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
        ui_begin_scissor(theme, area.x, bounds.y, area.width - 46.0f, pad_t);
        ui_draw_text(theme, cfg->title, area.x, bounds.y + 7.0f, 14.0f, theme->text_primary);
        EndScissorMode();
    }

    /* Fit button in top-right header corner. */
    Rectangle fit_btn = {bounds.x + bounds.width - 44.0f, bounds.y + 5.0f, 36.0f, 20.0f};
    if (ui_button(fit_btn, "Fit", false, theme)) {
        ui_plot_autofit(view, traces, trace_count, markers, marker_count, cfg->show_unit_circle);
    }

    PlotMapper mp = make_mapper(area, view);
    handle_interaction(area, view, &mp, traces, trace_count, markers, marker_count,
                       cfg->show_unit_circle);
    mp = make_mapper(area, view);

    draw_grid_and_axes(&mp, cfg, theme);

    ui_begin_scissor(theme, area.x, area.y, area.width, area.height);

    /* Optional unit circle for z-plane pole-zero diagrams. */
    if (cfg->show_unit_circle && !mp.log_x && !mp.log_y) {
        const int segs = 120;
        Vector2 prev = {0.0f, 0.0f};
        for (int i = 0; i <= segs; ++i) {
            double ang = RATA_TWO_PI * (double)i / (double)segs;
            Vector2 cur;
            if (map_to_px(&mp, cos(ang), sin(ang), &cur)) {
                if (i > 0) {
                    DrawLineEx(prev, cur, 1.3f, theme->border_strong);
                }
                prev = cur;
            }
        }
    }

    draw_traces(&mp, traces, trace_count);
    draw_markers(&mp, markers, marker_count, theme);

    /* Sample-snapped hover probe (catches exact sample amplitude anywhere in vertical column). */
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, area)) {
        draw_hover_probe(&mp, cfg, mouse, traces, trace_count, markers, marker_count, theme);
    }

    /* Top-right legend inside plot area. */
    if (cfg->show_legend && trace_count > 0u) {
        float ly = area.y + 6.0f;
        for (size_t i = 0u; i < trace_count; ++i) {
            if (traces[i].label == NULL || traces[i].label[0] == '\0') {
                continue;
            }
            int lw = ui_measure_text(theme, traces[i].label, 12.5f);
            float lx = area.x + area.width - (float)lw - 26.0f;
            DrawRectangle((int)lx - 5, (int)ly - 1, lw + 24, 17, (Color){38, 28, 22, 215});
            DrawRectangle((int)lx, (int)ly + 6, 10, 3, traces[i].color);
            ui_draw_text(theme, traces[i].label, lx + 14.0f, ly + 1.5f, 12.5f, theme->text_primary);
            ly += 19.0f;
        }
    }

    EndScissorMode();

    DrawRectangleLinesEx(area, 1.0f, theme->border);

    if (cfg->x_label != NULL && cfg->x_label[0] != '\0') {
        int tw = ui_measure_text(theme, cfg->x_label, 12.5f);
        ui_draw_text(theme, cfg->x_label, area.x + (area.width - (float)tw) * 0.5f,
                     bounds.y + bounds.height - 16.0f, 12.5f, theme->text_secondary);
    }
    if (cfg->y_label != NULL && cfg->y_label[0] != '\0') {
        int ylw = ui_measure_text(theme, cfg->y_label, 12.0f);
        float ylx = area.x - (float)ylw - 6.0f;
        if (ylx < bounds.x + 4.0f) {
            ylx = bounds.x + 4.0f;
        }
        ui_begin_scissor(theme, bounds.x + 2.0f, bounds.y + 2.0f, pad_l - 6.0f, pad_t);
        ui_draw_text(theme, cfg->y_label, ylx, area.y - 15.0f, 12.0f, theme->text_secondary);
        EndScissorMode();
    }
}

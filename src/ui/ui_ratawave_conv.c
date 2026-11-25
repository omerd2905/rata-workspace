#include "ui/ui_ratawave_conv.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engines/dsp_conv.h"
#include "ui/ui_formula.h"
#include "ui/ui_widgets.h"

static void format_signal_samples(const RataSignal *sig,
                                  size_t max_items,
                                  char *buf,
                                  size_t cap) {
    if (buf == NULL || cap == 0u) {
        return;
    }
    buf[0] = '\0';
    if (sig == NULL || sig->samples == NULL || sig->len == 0u) {
        return;
    }
    size_t pos = 0u;
    size_t limit = (sig->len < max_items) ? sig->len : max_items;
    for (size_t i = 0u; i < limit; ++i) {
        double v = sig->samples[i];
        if (fabs(v) < 1.0e-12) {
            v = 0.0;
        }
        int n = snprintf(buf + pos, cap - pos, (i == 0u) ? "%.8g" : ", %.8g", v);
        if (n < 0 || pos + (size_t)n >= cap - 6u) {
            break;
        }
        pos += (size_t)n;
    }
    if (limit < sig->len && pos + 5u < cap) {
        snprintf(buf + pos, cap - pos, ", ...");
    }
}

static bool draw_signal_input_block(Rectangle area,
                                    const char *label,
                                    char *expr_buf,
                                    int expr_cap,
                                    int32_t field_id,
                                    int32_t *n0_ptr,
                                    int32_t *len_ptr,
                                    bool compact_only,
                                    UiInputFocus *focus,
                                    const RataTheme *t) {
    bool changed = false;
    float x = area.x;
    float w = area.width;
    float y = area.y;

    if (ui_text_field((Rectangle){x, y, w, 38.0f}, label,
                      expr_buf, expr_cap, field_id, focus, t)) {
        changed = true;
    }
    if (compact_only) {
        return changed;
    }

    y += 41.0f;
    float hw = (w - 8.0f) * 0.5f;
    double n0_d = (double)(*n0_ptr);
    if (ui_slider_row((Rectangle){x, y, hw, 30.0f}, "n0", "", &n0_d, -12.0, 12.0, t)) {
        *n0_ptr = (int32_t)round(n0_d);
        changed = true;
    }
    double len_d = (double)(*len_ptr);
    if (ui_slider_row((Rectangle){x + hw + 8.0f, y, hw, 30.0f}, "Len", "", &len_d, 2.0, 32.0, t)) {
        *len_ptr = (int32_t)round(len_d);
        changed = true;
    }
    return changed;
}

void ui_ratawave_draw_conv_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx) {
    const RataTheme *t = ctx->theme;
    float ctrl_w = 304.0f;
    Rectangle left = {area.x, area.y, ctrl_w, area.height};
    Rectangle right = {area.x + ctrl_w + 8.0f, area.y, area.width - ctrl_w - 8.0f, area.height};

    ui_draw_panel(left, t);
    ui_draw_section_title((Rectangle){left.x, left.y, left.width, 28.0f},
                          "Convolution & Deconvolution", t);

    float x = left.x + 10.0f;
    float w = left.width - 20.0f;
    float y = left.y + 34.0f;

    ui_draw_text(t, "Solve for (or Verify all 3):", x, y, 12.0f, t->text_muted);
    y += 15.0f;
    static const char *solve_labels[4] = {"y[n]", "h[n]", "x[n]", "Verify"};
    if (ui_segmented_tabs((Rectangle){x, y, w, 24.0f}, solve_labels, 4, &st->conv_solve_mode, t)) {
        ctx->focus->active_id = 0;
        st->conv_playing = false;
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    y += 30.0f;

    int mode = st->conv_solve_mode;
    bool verify_all = (mode == CONV_VERIFY_ALL);

    /* Render the relevant user inputs (expressions OR comma-separated sample lists). */
    if (mode == CONV_SOLVE_Y || mode == CONV_SOLVE_H || verify_all) {
        if (draw_signal_input_block((Rectangle){x, y, w, verify_all ? 38.0f : 72.0f},
                                    "x[n] (expr or sample list):",
                                    st->conv_expr_x, (int)sizeof(st->conv_expr_x), 201,
                                    &st->conv_x_n0, &st->conv_x_len, verify_all,
                                    ctx->focus, t)) {
            ui_plot_view_reset(&st->view_conv_top);
            ui_plot_view_reset(&st->view_conv_bot);
        }
        y += verify_all ? 43.0f : 76.0f;
    }

    if (mode == CONV_SOLVE_Y || mode == CONV_SOLVE_X || verify_all) {
        if (draw_signal_input_block((Rectangle){x, y, w, verify_all ? 38.0f : 72.0f},
                                    "h[n] (expr or sample list):",
                                    st->conv_expr_h, (int)sizeof(st->conv_expr_h), 202,
                                    &st->conv_h_n0, &st->conv_h_len, verify_all,
                                    ctx->focus, t)) {
            ui_plot_view_reset(&st->view_conv_top);
            ui_plot_view_reset(&st->view_conv_bot);
        }
        y += verify_all ? 43.0f : 76.0f;
    }

    if (mode == CONV_SOLVE_H || mode == CONV_SOLVE_X || verify_all) {
        if (draw_signal_input_block((Rectangle){x, y, w, verify_all ? 38.0f : 72.0f},
                                    "y[n] (expr or sample list):",
                                    st->conv_expr_y, (int)sizeof(st->conv_expr_y), 203,
                                    &st->conv_y_n0, &st->conv_y_len, verify_all,
                                    ctx->focus, t)) {
            ui_plot_view_reset(&st->view_conv_top);
            ui_plot_view_reset(&st->view_conv_bot);
        }
        y += verify_all ? 45.0f : 76.0f;
    }

    /* Quick textbook presets */
    float bw = (w - 4.0f) * 0.5f;
    if (ui_button((Rectangle){x, y, bw, 23.0f}, "FIR Example", false, t)) {
        snprintf(st->conv_expr_x, sizeof(st->conv_expr_x), "1, 2, 1");
        snprintf(st->conv_expr_h, sizeof(st->conv_expr_h), "1, -1, 2");
        snprintf(st->conv_expr_y, sizeof(st->conv_expr_y), "1, 1, 1, 3, 2");
        st->conv_x_n0 = 0; st->conv_x_len = 3;
        st->conv_h_n0 = 0; st->conv_h_len = 3;
        st->conv_y_n0 = 0; st->conv_y_len = 5;
        st->conv_n_shift = 2;
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    if (ui_button((Rectangle){x + bw + 4.0f, y, bw, 23.0f}, "Pulse * Exp", false, t)) {
        snprintf(st->conv_expr_x, sizeof(st->conv_expr_x), "u(n) - u(n - 5)");
        snprintf(st->conv_expr_h, sizeof(st->conv_expr_h), "0.75^n * u(n)");
        snprintf(st->conv_expr_y, sizeof(st->conv_expr_y),
                 "1, 1.75, 2.3125, 2.734375, 3.05078125, 2.28808594, 1.71606445, "
                 "1.28704834, 0.86517334, 0.54876709, 0.311462402, 0.133483887");
        st->conv_x_n0 = -2; st->conv_x_len = 10;
        st->conv_h_n0 = 0;  st->conv_h_len = 8;
        st->conv_y_n0 = 0;  st->conv_y_len = 12;
        st->conv_n_shift = 2;
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    y += 28.0f;

    /* Evaluate given signals & solve for the unknown signal */
    size_t mark = rata_arena_mark(ctx->frame_arena);
    RataSignal sx, sh, sy, sy_given;
    memset(&sx, 0, sizeof(sx));
    memset(&sh, 0, sizeof(sh));
    memset(&sy, 0, sizeof(sy));
    memset(&sy_given, 0, sizeof(sy_given));

    RataExprError err_a, err_b, err_c;
    memset(&err_a, 0, sizeof(err_a));
    memset(&err_b, 0, sizeof(err_b));
    memset(&err_c, 0, sizeof(err_c));

    RataDeconvInfo dinfo;
    memset(&dinfo, 0, sizeof(dinfo));
    RataStatus solve_st = RATA_OK;

    size_t x_len = (st->conv_x_len < 2) ? 2u : (size_t)st->conv_x_len;
    size_t h_len = (st->conv_h_len < 2) ? 2u : (size_t)st->conv_h_len;
    size_t y_len = (st->conv_y_len < 2) ? 2u : (size_t)st->conv_y_len;

    if (mode == CONV_SOLVE_Y) {
        RataStatus stx = dsp_signal_from_text(st->conv_expr_x, st->conv_x_n0, x_len,
                                              st->fs_hz, &sx, NULL, &err_a, ctx->frame_arena);
        RataStatus sth = dsp_signal_from_text(st->conv_expr_h, st->conv_h_n0, h_len,
                                              st->fs_hz, &sh, NULL, &err_b, ctx->frame_arena);
        if (stx != RATA_OK || sth != RATA_OK) {
            char msg[192];
            snprintf(msg, sizeof(msg), "Input parse error (%s): %s",
                     (stx != RATA_OK) ? "x[n]" : "h[n]",
                     (stx != RATA_OK) ? err_a.message : err_b.message);
            ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f}, msg, BANNER_ERROR, t);
            rata_arena_restore(ctx->frame_arena, mark);
            return;
        }
        solve_st = dsp_conv_linear(&sx, &sh, &sy, ctx->frame_arena);
    } else if (mode == CONV_SOLVE_H) {
        RataStatus stx = dsp_signal_from_text(st->conv_expr_x, st->conv_x_n0, x_len,
                                              st->fs_hz, &sx, NULL, &err_a, ctx->frame_arena);
        RataStatus sty = dsp_signal_from_text(st->conv_expr_y, st->conv_y_n0, y_len,
                                              st->fs_hz, &sy_given, NULL, &err_b, ctx->frame_arena);
        if (stx != RATA_OK || sty != RATA_OK) {
            char msg[192];
            snprintf(msg, sizeof(msg), "Input parse error (%s): %s",
                     (stx != RATA_OK) ? "x[n]" : "y[n]",
                     (stx != RATA_OK) ? err_a.message : err_b.message);
            ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f}, msg, BANNER_ERROR, t);
            rata_arena_restore(ctx->frame_arena, mark);
            return;
        }
        size_t max_h = (sy_given.len >= 4u) ? sy_given.len : 16u;
        solve_st = dsp_deconv_linear(&sy_given, &sx, max_h, &sh, &dinfo, ctx->frame_arena);
        if (solve_st == RATA_ERR_DIV_ZERO) {
            ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f},
                           "Cannot solve for h[n]: input signal x[n] is identically zero.",
                           BANNER_ERROR, t);
            rata_arena_restore(ctx->frame_arena, mark);
            return;
        }
        if (solve_st != RATA_OK && solve_st != RATA_ERR_UNSTABLE) {
            ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f},
                           "Deconvolution failed due to numerical range.", BANNER_ERROR, t);
            rata_arena_restore(ctx->frame_arena, mark);
            return;
        }
        (void)dsp_conv_linear(&sx, &sh, &sy, ctx->frame_arena);
    } else if (mode == CONV_SOLVE_X) {
        RataStatus sth = dsp_signal_from_text(st->conv_expr_h, st->conv_h_n0, h_len,
                                              st->fs_hz, &sh, NULL, &err_a, ctx->frame_arena);
        RataStatus sty = dsp_signal_from_text(st->conv_expr_y, st->conv_y_n0, y_len,
                                              st->fs_hz, &sy_given, NULL, &err_b, ctx->frame_arena);
        if (sth != RATA_OK || sty != RATA_OK) {
            char msg[192];
            snprintf(msg, sizeof(msg), "Input parse error (%s): %s",
                     (sth != RATA_OK) ? "h[n]" : "y[n]",
                     (sth != RATA_OK) ? err_a.message : err_b.message);
            ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f}, msg, BANNER_ERROR, t);
            rata_arena_restore(ctx->frame_arena, mark);
            return;
        }
        size_t max_x = (sy_given.len >= 4u) ? sy_given.len : 16u;
        solve_st = dsp_deconv_linear(&sy_given, &sh, max_x, &sx, &dinfo, ctx->frame_arena);
        if (solve_st == RATA_ERR_DIV_ZERO) {
            ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f},
                           "Cannot solve for x[n]: impulse response h[n] is identically zero.",
                           BANNER_ERROR, t);
            rata_arena_restore(ctx->frame_arena, mark);
            return;
        }
        if (solve_st != RATA_OK && solve_st != RATA_ERR_UNSTABLE) {
            ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f},
                           "Deconvolution failed due to numerical range.", BANNER_ERROR, t);
            rata_arena_restore(ctx->frame_arena, mark);
            return;
        }
        (void)dsp_conv_linear(&sx, &sh, &sy, ctx->frame_arena);
    } else {
        RataStatus stx = dsp_signal_from_text(st->conv_expr_x, st->conv_x_n0, x_len,
                                              st->fs_hz, &sx, NULL, &err_a, ctx->frame_arena);
        RataStatus sth = dsp_signal_from_text(st->conv_expr_h, st->conv_h_n0, h_len,
                                              st->fs_hz, &sh, NULL, &err_b, ctx->frame_arena);
        RataStatus sty = dsp_signal_from_text(st->conv_expr_y, st->conv_y_n0, y_len,
                                              st->fs_hz, &sy_given, NULL, &err_c, ctx->frame_arena);
        if (stx != RATA_OK || sth != RATA_OK || sty != RATA_OK) {
            const char *which = (stx != RATA_OK) ? "x[n]" : ((sth != RATA_OK) ? "h[n]" : "y[n]");
            const char *emsg = (stx != RATA_OK) ? err_a.message
                                                : ((sth != RATA_OK) ? err_b.message : err_c.message);
            char msg[192];
            snprintf(msg, sizeof(msg), "Input parse error (%s): %s", which, emsg);
            ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f}, msg, BANNER_ERROR, t);
            rata_arena_restore(ctx->frame_arena, mark);
            return;
        }
        solve_st = dsp_conv_linear(&sx, &sh, &sy, ctx->frame_arena);
    }

    if (sy.samples == NULL || sy.len == 0u) {
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    const RataSignal *res_sig = &sy;
    const char *res_name = "y[n]";
    if (mode == CONV_SOLVE_H) {
        res_sig = &sh;
        res_name = "h[n]";
    } else if (mode == CONV_SOLVE_X) {
        res_sig = &sx;
        res_name = "x[n]";
    }

    if (!verify_all) {
        char copy_lbl[48];
        snprintf(copy_lbl, sizeof(copy_lbl), "Copy Solved %s -> Input", res_name);
        if (ui_button((Rectangle){x, y, w, 23.0f}, copy_lbl, false, t)) {
            if (mode == CONV_SOLVE_Y) {
                format_signal_samples(&sy, 24u, st->conv_expr_y, sizeof(st->conv_expr_y));
                st->conv_y_n0 = sy.n0;
                st->conv_y_len = (int32_t)sy.len;
            } else if (mode == CONV_SOLVE_H) {
                format_signal_samples(&sh, 24u, st->conv_expr_h, sizeof(st->conv_expr_h));
                st->conv_h_n0 = sh.n0;
                st->conv_h_len = (int32_t)sh.len;
            } else if (mode == CONV_SOLVE_X) {
                format_signal_samples(&sx, 24u, st->conv_expr_x, sizeof(st->conv_expr_x));
                st->conv_x_n0 = sx.n0;
                st->conv_x_len = (int32_t)sx.len;
            }
        }
        y += 29.0f;
    }

    int32_t y_min_n = sy.n0;
    int32_t y_max_n = sy.n0 + (int32_t)sy.len - 1;
    if (st->conv_n_shift < y_min_n) { st->conv_n_shift = y_min_n; }
    if (st->conv_n_shift > y_max_n) { st->conv_n_shift = y_max_n; }

    /* Flip-and-Shift Playback controls */
    float pbw = (w - 8.0f) / 3.0f;
    if (ui_button((Rectangle){x, y, pbw, 24.0f},
                  st->conv_playing ? "Pause" : "Play", st->conv_playing, t)) {
        st->conv_playing = !st->conv_playing;
    }
    if (ui_button((Rectangle){x + pbw + 4.0f, y, pbw, 24.0f}, "< Step", false, t)) {
        st->conv_playing = false;
        if (st->conv_n_shift > y_min_n) { st->conv_n_shift--; }
    }
    if (ui_button((Rectangle){x + 2.0f * (pbw + 4.0f), y, pbw, 24.0f}, "Step >", false, t)) {
        st->conv_playing = false;
        if (st->conv_n_shift < y_max_n) { st->conv_n_shift++; }
    }
    y += 28.0f;

    if (st->conv_playing) {
        st->conv_accum_dt += ctx->dt * st->conv_speed;
        if (st->conv_accum_dt >= 1.0) {
            st->conv_accum_dt = 0.0;
            st->conv_n_shift++;
            if (st->conv_n_shift > y_max_n) {
                st->conv_n_shift = y_min_n;
            }
        }
    }

    float sh_hw = (w - 8.0f) * 0.5f;
    double ns_d = (double)st->conv_n_shift;
    double max_ns = (y_max_n > y_min_n) ? (double)y_max_n : (double)(y_min_n + 1);
    if (ui_slider_row((Rectangle){x, y, sh_hw, 30.0f}, "Shift n", "",
                      &ns_d, (double)y_min_n, max_ns, t)) {
        st->conv_n_shift = (int32_t)round(ns_d);
        st->conv_playing = false;
    }
    (void)ui_slider_row((Rectangle){x + sh_hw + 8.0f, y, sh_hw, 30.0f}, "Speed", "/s",
                        &st->conv_speed, 0.5, 8.0, t);
    y += 36.0f;

    RataConvStep step;
    if (dsp_conv_step(&sx, &sh, st->conv_n_shift, &step, ctx->frame_arena) != RATA_OK) {
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    /* Resulting sequence summary in left panel */
    if (y + 34.0f <= left.y + left.height) {
        char hdr_line[96];
        char seq_line[96];
        snprintf(hdr_line, sizeof(hdr_line), "Result %s (n0=%d, L=%zu)  |  (x*h)[%d]=%.5g",
                 res_name, (int)res_sig->n0, res_sig->len, st->conv_n_shift, step.sum);
        format_signal_samples(res_sig, 7u, seq_line, sizeof(seq_line));
        ui_draw_text(t, hdr_line, x, y, 12.0f, t->text_secondary);
        ui_draw_text(t, seq_line, x, y + 16.0f, 12.5f, t->text_primary);
    }

    /* Right side: Formula card + Flip/Shift plot + Result plot + Status banner */
    const char *f_cap = "Discrete Linear Convolution";
    const char *f_mk = "y[n] = \\sum_{k=-\\infty}^{\\infty} x[k] h[n - k]";
    if (mode == CONV_SOLVE_H) {
        f_cap = "System Identification (Deconvolution for h[n])";
        f_mk = "H(z) = frac{Y(z)}{X(z)},   y[n] = (x * h)[n]";
    } else if (mode == CONV_SOLVE_X) {
        f_cap = "Input Recovery (Deconvolution for x[n])";
        f_mk = "X(z) = frac{Y(z)}{H(z)},   y[n] = (x * h)[n]";
    } else if (verify_all) {
        f_cap = "Convolution Verification";
        f_mk = "e[n] = y_{given}[n] - \\sum_{k=-\\infty}^{\\infty} x[k] h[n - k]";
    }
    ui_formula_card((Rectangle){right.x, right.y, right.width, 54.0f},
                    f_cap, f_mk, 14.5f, t, ctx->frame_arena);

    float banner_h = 24.0f;
    float avail_h = right.height - 62.0f - banner_h - 6.0f;
    float top_h = avail_h * 0.50f;
    float bot_h = avail_h - top_h - 8.0f;

    PlotConfig top_cfg = {
        "Step View: x[k], Flipped & Shifted h[n - k], Product x[k]*h[n - k]",
        "Summation Index k (samples)",
        "Amp",
        true, true, true, false
    };
    PlotTrace top_traces[3] = {
        {step.k_axis, step.x_aligned, step.k_len, "x[k]", t->traces[0], PLOT_STEM, 2.0f},
        {step.k_axis, step.h_shifted, step.k_len, "h[n - k]", t->traces[1], PLOT_STEM, 2.0f},
        {step.k_axis, step.product, step.k_len, "x[k]*h[n-k]", t->traces[2], PLOT_STEM, 2.5f}
    };
    ui_plot_draw((Rectangle){right.x, right.y + 62.0f, right.width, top_h},
                 &top_cfg, &st->view_conv_top, top_traces, 3u, NULL, 0u, t);

    Rectangle bot_r = {right.x, right.y + 62.0f + top_h + 8.0f, right.width, bot_h};
    char banner_msg[192];
    RataBannerLevel banner_lvl = BANNER_INFO;

    if (verify_all) {
        int32_t c_min = (sy.n0 < sy_given.n0) ? sy.n0 : sy_given.n0;
        int32_t c_end_a = sy.n0 + (int32_t)sy.len;
        int32_t c_end_b = sy_given.n0 + (int32_t)sy_given.len;
        int32_t c_end = (c_end_a > c_end_b) ? c_end_a : c_end_b;
        size_t c_len = (size_t)(c_end - c_min);

        double *n_ax = (double *)rata_arena_alloc(ctx->frame_arena, c_len * sizeof(double), 8u);
        double *y_comp = (double *)rata_arena_calloc(ctx->frame_arena, c_len, sizeof(double), 8u);
        double *y_giv = (double *)rata_arena_calloc(ctx->frame_arena, c_len, sizeof(double), 8u);
        double max_diff = 0.0;
        for (size_t i = 0u; i < c_len; ++i) {
            int32_t n = c_min + (int32_t)i;
            n_ax[i] = (double)n;
            if (n >= sy.n0 && n < sy.n0 + (int32_t)sy.len) {
                y_comp[i] = sy.samples[(size_t)(n - sy.n0)];
            }
            if (n >= sy_given.n0 && n < sy_given.n0 + (int32_t)sy_given.len) {
                y_giv[i] = sy_given.samples[(size_t)(n - sy_given.n0)];
            }
            double df = fabs(y_giv[i] - y_comp[i]);
            if (df > max_diff) {
                max_diff = df;
            }
        }
        PlotConfig bot_cfg = {
            "Verification: Computed (x * h)[n] vs Given y[n]",
            "Sample Index n (samples)",
            "Amp",
            true, true, true, false
        };
        PlotTrace vtraces[2] = {
            {n_ax, y_comp, c_len, "(x*h)[n]", t->traces[0], PLOT_STEM, 2.2f},
            {n_ax, y_giv, c_len, "given y[n]", t->traces[1], PLOT_SCATTER, 2.2f}
        };
        ui_plot_draw(bot_r, &bot_cfg, &st->view_conv_bot, vtraces, 2u, NULL, 0u, t);

        if (max_diff <= 1.0e-5) {
            banner_lvl = BANNER_OK;
            snprintf(banner_msg, sizeof(banner_msg),
                     "Verified: given y[n] matches (x * h)[n] across all samples (max |diff| = %.3g).",
                     max_diff);
        } else {
            banner_lvl = BANNER_WARN;
            snprintf(banner_msg, sizeof(banner_msg),
                     "Mismatch: given y[n] differs from (x * h)[n] (max |y - x*h| = %.5g).",
                     max_diff);
        }
    } else {
        double *r_axis = (double *)rata_arena_alloc(ctx->frame_arena,
                                                    res_sig->len * sizeof(double), 8u);
        for (size_t i = 0u; i < res_sig->len; ++i) {
            r_axis[i] = (double)(res_sig->n0 + (int32_t)i);
        }
        const char *bot_title = "Result: Convolution Output y[n] = (x * h)[n]";
        if (mode == CONV_SOLVE_H) {
            bot_title = "Result: Solved Impulse Response h[n]  (from x[n] and y[n])";
        } else if (mode == CONV_SOLVE_X) {
            bot_title = "Result: Solved Input Signal x[n]  (from h[n] and y[n])";
        }
        PlotConfig bot_cfg = {bot_title, "Sample Index n (samples)", res_name,
                              true, true, true, false};
        PlotTrace bot_trace = {r_axis, res_sig->samples, res_sig->len,
                               res_name, t->traces[0], PLOT_STEM, 2.2f};
        PlotMarker cur_mk = {PLOT_MARKER_VLINE, (double)st->conv_n_shift, 0.0,
                             "shift n", t->traces[1]};
        size_t nmk = (mode == CONV_SOLVE_Y) ? 1u : 0u;
        ui_plot_draw(bot_r, &bot_cfg, &st->view_conv_bot, &bot_trace, 1u,
                     (nmk > 0u) ? &cur_mk : NULL, nmk, t);

        if (mode == CONV_SOLVE_Y) {
            banner_lvl = BANNER_INFO;
            snprintf(banner_msg, sizeof(banner_msg),
                     "y[n] = (x * h)[n]: n0 = %d, length = %zu. Hover any column above/below stems to probe exact sample.",
                     (int)sy.n0, sy.len);
        } else if (solve_st == RATA_ERR_UNSTABLE) {
            banner_lvl = BANNER_ERROR;
            snprintf(banner_msg, sizeof(banner_msg),
                     "Unstable inverse filter: %s diverged (> 1e15) at sample %zu; truncated safely.",
                     res_name, dinfo.diverged_at);
        } else if (dinfo.exact_finite) {
            banner_lvl = BANNER_OK;
            snprintf(banner_msg, sizeof(banner_msg),
                     "Exact finite deconvolution: solved %s (n0 = %d, L = %zu), max |y - x*h| = %.3g.",
                     res_name, (int)res_sig->n0, res_sig->len, dinfo.max_residual);
        } else {
            banner_lvl = BANNER_WARN;
            snprintf(banner_msg, sizeof(banner_msg),
                     "Causal power-series deconvolution: non-terminating quotient truncated to %zu samples (tail residual = %.4g).",
                     res_sig->len, dinfo.max_residual);
        }
    }

    ui_draw_banner((Rectangle){right.x, right.y + right.height - banner_h, right.width, banner_h},
                   banner_msg, banner_lvl, t);

    rata_arena_restore(ctx->frame_arena, mark);
}

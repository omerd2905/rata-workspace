#include "ui/ui_ratawave.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engines/dsp_conv.h"
#include "ui/ui_formula.h"
#include "ui/ui_ratawave_freq.h"
#include "ui/ui_ratawave_sys.h"
#include "ui/ui_widgets.h"

void ui_ratawave_init(void *state_ptr) {
    RataWaveState *st = (RataWaveState *)state_ptr;
    if (st == NULL) {
        return;
    }
    memset(st, 0, sizeof(*st));
    st->active_tab = RW_TAB_SIGNAL;

    /* Signals defaults */
    st->sig_mode = SIG_SRC_EXPR;
    st->prim_kind = PRIM_SINUSOID;
    snprintf(st->expr_x, sizeof(st->expr_x),
             "sin(2*pi*50*n/fs) + 0.5*cos(2*pi*140*n/fs)");
    st->fs_hz = 1000.0;
    st->n0 = 0;
    st->n_len = 64;
    st->prim_amp = 1.0;
    st->prim_freq_hz = 60.0;
    st->prim_phase_rad = 0.0;
    st->prim_base = 0.85;
    st->prim_shift = 0;
    st->prim_width = 8;

    /* Convolution defaults: rectangular pulse * decaying exponential */
    snprintf(st->conv_expr_x, sizeof(st->conv_expr_x), "u(n) - u(n - 5)");
    snprintf(st->conv_expr_h, sizeof(st->conv_expr_h), "0.75^n * u(n)");
    st->conv_x_n0 = -2;
    st->conv_x_len = 10;
    st->conv_h_n0 = 0;
    st->conv_h_len = 8;
    st->conv_n_shift = 2;
    st->conv_playing = false;
    st->conv_speed = 2.0;

    /* FFT defaults */
    st->fft_window = WIN_HANN;
    st->fft_db = false;
    st->fft_log_x = false;
    st->fft_centered = false;
    st->fft_norm_omega = false;

    /* LTI defaults: 2nd-order resonant bandpass/lowpass with complex conjugate poles */
    snprintf(st->lti_b_str, sizeof(st->lti_b_str), "0.2, 0.2");
    snprintf(st->lti_a_str, sizeof(st->lti_a_str), "1.0, -1.1, 0.65");
    st->lti_right_mode = 0;
    st->lti_db = true;
    st->lti_log_x = false;
    st->lti_resp_len = 48;
    st->view_lti_pz.equal_aspect = true;

    /* Sampling & Aliasing defaults: classic aliasing example f0=800 Hz, fs=1000 Hz */
    st->samp_f0_hz = 800.0;
    st->samp_fs_hz = 1000.0;
    st->samp_phase_rad = 0.0;
    st->samp_duration_s = 0.015;
    st->samp_show_sinc = true;
    st->samp_show_alias = false;

    /* Filter design defaults: 4th-order Butterworth Low-Pass */
    st->flt_family = 1;
    st->flt_band = FILTER_LOWPASS;
    st->flt_window = WIN_HAMMING;
    st->flt_fir_taps = 31;
    st->flt_iir_order = 4;
    st->flt_fs_hz = 1000.0;
    st->flt_fc1_hz = 200.0;
    st->flt_fc2_hz = 340.0;
    st->flt_ripple_db = 1.0;
    st->flt_db = true;
    st->flt_log_x = false;
    st->flt_aux_view = 1; /* Pole-Zero */
}

RataStatus ui_ratawave_build_signal(const RataWaveState *st,
                                    RataSignal *out,
                                    RataExprError *err_out,
                                    RataArena *arena) {
    if (st == NULL || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t len = (st->n_len < 4) ? 4u : ((st->n_len > 2048) ? 2048u : (size_t)st->n_len);
    double fs = (st->fs_hz > 1.0) ? st->fs_hz : 1.0;

    if (st->sig_mode == SIG_SRC_EXPR) {
        return dsp_signal_from_expr(st->expr_x, st->n0, len, fs, out, err_out, arena);
    }

    switch (st->prim_kind) {
        case PRIM_IMPULSE:
            return dsp_signal_impulse(st->n0, len, fs, st->prim_amp, st->prim_shift, out, arena);
        case PRIM_STEP:
            return dsp_signal_step(st->n0, len, fs, st->prim_amp, st->prim_shift, out, arena);
        case PRIM_SINUSOID:
            return dsp_signal_sinusoid(st->n0, len, fs, st->prim_amp,
                                       st->prim_freq_hz, st->prim_phase_rad, out, arena);
        case PRIM_EXP:
            return dsp_signal_exp(st->n0, len, fs, st->prim_amp,
                                  st->prim_base, st->prim_shift, out, arena);
        case PRIM_RECT: {
            size_t w = (st->prim_width < 1) ? 1u : (size_t)st->prim_width;
            return dsp_signal_rect(st->n0, len, fs, st->prim_amp, st->prim_shift, w, out, arena);
        }
        case PRIM_PIECEWISE: {
            RataPiecewiseSeg segs[3] = {
                {0, 7, "n / 7"},
                {8, 15, "1"},
                {16, 23, "(23 - n) / 7"}
            };
            return dsp_signal_piecewise(st->n0, len, fs, segs, 3u, out, err_out, arena);
        }
        default:
            return RATA_ERR_INVALID_ARG;
    }
}

static void draw_signal_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx) {
    const RataTheme *t = ctx->theme;
    float ctrl_w = 290.0f;
    Rectangle left = {area.x, area.y, ctrl_w, area.height};
    Rectangle right = {area.x + ctrl_w + 8.0f, area.y, area.width - ctrl_w - 8.0f, area.height};

    ui_draw_panel(left, t);
    ui_draw_section_title((Rectangle){left.x, left.y, left.width, 28.0f},
                          "Signal Generator", t);

    float x = left.x + 10.0f;
    float w = left.width - 20.0f;
    float y = left.y + 36.0f;

    static const char *mode_labels[2] = {"Expression", "Primitives"};
    if (ui_segmented_tabs((Rectangle){x, y, w, 26.0f}, mode_labels, 2, &st->sig_mode, t)) {
        ui_plot_view_reset(&st->view_sig);
    }
    y += 34.0f;

    if (st->sig_mode == SIG_SRC_EXPR) {
        if (ui_text_field((Rectangle){x, y, w, 42.0f}, "x[n] expression (vars: n, t, fs):",
                          st->expr_x, (int)sizeof(st->expr_x), 101, ctx->focus, t)) {
            ui_plot_view_reset(&st->view_sig);
        }
        y += 50.0f;

        ui_draw_text(t, "Quick Presets:", x, y, 12.0f, t->text_muted);
        y += 16.0f;
        float bw = (w - 4.0f) * 0.5f;
        if (ui_button((Rectangle){x, y, bw, 24.0f}, "2-Tone Sin", false, t)) {
            snprintf(st->expr_x, sizeof(st->expr_x),
                     "sin(2*pi*50*n/fs) + 0.5*cos(2*pi*140*n/fs)");
            st->n0 = 0; st->n_len = 64;
            ui_plot_view_reset(&st->view_sig);
        }
        if (ui_button((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "Impulse d[n]", false, t)) {
            snprintf(st->expr_x, sizeof(st->expr_x), "delta(n) - 0.5*delta(n - 4)");
            st->n0 = -4; st->n_len = 24;
            ui_plot_view_reset(&st->view_sig);
        }
        y += 28.0f;
        if (ui_button((Rectangle){x, y, bw, 24.0f}, "Decaying Exp", false, t)) {
            snprintf(st->expr_x, sizeof(st->expr_x), "0.85^n * u(n)");
            st->n0 = -4; st->n_len = 32;
            ui_plot_view_reset(&st->view_sig);
        }
        if (ui_button((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "Piecewise", false, t)) {
            snprintf(st->expr_x, sizeof(st->expr_x), "if(n >= 0, if(n < 10, n/10, 0.8^(n-10)), 0)");
            st->n0 = -4; st->n_len = 32;
            ui_plot_view_reset(&st->view_sig);
        }
        y += 28.0f;
        if (ui_button((Rectangle){x, y, w, 24.0f}, "Normalized sinc(n/4)", false, t)) {
            snprintf(st->expr_x, sizeof(st->expr_x), "sinc(n / 4)");
            st->n0 = -16; st->n_len = 33;
            ui_plot_view_reset(&st->view_sig);
        }
        y += 32.0f;
    } else {
        static const char *prim_row1[3] = {"delta[n]", "u[n]", "Sinusoid"};
        static const char *prim_row2[3] = {"a^n u[n]", "Rect", "Piecewise"};
        int p_sel = (st->prim_kind < 3) ? st->prim_kind : -1;
        if (ui_segmented_tabs((Rectangle){x, y, w, 24.0f}, prim_row1, 3, &p_sel, t)) {
            st->prim_kind = p_sel;
            ui_plot_view_reset(&st->view_sig);
        }
        y += 28.0f;
        int p_sel2 = (st->prim_kind >= 3) ? (st->prim_kind - 3) : -1;
        if (ui_segmented_tabs((Rectangle){x, y, w, 24.0f}, prim_row2, 3, &p_sel2, t)) {
            st->prim_kind = p_sel2 + 3;
            ui_plot_view_reset(&st->view_sig);
        }
        y += 34.0f;

        if (st->prim_kind != PRIM_PIECEWISE) {
            if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Amplitude A", "",
                              &st->prim_amp, -5.0, 5.0, t)) {
                ui_plot_view_reset(&st->view_sig);
            }
            y += 38.0f;
        }
        if (st->prim_kind == PRIM_SINUSOID) {
            if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Frequency f0", "Hz",
                              &st->prim_freq_hz, 1.0, st->fs_hz * 0.49, t)) {
                ui_plot_view_reset(&st->view_sig);
            }
            y += 38.0f;
            if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Phase", "rad",
                              &st->prim_phase_rad, -RATA_PI, RATA_PI, t)) {
                ui_plot_view_reset(&st->view_sig);
            }
            y += 38.0f;
        } else if (st->prim_kind == PRIM_EXP) {
            if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Base a", "",
                              &st->prim_base, -1.2, 1.2, t)) {
                ui_plot_view_reset(&st->view_sig);
            }
            y += 38.0f;
        }
        if (st->prim_kind == PRIM_IMPULSE || st->prim_kind == PRIM_STEP ||
            st->prim_kind == PRIM_EXP || st->prim_kind == PRIM_RECT) {
            double sh = (double)st->prim_shift;
            if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Shift k", "samples",
                              &sh, -16.0, 32.0, t)) {
                st->prim_shift = (int32_t)round(sh);
                ui_plot_view_reset(&st->view_sig);
            }
            y += 38.0f;
        }
        if (st->prim_kind == PRIM_RECT) {
            double wd = (double)st->prim_width;
            if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Pulse Width", "samples",
                              &wd, 1.0, 32.0, t)) {
                st->prim_width = (int32_t)round(wd);
                ui_plot_view_reset(&st->view_sig);
            }
            y += 38.0f;
        }
    }

    /* Common sampling & range controls */
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Sampling Rate fs", "Hz",
                      &st->fs_hz, 50.0, 8000.0, t)) {
        st->fs_hz = floor(st->fs_hz + 0.5);
        ui_plot_view_reset(&st->view_sig);
    }
    y += 38.0f;

    double n0_d = (double)st->n0;
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Start Index n0", "samples",
                      &n0_d, -32.0, 32.0, t)) {
        st->n0 = (int32_t)round(n0_d);
        ui_plot_view_reset(&st->view_sig);
    }
    y += 38.0f;

    double nlen_d = (double)st->n_len;
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Length N", "samples",
                      &nlen_d, 8.0, 256.0, t)) {
        st->n_len = (int32_t)round(nlen_d);
        ui_plot_view_reset(&st->view_sig);
    }
    y += 42.0f;

    /* Evaluate signal into frame arena */
    size_t mark = rata_arena_mark(ctx->frame_arena);
    RataSignal sig;
    RataExprError err;
    memset(&err, 0, sizeof(err));
    RataStatus status = ui_ratawave_build_signal(st, &sig, &err, ctx->frame_arena);

    if (status == RATA_OK) {
        RataSignalStats stats;
        dsp_signal_stats(&sig, &stats);
        char line1[96], line2[96], line3[96];
        snprintf(line1, sizeof(line1), "Energy E = %.4g  Power = %.4g",
                 stats.energy, stats.avg_power);
        snprintf(line2, sizeof(line2), "Mean = %.4g  RMS = %.4g",
                 stats.mean, stats.rms);
        snprintf(line3, sizeof(line3), "Min = %.4g  Max = %.4g  Ts = %.3g ms",
                 stats.min_val, stats.max_val, 1000.0 / sig.fs);
        ui_draw_text(t, "Signal Metrics:", x, y, 13.0f, t->accent);
        ui_draw_text(t, line1, x, y + 18.0f, 12.0f, t->text_primary);
        ui_draw_text(t, line2, x, y + 34.0f, 12.0f, t->text_primary);
        ui_draw_text(t, line3, x, y + 50.0f, 12.0f, t->text_secondary);

        double *n_axis = (double *)rata_arena_alloc(ctx->frame_arena,
                                                    sig.len * sizeof(double), 8u);
        for (size_t i = 0u; i < sig.len; ++i) {
            n_axis[i] = (double)(sig.n0 + (int32_t)i);
        }

        /* Formula card at top of right side */
        const char *formula_markup = "E = \\sum_n |x[n]|^2,   P = frac{1}{N} \\sum_n |x[n]|^2";
        if (st->sig_mode == SIG_SRC_PRIM && st->prim_kind == PRIM_SINUSOID) {
            formula_markup = "x[n] = A cos(frac{2 \\pi f_0}{f_s} n + \\phi)";
        } else if (st->sig_mode == SIG_SRC_PRIM && st->prim_kind == PRIM_EXP) {
            formula_markup = "x[n] = A a^{n - k} u[n - k]";
        }
        ui_formula_card((Rectangle){right.x, right.y, right.width, 58.0f},
                        "Discrete-Time Signal Definition", formula_markup,
                        15.0f, t, ctx->frame_arena);

        Rectangle plot_r = {right.x, right.y + 66.0f, right.width, right.height - 96.0f};
        PlotConfig pcfg = {
            "Discrete-Time Signal x[n]",
            "Sample Index n (samples)",
            "x[n]",
            true, true, true, false
        };
        PlotTrace tr = {n_axis, sig.samples, sig.len, "x[n]", t->traces[0], PLOT_STEM, 2.0f};
        ui_plot_draw(plot_r, &pcfg, &st->view_sig, &tr, 1u, NULL, 0u, t);

        char assume[160];
        snprintf(assume, sizeof(assume),
                 "Assumptions: discrete index n, t = n / fs (fs = %.4g Hz); sinc(x) = sin(pi*x)/(pi*x). Scroll=zoom, drag=pan.",
                 sig.fs);
        ui_draw_banner((Rectangle){right.x, right.y + right.height - 24.0f, right.width, 24.0f},
                       assume, BANNER_INFO, t);
    } else {
        char msg[192];
        snprintf(msg, sizeof(msg), "Signal error (%s): %s",
                 rata_status_str(status), err.message);
        ui_draw_banner((Rectangle){right.x, right.y, right.width, 36.0f},
                       msg, BANNER_ERROR, t);
    }
    rata_arena_restore(ctx->frame_arena, mark);
}

static void draw_conv_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx) {
    const RataTheme *t = ctx->theme;
    float ctrl_w = 290.0f;
    Rectangle left = {area.x, area.y, ctrl_w, area.height};
    Rectangle right = {area.x + ctrl_w + 8.0f, area.y, area.width - ctrl_w - 8.0f, area.height};

    ui_draw_panel(left, t);
    ui_draw_section_title((Rectangle){left.x, left.y, left.width, 28.0f},
                          "Convolution Flip & Shift", t);

    float x = left.x + 10.0f;
    float w = left.width - 20.0f;
    float y = left.y + 36.0f;

    if (ui_text_field((Rectangle){x, y, w, 40.0f}, "Input x[n]:",
                      st->conv_expr_x, (int)sizeof(st->conv_expr_x), 201, ctx->focus, t)) {
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    y += 46.0f;
    if (ui_text_field((Rectangle){x, y, w, 40.0f}, "Impulse response h[n]:",
                      st->conv_expr_h, (int)sizeof(st->conv_expr_h), 202, ctx->focus, t)) {
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    y += 48.0f;

    float bw = (w - 4.0f) * 0.5f;
    if (ui_button((Rectangle){x, y, bw, 24.0f}, "Pulse * Pulse", false, t)) {
        snprintf(st->conv_expr_x, sizeof(st->conv_expr_x), "u(n) - u(n - 5)");
        snprintf(st->conv_expr_h, sizeof(st->conv_expr_h), "u(n) - u(n - 5)");
        st->conv_x_n0 = 0; st->conv_x_len = 6;
        st->conv_h_n0 = 0; st->conv_h_len = 6;
        st->conv_n_shift = 0;
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    if (ui_button((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "Pulse * Exp", false, t)) {
        snprintf(st->conv_expr_x, sizeof(st->conv_expr_x), "u(n) - u(n - 5)");
        snprintf(st->conv_expr_h, sizeof(st->conv_expr_h), "0.75^n * u(n)");
        st->conv_x_n0 = -2; st->conv_x_len = 10;
        st->conv_h_n0 = 0; st->conv_h_len = 8;
        st->conv_n_shift = 0;
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    y += 32.0f;

    double lx = (double)st->conv_x_len;
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "x[n] Length", "samples", &lx, 2.0, 24.0, t)) {
        st->conv_x_len = (int32_t)round(lx);
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    y += 38.0f;

    double lh = (double)st->conv_h_len;
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "h[n] Length", "samples", &lh, 2.0, 24.0, t)) {
        st->conv_h_len = (int32_t)round(lh);
        ui_plot_view_reset(&st->view_conv_top);
        ui_plot_view_reset(&st->view_conv_bot);
    }
    y += 42.0f;

    /* Compute x[n], h[n], and full convolution y[n]. */
    size_t mark = rata_arena_mark(ctx->frame_arena);
    RataSignal sx, sh, sy;
    RataExprError err_x, err_h;
    RataStatus stx = dsp_signal_from_expr(st->conv_expr_x, st->conv_x_n0,
                                          (size_t)st->conv_x_len, st->fs_hz,
                                          &sx, &err_x, ctx->frame_arena);
    RataStatus sth = dsp_signal_from_expr(st->conv_expr_h, st->conv_h_n0,
                                          (size_t)st->conv_h_len, st->fs_hz,
                                          &sh, &err_h, ctx->frame_arena);

    if (stx != RATA_OK || sth != RATA_OK) {
        char msg[192];
        snprintf(msg, sizeof(msg), "Convolution parse error: %s",
                 (stx != RATA_OK) ? err_x.message : err_h.message);
        ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f}, msg, BANNER_ERROR, t);
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    if (dsp_conv_linear(&sx, &sh, &sy, ctx->frame_arena) != RATA_OK) {
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    int32_t y_min_n = sy.n0;
    int32_t y_max_n = sy.n0 + (int32_t)sy.len - 1;
    if (st->conv_n_shift < y_min_n) { st->conv_n_shift = y_min_n; }
    if (st->conv_n_shift > y_max_n) { st->conv_n_shift = y_max_n; }

    /* Playback animation controls */
    ui_draw_text(t, "Flip-and-Shift Playback:", x, y, 13.0f, t->accent);
    y += 18.0f;
    float pbw = (w - 8.0f) / 3.0f;
    if (ui_button((Rectangle){x, y, pbw, 26.0f},
                  st->conv_playing ? "Pause" : "Play", st->conv_playing, t)) {
        st->conv_playing = !st->conv_playing;
    }
    if (ui_button((Rectangle){x + pbw + 4.0f, y, pbw, 26.0f}, "< Step", false, t)) {
        st->conv_playing = false;
        if (st->conv_n_shift > y_min_n) { st->conv_n_shift--; }
    }
    if (ui_button((Rectangle){x + 2.0f * (pbw + 4.0f), y, pbw, 26.0f}, "Step >", false, t)) {
        st->conv_playing = false;
        if (st->conv_n_shift < y_max_n) { st->conv_n_shift++; }
    }
    y += 34.0f;

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

    double ns_d = (double)st->conv_n_shift;
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Current Shift n", "samples",
                      &ns_d, (double)y_min_n, (double)y_max_n, t)) {
        st->conv_n_shift = (int32_t)round(ns_d);
        st->conv_playing = false;
    }
    y += 38.0f;

    (void)ui_slider_row((Rectangle){x, y, w, 34.0f}, "Playback Speed", "steps/s",
                        &st->conv_speed, 0.5, 8.0, t);
    y += 44.0f;

    RataConvStep step;
    if (dsp_conv_step(&sx, &sh, st->conv_n_shift, &step, ctx->frame_arena) != RATA_OK) {
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    char cur_val[96];
    snprintf(cur_val, sizeof(cur_val), "y[%d] = %.6g", st->conv_n_shift, step.sum);
    ui_draw_text(t, "Current Output Sample:", x, y, 12.0f, t->text_muted);
    ui_draw_text(t, cur_val, x, y + 17.0f, 19.0f, t->success);

    /* Right side: formula card + top flip/shift plot + bottom output y[n] plot */
    char fstr[128];
    snprintf(fstr, sizeof(fstr), "y[n] = \\sum_{k=-\\infty}^{\\infty} x[k] h[n - k]   (y[%d] = %.4g)",
             st->conv_n_shift, step.sum);
    ui_formula_card((Rectangle){right.x, right.y, right.width, 56.0f},
                    "Discrete Linear Convolution", fstr, 15.0f, t, ctx->frame_arena);

    float avail_h = right.height - 64.0f;
    float top_h = avail_h * 0.52f;
    float bot_h = avail_h - top_h - 8.0f;

    PlotConfig top_cfg = {
        "Step View: Stationary x[k], Flipped & Shifted h[n - k], Product x[k]*h[n - k]",
        "Summation Index k (samples)",
        "Amp",
        true, true, true, false
    };
    PlotTrace top_traces[3] = {
        {step.k_axis, step.x_aligned, step.k_len, "x[k]", t->traces[0], PLOT_STEM, 2.0f},
        {step.k_axis, step.h_shifted, step.k_len, "h[n - k]", t->traces[1], PLOT_STEM, 2.0f},
        {step.k_axis, step.product, step.k_len, "x[k]*h[n-k]", t->traces[2], PLOT_STEM, 2.6f}
    };
    ui_plot_draw((Rectangle){right.x, right.y + 64.0f, right.width, top_h},
                 &top_cfg, &st->view_conv_top, top_traces, 3u, NULL, 0u, t);

    double *y_axis = (double *)rata_arena_alloc(ctx->frame_arena, sy.len * sizeof(double), 8u);
    for (size_t i = 0u; i < sy.len; ++i) {
        y_axis[i] = (double)(sy.n0 + (int32_t)i);
    }
    PlotConfig bot_cfg = {
        "Convolution Output y[n] = (x * h)[n]",
        "Output Index n (samples)",
        "y[n]",
        true, true, true, false
    };
    PlotTrace bot_trace = {y_axis, sy.samples, sy.len, "y[n]", t->traces[4], PLOT_STEM, 2.2f};
    PlotMarker cur_mk = {PLOT_MARKER_VLINE, (double)st->conv_n_shift, 0.0, "current n", t->success};
    ui_plot_draw((Rectangle){right.x, right.y + 64.0f + top_h + 8.0f, right.width, bot_h},
                 &bot_cfg, &st->view_conv_bot, &bot_trace, 1u, &cur_mk, 1u, t);

    rata_arena_restore(ctx->frame_arena, mark);
}

void ui_ratawave_draw(void *state_ptr, Rectangle viewport, RataWorkspaceCtx *ctx) {
    RataWaveState *st = (RataWaveState *)state_ptr;
    if (st == NULL || ctx == NULL) {
        return;
    }

    static const char *tab_names[RW_TAB_COUNT] = {
        "1. Signals", "2. Convolution", "3. DFT / FFT",
        "4. Z-Plane & LTI", "5. Sampling", "6. Filter Design"
    };
    Rectangle tab_bar = {viewport.x, viewport.y, viewport.width, 30.0f};
    if (ui_segmented_tabs(tab_bar, tab_names, RW_TAB_COUNT, &st->active_tab, ctx->theme)) {
        ctx->focus->active_id = 0;
    }

    Rectangle content = {viewport.x, viewport.y + 38.0f, viewport.width, viewport.height - 38.0f};
    switch (st->active_tab) {
        case RW_TAB_SIGNAL:   draw_signal_tab(st, content, ctx); break;
        case RW_TAB_CONV:     draw_conv_tab(st, content, ctx); break;
        case RW_TAB_FFT:      ui_ratawave_draw_fft_tab(st, content, ctx); break;
        case RW_TAB_LTI:      ui_ratawave_draw_lti_tab(st, content, ctx); break;
        case RW_TAB_SAMPLING: ui_ratawave_draw_sampling_tab(st, content, ctx); break;
        case RW_TAB_FILTER:   ui_ratawave_draw_filter_tab(st, content, ctx); break;
        default: break;
    }
}

RataStatus ui_ratawave_save(const void *state_ptr, RataProjectDoc *doc) {
    const RataWaveState *st = (const RataWaveState *)state_ptr;
    if (st == NULL || doc == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    char num[64];
    snprintf(num, sizeof(num), "%d", st->active_tab);
    project_doc_set(doc, "ratawave", "active_tab", num);
    project_doc_set(doc, "ratawave", "expr_x", st->expr_x);
    snprintf(num, sizeof(num), "%.10g", st->fs_hz);
    project_doc_set(doc, "ratawave", "fs_hz", num);
    snprintf(num, sizeof(num), "%d", (int)st->n0);
    project_doc_set(doc, "ratawave", "n0", num);
    snprintf(num, sizeof(num), "%d", (int)st->n_len);
    project_doc_set(doc, "ratawave", "n_len", num);
    project_doc_set(doc, "ratawave", "conv_expr_x", st->conv_expr_x);
    project_doc_set(doc, "ratawave", "conv_expr_h", st->conv_expr_h);
    project_doc_set(doc, "ratawave", "lti_b", st->lti_b_str);
    project_doc_set(doc, "ratawave", "lti_a", st->lti_a_str);
    snprintf(num, sizeof(num), "%.10g", st->samp_f0_hz);
    project_doc_set(doc, "ratawave", "samp_f0_hz", num);
    snprintf(num, sizeof(num), "%.10g", st->samp_fs_hz);
    project_doc_set(doc, "ratawave", "samp_fs_hz", num);
    return RATA_OK;
}

RataStatus ui_ratawave_load(void *state_ptr, const RataProjectDoc *doc) {
    RataWaveState *st = (RataWaveState *)state_ptr;
    if (st == NULL || doc == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    int32_t iv = 0;
    double dv = 0.0;
    if (project_doc_get_i32(doc, "ratawave", "active_tab", &iv) &&
        iv >= 0 && iv < RW_TAB_COUNT) {
        st->active_tab = iv;
    }
    const char *s = project_doc_get(doc, "ratawave", "expr_x");
    if (s != NULL) { snprintf(st->expr_x, sizeof(st->expr_x), "%s", s); }
    if (project_doc_get_double(doc, "ratawave", "fs_hz", &dv) && dv > 0.0) { st->fs_hz = dv; }
    if (project_doc_get_i32(doc, "ratawave", "n0", &iv)) { st->n0 = iv; }
    if (project_doc_get_i32(doc, "ratawave", "n_len", &iv) && iv >= 4 && iv <= 2048) {
        st->n_len = iv;
    }
    s = project_doc_get(doc, "ratawave", "conv_expr_x");
    if (s != NULL) { snprintf(st->conv_expr_x, sizeof(st->conv_expr_x), "%s", s); }
    s = project_doc_get(doc, "ratawave", "conv_expr_h");
    if (s != NULL) { snprintf(st->conv_expr_h, sizeof(st->conv_expr_h), "%s", s); }
    s = project_doc_get(doc, "ratawave", "lti_b");
    if (s != NULL) { snprintf(st->lti_b_str, sizeof(st->lti_b_str), "%s", s); }
    s = project_doc_get(doc, "ratawave", "lti_a");
    if (s != NULL) { snprintf(st->lti_a_str, sizeof(st->lti_a_str), "%s", s); }
    if (project_doc_get_double(doc, "ratawave", "samp_f0_hz", &dv) && dv >= 0.0) {
        st->samp_f0_hz = dv;
    }
    if (project_doc_get_double(doc, "ratawave", "samp_fs_hz", &dv) && dv > 0.0) {
        st->samp_fs_hz = dv;
    }

    ui_plot_view_reset(&st->view_sig);
    ui_plot_view_reset(&st->view_conv_top);
    ui_plot_view_reset(&st->view_conv_bot);
    ui_plot_view_reset(&st->view_fft_mag);
    ui_plot_view_reset(&st->view_fft_phase);
    ui_plot_view_reset(&st->view_lti_pz);
    ui_plot_view_reset(&st->view_lti_mag);
    ui_plot_view_reset(&st->view_lti_phase);
    ui_plot_view_reset(&st->view_lti_time);
    ui_plot_view_reset(&st->view_sampling);
    return RATA_OK;
}

#include "ui/ui_ratawave_sys.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "engines/dsp_filter.h"
#include "engines/dsp_lti.h"
#include "ui/ui_formula.h"
#include "ui/ui_widgets.h"

static void format_coeff_csv(const double *c, size_t n, char *buf, size_t cap) {
    size_t pos = 0u;
    buf[0] = '\0';
    for (size_t i = 0u; i < n; ++i) {
        int written = snprintf(buf + pos, cap - pos, (i == 0u) ? "%.5g" : ", %.5g", c[i]);
        if (written < 0 || pos + (size_t)written >= cap - 1u) {
            break;
        }
        pos += (size_t)written;
    }
}

static size_t build_pz_markers(const RataPoleZero *pz,
                               PlotMarker *markers,
                               size_t cap,
                               const RataTheme *t) {
    size_t count = 0u;
    for (size_t i = 0u; i < pz->num_zeros && count < cap; ++i) {
        markers[count++] = (PlotMarker){
            PLOT_MARKER_ZERO, pz->zeros[i].re, pz->zeros[i].im, NULL, t->traces[0]
        };
    }
    for (size_t i = 0u; i < pz->num_poles && count < cap; ++i) {
        markers[count++] = (PlotMarker){
            PLOT_MARKER_POLE, pz->poles[i].re, pz->poles[i].im, NULL, t->danger
        };
    }
    return count;
}

void ui_ratawave_draw_lti_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx) {
    const RataTheme *t = ctx->theme;
    float ctrl_w = 290.0f;
    Rectangle left = {area.x, area.y, ctrl_w, area.height};
    Rectangle right = {area.x + ctrl_w + 8.0f, area.y, area.width - ctrl_w - 8.0f, area.height};

    ui_draw_panel(left, t);
    ui_draw_section_title((Rectangle){left.x, left.y, left.width, 28.0f},
                          "Z-Transform & LTI System", t);

    float x = left.x + 10.0f;
    float w = left.width - 20.0f;
    float y = left.y + 36.0f;

    if (ui_text_field((Rectangle){x, y, w, 40.0f}, "Numerator b (b0, b1, ... in z^-1):",
                      st->lti_b_str, (int)sizeof(st->lti_b_str), 401, ctx->focus, t)) {
        ui_plot_view_reset(&st->view_lti_pz);
        ui_plot_view_reset(&st->view_lti_mag);
        ui_plot_view_reset(&st->view_lti_phase);
        ui_plot_view_reset(&st->view_lti_time);
    }
    y += 46.0f;
    if (ui_text_field((Rectangle){x, y, w, 40.0f}, "Denominator a (a0, a1, ... in z^-1):",
                      st->lti_a_str, (int)sizeof(st->lti_a_str), 402, ctx->focus, t)) {
        ui_plot_view_reset(&st->view_lti_pz);
        ui_plot_view_reset(&st->view_lti_mag);
        ui_plot_view_reset(&st->view_lti_phase);
        ui_plot_view_reset(&st->view_lti_time);
    }
    y += 48.0f;

    ui_draw_text(t, "Classic LTI Presets:", x, y, 12.0f, t->text_muted);
    y += 16.0f;
    float bw = (w - 4.0f) * 0.5f;
    if (ui_button((Rectangle){x, y, bw, 24.0f}, "1st-Ord LP", false, t)) {
        snprintf(st->lti_b_str, sizeof(st->lti_b_str), "0.25");
        snprintf(st->lti_a_str, sizeof(st->lti_a_str), "1, -0.75");
        ui_plot_view_reset(&st->view_lti_pz); ui_plot_view_reset(&st->view_lti_mag);
        ui_plot_view_reset(&st->view_lti_time);
    }
    if (ui_button((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "Resonant", false, t)) {
        snprintf(st->lti_b_str, sizeof(st->lti_b_str), "0.2, 0.2");
        snprintf(st->lti_a_str, sizeof(st->lti_a_str), "1.0, -1.1, 0.65");
        ui_plot_view_reset(&st->view_lti_pz); ui_plot_view_reset(&st->view_lti_mag);
        ui_plot_view_reset(&st->view_lti_time);
    }
    y += 28.0f;
    if (ui_button((Rectangle){x, y, bw, 24.0f}, "4-pt MA FIR", false, t)) {
        snprintf(st->lti_b_str, sizeof(st->lti_b_str), "0.25, 0.25, 0.25, 0.25");
        snprintf(st->lti_a_str, sizeof(st->lti_a_str), "1");
        ui_plot_view_reset(&st->view_lti_pz); ui_plot_view_reset(&st->view_lti_mag);
        ui_plot_view_reset(&st->view_lti_time);
    }
    if (ui_button((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "Unstable |p|>1", false, t)) {
        snprintf(st->lti_b_str, sizeof(st->lti_b_str), "1");
        snprintf(st->lti_a_str, sizeof(st->lti_a_str), "1, -1.2");
        ui_plot_view_reset(&st->view_lti_pz); ui_plot_view_reset(&st->view_lti_mag);
        ui_plot_view_reset(&st->view_lti_time);
    }
    y += 28.0f;
    if (ui_button((Rectangle){x, y, w, 24.0f}, "Oscillator (Unit-Circle Poles |p|=1)", false, t)) {
        snprintf(st->lti_b_str, sizeof(st->lti_b_str), "1");
        snprintf(st->lti_a_str, sizeof(st->lti_a_str), "1, 0, 1");
        ui_plot_view_reset(&st->view_lti_pz); ui_plot_view_reset(&st->view_lti_mag);
        ui_plot_view_reset(&st->view_lti_time);
    }
    y += 30.0f;

    static const char *rmodes[4] = {"Freqz", "h[n]", "s[n]", "y[n]"};
    if (ui_segmented_tabs((Rectangle){x, y, w, 24.0f}, rmodes, 4, &st->lti_right_mode, t)) {
        ui_plot_view_reset(&st->view_lti_time);
    }
    y += 30.0f;

    if (st->lti_right_mode == 0) {
        if (ui_toggle_chip((Rectangle){x, y, bw, 24.0f}, "Mag dB", &st->lti_db, t)) {
            ui_plot_view_reset(&st->view_lti_mag);
        }
        if (ui_toggle_chip((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "Log Freq", &st->lti_log_x, t)) {
            st->view_lti_mag.log_x = st->lti_log_x;
            st->view_lti_phase.log_x = st->lti_log_x;
            ui_plot_view_reset(&st->view_lti_mag);
            ui_plot_view_reset(&st->view_lti_phase);
        }
        y += 30.0f;
    } else if (st->lti_right_mode == 1 || st->lti_right_mode == 2) {
        double rl = (double)st->lti_resp_len;
        if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Response Length", "samples",
                          &rl, 16.0, 200.0, t)) {
            st->lti_resp_len = (int32_t)round(rl);
            ui_plot_view_reset(&st->view_lti_time);
        }
        y += 38.0f;
    }

    /* Parse coefficients and run pole-zero analysis */
    double b[RATA_LTI_MAX_COEFFS], a[RATA_LTI_MAX_COEFFS];
    size_t nb = 0u, na = 0u;
    RataStatus sb = dsp_parse_coeff_list(st->lti_b_str, b, RATA_LTI_MAX_COEFFS, &nb);
    RataStatus sa = dsp_parse_coeff_list(st->lti_a_str, a, RATA_LTI_MAX_COEFFS, &na);
    RataLti sys;
    RataStatus si = (sb == RATA_OK && sa == RATA_OK)
                        ? dsp_lti_init(&sys, b, nb, a, na, st->fs_hz)
                        : RATA_ERR_PARSE;

    if (si != RATA_OK) {
        char msg[160];
        snprintf(msg, sizeof(msg), "Invalid transfer function coefficients (%s). Check a0 != 0.",
                 rata_status_str(si));
        ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f}, msg, BANNER_ERROR, t);
        return;
    }

    size_t mark = rata_arena_mark(ctx->frame_arena);
    RataPoleZero pz;
    RataStatus spz = dsp_lti_poles_zeros(&sys, &pz, ctx->frame_arena);
    if (spz != RATA_OK) {
        ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f},
                       "Pole-zero root solver did not converge.", BANNER_ERROR, t);
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    /* Left-side Pole/Zero listing */
    char s1[96], s2[96];
    snprintf(s1, sizeof(s1), "Poles: %zu  Zeros: %zu  max|p| = %.4f",
             pz.num_poles, pz.num_zeros, pz.max_pole_mag);
    if (pz.dc_gain_finite) {
        snprintf(s2, sizeof(s2), "DC Gain H(1) = %.4g", pz.dc_gain);
    } else {
        snprintf(s2, sizeof(s2), "DC Gain H(1) = inf (pole at z=1)");
    }
    ui_draw_text(t, "Pole-Zero Summary:", x, y, 13.0f, t->accent);
    ui_draw_text(t, s1, x, y + 18.0f, 12.0f, t->text_primary);
    ui_draw_text(t, s2, x, y + 34.0f, 12.0f, t->text_secondary);

    float list_y = y + 54.0f;
    size_t max_show = (pz.num_poles < 6u) ? pz.num_poles : 6u;
    for (size_t i = 0u; i < max_show && list_y < left.y + left.height - 16.0f; ++i) {
        char pstr[80];
        snprintf(pstr, sizeof(pstr), "p%zu = %.3f %+.3fj (|p|=%.3f)",
                 i + 1u, pz.poles[i].re, pz.poles[i].im, cplx_abs(pz.poles[i]));
        ui_draw_text(t, pstr, x, list_y, 12.0f,
                     (cplx_abs(pz.poles[i]) >= 1.0 - 1.0e-6) ? t->danger : t->text_secondary);
        list_y += 16.0f;
    }

    /* Right side top: formula + stability banner */
    ui_formula_card((Rectangle){right.x, right.y, right.width, 54.0f},
                    "Discrete-Time Transfer Function",
                    "H(z) = frac{b_0 + b_1 z^{-1} + ... + b_M z^{-M}}{a_0 + a_1 z^{-1} + ... + a_N z^{-N}}",
                    14.0f, t, ctx->frame_arena);

    RataBannerLevel slev = (pz.stability == LTI_STABLE)
                               ? BANNER_OK
                               : ((pz.stability == LTI_MARGINAL) ? BANNER_WARN : BANNER_ERROR);
    ui_draw_banner((Rectangle){right.x, right.y + 60.0f, right.width, 26.0f},
                   dsp_stability_str(pz.stability), slev, t);

    float body_y = right.y + 92.0f;
    float body_h = right.height - 92.0f;
    float pz_w = right.width * 0.42f;
    float resp_w = right.width - pz_w - 8.0f;

    /* Z-plane pole-zero plot */
    PlotMarker pz_markers[260];
    size_t nmk = build_pz_markers(&pz, pz_markers, 260u, t);
    st->view_lti_pz.equal_aspect = true;
    PlotConfig pz_cfg = {
        "Z-Plane Pole-Zero Map (X=Pole, O=Zero)",
        "Re(z)",
        "Im(z)",
        true, false, true, true
    };
    ui_plot_draw((Rectangle){right.x, body_y, pz_w, body_h},
                 &pz_cfg, &st->view_lti_pz, NULL, 0u, pz_markers, nmk, t);

    Rectangle resp_r = {right.x + pz_w + 8.0f, body_y, resp_w, body_h};
    if (st->lti_right_mode == 0) {
        RataFreqz fz;
        if (dsp_lti_freqz(&sys, 513u, false, -100.0, &fz, ctx->frame_arena) == RATA_OK) {
            float clamp_h = fz.has_pole_on_grid ? 28.0f : 0.0f;
            if (fz.has_pole_on_grid) {
                ui_draw_banner((Rectangle){resp_r.x, resp_r.y, resp_r.width, 24.0f},
                               "Clamped at +120 dB (1e6): frequency grid hit/approached a unit-circle pole (true peak -> inf).",
                               BANNER_WARN, t);
            }
            float avail_h = resp_r.height - clamp_h;
            float mh = avail_h * 0.54f;
            float ph = avail_h - mh - 8.0f;
            st->view_lti_mag.log_x = st->lti_log_x;
            st->view_lti_phase.log_x = st->lti_log_x;
            PlotConfig mcfg = {
                st->lti_db ? "Frequency Response |H(e^{jw})| (dB)" : "Magnitude |H(e^{jw})|",
                "Frequency (Hz)",
                st->lti_db ? "dB" : "|H|",
                true, true, true, false
            };
            PlotTrace mtr = {fz.freq_hz, st->lti_db ? fz.mag_db : fz.mag, fz.len,
                             "|H(f)|", t->traces[0], PLOT_LINE, 2.0f};
            ui_plot_draw((Rectangle){resp_r.x, resp_r.y + clamp_h, resp_r.width, mh},
                         &mcfg, &st->view_lti_mag, &mtr, 1u, NULL, 0u, t);

            PlotConfig pcfg = {
                "Phase Response (rad)",
                "Frequency (Hz)",
                "rad",
                true, true, true, false
            };
            PlotTrace ptr = {fz.freq_hz, fz.phase_rad, fz.len,
                             "angle(H)", t->traces[1], PLOT_LINE, 1.8f};
            ui_plot_draw((Rectangle){resp_r.x, resp_r.y + clamp_h + mh + 8.0f, resp_r.width, ph},
                         &pcfg, &st->view_lti_phase, &ptr, 1u, NULL, 0u, t);
        }
    } else {
        RataSignal ysig;
        size_t div_at = 0u;
        RataStatus fst = RATA_OK;
        const char *title = "Impulse Response h[n]";
        if (st->lti_right_mode == 1) {
            fst = dsp_lti_impulse_response(&sys, (size_t)st->lti_resp_len,
                                           &ysig, &div_at, ctx->frame_arena);
        } else if (st->lti_right_mode == 2) {
            title = "Step Response s[n]";
            fst = dsp_lti_step_response(&sys, (size_t)st->lti_resp_len,
                                        &ysig, &div_at, ctx->frame_arena);
        } else {
            title = "Filtered Output y[n] = H(z){x[n]}";
            RataSignal xsig;
            RataExprError xerr;
            fst = ui_ratawave_build_signal(st, &xsig, &xerr, ctx->frame_arena);
            if (fst == RATA_OK) {
                fst = dsp_lti_filter(&sys, &xsig, &ysig, &div_at, ctx->frame_arena);
            }
        }
        if (fst == RATA_OK || fst == RATA_ERR_UNSTABLE) {
            float warn_h = (fst == RATA_ERR_UNSTABLE) ? 28.0f : 0.0f;
            if (fst == RATA_ERR_UNSTABLE) {
                char wmsg[128];
                snprintf(wmsg, sizeof(wmsg),
                         "Unstable output diverged (> 1e15) at sample %zu; truncated safely.",
                         div_at);
                ui_draw_banner((Rectangle){resp_r.x, resp_r.y, resp_r.width, 24.0f},
                               wmsg, BANNER_ERROR, t);
            }
            double *nax = (double *)rata_arena_alloc(ctx->frame_arena,
                                                     ysig.len * sizeof(double), 8u);
            for (size_t i = 0u; i < ysig.len; ++i) {
                nax[i] = (double)(ysig.n0 + (int32_t)i);
            }
            PlotConfig tcfg = {title, "Sample Index n (samples)", "Amp",
                               true, true, true, false};
            PlotTrace ttr = {nax, ysig.samples, ysig.len, "output", t->traces[2], PLOT_STEM, 2.0f};
            ui_plot_draw((Rectangle){resp_r.x, resp_r.y + warn_h, resp_r.width, resp_r.height - warn_h},
                         &tcfg, &st->view_lti_time, &ttr, 1u, NULL, 0u, t);
        }
    }

    rata_arena_restore(ctx->frame_arena, mark);
}

void ui_ratawave_draw_filter_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx) {
    const RataTheme *t = ctx->theme;
    float ctrl_w = 290.0f;
    Rectangle left = {area.x, area.y, ctrl_w, area.height};
    Rectangle right = {area.x + ctrl_w + 8.0f, area.y, area.width - ctrl_w - 8.0f, area.height};

    ui_draw_panel(left, t);
    ui_draw_section_title((Rectangle){left.x, left.y, left.width, 28.0f},
                          "FIR / IIR Filter Designer", t);

    float x = left.x + 10.0f;
    float w = left.width - 20.0f;
    float y = left.y + 36.0f;

    static const char *fams[3] = {"FIR Sinc", "Butterworth", "Chebyshev I"};
    if (ui_segmented_tabs((Rectangle){x, y, w, 26.0f}, fams, 3, &st->flt_family, t)) {
        ui_plot_view_reset(&st->view_flt_mag);
        ui_plot_view_reset(&st->view_flt_aux);
    }
    y += 34.0f;

    static const char *bands[4] = {"LP", "HP", "BP", "BS"};
    if (ui_segmented_tabs((Rectangle){x, y, w, 24.0f}, bands, 4, &st->flt_band, t)) {
        ui_plot_view_reset(&st->view_flt_mag);
        ui_plot_view_reset(&st->view_flt_aux);
    }
    y += 34.0f;

    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Sampling Rate fs", "Hz",
                      &st->flt_fs_hz, 100.0, 8000.0, t)) {
        st->flt_fs_hz = floor(st->flt_fs_hz + 0.5);
        if (st->flt_fc1_hz >= st->flt_fs_hz * 0.45) { st->flt_fc1_hz = st->flt_fs_hz * 0.2; }
        if (st->flt_fc2_hz >= st->flt_fs_hz * 0.48) { st->flt_fc2_hz = st->flt_fs_hz * 0.35; }
        ui_plot_view_reset(&st->view_flt_mag);
    }
    y += 38.0f;

    double nyq = 0.5 * st->flt_fs_hz;
    bool is_bp_bs = (st->flt_band == FILTER_BANDPASS || st->flt_band == FILTER_BANDSTOP);
    double max_fc1 = is_bp_bs ? (nyq * 0.85) : (nyq * 0.96);
    if (st->flt_fc1_hz >= max_fc1) { st->flt_fc1_hz = max_fc1 * 0.5; }

    (void)ui_slider_row((Rectangle){x, y, w, 34.0f},
                        is_bp_bs ? "Lower Cutoff fc1" : "Cutoff fc", "Hz",
                        &st->flt_fc1_hz, nyq * 0.03, max_fc1, t);
    st->flt_fc1_hz = floor(st->flt_fc1_hz + 0.5);
    y += 38.0f;

    if (is_bp_bs) {
        double min_fc2 = st->flt_fc1_hz + nyq * 0.04;
        if (st->flt_fc2_hz < min_fc2) { st->flt_fc2_hz = min_fc2; }
        if (st->flt_fc2_hz > nyq * 0.96) { st->flt_fc2_hz = nyq * 0.96; }
        (void)ui_slider_row((Rectangle){x, y, w, 34.0f}, "Upper Cutoff fc2", "Hz",
                            &st->flt_fc2_hz, min_fc2, nyq * 0.96, t);
        st->flt_fc2_hz = floor(st->flt_fc2_hz + 0.5);
        y += 38.0f;
    }

    if (st->flt_family == 0) {
        double td = (double)st->flt_fir_taps;
        if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "FIR Taps (odd)", "taps",
                          &td, 5.0, 65.0, t)) {
            int32_t ti = (int32_t)round(td);
            if ((ti % 2) == 0) { ti++; }
            st->flt_fir_taps = ti;
            ui_plot_view_reset(&st->view_flt_aux);
        }
        y += 38.0f;
        static const char *wlabels[4] = {"Rect", "Hann", "Hamm", "Blkmn"};
        (void)ui_segmented_tabs((Rectangle){x, y, w, 24.0f}, wlabels, 4, &st->flt_window, t);
        y += 32.0f;
    } else {
        int32_t max_ord = is_bp_bs ? 4 : 8;
        if (st->flt_iir_order > max_ord) { st->flt_iir_order = max_ord; }
        double od = (double)st->flt_iir_order;
        if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Prototype Order N", "",
                          &od, 1.0, (double)max_ord, t)) {
            st->flt_iir_order = (int32_t)round(od);
            ui_plot_view_reset(&st->view_flt_aux);
        }
        y += 38.0f;
        if (st->flt_family == 2) {
            (void)ui_slider_row((Rectangle){x, y, w, 34.0f}, "Passband Ripple Rp", "dB",
                                &st->flt_ripple_db, 0.1, 6.0, t);
            y += 38.0f;
        }
    }

    float bw = (w - 4.0f) * 0.5f;
    if (ui_toggle_chip((Rectangle){x, y, bw, 24.0f}, "Mag dB", &st->flt_db, t)) {
        ui_plot_view_reset(&st->view_flt_mag);
    }
    if (ui_toggle_chip((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "Log Freq", &st->flt_log_x, t)) {
        st->view_flt_mag.log_x = st->flt_log_x;
        ui_plot_view_reset(&st->view_flt_mag);
    }
    y += 32.0f;

    static const char *aux_tabs[3] = {"Phase", "Pole-Zero", "Impulse h[n]"};
    if (ui_segmented_tabs((Rectangle){x, y, w, 24.0f}, aux_tabs, 3, &st->flt_aux_view, t)) {
        ui_plot_view_reset(&st->view_flt_aux);
    }
    y += 34.0f;

    /* Run filter design */
    size_t mark = rata_arena_mark(ctx->frame_arena);
    RataLti sys;
    RataStatus dst;
    if (st->flt_family == 0) {
        RataFirSpec spec = {
            (RataFilterBand)st->flt_band, (size_t)st->flt_fir_taps,
            st->flt_fs_hz, st->flt_fc1_hz, st->flt_fc2_hz, (RataWindowKind)st->flt_window
        };
        dst = dsp_design_fir(&spec, &sys, ctx->frame_arena);
    } else {
        RataIirSpec spec = {
            (st->flt_family == 1) ? IIR_BUTTERWORTH : IIR_CHEBYSHEV1,
            (RataFilterBand)st->flt_band, (size_t)st->flt_iir_order,
            st->flt_fs_hz, st->flt_fc1_hz, st->flt_fc2_hz, st->flt_ripple_db
        };
        dst = dsp_design_iir(&spec, &sys, ctx->frame_arena);
    }

    if (dst != RATA_OK) {
        char msg[160];
        snprintf(msg, sizeof(msg), "Filter design error: %s", rata_status_str(dst));
        ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f}, msg, BANNER_ERROR, t);
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    if (ui_button((Rectangle){x, y, w, 28.0f}, "Send Designed Filter -> LTI Tab", true, t)) {
        format_coeff_csv(sys.b, sys.nb, st->lti_b_str, sizeof(st->lti_b_str));
        format_coeff_csv(sys.a, sys.na, st->lti_a_str, sizeof(st->lti_a_str));
        st->fs_hz = st->flt_fs_hz;
        st->active_tab = RW_TAB_LTI;
        ui_plot_view_reset(&st->view_lti_pz);
        ui_plot_view_reset(&st->view_lti_mag);
        ui_plot_view_reset(&st->view_lti_time);
    }
    y += 38.0f;

    char summary[96];
    snprintf(summary, sizeof(summary), "Coeffs: nb=%zu, na=%zu (fs=%.4g Hz)",
             sys.nb, sys.na, sys.fs);
    ui_draw_text(t, summary, x, y, 12.0f, t->text_secondary);

    /* Right side: formula + magnitude plot + aux plot */
    const char *f_markup =
        (st->flt_family == 0)
            ? "h[n] = h_d[n] \\cdot w[n],   \\omega_c = frac{2 \\pi f_c}{f_s}"
            : "s = 2 f_s frac{1 - z^{-1}}{1 + z^{-1}},   \\Omega_c = 2 f_s tan(frac{\\pi f_c}{f_s})";
    ui_formula_card((Rectangle){right.x, right.y, right.width, 54.0f},
                    (st->flt_family == 0) ? "Windowed-Sinc Type-I Linear-Phase FIR"
                                          : "Pre-Warped Bilinear Transform IIR",
                    f_markup, 14.0f, t, ctx->frame_arena);

    RataFreqz fz;
    if (dsp_lti_freqz(&sys, 513u, false, -100.0, &fz, ctx->frame_arena) == RATA_OK) {
        float clamp_h = fz.has_pole_on_grid ? 28.0f : 0.0f;
        if (fz.has_pole_on_grid) {
            ui_draw_banner((Rectangle){right.x, right.y + 62.0f, right.width, 24.0f},
                           "Clamped at +120 dB (1e6): frequency grid hit/approached a unit-circle pole.",
                           BANNER_WARN, t);
        }
        float avail_h = right.height - 62.0f - clamp_h;
        float top_h = avail_h * 0.52f;
        float bot_h = avail_h - top_h - 8.0f;

        st->view_flt_mag.log_x = st->flt_log_x;
        PlotMarker mk[3];
        size_t nmk = 0u;
        mk[nmk++] = (PlotMarker){PLOT_MARKER_VLINE, st->flt_fc1_hz, 0.0, "fc1", t->warning};
        if (is_bp_bs) {
            mk[nmk++] = (PlotMarker){PLOT_MARKER_VLINE, st->flt_fc2_hz, 0.0, "fc2", t->warning};
        }
        if (st->flt_db) {
            double ref_db = (st->flt_family == 2) ? -st->flt_ripple_db : -3.0103;
            mk[nmk++] = (PlotMarker){PLOT_MARKER_HLINE, 0.0, ref_db,
                                     (st->flt_family == 2) ? "-Rp dB" : "-3 dB", t->success};
        }

        PlotConfig mcfg = {
            st->flt_db ? "Filter Magnitude Response (dB)" : "Filter Magnitude Response (Linear)",
            "Frequency (Hz)",
            st->flt_db ? "dB" : "|H|",
            true, true, true, false
        };
        PlotTrace mtr = {fz.freq_hz, st->flt_db ? fz.mag_db : fz.mag, fz.len,
                         "|H(f)|", t->traces[0], PLOT_LINE, 2.2f};
        ui_plot_draw((Rectangle){right.x, right.y + 62.0f + clamp_h, right.width, top_h},
                     &mcfg, &st->view_flt_mag, &mtr, 1u, mk, nmk, t);

        Rectangle bot_r = {right.x, right.y + 62.0f + clamp_h + top_h + 8.0f, right.width, bot_h};
        if (st->flt_aux_view == 0) {
            st->view_flt_aux.equal_aspect = false;
            st->view_flt_aux.log_x = st->flt_log_x;
            PlotConfig pcfg = {"Phase Response (rad)", "Frequency (Hz)", "rad",
                               true, true, true, false};
            PlotTrace ptr = {fz.freq_hz, fz.phase_rad, fz.len, "angle(H)", t->traces[1],
                             PLOT_LINE, 1.8f};
            ui_plot_draw(bot_r, &pcfg, &st->view_flt_aux, &ptr, 1u, NULL, 0u, t);
        } else if (st->flt_aux_view == 1) {
            RataPoleZero pz;
            if (dsp_lti_poles_zeros(&sys, &pz, ctx->frame_arena) == RATA_OK) {
                PlotMarker pzm[260];
                size_t npzm = build_pz_markers(&pz, pzm, 260u, t);
                st->view_flt_aux.equal_aspect = true;
                st->view_flt_aux.log_x = false;
                PlotConfig pzcfg = {"Designed Filter Z-Plane (X=Pole, O=Zero)", "Re(z)", "Im(z)",
                                    true, false, true, true};
                ui_plot_draw(bot_r, &pzcfg, &st->view_flt_aux, NULL, 0u, pzm, npzm, t);
            }
        } else {
            RataSignal imp;
            size_t div_at = 0u;
            size_t ilen = (st->flt_family == 0) ? (sys.nb + 8u) : 64u;
            if (dsp_lti_impulse_response(&sys, ilen, &imp, &div_at, ctx->frame_arena) == RATA_OK) {
                double *nax = (double *)rata_arena_alloc(ctx->frame_arena,
                                                         imp.len * sizeof(double), 8u);
                for (size_t i = 0u; i < imp.len; ++i) {
                    nax[i] = (double)i;
                }
                st->view_flt_aux.equal_aspect = false;
                st->view_flt_aux.log_x = false;
                PlotConfig icfg = {"Filter Impulse Response h[n]", "Sample Index n", "h[n]",
                                   true, true, true, false};
                PlotTrace itr = {nax, imp.samples, imp.len, "h[n]", t->traces[2], PLOT_STEM, 2.0f};
                ui_plot_draw(bot_r, &icfg, &st->view_flt_aux, &itr, 1u, NULL, 0u, t);
            }
        }
    }

    rata_arena_restore(ctx->frame_arena, mark);
}

#include "ui/ui_ratawave_freq.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "core/cplx.h"
#include "core/fft.h"
#include "engines/dsp_sampling.h"
#include "ui/ui_formula.h"
#include "ui/ui_widgets.h"

void ui_ratawave_draw_fft_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx) {
    const RataTheme *t = ctx->theme;
    float ctrl_w = 290.0f;
    Rectangle left = {area.x, area.y, ctrl_w, area.height};
    Rectangle right = {area.x + ctrl_w + 8.0f, area.y, area.width - ctrl_w - 8.0f, area.height};

    ui_draw_panel(left, t);
    ui_draw_section_title((Rectangle){left.x, left.y, left.width, 28.0f},
                          "DFT / FFT Spectrum", t);

    float x = left.x + 10.0f;
    float w = left.width - 20.0f;
    float y = left.y + 36.0f;

    if (ui_text_field((Rectangle){x, y, w, 40.0f}, "Signal x[n] (shared with Signals tab):",
                      st->expr_x, (int)sizeof(st->expr_x), 301, ctx->focus, t)) {
        st->sig_mode = SIG_SRC_EXPR;
        ui_plot_view_reset(&st->view_fft_mag);
        ui_plot_view_reset(&st->view_fft_phase);
    }
    y += 48.0f;

    double n_d = (double)st->n_len;
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Transform Length N", "samples",
                      &n_d, 8.0, 512.0, t)) {
        st->n_len = (int32_t)round(n_d);
        ui_plot_view_reset(&st->view_fft_mag);
        ui_plot_view_reset(&st->view_fft_phase);
    }
    y += 38.0f;

    /* Quick N buttons demonstrating both Radix-2 and arbitrary-N Bluestein paths */
    float nbw = (w - 12.0f) / 4.0f;
    const int32_t n_presets[4] = {50, 64, 100, 256};
    const char *n_labels[4] = {"N=50", "N=64", "N=100", "N=256"};
    for (int i = 0; i < 4; ++i) {
        if (ui_button((Rectangle){x + (float)i * (nbw + 4.0f), y, nbw, 24.0f},
                      n_labels[i], st->n_len == n_presets[i], t)) {
            st->n_len = n_presets[i];
            ui_plot_view_reset(&st->view_fft_mag);
            ui_plot_view_reset(&st->view_fft_phase);
        }
    }
    y += 32.0f;

    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Sampling Rate fs", "Hz",
                      &st->fs_hz, 50.0, 8000.0, t)) {
        st->fs_hz = floor(st->fs_hz + 0.5);
        ui_plot_view_reset(&st->view_fft_mag);
        ui_plot_view_reset(&st->view_fft_phase);
    }
    y += 42.0f;

    DrawText("Window Function:", (int)x, (int)y, 12, t->text_secondary);
    y += 16.0f;
    static const char *win_labels[4] = {"Rect", "Hann", "Hamm", "Blkmn"};
    if (ui_segmented_tabs((Rectangle){x, y, w, 24.0f}, win_labels, 4, &st->fft_window, t)) {
        ui_plot_view_reset(&st->view_fft_mag);
        ui_plot_view_reset(&st->view_fft_phase);
    }
    y += 34.0f;

    float cw = (w - 4.0f) * 0.5f;
    if (ui_toggle_chip((Rectangle){x, y, cw, 26.0f}, "Magnitude dB", &st->fft_db, t)) {
        ui_plot_view_reset(&st->view_fft_mag);
    }
    if (ui_toggle_chip((Rectangle){x + cw + 4.0f, y, cw, 26.0f}, "Log Freq X", &st->fft_log_x, t)) {
        st->view_fft_mag.log_x = st->fft_log_x;
        st->view_fft_phase.log_x = st->fft_log_x;
        ui_plot_view_reset(&st->view_fft_mag);
        ui_plot_view_reset(&st->view_fft_phase);
    }
    y += 32.0f;
    if (ui_toggle_chip((Rectangle){x, y, cw, 26.0f}, "Two-Sided", &st->fft_centered, t)) {
        if (st->fft_centered) {
            st->fft_log_x = false;
            st->view_fft_mag.log_x = false;
            st->view_fft_phase.log_x = false;
        }
        ui_plot_view_reset(&st->view_fft_mag);
        ui_plot_view_reset(&st->view_fft_phase);
    }
    if (ui_toggle_chip((Rectangle){x + cw + 4.0f, y, cw, 26.0f}, "Omega/pi Axis",
                       &st->fft_norm_omega, t)) {
        ui_plot_view_reset(&st->view_fft_mag);
        ui_plot_view_reset(&st->view_fft_phase);
    }
    y += 38.0f;

    /* Compute signal, apply window, compute FFT (Radix-2 or Bluestein) */
    size_t mark = rata_arena_mark(ctx->frame_arena);
    RataSignal sig, win_sig;
    RataExprError err;
    memset(&err, 0, sizeof(err));
    RataStatus status = ui_ratawave_build_signal(st, &sig, &err, ctx->frame_arena);
    if (status != RATA_OK) {
        char msg[192];
        snprintf(msg, sizeof(msg), "FFT input error: %s", err.message);
        ui_draw_banner((Rectangle){right.x, right.y, right.width, 34.0f}, msg, BANNER_ERROR, t);
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    dsp_signal_apply_window(&sig, (RataWindowKind)st->fft_window, &win_sig, ctx->frame_arena);

    size_t N = win_sig.len;
    cplx *spec = (cplx *)rata_arena_alloc(ctx->frame_arena, N * sizeof(cplx), 8u);
    cplx *spec_plot = (cplx *)rata_arena_alloc(ctx->frame_arena, N * sizeof(cplx), 8u);
    double *mag = (double *)rata_arena_alloc(ctx->frame_arena, N * sizeof(double), 8u);
    double *phase = (double *)rata_arena_alloc(ctx->frame_arena, N * sizeof(double), 8u);
    double *faxis = (double *)rata_arena_alloc(ctx->frame_arena, N * sizeof(double), 8u);
    if (spec == NULL || spec_plot == NULL || mag == NULL || phase == NULL || faxis == NULL) {
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    fft_real_forward(win_sig.samples, spec, N, ctx->frame_arena);

    size_t plot_n = N;
    if (st->fft_centered) {
        fft_shift(spec, spec_plot, N);
        int32_t half = (int32_t)(N / 2u);
        for (size_t i = 0u; i < N; ++i) {
            int32_t k = (int32_t)i - half;
            faxis[i] = st->fft_norm_omega
                           ? (2.0 * (double)k / (double)N)
                           : ((double)k * sig.fs / (double)N);
        }
    } else {
        plot_n = N / 2u + 1u;
        memcpy(spec_plot, spec, plot_n * sizeof(cplx));
        for (size_t i = 0u; i < plot_n; ++i) {
            faxis[i] = st->fft_norm_omega
                           ? (2.0 * (double)i / (double)N)
                           : ((double)i * sig.fs / (double)N);
        }
    }

    if (st->fft_db) {
        fft_magnitude_db(spec_plot, mag, plot_n, -100.0);
    } else {
        fft_magnitude(spec_plot, mag, plot_n);
    }
    fft_phase(spec_plot, phase, plot_n, 1.0e-9);

    /* Find dominant peak in plotted spectrum */
    size_t peak_i = 0u;
    double peak_mag = -1.0e300;
    for (size_t i = 0u; i < plot_n; ++i) {
        if (mag[i] > peak_mag) {
            peak_mag = mag[i];
            peak_i = i;
        }
    }

    /* Info readout on left panel */
    bool is_pow2 = fft_is_power_of_two(N);
    char info1[96], info2[96], info3[96];
    snprintf(info1, sizeof(info1), "FFT Path: %s",
             is_pow2 ? "Radix-2 Cooley-Tukey" : "Bluestein Arbitrary-N");
    snprintf(info2, sizeof(info2), "Resolution df = %.4g Hz (N = %zu)", sig.fs / (double)N, N);
    snprintf(info3, sizeof(info3), "Peak at %.4g %s (%.4g %s)",
             faxis[peak_i], st->fft_norm_omega ? "*pi rad" : "Hz",
             peak_mag, st->fft_db ? "dB" : "");
    DrawText("Spectrum Info:", (int)x, (int)y, 12, t->accent);
    DrawText(info1, (int)x, (int)y + 18, 11, is_pow2 ? t->success : t->warning);
    DrawText(info2, (int)x, (int)y + 34, 11, t->text_primary);
    DrawText(info3, (int)x, (int)y + 50, 11, t->text_secondary);

    /* Right side: formula + magnitude plot + phase plot + assumption banner */
    ui_formula_card((Rectangle){right.x, right.y, right.width, 58.0f},
                    "Discrete Fourier Transform",
                    "X[k] = \\sum_{n=0}^{N-1} x[n] w[n] e^{-j 2 \\pi k n / N}",
                    15.0f, t, ctx->frame_arena);

    float avail_h = right.height - 92.0f;
    float mag_h = avail_h * 0.56f;
    float ph_h = avail_h - mag_h - 8.0f;

    st->view_fft_mag.log_x = st->fft_log_x && !st->fft_centered;
    st->view_fft_phase.log_x = st->fft_log_x && !st->fft_centered;

    const char *xlab = st->fft_norm_omega ? "Normalized Frequency (omega / pi)" : "Frequency (Hz)";
    PlotConfig mcfg = {
        st->fft_db ? "FFT Magnitude |X[k]| (dB)" : "FFT Magnitude |X[k]| (Linear)",
        xlab,
        st->fft_db ? "|X| (dB)" : "|X|",
        true, true, true, false
    };
    PlotTrace mtr = {faxis, mag, plot_n, "|X[k]|", t->traces[0],
                     (plot_n <= 65u) ? PLOT_STEM : PLOT_LINE, 2.0f};
    ui_plot_draw((Rectangle){right.x, right.y + 66.0f, right.width, mag_h},
                 &mcfg, &st->view_fft_mag, &mtr, 1u, NULL, 0u, t);

    PlotConfig pcfg = {
        "FFT Phase angle(X[k]) (rad)",
        xlab,
        "rad",
        true, true, true, false
    };
    PlotTrace ptr = {faxis, phase, plot_n, "angle(X[k])", t->traces[1],
                     (plot_n <= 65u) ? PLOT_STEM : PLOT_LINE, 1.8f};
    ui_plot_draw((Rectangle){right.x, right.y + 66.0f + mag_h + 8.0f, right.width, ph_h},
                 &pcfg, &st->view_fft_phase, &ptr, 1u, NULL, 0u, t);

    ui_draw_banner((Rectangle){right.x, right.y + right.height - 22.0f, right.width, 22.0f},
                   "Assumptions: unnormalized DFT; phase zeroed where |X[k]| < 1e-9; Log-X omits DC (0 Hz).",
                   BANNER_INFO, t);

    rata_arena_restore(ctx->frame_arena, mark);
}

void ui_ratawave_draw_sampling_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx) {
    const RataTheme *t = ctx->theme;
    float ctrl_w = 290.0f;
    Rectangle left = {area.x, area.y, ctrl_w, area.height};
    Rectangle right = {area.x + ctrl_w + 8.0f, area.y, area.width - ctrl_w - 8.0f, area.height};

    ui_draw_panel(left, t);
    ui_draw_section_title((Rectangle){left.x, left.y, left.width, 28.0f},
                          "Sampling & Aliasing Demo", t);

    float x = left.x + 10.0f;
    float w = left.width - 20.0f;
    float y = left.y + 36.0f;

    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Analog Signal f0", "Hz",
                      &st->samp_f0_hz, 10.0, 2000.0, t)) {
        st->samp_f0_hz = floor(st->samp_f0_hz + 0.5);
    }
    y += 40.0f;
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Sampling Rate fs", "Hz",
                      &st->samp_fs_hz, 100.0, 3000.0, t)) {
        st->samp_fs_hz = floor(st->samp_fs_hz + 0.5);
    }
    y += 40.0f;
    (void)ui_slider_row((Rectangle){x, y, w, 34.0f}, "Initial Phase phi", "rad",
                        &st->samp_phase_rad, -RATA_PI, RATA_PI, t);
    y += 40.0f;
    if (ui_slider_row((Rectangle){x, y, w, 34.0f}, "Window Duration T", "s",
                      &st->samp_duration_s, 0.004, 0.04, t)) {
        ui_plot_view_reset(&st->view_sampling);
    }
    y += 44.0f;

    DrawText("Textbook Scenarios:", (int)x, (int)y, 11, t->text_muted);
    y += 16.0f;
    float bw = (w - 4.0f) * 0.5f;
    if (ui_button((Rectangle){x, y, bw, 24.0f}, "Well-Sampled", false, t)) {
        st->samp_f0_hz = 150.0; st->samp_fs_hz = 1000.0; st->samp_phase_rad = 0.0;
        ui_plot_view_reset(&st->view_sampling);
    }
    if (ui_button((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "Near Nyquist", false, t)) {
        st->samp_f0_hz = 460.0; st->samp_fs_hz = 1000.0; st->samp_phase_rad = 0.0;
        ui_plot_view_reset(&st->view_sampling);
    }
    y += 28.0f;
    if (ui_button((Rectangle){x, y, bw, 24.0f}, "Alias 800/1k", false, t)) {
        st->samp_f0_hz = 800.0; st->samp_fs_hz = 1000.0; st->samp_phase_rad = 0.4;
        ui_plot_view_reset(&st->view_sampling);
    }
    if (ui_button((Rectangle){x + bw + 4.0f, y, bw, 24.0f}, "2nd Fold 1.3k", false, t)) {
        st->samp_f0_hz = 1300.0; st->samp_fs_hz = 1000.0; st->samp_phase_rad = 0.4;
        ui_plot_view_reset(&st->view_sampling);
    }
    y += 34.0f;

    (void)ui_toggle_chip((Rectangle){x, y, bw, 26.0f}, "Sinc Recon", &st->samp_show_sinc, t);
    (void)ui_toggle_chip((Rectangle){x + bw + 4.0f, y, bw, 26.0f}, "Exact Alias",
                         &st->samp_show_alias, t);
    y += 38.0f;

    size_t mark = rata_arena_mark(ctx->frame_arena);
    RataSamplingConfig cfg = {
        st->samp_f0_hz, 1.0, st->samp_phase_rad,
        st->samp_fs_hz, st->samp_duration_s, 600u
    };
    RataSamplingResult res;
    RataStatus status = dsp_sampling_simulate(&cfg, &res, ctx->frame_arena);
    if (status != RATA_OK) {
        ui_draw_banner((Rectangle){right.x, right.y, right.width, 32.0f},
                       "Invalid sampling parameters.", BANNER_ERROR, t);
        rata_arena_restore(ctx->frame_arena, mark);
        return;
    }

    char r1[96], r2[96], r3[96], r4[96];
    snprintf(r1, sizeof(r1), "Nyquist Freq (fs/2): %.4g Hz", res.nyquist_freq_hz);
    snprintf(r2, sizeof(r2), "Nyquist Rate (2*f0): %.4g Hz", res.nyquist_rate_hz);
    snprintf(r3, sizeof(r3), "Apparent Freq:       %.4g Hz", res.aliased_freq_hz);
    snprintf(r4, sizeof(r4), "Phase Inverted:      %s", res.phase_inverted ? "YES (odd zone)" : "No");

    DrawText("Nyquist Analysis:", (int)x, (int)y, 12, t->accent);
    DrawText(r1, (int)x, (int)y + 18, 12, t->text_primary);
    DrawText(r2, (int)x, (int)y + 36, 12, t->text_primary);
    DrawText(r3, (int)x, (int)y + 54, 12, res.is_aliased ? t->danger : t->success);
    DrawText(r4, (int)x, (int)y + 72, 12, res.phase_inverted ? t->warning : t->text_secondary);

    ui_formula_card((Rectangle){right.x, right.y, right.width, 58.0f},
                    "Nyquist-Shannon Sampling & Reconstruction",
                    "f_{Nyq} = frac{f_s}{2},   x_{recon}(t) = \\sum_n x[n] sinc(f_s t - n)",
                    15.0f, t, ctx->frame_arena);

    char banner[192];
    if (res.is_aliased) {
        snprintf(banner, sizeof(banner),
                 "ALIASING DETECTED: f0 = %.4g Hz > fs/2 = %.4g Hz -> folds to %.4g Hz%s",
                 st->samp_f0_hz, res.nyquist_freq_hz, res.aliased_freq_hz,
                 res.phase_inverted ? " (phase inverted)" : "");
    } else {
        snprintf(banner, sizeof(banner),
                 "Nyquist Satisfied: f0 = %.4g Hz <= fs/2 = %.4g Hz (exact reconstruction possible).",
                 st->samp_f0_hz, res.nyquist_freq_hz);
    }
    ui_draw_banner((Rectangle){right.x, right.y + 64.0f, right.width, 28.0f},
                   banner, res.is_aliased ? BANNER_ERROR : BANNER_OK, t);

    PlotTrace traces[4];
    size_t ntr = 0u;
    traces[ntr++] = (PlotTrace){res.t_cont, res.x_analog, res.cont_len,
                                "Analog x_a(t)", t->traces[0], PLOT_LINE, 1.8f};
    if (st->samp_show_sinc) {
        traces[ntr++] = (PlotTrace){res.t_cont, res.x_recon_sinc, res.cont_len,
                                    "Sinc Recon x_hat(t)", t->traces[3], PLOT_LINE, 2.2f};
    }
    if (st->samp_show_alias) {
        traces[ntr++] = (PlotTrace){res.t_cont, res.x_alias_exact, res.cont_len,
                                    "Ideal Alias Tone", t->traces[2], PLOT_LINE, 1.8f};
    }
    traces[ntr++] = (PlotTrace){res.t_samples, res.x_samples, res.sample_len,
                                "Samples x[n]", t->traces[1], PLOT_STEM, 2.0f};

    PlotConfig pcfg = {
        "Analog Signal vs Discrete Samples & Reconstruction",
        "Time t (seconds)",
        "x(t)",
        true, true, true, false
    };
    ui_plot_draw((Rectangle){right.x, right.y + 98.0f, right.width, right.height - 98.0f},
                 &pcfg, &st->view_sampling, traces, ntr, NULL, 0u, t);

    rata_arena_restore(ctx->frame_arena, mark);
}

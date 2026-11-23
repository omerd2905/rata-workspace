#ifndef RATA_UI_RATAWAVE_H
#define RATA_UI_RATAWAVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/status.h"
#include "engines/dsp_filter.h"
#include "engines/dsp_lti.h"
#include "engines/dsp_signal.h"
#include "engines/project_file.h"
#include "raylib.h"
#include "ui/ui_plot.h"
#include "ui/ui_workspace.h"

typedef enum RataWaveTab {
    RW_TAB_SIGNAL = 0,
    RW_TAB_CONV = 1,
    RW_TAB_FFT = 2,
    RW_TAB_LTI = 3,
    RW_TAB_SAMPLING = 4,
    RW_TAB_FILTER = 5,
    RW_TAB_COUNT = 6
} RataWaveTab;

typedef enum RataSigSourceMode {
    SIG_SRC_EXPR = 0,
    SIG_SRC_PRIM = 1
} RataSigSourceMode;

typedef enum RataPrimKind {
    PRIM_IMPULSE = 0,
    PRIM_STEP = 1,
    PRIM_SINUSOID = 2,
    PRIM_EXP = 3,
    PRIM_RECT = 4,
    PRIM_PIECEWISE = 5
} RataPrimKind;

/* Full RataWave workspace state (owned by app; zero hidden globals). */
typedef struct RataWaveState {
    int active_tab;

    /* Shared signal configuration (Tab 1: Signals & Tab 3: FFT & Tab 4: Filter input). */
    int sig_mode;
    int prim_kind;
    char expr_x[192];
    double fs_hz;
    int32_t n0;
    int32_t n_len;
    double prim_amp;
    double prim_freq_hz;
    double prim_phase_rad;
    double prim_base;
    int32_t prim_shift;
    int32_t prim_width;
    PlotView view_sig;

    /* Tab 2: Convolution (flip-and-shift animation). */
    char conv_expr_x[160];
    char conv_expr_h[160];
    int32_t conv_x_n0;
    int32_t conv_x_len;
    int32_t conv_h_n0;
    int32_t conv_h_len;
    int32_t conv_n_shift;
    bool conv_playing;
    double conv_speed;
    double conv_accum_dt;
    PlotView view_conv_top;
    PlotView view_conv_bot;

    /* Tab 3: DFT / FFT. */
    int fft_window;
    bool fft_db;
    bool fft_log_x;
    bool fft_centered;
    bool fft_norm_omega;
    PlotView view_fft_mag;
    PlotView view_fft_phase;

    /* Tab 4: Z-Transform & LTI System. */
    char lti_b_str[256];
    char lti_a_str[256];
    int lti_right_mode; /* 0 = Freq Response, 1 = Impulse, 2 = Step, 3 = Filter x[n] */
    bool lti_db;
    bool lti_log_x;
    int32_t lti_resp_len;
    PlotView view_lti_pz;
    PlotView view_lti_mag;
    PlotView view_lti_phase;
    PlotView view_lti_time;

    /* Tab 5: Sampling & Aliasing. */
    double samp_f0_hz;
    double samp_fs_hz;
    double samp_phase_rad;
    double samp_duration_s;
    bool samp_show_sinc;
    bool samp_show_alias;
    PlotView view_sampling;

    /* Tab 6: FIR / IIR Filter Design. */
    int flt_family; /* 0 = FIR, 1 = Butterworth, 2 = Chebyshev I */
    int flt_band;   /* RataFilterBand */
    int flt_window; /* RataWindowKind (FIR) */
    int32_t flt_fir_taps;
    int32_t flt_iir_order;
    double flt_fs_hz;
    double flt_fc1_hz;
    double flt_fc2_hz;
    double flt_ripple_db;
    bool flt_db;
    bool flt_log_x;
    int flt_aux_view; /* 0 = Phase, 1 = Pole-Zero, 2 = Impulse */
    PlotView view_flt_mag;
    PlotView view_flt_aux;
} RataWaveState;

/* Builds the current primary signal x[n] into `out` (samples allocated in `arena`). */
RataStatus ui_ratawave_build_signal(const RataWaveState *st,
                                    RataSignal *out,
                                    RataExprError *err_out,
                                    RataArena *arena);

/* Workspace registry callbacks. */
void ui_ratawave_init(void *state_ptr);
void ui_ratawave_draw(void *state_ptr, Rectangle viewport, RataWorkspaceCtx *ctx);
RataStatus ui_ratawave_save(const void *state_ptr, RataProjectDoc *doc);
RataStatus ui_ratawave_load(void *state_ptr, const RataProjectDoc *doc);

#endif /* RATA_UI_RATAWAVE_H */

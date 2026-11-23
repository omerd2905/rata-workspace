#ifndef RATA_ENGINES_DSP_SIGNAL_H
#define RATA_ENGINES_DSP_SIGNAL_H

#include <stddef.h>
#include <stdint.h>

#include "core/arena.h"
#include "core/status.h"
#include "engines/expr.h"

#define RATA_DSP_MAX_SAMPLES 16384u

/* Discrete-time signal x[n] for n = n0, n0+1, ..., n0+len-1.
 * `samples` memory is owned by the arena or caller buffer provided at creation. */
typedef struct RataSignal {
    double *samples;
    size_t len;
    int32_t n0;
    double fs;
} RataSignal;

typedef enum RataWindowKind {
    WIN_RECT = 0,
    WIN_HANN = 1,
    WIN_HAMMING = 2,
    WIN_BLACKMAN = 3
} RataWindowKind;

typedef struct RataPiecewiseSeg {
    int32_t n_start;
    int32_t n_end; /* inclusive */
    const char *expr;
} RataPiecewiseSeg;

typedef struct RataSignalStats {
    double energy;
    double avg_power;
    double mean;
    double rms;
    double min_val;
    double max_val;
} RataSignalStats;

/* Allocates a zero-initialized signal of length `len` in `arena` (owned by `arena`). */
RataStatus dsp_signal_alloc(RataSignal *out, size_t len, int32_t n0, double fs, RataArena *arena);

/* Evaluates an expression string with variables `n`, `t = n/fs`, and `fs`.
 * Allocates `out->samples` in `arena` (owned by `arena`). */
RataStatus dsp_signal_from_expr(const char *expr_str,
                                int32_t n0,
                                size_t len,
                                double fs,
                                RataSignal *out,
                                RataExprError *err_out,
                                RataArena *arena);

/* Primitive signal generators (all allocate `out->samples` in `arena`). */
RataStatus dsp_signal_impulse(int32_t n0, size_t len, double fs,
                              double amp, int32_t shift,
                              RataSignal *out, RataArena *arena);

RataStatus dsp_signal_step(int32_t n0, size_t len, double fs,
                           double amp, int32_t shift,
                           RataSignal *out, RataArena *arena);

RataStatus dsp_signal_sinusoid(int32_t n0, size_t len, double fs,
                               double amp, double freq_hz, double phase_rad,
                               RataSignal *out, RataArena *arena);

RataStatus dsp_signal_exp(int32_t n0, size_t len, double fs,
                          double amp, double base, int32_t shift,
                          RataSignal *out, RataArena *arena);

RataStatus dsp_signal_rect(int32_t n0, size_t len, double fs,
                           double amp, int32_t start_n, size_t width,
                           RataSignal *out, RataArena *arena);

/* Evaluates piecewise expression segments (first matching segment wins; 0 outside all segments).
 * Allocates `out->samples` in `arena` (owned by `arena`). */
RataStatus dsp_signal_piecewise(int32_t n0, size_t len, double fs,
                                const RataPiecewiseSeg *segs, size_t seg_count,
                                RataSignal *out, RataExprError *err_out, RataArena *arena);

/* Computes symmetric window coefficients into caller-owned `win_out[0..len-1]`. */
RataStatus dsp_window_coeffs(RataWindowKind kind, double *win_out, size_t len);

/* Applies a window to `in` and allocates windowed signal `out` in `arena`. */
RataStatus dsp_signal_apply_window(const RataSignal *in,
                                   RataWindowKind kind,
                                   RataSignal *out,
                                   RataArena *arena);

/* Computes energy, average power, mean, RMS, min, and max of `sig`. */
RataStatus dsp_signal_stats(const RataSignal *sig, RataSignalStats *stats_out);

/* Human-readable static name for a window kind. */
const char *dsp_window_name(RataWindowKind kind);

#endif /* RATA_ENGINES_DSP_SIGNAL_H */

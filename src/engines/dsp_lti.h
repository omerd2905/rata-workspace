#ifndef RATA_ENGINES_DSP_LTI_H
#define RATA_ENGINES_DSP_LTI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/arena.h"
#include "core/cplx.h"
#include "core/status.h"
#include "engines/dsp_signal.h"

#define RATA_LTI_MAX_COEFFS 130u

/* Discrete-time LTI transfer function in powers of z^-1:
 * H(z) = (b[0] + b[1]*z^-1 + ... + b[nb-1]*z^-(nb-1))
 *      / (a[0] + a[1]*z^-1 + ... + a[na-1]*z^-(na-1)). */
typedef struct RataLti {
    double b[RATA_LTI_MAX_COEFFS];
    size_t nb;
    double a[RATA_LTI_MAX_COEFFS];
    size_t na;
    double fs;
} RataLti;

typedef enum RataStability {
    LTI_STABLE = 0,    /* All poles strictly inside unit circle |p| < 1 */
    LTI_MARGINAL = 1,  /* Simple poles on |p| = 1, none outside */
    LTI_UNSTABLE = 2   /* At least one pole |p| > 1, or repeated pole on |p| = 1 */
} RataStability;

/* Pole-zero analysis result (including poles/zeros at the origin z = 0). */
typedef struct RataPoleZero {
    cplx zeros[RATA_LTI_MAX_COEFFS];
    size_t num_zeros;
    cplx poles[RATA_LTI_MAX_COEFFS];
    size_t num_poles;
    double gain;
    double max_pole_mag;
    double dc_gain;
    bool dc_gain_finite;
    RataStability stability;
} RataPoleZero;

/* Frequency response H(e^{j*omega}) over a grid of `len` points.
 * All arrays are allocated in the caller's RataArena (owned by `arena`). */
typedef struct RataFreqz {
    size_t len;
    double *omega_rad;          /* Digital angular frequency (rad/sample) */
    double *freq_hz;            /* Physical frequency (Hz) = omega * fs / (2*pi) */
    cplx *H;                    /* Complex response H(e^{j*omega}) */
    double *mag;                /* Linear magnitude |H| */
    double *mag_db;             /* Magnitude in dB (clamped to floor_db) */
    double *phase_rad;          /* Wrapped phase in [-pi, pi] */
    double *phase_unwrapped;    /* Unwrapped phase (rad) */
    double *group_delay;        /* Analytic group delay (samples) */
    bool has_pole_on_grid;      /* True if a frequency bin hit a unit-circle pole */
} RataFreqz;

/* Validates and initializes an LTI transfer function struct. */
RataStatus dsp_lti_init(RataLti *sys,
                        const double *b, size_t nb,
                        const double *a, size_t na,
                        double fs);

/* Parses comma- or whitespace-separated real coefficients into caller-owned `out[0..cap-1]`. */
RataStatus dsp_parse_coeff_list(const char *text, double *out, size_t cap, size_t *count_out);

/* Computes all finite poles & zeros of H(z) (including origin poles/zeros), DC gain, and
 * BIBO stability verdict. Uses `scratch` for temporary allocations (restores mark). */
RataStatus dsp_lti_poles_zeros(const RataLti *sys, RataPoleZero *pz_out, RataArena *scratch);

/* Evaluates H(e^{j*omega}) at a single digital angular frequency `omega_rad`. */
RataStatus dsp_lti_eval_freq(const RataLti *sys, double omega_rad, cplx *h_out);

/* Evaluates frequency response on `num_points` uniformly spaced frequencies in [0, pi]
 * (or [0, 2*pi) if `full_circle` is true). Allocates arrays in `arena` (owned by `arena`). */
RataStatus dsp_lti_freqz(const RataLti *sys,
                         size_t num_points,
                         bool full_circle,
                         double floor_db,
                         RataFreqz *out,
                         RataArena *arena);

/* Filters input signal `x` through `sys` via Direct Form II Transposed.
 * Allocates `y_out->samples` in `arena` (owned by `arena`).
 * If output diverges (unstable system, |y[n]| > 1e15 or non-finite), sets `*diverged_at`
 * (if non-NULL), zeroes remaining samples, and returns RATA_ERR_UNSTABLE. */
RataStatus dsp_lti_filter(const RataLti *sys,
                          const RataSignal *x,
                          RataSignal *y_out,
                          size_t *diverged_at,
                          RataArena *arena);

/* Convenience impulse response h[n] and unit-step response s[n] for n = 0 .. len-1. */
RataStatus dsp_lti_impulse_response(const RataLti *sys,
                                    size_t len,
                                    RataSignal *h_out,
                                    size_t *diverged_at,
                                    RataArena *arena);

RataStatus dsp_lti_step_response(const RataLti *sys,
                                 size_t len,
                                 RataSignal *s_out,
                                 size_t *diverged_at,
                                 RataArena *arena);

const char *dsp_stability_str(RataStability s);

#endif /* RATA_ENGINES_DSP_LTI_H */

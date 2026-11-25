#ifndef RATA_ENGINES_DSP_CONV_H
#define RATA_ENGINES_DSP_CONV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/arena.h"
#include "core/status.h"
#include "engines/dsp_signal.h"

/* Snapshot of a single step n of discrete convolution y[n] = sum_k x[k] * h[n - k].
 * Arrays `k_axis`, `x_aligned`, `h_shifted`, and `product` are allocated in the caller's
 * RataArena and share length `k_len` covering indices k = k_min .. k_min + k_len - 1. */
typedef struct RataConvStep {
    int32_t n_shift;
    int32_t k_min;
    size_t k_len;
    double *k_axis;
    double *x_aligned;
    double *h_shifted;
    double *product;
    double sum;
} RataConvStep;

/* Computes exact linear convolution y[n] = (x * h)[n] with index tracking:
 * y.n0 = x.n0 + h.n0, y.len = x.len + h.len - 1.
 * Allocates `y_out->samples` in `arena` (owned by `arena`). */
RataStatus dsp_conv_linear(const RataSignal *x,
                           const RataSignal *h,
                           RataSignal *y_out,
                           RataArena *arena);

/* Diagnostics returned by discrete deconvolution `dsp_deconv_linear`. */
typedef struct RataDeconvInfo {
    bool exact_finite;    /* True when y == known * out holds over full finite support */
    bool truncated_iir;   /* True when Y(z)/Known(z) is non-terminating (truncated to max_out_len) */
    double max_residual;  /* max_n |y[n] - (known * out)[n]| across full support */
    size_t diverged_at;   /* Sample index where |out[m]| exceeded 1e15 (if RATA_ERR_UNSTABLE) */
} RataDeconvInfo;

/* Computes aligned x[k], flipped-and-shifted h[n_shift - k], pointwise product x[k]*h[n_shift-k],
 * and accumulated sum y[n_shift] on a steady common k-axis covering the entire sweep of h[n-k].
 * Allocates all output arrays in `arena` (owned by `arena`). */
RataStatus dsp_conv_step(const RataSignal *x,
                         const RataSignal *h,
                         int32_t n_shift,
                         RataConvStep *step_out,
                         RataArena *arena);

/* Solves discrete linear convolution `y = known * out` (or `y = out * known`) for `out`
 * via causal ascending-power long division after stripping leading/trailing near-zero samples:
 * - Tracks shift automatically: `out->n0 = y_first_nonzero_n - known_first_nonzero_n`.
 * - If `y` is an exact finite convolution of `known`, trims `out` to exact finite support.
 * - Otherwise computes `max_out_len` causal samples of `Y(z) / Known(z)`.
 * - Returns RATA_ERR_DIV_ZERO if `known` is identically zero, or RATA_ERR_UNSTABLE if
 *   quotient magnitude exceeds 1e15 (writing finite prefix into `out`).
 * Allocates `out->samples` in `arena` (owned by `arena`). */
RataStatus dsp_deconv_linear(const RataSignal *y,
                             const RataSignal *known,
                             size_t max_out_len,
                             RataSignal *out,
                             RataDeconvInfo *info_out,
                             RataArena *arena);

#endif /* RATA_ENGINES_DSP_CONV_H */

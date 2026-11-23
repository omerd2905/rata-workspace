#ifndef RATA_ENGINES_DSP_CONV_H
#define RATA_ENGINES_DSP_CONV_H

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

/* Computes aligned x[k], flipped-and-shifted h[n_shift - k], pointwise product x[k]*h[n_shift-k],
 * and accumulated sum y[n_shift] on a steady common k-axis covering the entire sweep of h[n-k].
 * Allocates all output arrays in `arena` (owned by `arena`). */
RataStatus dsp_conv_step(const RataSignal *x,
                         const RataSignal *h,
                         int32_t n_shift,
                         RataConvStep *step_out,
                         RataArena *arena);

#endif /* RATA_ENGINES_DSP_CONV_H */

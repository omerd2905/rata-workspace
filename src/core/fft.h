#ifndef RATA_CORE_FFT_H
#define RATA_CORE_FFT_H

#include <stdbool.h>
#include <stddef.h>

#include "core/arena.h"
#include "core/cplx.h"
#include "core/status.h"

/* Returns true if n > 0 and n is a power of two. */
bool fft_is_power_of_two(size_t n);

/* Returns smallest power of two >= n (returns 0 on overflow or n == 0). */
size_t fft_next_power_of_two(size_t n);

/* Direct O(N^2) forward DFT: X[k] = sum_{n=0..N-1} x[n] * exp(-j*2*pi*k*n/N).
 * Caller owns `in` and `out`; `in` and `out` must not alias. */
RataStatus dft_direct_forward(const cplx *in, cplx *out, size_t n);

/* Direct O(N^2) inverse DFT: x[n] = (1/N) * sum_{k=0..N-1} X[k] * exp(+j*2*pi*k*n/N).
 * Caller owns `in` and `out`; `in` and `out` must not alias. */
RataStatus dft_direct_inverse(const cplx *in, cplx *out, size_t n);

/* Forward FFT for ANY length n >= 1:
 * - Power-of-two n: Cooley-Tukey radix-2 DIT FFT (O(N log N), no scratch needed).
 * - Arbitrary n: Bluestein chirp-z FFT (O(N log N), uses `scratch` and restores mark).
 * Caller owns `in` and `out`; in-place (`in == out`) is supported. */
RataStatus fft_forward(const cplx *in, cplx *out, size_t n, RataArena *scratch);

/* Inverse FFT for ANY length n >= 1 (normalized by 1/N).
 * Caller owns `in` and `out`; in-place (`in == out`) is supported. */
RataStatus fft_inverse(const cplx *in, cplx *out, size_t n, RataArena *scratch);

/* Convenience forward FFT for real-valued input sequence `in[0..n-1]`.
 * Caller owns `in` and `out`. Uses `scratch` and restores mark. */
RataStatus fft_real_forward(const double *in, cplx *out, size_t n, RataArena *scratch);

/* Computes linear magnitude |spec[k]| into caller-owned `mag[0..n-1]`. */
RataStatus fft_magnitude(const cplx *spec, double *mag, size_t n);

/* Computes magnitude in dB: 20*log10(|spec[k]|), clamped below to `floor_db`.
 * Caller owns `spec` and `mag_db`. */
RataStatus fft_magnitude_db(const cplx *spec, double *mag_db, size_t n, double floor_db);

/* Computes phase angle in radians in [-pi, pi] into caller-owned `phase_rad[0..n-1]`.
 * Bins with |spec[k]| < mag_thresh are set to 0.0 to suppress numerical-noise phase. */
RataStatus fft_phase(const cplx *spec, double *phase_rad, size_t n, double mag_thresh);

/* Circularly shifts zero-frequency bin to the center (MATLAB fftshift convention).
 * Caller owns `in` and `out`; `in` and `out` must not alias. */
RataStatus fft_shift(const cplx *in, cplx *out, size_t n);

#endif /* RATA_CORE_FFT_H */

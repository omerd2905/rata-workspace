#ifndef RATA_ENGINES_DSP_SAMPLING_H
#define RATA_ENGINES_DSP_SAMPLING_H

#include <stdbool.h>
#include <stddef.h>

#include "core/arena.h"
#include "core/status.h"

typedef struct RataSamplingConfig {
    double f0_hz;        /* Analog tone frequency (Hz), >= 0 */
    double amp;          /* Amplitude */
    double phase_rad;    /* Initial phase (rad) */
    double fs_hz;        /* Sampling frequency (Hz), > 0 */
    double duration_s;   /* Time window duration (s), > 0 */
    size_t cont_points;  /* Continuous-grid resolution (e.g. 400..1000) */
} RataSamplingConfig;

/* Sampling & aliasing simulation result.
 * All array pointers are allocated in the caller's RataArena (owned by `arena`). */
typedef struct RataSamplingResult {
    double nyquist_freq_hz;   /* fs / 2 */
    double nyquist_rate_hz;   /* 2 * f0 */
    double aliased_freq_hz;   /* Apparent frequency in [0, fs/2] */
    bool is_aliased;          /* True when f0 > fs/2 */
    bool phase_inverted;      /* True when folded into odd Nyquist zone */

    /* Continuous-time grid (length `cont_len`). */
    size_t cont_len;
    double *t_cont;
    double *x_analog;         /* True analog tone A*cos(2*pi*f0*t + phi) */
    double *x_recon_sinc;     /* Whittaker-Shannon sinc interpolation from finite samples */
    double *x_alias_exact;    /* Ideal infinite-window aliased sinusoid */

    /* Discrete samples (length `sample_len`). */
    size_t sample_len;
    double *t_samples;
    double *x_samples;
} RataSamplingResult;

/* Computes exact folded frequency in [0, fs/2] and phase-inversion flag. */
RataStatus dsp_alias_frequency(double f0_hz,
                               double fs_hz,
                               double *aliased_hz_out,
                               bool *is_aliased_out,
                               bool *phase_inverted_out);

/* Runs sampling, aliasing, and Whittaker-Shannon sinc reconstruction demo.
 * Allocates all waveform arrays in `arena` (owned by `arena`). */
RataStatus dsp_sampling_simulate(const RataSamplingConfig *cfg,
                                 RataSamplingResult *out,
                                 RataArena *arena);

#endif /* RATA_ENGINES_DSP_SAMPLING_H */

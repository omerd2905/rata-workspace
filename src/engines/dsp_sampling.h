#ifndef RATA_ENGINES_DSP_SAMPLING_H
#define RATA_ENGINES_DSP_SAMPLING_H

#include <stdbool.h>
#include <stddef.h>

#include "core/arena.h"
#include "core/status.h"

typedef enum RataSamplingBoundary {
    SAMP_BOUNDARY_NONE = 0,    /* General frequency: phase is recoverable */
    SAMP_BOUNDARY_DC = 1,      /* f0 == 0 (mod fs): constant A*cos(phi), phase not recoverable */
    SAMP_BOUNDARY_NYQUIST = 2  /* f0 == fs/2 (mod fs): A*cos(phi)*(-1)^n, phase not recoverable */
} RataSamplingBoundary;

typedef struct RataSamplingConfig {
    double f0_hz;        /* Analog tone frequency (Hz), >= 0 */
    double amp;          /* Amplitude A */
    double phase_rad;    /* Initial phase phi (rad) for A*cos(2*pi*f0*t + phi) */
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
    double aliased_phase_rad; /* Apparent phase (+phi or -phi); 0 when phase is unrecoverable */
    double effective_amp;     /* A normally, or A*cos(phi) at f0 == 0 or fs/2 (mod fs) */
    bool is_aliased;          /* True when f0 > fs/2 (or folded at boundary with f0 >= fs/2) */
    bool phase_negated;       /* True when phase_recoverable and folded into odd Nyquist zone */
    bool phase_recoverable;   /* False at Nyquist boundaries f0 == 0 or fs/2 (mod fs) */
    RataSamplingBoundary boundary;

    /* Continuous-time grid (length `cont_len`). */
    size_t cont_len;
    double *t_cont;
    double *x_analog;         /* True analog tone A*cos(2*pi*f0*t + phi) */
    double *x_recon_sinc;     /* Whittaker-Shannon sinc interpolation from finite samples */
    double *x_alias_exact;    /* Ideal aliased waveform matching all discrete samples */

    /* Discrete samples (length `sample_len`). */
    size_t sample_len;
    double *t_samples;
    double *x_samples;
} RataSamplingResult;

/* Computes exact folded frequency in [0, fs/2], phase-negation flag, and Nyquist boundary status.
 * For fs/2 < (f0 mod fs) < fs: cos(2*pi*f0*n/fs + phi) == cos(2*pi*(fs-r)*n/fs - phi) (phi -> -phi).
 * At f0 == 0 or fs/2 (mod fs) within relative tolerance 1e-9: phase is NOT recoverable and
 * samples collapse to A*cos(phi)*(+/-1)^n. */
RataStatus dsp_alias_frequency(double f0_hz,
                               double fs_hz,
                               double *aliased_hz_out,
                               bool *is_aliased_out,
                               bool *phase_negated_out);

/* Extended frequency/boundary classifier also returning boundary kind. */
RataStatus dsp_alias_classify(double f0_hz,
                              double fs_hz,
                              double *aliased_hz_out,
                              bool *is_aliased_out,
                              bool *phase_negated_out,
                              RataSamplingBoundary *boundary_out);

/* Runs sampling, aliasing, and Whittaker-Shannon sinc reconstruction demo.
 * Allocates all waveform arrays in `arena` (owned by `arena`). */
RataStatus dsp_sampling_simulate(const RataSamplingConfig *cfg,
                                 RataSamplingResult *out,
                                 RataArena *arena);

#endif /* RATA_ENGINES_DSP_SAMPLING_H */

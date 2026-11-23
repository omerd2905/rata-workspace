#include "engines/dsp_sampling.h"

#include <math.h>
#include <string.h>

static double norm_sinc(double x) {
    if (fabs(x) < 1.0e-12) {
        return 1.0;
    }
    double px = RATA_PI * x;
    return sin(px) / px;
}

RataStatus dsp_alias_classify(double f0_hz,
                              double fs_hz,
                              double *aliased_hz_out,
                              bool *is_aliased_out,
                              bool *phase_negated_out,
                              RataSamplingBoundary *boundary_out) {
    if (!isfinite(f0_hz) || !isfinite(fs_hz) || f0_hz < 0.0 || fs_hz <= 0.0 ||
        aliased_hz_out == NULL || is_aliased_out == NULL ||
        phase_negated_out == NULL || boundary_out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }

    double nyq = 0.5 * fs_hz;
    double tol = 1.0e-9 * ((f0_hz > fs_hz) ? f0_hz : fs_hz);

    /* Reduce arbitrary f0 modulo fs into [0, fs). */
    double rem = fmod(f0_hz, fs_hz);
    if (rem < 0.0) {
        rem += fs_hz;
    }

    /* Boundary 1: f0 == 0 (mod fs) -> samples are constant A*cos(phi), phase unrecoverable. */
    if (rem <= tol || (fs_hz - rem) <= tol) {
        *aliased_hz_out = 0.0;
        *is_aliased_out = (f0_hz > nyq - tol);
        *phase_negated_out = false;
        *boundary_out = SAMP_BOUNDARY_DC;
        return RATA_OK;
    }

    /* Boundary 2: f0 == fs/2 (mod fs) -> samples are A*cos(phi)*(-1)^n, phase unrecoverable. */
    if (fabs(rem - nyq) <= tol) {
        *aliased_hz_out = nyq;
        *is_aliased_out = (f0_hz > nyq + tol);
        *phase_negated_out = false;
        *boundary_out = SAMP_BOUNDARY_NYQUIST;
        return RATA_OK;
    }

    *boundary_out = SAMP_BOUNDARY_NONE;
    *is_aliased_out = (f0_hz > nyq + tol);

    /* For fs/2 < rem < fs: cos(2*pi*rem*n/fs + phi) == cos(2*pi*(fs - rem)*n/fs - phi). */
    if (rem > nyq) {
        *aliased_hz_out = fs_hz - rem;
        *phase_negated_out = true;
    } else {
        *aliased_hz_out = rem;
        *phase_negated_out = false;
    }
    return RATA_OK;
}

RataStatus dsp_alias_frequency(double f0_hz,
                               double fs_hz,
                               double *aliased_hz_out,
                               bool *is_aliased_out,
                               bool *phase_negated_out) {
    RataSamplingBoundary b = SAMP_BOUNDARY_NONE;
    return dsp_alias_classify(f0_hz, fs_hz, aliased_hz_out, is_aliased_out, phase_negated_out, &b);
}

RataStatus dsp_sampling_simulate(const RataSamplingConfig *cfg,
                                 RataSamplingResult *out,
                                 RataArena *arena) {
    if (cfg == NULL || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (!isfinite(cfg->f0_hz) || cfg->f0_hz < 0.0 ||
        !isfinite(cfg->fs_hz) || cfg->fs_hz <= 0.0 ||
        !isfinite(cfg->duration_s) || cfg->duration_s <= 0.0 ||
        !isfinite(cfg->amp) || !isfinite(cfg->phase_rad) ||
        cfg->cont_points < 2u || cfg->cont_points > 8192u) {
        return RATA_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    RataStatus st = dsp_alias_classify(cfg->f0_hz,
                                       cfg->fs_hz,
                                       &out->aliased_freq_hz,
                                       &out->is_aliased,
                                       &out->phase_negated,
                                       &out->boundary);
    if (st != RATA_OK) {
        return st;
    }
    out->nyquist_freq_hz = 0.5 * cfg->fs_hz;
    out->nyquist_rate_hz = 2.0 * cfg->f0_hz;

    if (out->boundary != SAMP_BOUNDARY_NONE) {
        out->phase_recoverable = false;
        out->phase_negated = false;
        out->aliased_phase_rad = 0.0;
        out->effective_amp = cfg->amp * cos(cfg->phase_rad);
        if (fabs(out->effective_amp) <= 1.0e-12 * fabs(cfg->amp)) {
            out->effective_amp = 0.0;
        }
    } else {
        out->phase_recoverable = true;
        out->effective_amp = cfg->amp;
        out->aliased_phase_rad = out->phase_negated ? -cfg->phase_rad : cfg->phase_rad;
        if (fabs(out->aliased_phase_rad) < 1.0e-12) {
            out->aliased_phase_rad = 0.0;
        }
    }

    double est_samples = floor(cfg->duration_s * cfg->fs_hz + 1.0e-9) + 1.0;
    if (!isfinite(est_samples) || est_samples < 1.0 || est_samples > 4096.0) {
        return RATA_ERR_RANGE;
    }
    size_t ns = (size_t)est_samples;
    size_t nc = cfg->cont_points;

    out->t_cont = (double *)rata_arena_alloc(arena, nc * sizeof(double), 8u);
    out->x_analog = (double *)rata_arena_alloc(arena, nc * sizeof(double), 8u);
    out->x_recon_sinc = (double *)rata_arena_alloc(arena, nc * sizeof(double), 8u);
    out->x_alias_exact = (double *)rata_arena_alloc(arena, nc * sizeof(double), 8u);
    out->t_samples = (double *)rata_arena_alloc(arena, ns * sizeof(double), 8u);
    out->x_samples = (double *)rata_arena_alloc(arena, ns * sizeof(double), 8u);
    if (out->t_cont == NULL || out->x_analog == NULL || out->x_recon_sinc == NULL ||
        out->x_alias_exact == NULL || out->t_samples == NULL || out->x_samples == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }

    out->cont_len = nc;
    out->sample_len = ns;

    for (size_t n = 0u; n < ns; ++n) {
        double tn = (double)n / cfg->fs_hz;
        out->t_samples[n] = tn;
        double sample_v = cfg->amp * cos(RATA_TWO_PI * cfg->f0_hz * tn + cfg->phase_rad);
        if (fabs(sample_v) <= 1.0e-14 * fabs(cfg->amp)) {
            sample_v = 0.0;
        }
        out->x_samples[n] = sample_v;
    }

    /* Pad samples on both ends for accurate Whittaker-Shannon sinc interpolation inside [0, T]. */
    const int32_t pad = 32;
    for (size_t i = 0u; i < nc; ++i) {
        double t = cfg->duration_s * (double)i / (double)(nc - 1u);
        out->t_cont[i] = t;
        out->x_analog[i] = cfg->amp * cos(RATA_TWO_PI * cfg->f0_hz * t + cfg->phase_rad);

        if (out->boundary == SAMP_BOUNDARY_DC) {
            out->x_alias_exact[i] = out->effective_amp;
        } else if (out->boundary == SAMP_BOUNDARY_NYQUIST) {
            out->x_alias_exact[i] = out->effective_amp * cos(RATA_TWO_PI * out->nyquist_freq_hz * t);
        } else {
            out->x_alias_exact[i] =
                cfg->amp * cos(RATA_TWO_PI * out->aliased_freq_hz * t + out->aliased_phase_rad);
        }

        double recon = 0.0;
        double u = cfg->fs_hz * t;
        for (int32_t k = -pad; k < (int32_t)ns + pad; ++k) {
            double xk;
            if (k >= 0 && (size_t)k < ns) {
                xk = out->x_samples[k];
            } else {
                double tk = (double)k / cfg->fs_hz;
                xk = cfg->amp * cos(RATA_TWO_PI * cfg->f0_hz * tk + cfg->phase_rad);
            }
            recon += xk * norm_sinc(u - (double)k);
        }
        out->x_recon_sinc[i] = recon;
    }

    return RATA_OK;
}

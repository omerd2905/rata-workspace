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

RataStatus dsp_alias_frequency(double f0_hz,
                               double fs_hz,
                               double *aliased_hz_out,
                               bool *is_aliased_out,
                               bool *phase_inverted_out) {
    if (!isfinite(f0_hz) || !isfinite(fs_hz) || f0_hz < 0.0 || fs_hz <= 0.0 ||
        aliased_hz_out == NULL || is_aliased_out == NULL || phase_inverted_out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }

    double nyq = 0.5 * fs_hz;
    *is_aliased_out = (f0_hz > nyq + 1.0e-12);

    double rem = fmod(f0_hz, fs_hz);
    if (rem < 0.0) {
        rem += fs_hz;
    }

    if (rem > nyq) {
        *aliased_hz_out = fs_hz - rem;
        *phase_inverted_out = true;
    } else {
        *aliased_hz_out = rem;
        *phase_inverted_out = false;
    }
    return RATA_OK;
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
    RataStatus st = dsp_alias_frequency(cfg->f0_hz,
                                        cfg->fs_hz,
                                        &out->aliased_freq_hz,
                                        &out->is_aliased,
                                        &out->phase_inverted);
    if (st != RATA_OK) {
        return st;
    }
    out->nyquist_freq_hz = 0.5 * cfg->fs_hz;
    out->nyquist_rate_hz = 2.0 * cfg->f0_hz;

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
        out->x_samples[n] = cfg->amp * cos(RATA_TWO_PI * cfg->f0_hz * tn + cfg->phase_rad);
    }

    /* For cos(2*pi*f0*t + phi): if folded into odd Nyquist zone, apparent cosine has -phi. */
    double alias_phase = out->phase_inverted ? -cfg->phase_rad : cfg->phase_rad;

    /* Use extra padding samples on both sides for clean Whittaker-Shannon sinc interpolation
     * inside [0, duration_s] without severe boundary Gibbs distortion. */
    const int32_t pad = 32;
    for (size_t i = 0u; i < nc; ++i) {
        double t = cfg->duration_s * (double)i / (double)(nc - 1u);
        out->t_cont[i] = t;
        out->x_analog[i] = cfg->amp * cos(RATA_TWO_PI * cfg->f0_hz * t + cfg->phase_rad);
        out->x_alias_exact[i] =
            cfg->amp * cos(RATA_TWO_PI * out->aliased_freq_hz * t + alias_phase);

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

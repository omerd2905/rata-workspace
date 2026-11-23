#include "engines/dsp_lti.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "core/poly.h"

RataStatus dsp_lti_init(RataLti *sys,
                        const double *b, size_t nb,
                        const double *a, size_t na,
                        double fs) {
    if (sys == NULL || b == NULL || a == NULL ||
        nb == 0u || na == 0u ||
        nb > RATA_LTI_MAX_COEFFS || na > RATA_LTI_MAX_COEFFS ||
        !isfinite(fs) || fs <= 0.0) {
        return RATA_ERR_INVALID_ARG;
    }
    for (size_t i = 0u; i < nb; ++i) {
        if (!isfinite(b[i])) {
            return RATA_ERR_INVALID_ARG;
        }
    }
    for (size_t i = 0u; i < na; ++i) {
        if (!isfinite(a[i])) {
            return RATA_ERR_INVALID_ARG;
        }
    }
    if (fabs(a[0]) < 1.0e-15) {
        return RATA_ERR_DIV_ZERO;
    }

    memset(sys, 0, sizeof(*sys));
    memcpy(sys->b, b, nb * sizeof(double));
    memcpy(sys->a, a, na * sizeof(double));
    sys->nb = nb;
    sys->na = na;
    sys->fs = fs;
    return RATA_OK;
}

RataStatus dsp_parse_coeff_list(const char *text, double *out, size_t cap, size_t *count_out) {
    if (text == NULL || out == NULL || cap == 0u || count_out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t count = 0u;
    const char *p = text;
    while (*p != '\0') {
        while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',' || *p == ';' ||
                              *p == '[' || *p == ']')) {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        if (count >= cap) {
            return RATA_ERR_RANGE;
        }
        char *end = NULL;
        double val = strtod(p, &end);
        if (end == p || !isfinite(val)) {
            return RATA_ERR_PARSE;
        }
        out[count++] = val;
        p = end;
        if (*p != '\0' && !isspace((unsigned char)*p) && *p != ',' && *p != ';' && *p != ']') {
            return RATA_ERR_PARSE;
        }
    }
    if (count == 0u) {
        return RATA_ERR_PARSE;
    }
    *count_out = count;
    return RATA_OK;
}

static size_t trimmed_len(const double *c, size_t n) {
    while (n > 1u && fabs(c[n - 1u]) == 0.0) {
        n--;
    }
    return n;
}

RataStatus dsp_lti_poles_zeros(const RataLti *sys, RataPoleZero *pz_out, RataArena *scratch) {
    if (sys == NULL || pz_out == NULL || scratch == NULL ||
        sys->nb == 0u || sys->na == 0u || fabs(sys->a[0]) < 1.0e-15) {
        return RATA_ERR_INVALID_ARG;
    }

    memset(pz_out, 0, sizeof(*pz_out));

    size_t nb_eff = trimmed_len(sys->b, sys->nb);
    size_t na_eff = trimmed_len(sys->a, sys->na);
    size_t deg_b = nb_eff - 1u;
    size_t deg_a = na_eff - 1u;
    size_t L = (deg_b > deg_a) ? deg_b : deg_a;

    /* Gain = first nonzero b_k / a_0. */
    double first_nz_b = 0.0;
    for (size_t i = 0u; i < nb_eff; ++i) {
        if (fabs(sys->b[i]) > 0.0) {
            first_nz_b = sys->b[i];
            break;
        }
    }
    pz_out->gain = first_nz_b / sys->a[0];

    /* DC gain H(1) = sum(b) / sum(a). */
    double sum_b = 0.0;
    double sum_a = 0.0;
    for (size_t i = 0u; i < nb_eff; ++i) {
        sum_b += sys->b[i];
    }
    for (size_t i = 0u; i < na_eff; ++i) {
        sum_a += sys->a[i];
    }
    if (fabs(sum_a) > 1.0e-12) {
        pz_out->dc_gain = sum_b / sum_a;
        pz_out->dc_gain_finite = isfinite(pz_out->dc_gain);
    } else {
        pz_out->dc_gain = 0.0;
        pz_out->dc_gain_finite = false;
    }

    /* Build polynomials in positive powers of z:
     * N(z) = sum_{k=0..deg_b} b[k] * z^(L - k)
     * D(z) = sum_{k=0..deg_a} a[k] * z^(L - k) */
    size_t mark = rata_arena_mark(scratch);
    RataPoly num_p, den_p;
    RataStatus st = poly_alloc(&num_p, L + 1u, scratch);
    if (st != RATA_OK) {
        rata_arena_restore(scratch, mark);
        return st;
    }
    st = poly_alloc(&den_p, L + 1u, scratch);
    if (st != RATA_OK) {
        rata_arena_restore(scratch, mark);
        return st;
    }

    for (size_t k = 0u; k <= deg_b; ++k) {
        num_p.coeffs[L - k] = sys->b[k];
    }
    num_p.deg = L;
    poly_trim(&num_p, 0.0);

    for (size_t k = 0u; k <= deg_a; ++k) {
        den_p.coeffs[L - k] = sys->a[k];
    }
    den_p.deg = L;
    poly_trim(&den_p, 0.0);

    st = poly_roots(&num_p, pz_out->zeros, RATA_LTI_MAX_COEFFS, &pz_out->num_zeros, scratch);
    if (st != RATA_OK) {
        rata_arena_restore(scratch, mark);
        return st;
    }
    st = poly_roots(&den_p, pz_out->poles, RATA_LTI_MAX_COEFFS, &pz_out->num_poles, scratch);
    rata_arena_restore(scratch, mark);
    if (st != RATA_OK) {
        return st;
    }

    /* Classify BIBO stability from pole radii and unit-circle multiplicity. */
    double max_mag = 0.0;
    bool has_unit_pole = false;
    bool has_repeated_unit_pole = false;
    bool has_outside_pole = false;
    const double unit_tol = 1.0e-7;

    for (size_t i = 0u; i < pz_out->num_poles; ++i) {
        double m = cplx_abs(pz_out->poles[i]);
        if (m > max_mag) {
            max_mag = m;
        }
        if (m > 1.0 + unit_tol) {
            has_outside_pole = true;
        } else if (fabs(m - 1.0) <= unit_tol) {
            has_unit_pole = true;
            for (size_t j = i + 1u; j < pz_out->num_poles; ++j) {
                if (cplx_abs(cplx_sub(pz_out->poles[i], pz_out->poles[j])) < 1.0e-5) {
                    has_repeated_unit_pole = true;
                }
            }
        }
    }

    pz_out->max_pole_mag = max_mag;
    if (has_outside_pole || has_repeated_unit_pole) {
        pz_out->stability = LTI_UNSTABLE;
    } else if (has_unit_pole) {
        pz_out->stability = LTI_MARGINAL;
    } else {
        pz_out->stability = LTI_STABLE;
    }
    return RATA_OK;
}

static void eval_zinv_poly_and_weighted(const double *c,
                                        size_t n,
                                        cplx zinv,
                                        cplx *val_out,
                                        cplx *weighted_out) {
    /* Evaluates P(z^-1) = sum_{k=0..n-1} c[k] z^-k and W(z^-1) = sum_{k=0..n-1} k*c[k] z^-k. */
    cplx p_acc = cplx_make(c[n - 1u], 0.0);
    cplx w_acc = cplx_make((double)(n - 1u) * c[n - 1u], 0.0);
    for (size_t i = n - 1u; i > 0u; --i) {
        size_t k = i - 1u;
        p_acc = cplx_add(cplx_mul(p_acc, zinv), cplx_make(c[k], 0.0));
        w_acc = cplx_add(cplx_mul(w_acc, zinv), cplx_make((double)k * c[k], 0.0));
    }
    *val_out = p_acc;
    if (weighted_out != NULL) {
        *weighted_out = w_acc;
    }
}

RataStatus dsp_lti_eval_freq(const RataLti *sys, double omega_rad, cplx *h_out) {
    if (sys == NULL || h_out == NULL || sys->nb == 0u || sys->na == 0u ||
        !isfinite(omega_rad)) {
        return RATA_ERR_INVALID_ARG;
    }
    cplx zinv = cplx_make(cos(omega_rad), -sin(omega_rad));
    cplx num, den;
    eval_zinv_poly_and_weighted(sys->b, sys->nb, zinv, &num, NULL);
    eval_zinv_poly_and_weighted(sys->a, sys->na, zinv, &den, NULL);
    if (cplx_abs(den) < 1.0e-14) {
        *h_out = cplx_zero();
        return RATA_ERR_DIV_ZERO;
    }
    return cplx_div_safe(num, den, h_out);
}

RataStatus dsp_lti_freqz(const RataLti *sys,
                         size_t num_points,
                         bool full_circle,
                         double floor_db,
                         RataFreqz *out,
                         RataArena *arena) {
    if (sys == NULL || out == NULL || arena == NULL ||
        sys->nb == 0u || sys->na == 0u || num_points < 2u || num_points > 8192u) {
        return RATA_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->omega_rad = (double *)rata_arena_alloc(arena, num_points * sizeof(double), 8u);
    out->freq_hz = (double *)rata_arena_alloc(arena, num_points * sizeof(double), 8u);
    out->H = (cplx *)rata_arena_alloc(arena, num_points * sizeof(cplx), 8u);
    out->mag = (double *)rata_arena_alloc(arena, num_points * sizeof(double), 8u);
    out->mag_db = (double *)rata_arena_alloc(arena, num_points * sizeof(double), 8u);
    out->phase_rad = (double *)rata_arena_alloc(arena, num_points * sizeof(double), 8u);
    out->phase_unwrapped = (double *)rata_arena_alloc(arena, num_points * sizeof(double), 8u);
    out->group_delay = (double *)rata_arena_alloc(arena, num_points * sizeof(double), 8u);
    if (out->omega_rad == NULL || out->freq_hz == NULL || out->H == NULL ||
        out->mag == NULL || out->mag_db == NULL || out->phase_rad == NULL ||
        out->phase_unwrapped == NULL || out->group_delay == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }

    out->len = num_points;
    out->has_pole_on_grid = false;

    const double pole_cap_mag = 1.0e6;
    const double pole_cap_db = 120.0;

    for (size_t k = 0u; k < num_points; ++k) {
        double w;
        if (full_circle) {
            w = RATA_TWO_PI * (double)k / (double)num_points;
        } else {
            w = RATA_PI * (double)k / (double)(num_points - 1u);
        }
        out->omega_rad[k] = w;
        out->freq_hz[k] = w * sys->fs / RATA_TWO_PI;

        cplx zinv = cplx_make(cos(w), -sin(w));
        cplx num, num_w, den, den_w;
        eval_zinv_poly_and_weighted(sys->b, sys->nb, zinv, &num, &num_w);
        eval_zinv_poly_and_weighted(sys->a, sys->na, zinv, &den, &den_w);

        double den_abs = cplx_abs(den);
        if (den_abs < 1.0e-13) {
            out->has_pole_on_grid = true;
            out->H[k] = cplx_make(pole_cap_mag, 0.0);
            out->mag[k] = pole_cap_mag;
            out->mag_db[k] = pole_cap_db;
            out->phase_rad[k] = 0.0;
            out->group_delay[k] = 0.0;
            continue;
        }

        cplx h = cplx_div(num, den);
        double m = cplx_abs(h);
        if (!isfinite(m) || m >= pole_cap_mag) {
            out->has_pole_on_grid = true;
            out->H[k] = cplx_make(pole_cap_mag, 0.0);
            out->mag[k] = pole_cap_mag;
            out->mag_db[k] = pole_cap_db;
            out->phase_rad[k] = isfinite(cplx_arg(h)) ? cplx_arg(h) : 0.0;
            out->group_delay[k] = 0.0;
            continue;
        }
        out->H[k] = h;
        out->mag[k] = m;
        if (m <= 0.0) {
            out->mag_db[k] = floor_db;
        } else {
            double db = 20.0 * log10(m);
            out->mag_db[k] = (db < floor_db) ? floor_db : db;
        }
        out->phase_rad[k] = (m < 1.0e-12) ? 0.0 : cplx_arg(h);

        /* Analytic group delay: Re(num_w / num) - Re(den_w / den). */
        double gd = 0.0;
        if (cplx_abs(num) > 1.0e-13) {
            gd += cplx_div(num_w, num).re;
        }
        gd -= cplx_div(den_w, den).re;
        out->group_delay[k] = isfinite(gd) ? gd : 0.0;
    }

    /* Unwrap phase cleanly. */
    out->phase_unwrapped[0] = out->phase_rad[0];
    double offset = 0.0;
    for (size_t k = 1u; k < num_points; ++k) {
        double dp = out->phase_rad[k] - out->phase_rad[k - 1u];
        while (dp > RATA_PI) {
            offset -= RATA_TWO_PI;
            dp -= RATA_TWO_PI;
        }
        while (dp < -RATA_PI) {
            offset += RATA_TWO_PI;
            dp += RATA_TWO_PI;
        }
        out->phase_unwrapped[k] = out->phase_rad[k] + offset;
    }

    return RATA_OK;
}

RataStatus dsp_lti_filter(const RataLti *sys,
                          const RataSignal *x,
                          RataSignal *y_out,
                          size_t *diverged_at,
                          RataArena *arena) {
    if (sys == NULL || x == NULL || x->samples == NULL || x->len == 0u ||
        y_out == NULL || arena == NULL || sys->nb == 0u || sys->na == 0u ||
        fabs(sys->a[0]) < 1.0e-15) {
        return RATA_ERR_INVALID_ARG;
    }
    if (diverged_at != NULL) {
        *diverged_at = x->len;
    }

    RataStatus st = dsp_signal_alloc(y_out, x->len, x->n0, x->fs, arena);
    if (st != RATA_OK) {
        return st;
    }

    size_t order = (sys->nb > sys->na) ? sys->nb : sys->na;
    double bn[RATA_LTI_MAX_COEFFS];
    double an[RATA_LTI_MAX_COEFFS];
    double state[RATA_LTI_MAX_COEFFS];
    memset(bn, 0, sizeof(bn));
    memset(an, 0, sizeof(an));
    memset(state, 0, sizeof(state));

    double inv_a0 = 1.0 / sys->a[0];
    for (size_t i = 0u; i < sys->nb; ++i) {
        bn[i] = sys->b[i] * inv_a0;
    }
    for (size_t i = 0u; i < sys->na; ++i) {
        an[i] = sys->a[i] * inv_a0;
    }

    const double diverge_limit = 1.0e15;
    for (size_t n = 0u; n < x->len; ++n) {
        double xn = x->samples[n];
        double yn = bn[0] * xn + ((order > 1u) ? state[0] : 0.0);
        if (!isfinite(yn) || fabs(yn) > diverge_limit) {
            if (diverged_at != NULL) {
                *diverged_at = n;
            }
            for (size_t k = n; k < x->len; ++k) {
                y_out->samples[k] = 0.0;
            }
            return RATA_ERR_UNSTABLE;
        }
        y_out->samples[n] = yn;
        for (size_t i = 1u; i < order; ++i) {
            double next_s = (i + 1u < order) ? state[i] : 0.0;
            state[i - 1u] = bn[i] * xn - an[i] * yn + next_s;
        }
    }
    return RATA_OK;
}

RataStatus dsp_lti_impulse_response(const RataLti *sys,
                                    size_t len,
                                    RataSignal *h_out,
                                    size_t *diverged_at,
                                    RataArena *arena) {
    if (sys == NULL || h_out == NULL || arena == NULL || len == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    RataSignal imp;
    size_t mark = rata_arena_mark(arena);
    RataStatus st = dsp_signal_impulse(0, len, sys->fs, 1.0, 0, &imp, arena);
    if (st != RATA_OK) {
        rata_arena_restore(arena, mark);
        return st;
    }
    /* Allocate output after impulse, or keep output in arena? Keep output before scratch! */
    rata_arena_restore(arena, mark);

    st = dsp_signal_alloc(h_out, len, 0, sys->fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    size_t tmp_mark = rata_arena_mark(arena);
    RataSignal tmp_imp;
    st = dsp_signal_impulse(0, len, sys->fs, 1.0, 0, &tmp_imp, arena);
    if (st != RATA_OK) {
        rata_arena_restore(arena, tmp_mark);
        return st;
    }
    RataSignal tmp_y;
    RataStatus fst = dsp_lti_filter(sys, &tmp_imp, &tmp_y, diverged_at, arena);
    if (fst == RATA_OK || fst == RATA_ERR_UNSTABLE) {
        memcpy(h_out->samples, tmp_y.samples, len * sizeof(double));
    }
    rata_arena_restore(arena, tmp_mark);
    return fst;
}

RataStatus dsp_lti_step_response(const RataLti *sys,
                                 size_t len,
                                 RataSignal *s_out,
                                 size_t *diverged_at,
                                 RataArena *arena) {
    if (sys == NULL || s_out == NULL || arena == NULL || len == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(s_out, len, 0, sys->fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    size_t tmp_mark = rata_arena_mark(arena);
    RataSignal tmp_u;
    st = dsp_signal_step(0, len, sys->fs, 1.0, 0, &tmp_u, arena);
    if (st != RATA_OK) {
        rata_arena_restore(arena, tmp_mark);
        return st;
    }
    RataSignal tmp_y;
    RataStatus fst = dsp_lti_filter(sys, &tmp_u, &tmp_y, diverged_at, arena);
    if (fst == RATA_OK || fst == RATA_ERR_UNSTABLE) {
        memcpy(s_out->samples, tmp_y.samples, len * sizeof(double));
    }
    rata_arena_restore(arena, tmp_mark);
    return fst;
}

const char *dsp_stability_str(RataStability s) {
    switch (s) {
        case LTI_STABLE:   return "BIBO Stable (all |p| < 1)";
        case LTI_MARGINAL: return "Marginally Stable (simple pole on |z| = 1)";
        case LTI_UNSTABLE: return "UNSTABLE (pole outside |z| = 1 or repeated on |z| = 1)";
        default:           return "Unknown";
    }
}

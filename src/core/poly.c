#include "core/poly.h"

#include <math.h>
#include <string.h>

RataStatus poly_init(RataPoly *p, double *buf, size_t cap, const double *asc_coeffs, size_t len) {
    if (p == NULL || buf == NULL || cap == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    if (len > cap) {
        return RATA_ERR_RANGE;
    }
    p->coeffs = buf;
    p->cap = cap;
    if (len == 0u || asc_coeffs == NULL) {
        p->coeffs[0] = 0.0;
        p->deg = 0u;
    } else {
        memcpy(p->coeffs, asc_coeffs, len * sizeof(double));
        p->deg = len - 1u;
    }
    poly_trim(p, 0.0);
    return RATA_OK;
}

RataStatus poly_alloc(RataPoly *p, size_t cap, RataArena *arena) {
    if (p == NULL || cap == 0u || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    double *buf = (double *)rata_arena_calloc(arena, cap, sizeof(double), 8u);
    if (buf == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }
    p->coeffs = buf;
    p->deg = 0u;
    p->cap = cap;
    return RATA_OK;
}

RataStatus poly_from_descending(RataPoly *out, const double *desc, size_t len, RataArena *arena) {
    if (out == NULL || desc == NULL || len == 0u || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = poly_alloc(out, len, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i < len; ++i) {
        out->coeffs[i] = desc[len - 1u - i];
    }
    out->deg = len - 1u;
    poly_trim(out, 0.0);
    return RATA_OK;
}

RataStatus poly_copy(RataPoly *dst, const RataPoly *src, RataArena *arena) {
    if (dst == NULL || src == NULL || src->coeffs == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t len = src->deg + 1u;
    RataStatus st = poly_alloc(dst, len, arena);
    if (st != RATA_OK) {
        return st;
    }
    memcpy(dst->coeffs, src->coeffs, len * sizeof(double));
    dst->deg = src->deg;
    return RATA_OK;
}

void poly_trim(RataPoly *p, double tol) {
    if (p == NULL || p->coeffs == NULL) {
        return;
    }
    while (p->deg > 0u && fabs(p->coeffs[p->deg]) <= tol) {
        p->deg--;
    }
    if (p->deg == 0u && fabs(p->coeffs[0]) <= tol) {
        p->coeffs[0] = 0.0;
    }
}

bool poly_is_zero(const RataPoly *p, double tol) {
    if (p == NULL || p->coeffs == NULL) {
        return true;
    }
    for (size_t i = 0u; i <= p->deg; ++i) {
        if (fabs(p->coeffs[i]) > tol) {
            return false;
        }
    }
    return true;
}

double poly_eval_real(const RataPoly *p, double x) {
    if (p == NULL || p->coeffs == NULL) {
        return 0.0;
    }
    double acc = p->coeffs[p->deg];
    for (size_t i = p->deg; i > 0u; --i) {
        acc = acc * x + p->coeffs[i - 1u];
    }
    return acc;
}

cplx poly_eval_cplx(const RataPoly *p, cplx z) {
    if (p == NULL || p->coeffs == NULL) {
        return cplx_zero();
    }
    cplx acc = cplx_make(p->coeffs[p->deg], 0.0);
    for (size_t i = p->deg; i > 0u; --i) {
        acc = cplx_add(cplx_mul(acc, z), cplx_make(p->coeffs[i - 1u], 0.0));
    }
    return acc;
}

void poly_eval_cplx_deriv(const RataPoly *p, cplx z, cplx *val_out, cplx *deriv_out) {
    if (p == NULL || p->coeffs == NULL) {
        if (val_out != NULL) {
            *val_out = cplx_zero();
        }
        if (deriv_out != NULL) {
            *deriv_out = cplx_zero();
        }
        return;
    }
    cplx val = cplx_make(p->coeffs[p->deg], 0.0);
    cplx der = cplx_zero();
    for (size_t i = p->deg; i > 0u; --i) {
        der = cplx_add(cplx_mul(der, z), val);
        val = cplx_add(cplx_mul(val, z), cplx_make(p->coeffs[i - 1u], 0.0));
    }
    if (val_out != NULL) {
        *val_out = val;
    }
    if (deriv_out != NULL) {
        *deriv_out = der;
    }
}

RataStatus poly_deriv(const RataPoly *p, RataPoly *out, RataArena *arena) {
    if (p == NULL || p->coeffs == NULL || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (p->deg == 0u) {
        RataStatus st = poly_alloc(out, 1u, arena);
        if (st != RATA_OK) {
            return st;
        }
        out->coeffs[0] = 0.0;
        out->deg = 0u;
        return RATA_OK;
    }
    RataStatus st = poly_alloc(out, p->deg, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 1u; i <= p->deg; ++i) {
        out->coeffs[i - 1u] = p->coeffs[i] * (double)i;
    }
    out->deg = p->deg - 1u;
    poly_trim(out, 0.0);
    return RATA_OK;
}

RataStatus poly_add(const RataPoly *a, const RataPoly *b, RataPoly *out, RataArena *arena) {
    if (a == NULL || b == NULL || a->coeffs == NULL || b->coeffs == NULL ||
        out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t max_deg = (a->deg > b->deg) ? a->deg : b->deg;
    RataStatus st = poly_alloc(out, max_deg + 1u, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i <= max_deg; ++i) {
        double av = (i <= a->deg) ? a->coeffs[i] : 0.0;
        double bv = (i <= b->deg) ? b->coeffs[i] : 0.0;
        out->coeffs[i] = av + bv;
    }
    out->deg = max_deg;
    poly_trim(out, 0.0);
    return RATA_OK;
}

RataStatus poly_sub(const RataPoly *a, const RataPoly *b, RataPoly *out, RataArena *arena) {
    if (a == NULL || b == NULL || a->coeffs == NULL || b->coeffs == NULL ||
        out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t max_deg = (a->deg > b->deg) ? a->deg : b->deg;
    RataStatus st = poly_alloc(out, max_deg + 1u, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i <= max_deg; ++i) {
        double av = (i <= a->deg) ? a->coeffs[i] : 0.0;
        double bv = (i <= b->deg) ? b->coeffs[i] : 0.0;
        out->coeffs[i] = av - bv;
    }
    out->deg = max_deg;
    poly_trim(out, 0.0);
    return RATA_OK;
}

RataStatus poly_scale(const RataPoly *a, double s, RataPoly *out, RataArena *arena) {
    if (a == NULL || a->coeffs == NULL || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = poly_alloc(out, a->deg + 1u, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i <= a->deg; ++i) {
        out->coeffs[i] = a->coeffs[i] * s;
    }
    out->deg = a->deg;
    poly_trim(out, 0.0);
    return RATA_OK;
}

RataStatus poly_mul(const RataPoly *a, const RataPoly *b, RataPoly *out, RataArena *arena) {
    if (a == NULL || b == NULL || a->coeffs == NULL || b->coeffs == NULL ||
        out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t out_deg = a->deg + b->deg;
    RataStatus st = poly_alloc(out, out_deg + 1u, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i <= a->deg; ++i) {
        for (size_t j = 0u; j <= b->deg; ++j) {
            out->coeffs[i + j] += a->coeffs[i] * b->coeffs[j];
        }
    }
    out->deg = out_deg;
    poly_trim(out, 0.0);
    return RATA_OK;
}

RataStatus poly_divmod(const RataPoly *a,
                       const RataPoly *b,
                       RataPoly *q_out,
                       RataPoly *r_out,
                       RataArena *arena) {
    if (a == NULL || b == NULL || a->coeffs == NULL || b->coeffs == NULL ||
        q_out == NULL || r_out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (poly_is_zero(b, 0.0)) {
        return RATA_ERR_DIV_ZERO;
    }
    RataPoly rem;
    RataStatus st = poly_copy(&rem, a, arena);
    if (st != RATA_OK) {
        return st;
    }
    poly_trim(&rem, 0.0);

    size_t b_deg = b->deg;
    while (b_deg > 0u && b->coeffs[b_deg] == 0.0) {
        b_deg--;
    }
    double b_lead = b->coeffs[b_deg];
    if (b_lead == 0.0) {
        return RATA_ERR_DIV_ZERO;
    }

    if (rem.deg < b_deg) {
        st = poly_alloc(q_out, 1u, arena);
        if (st != RATA_OK) {
            return st;
        }
        q_out->coeffs[0] = 0.0;
        q_out->deg = 0u;
        *r_out = rem;
        return RATA_OK;
    }

    size_t q_deg = rem.deg - b_deg;
    st = poly_alloc(q_out, q_deg + 1u, arena);
    if (st != RATA_OK) {
        return st;
    }
    q_out->deg = q_deg;

    for (size_t k = q_deg + 1u; k > 0u; --k) {
        size_t idx = k - 1u;
        double coeff = rem.coeffs[idx + b_deg] / b_lead;
        q_out->coeffs[idx] = coeff;
        for (size_t j = 0u; j <= b_deg; ++j) {
            rem.coeffs[idx + j] -= coeff * b->coeffs[j];
        }
    }

    poly_trim(q_out, 0.0);
    poly_trim(&rem, 1.0e-14);
    *r_out = rem;
    return RATA_OK;
}

RataStatus poly_from_roots_cplx(const cplx *roots,
                                size_t count,
                                double gain,
                                RataPoly *out,
                                RataArena *arena) {
    if (out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (count > 0u && roots == NULL) {
        return RATA_ERR_INVALID_ARG;
    }

    RataStatus st = poly_alloc(out, count + 1u, arena);
    if (st != RATA_OK) {
        return st;
    }
    if (count == 0u) {
        out->coeffs[0] = gain;
        out->deg = 0u;
        return RATA_OK;
    }

    size_t mark = rata_arena_mark(arena);
    cplx *c = (cplx *)rata_arena_calloc(arena, count + 1u, sizeof(cplx), 8u);
    if (c == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }
    c[0] = cplx_make(1.0, 0.0);

    for (size_t i = 0u; i < count; ++i) {
        cplx neg_r = cplx_neg(roots[i]);
        for (size_t j = i + 1u; j > 0u; --j) {
            c[j] = cplx_add(c[j - 1u], cplx_mul(c[j], neg_r));
        }
        c[0] = cplx_mul(c[0], neg_r);
    }

    double max_abs = 0.0;
    for (size_t i = 0u; i <= count; ++i) {
        double a = fabs(c[i].re);
        if (a > max_abs) {
            max_abs = a;
        }
    }
    double imag_tol = 1.0e-6 * ((max_abs > 1.0) ? max_abs : 1.0);
    for (size_t i = 0u; i <= count; ++i) {
        if (fabs(c[i].im) > imag_tol) {
            rata_arena_restore(arena, mark);
            return RATA_ERR_INVALID_ARG;
        }
        out->coeffs[i] = c[i].re * gain;
    }
    rata_arena_restore(arena, mark);

    out->deg = count;
    poly_trim(out, 0.0);
    return RATA_OK;
}

/* Overflow-safe Newton ratio P(z)/P'(z) and running error bound for monic polynomial. */
static void eval_newton_step_monic(const double *c,
                                   size_t n,
                                   cplx z,
                                   cplx *newton_out,
                                   double *val_abs_out,
                                   double *err_bound_out) {
    double r = cplx_abs(z);
    if (r <= 1.0) {
        cplx val = cplx_make(c[n], 0.0);
        cplx der = cplx_zero();
        double bound = fabs(c[n]);
        for (size_t i = n; i > 0u; --i) {
            der = cplx_add(cplx_mul(der, z), val);
            val = cplx_add(cplx_mul(val, z), cplx_make(c[i - 1u], 0.0));
            bound = bound * r + fabs(c[i - 1u]);
        }
        *val_abs_out = cplx_abs(val);
        *err_bound_out = bound;
        if (cplx_abs(der) > 0.0) {
            *newton_out = cplx_div(val, der);
        } else {
            *newton_out = cplx_zero();
        }
        return;
    }

    /* For |z| > 1, evaluate reversed polynomial Q(w) = w^n * P(1/w), w = 1/z. */
    cplx w = cplx_div(cplx_make(1.0, 0.0), z);
    double rw = cplx_abs(w);
    cplx qval = cplx_make(c[0], 0.0);
    cplx qder = cplx_zero();
    double qbound = fabs(c[0]);
    for (size_t i = 1u; i <= n; ++i) {
        qder = cplx_add(cplx_mul(qder, w), qval);
        qval = cplx_add(cplx_mul(qval, w), cplx_make(c[i], 0.0));
        qbound = qbound * rw + fabs(c[i]);
    }
    *val_abs_out = cplx_abs(qval);
    *err_bound_out = qbound;

    /* P(z) / P'(z) = Q(w) / (w * (n * Q(w) - w * Q'(w))) */
    cplx denom_inner = cplx_sub(cplx_scale(qval, (double)n), cplx_mul(w, qder));
    cplx denom = cplx_mul(w, denom_inner);
    if (cplx_abs(denom) > 0.0) {
        *newton_out = cplx_div(qval, denom);
    } else {
        *newton_out = cplx_zero();
    }
}

static void sort_roots_deterministic(cplx *roots, size_t count) {
    /* Simple insertion sort: descending real part, then descending imaginary part. */
    const double re_tie_tol = 1.0e-9;
    for (size_t i = 1u; i < count; ++i) {
        cplx key = roots[i];
        size_t j = i;
        while (j > 0u) {
            cplx prev = roots[j - 1u];
            bool swap = false;
            if (fabs(key.re - prev.re) > re_tie_tol) {
                swap = (key.re > prev.re);
            } else {
                swap = (key.im > prev.im);
            }
            if (!swap) {
                break;
            }
            roots[j] = prev;
            j--;
        }
        roots[j] = key;
    }
}

static void solve_quadratic(double c0, double c1, double c2, cplx *r0, cplx *r1) {
    /* Solve c2*z^2 + c1*z + c0 = 0 avoiding catastrophic cancellation. */
    double b = c1 / c2;
    double c = c0 / c2;
    double disc = b * b - 4.0 * c;
    if (disc >= 0.0) {
        double s = sqrt(disc);
        double q = -0.5 * (b + ((b >= 0.0) ? s : -s));
        if (fabs(q) > 0.0) {
            *r0 = cplx_make(q, 0.0);
            *r1 = cplx_make(c / q, 0.0);
        } else {
            *r0 = cplx_zero();
            *r1 = cplx_make(-b, 0.0);
        }
    } else {
        double re = -0.5 * b;
        double im = 0.5 * sqrt(-disc);
        *r0 = cplx_make(re, im);
        *r1 = cplx_make(re, -im);
    }
}

RataStatus poly_roots(const RataPoly *p,
                      cplx *roots_out,
                      size_t roots_cap,
                      size_t *count_out,
                      RataArena *scratch) {
    if (p == NULL || p->coeffs == NULL || count_out == NULL || scratch == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t deg = p->deg;
    while (deg > 0u && fabs(p->coeffs[deg]) == 0.0) {
        deg--;
    }
    if (deg == 0u) {
        *count_out = 0u;
        return RATA_OK;
    }
    if (roots_out == NULL || roots_cap < deg) {
        return RATA_ERR_RANGE;
    }
    for (size_t i = 0u; i <= deg; ++i) {
        if (!isfinite(p->coeffs[i])) {
            return RATA_ERR_INVALID_ARG;
        }
    }

    /* Deflate exact zero roots first. */
    size_t z_zeros = 0u;
    while (z_zeros < deg && p->coeffs[z_zeros] == 0.0) {
        roots_out[z_zeros] = cplx_zero();
        z_zeros++;
    }
    size_t n = deg - z_zeros;
    cplx *rem_roots = roots_out + z_zeros;

    if (n == 0u) {
        *count_out = deg;
        sort_roots_deterministic(roots_out, deg);
        return RATA_OK;
    }

    if (n == 1u) {
        rem_roots[0] = cplx_make(-p->coeffs[z_zeros] / p->coeffs[z_zeros + 1u], 0.0);
        *count_out = deg;
        sort_roots_deterministic(roots_out, deg);
        return RATA_OK;
    }

    if (n == 2u) {
        solve_quadratic(p->coeffs[z_zeros],
                        p->coeffs[z_zeros + 1u],
                        p->coeffs[z_zeros + 2u],
                        &rem_roots[0],
                        &rem_roots[1]);
        *count_out = deg;
        sort_roots_deterministic(roots_out, deg);
        return RATA_OK;
    }

    /* Degree >= 3: normalize deflated polynomial to monic and run Aberth-Ehrlich. */
    size_t mark = rata_arena_mark(scratch);
    double *c = (double *)rata_arena_alloc(scratch, (n + 1u) * sizeof(double), 8u);
    bool *done = (bool *)rata_arena_calloc(scratch, n, sizeof(bool), 1u);
    if (c == NULL || done == NULL) {
        rata_arena_restore(scratch, mark);
        return RATA_ERR_OUT_OF_MEMORY;
    }

    double lead = p->coeffs[z_zeros + n];
    for (size_t i = 0u; i <= n; ++i) {
        c[i] = p->coeffs[z_zeros + i] / lead;
    }
    c[n] = 1.0;

    /* Initial radius from geometric mean |c[0]|^(1/n) clamped within Cauchy bound. */
    double r0 = pow(fabs(c[0]), 1.0 / (double)n);
    if (!isfinite(r0) || r0 < 1.0e-3) {
        r0 = 1.0;
    }
    double max_coeff = 0.0;
    for (size_t i = 0u; i < n; ++i) {
        double a = fabs(c[i]);
        if (a > max_coeff) {
            max_coeff = a;
        }
    }
    double cauchy = 1.0 + max_coeff;
    if (r0 > cauchy) {
        r0 = cauchy;
    }

    /* Place starting points on slightly perturbed circle to break symmetries. */
    const double angle_offset = 0.1234567;
    for (size_t k = 0u; k < n; ++k) {
        double theta = (RATA_TWO_PI * (double)k) / (double)n + angle_offset;
        double rad = r0 * (1.0 + 0.07 * sin((double)(k + 1u) * 1.7));
        rem_roots[k] = cplx_from_polar(rad, theta);
    }

    const size_t max_iter = 500u;
    bool converged = false;
    for (size_t iter = 0u; iter < max_iter; ++iter) {
        bool all_done = true;
        for (size_t k = 0u; k < n; ++k) {
            if (done[k]) {
                continue;
            }
            cplx newton;
            double val_abs = 0.0;
            double err_bound = 0.0;
            eval_newton_step_monic(c, n, rem_roots[k], &newton, &val_abs, &err_bound);

            if (val_abs <= 1.0e-14 * err_bound) {
                done[k] = true;
                continue;
            }

            /* Aberth sum: S = sum_{j != k} 1 / (z_k - z_j). */
            cplx sum = cplx_zero();
            for (size_t j = 0u; j < n; ++j) {
                if (j == k) {
                    continue;
                }
                cplx diff = cplx_sub(rem_roots[k], rem_roots[j]);
                if (cplx_abs(diff) > 1.0e-15) {
                    sum = cplx_add(sum, cplx_div(cplx_make(1.0, 0.0), diff));
                }
            }

            cplx denom = cplx_sub(cplx_make(1.0, 0.0), cplx_mul(newton, sum));
            cplx step;
            if (cplx_abs(denom) > 1.0e-15) {
                step = cplx_div(newton, denom);
            } else {
                step = cplx_scale(newton, 0.5);
            }

            if (!cplx_is_finite(step)) {
                rata_arena_restore(scratch, mark);
                return RATA_ERR_NO_CONVERGENCE;
            }

            rem_roots[k] = cplx_sub(rem_roots[k], step);
            double scale = cplx_abs(rem_roots[k]);
            if (scale < 1.0) {
                scale = 1.0;
            }
            if (cplx_abs(step) <= 1.0e-13 * scale) {
                done[k] = true;
            } else {
                all_done = false;
            }
        }
        if (all_done) {
            converged = true;
            break;
        }
    }

    /* Accept high-multiplicity roots if backward error is small even if step hit roundoff. */
    if (!converged) {
        bool all_good = true;
        for (size_t k = 0u; k < n; ++k) {
            if (!cplx_is_finite(rem_roots[k])) {
                all_good = false;
                break;
            }
            cplx unused_n;
            double val_abs = 0.0;
            double err_bound = 0.0;
            eval_newton_step_monic(c, n, rem_roots[k], &unused_n, &val_abs, &err_bound);
            if (val_abs > 1.0e-7 * ((err_bound > 1.0) ? err_bound : 1.0)) {
                all_good = false;
                break;
            }
        }
        if (!all_good) {
            rata_arena_restore(scratch, mark);
            return RATA_ERR_NO_CONVERGENCE;
        }
    }

    /* Clean tiny numerical imaginary noise on real roots if real evaluation is better/equal. */
    for (size_t k = 0u; k < n; ++k) {
        double scale = fabs(rem_roots[k].re);
        if (scale < 1.0) {
            scale = 1.0;
        }
        if (fabs(rem_roots[k].im) < 1.0e-11 * scale) {
            cplx real_cand = cplx_make(rem_roots[k].re, 0.0);
            cplx unused_n;
            double abs_orig = 0.0, abs_real = 0.0, b1 = 0.0, b2 = 0.0;
            eval_newton_step_monic(c, n, rem_roots[k], &unused_n, &abs_orig, &b1);
            eval_newton_step_monic(c, n, real_cand, &unused_n, &abs_real, &b2);
            if (abs_real <= abs_orig + 1.0e-13 * b2) {
                rem_roots[k] = real_cand;
            }
        }
    }

    rata_arena_restore(scratch, mark);
    *count_out = deg;
    sort_roots_deterministic(roots_out, deg);
    return RATA_OK;
}

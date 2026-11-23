#ifndef RATA_CORE_POLY_H
#define RATA_CORE_POLY_H

#include <stdbool.h>
#include <stddef.h>

#include "core/arena.h"
#include "core/cplx.h"
#include "core/status.h"

/* Real polynomial in ascending powers:
 * P(x) = coeffs[0] + coeffs[1]*x + ... + coeffs[deg]*x^deg.
 * Memory for `coeffs` is owned by the caller or the arena passed at creation. */
typedef struct RataPoly {
    double *coeffs;
    size_t deg;
    size_t cap;
} RataPoly;

/* Initializes a polynomial backed by caller-owned `coeffs` buffer of length `cap`.
 * Copies `len` ascending coefficients (or sets zero polynomial if len == 0). */
RataStatus poly_init(RataPoly *p, double *buf, size_t cap, const double *asc_coeffs, size_t len);

/* Allocates a polynomial of capacity `cap` in `arena` (owned by `arena`). */
RataStatus poly_alloc(RataPoly *p, size_t cap, RataArena *arena);

/* Builds an ascending-power polynomial from descending coefficients:
 * desc[0]*x^(len-1) + ... + desc[len-1]. Allocates coeffs in `arena` (owned by `arena`). */
RataStatus poly_from_descending(RataPoly *out, const double *desc, size_t len, RataArena *arena);

/* Deep-copies `src` into `dst` using `arena` (owned by `arena`). */
RataStatus poly_copy(RataPoly *dst, const RataPoly *src, RataArena *arena);

/* Strips trailing near-zero high-order terms (|c| <= tol). */
void poly_trim(RataPoly *p, double tol);

/* Returns true if polynomial is identically zero (deg == 0 and |coeffs[0]| <= tol). */
bool poly_is_zero(const RataPoly *p, double tol);

/* Evaluates P(x) for real x using Horner's method. */
double poly_eval_real(const RataPoly *p, double x);

/* Evaluates P(z) for complex z using Horner's method. */
cplx poly_eval_cplx(const RataPoly *p, cplx z);

/* Evaluates both P(z) and P'(z) in a single Horner pass. */
void poly_eval_cplx_deriv(const RataPoly *p, cplx z, cplx *val_out, cplx *deriv_out);

/* Computes derivative P'(x). Allocates result coeffs in `arena` (owned by `arena`). */
RataStatus poly_deriv(const RataPoly *p, RataPoly *out, RataArena *arena);

/* Arithmetic operations. Result coeffs are allocated in `arena` (owned by `arena`). */
RataStatus poly_add(const RataPoly *a, const RataPoly *b, RataPoly *out, RataArena *arena);
RataStatus poly_sub(const RataPoly *a, const RataPoly *b, RataPoly *out, RataArena *arena);
RataStatus poly_scale(const RataPoly *a, double s, RataPoly *out, RataArena *arena);
RataStatus poly_mul(const RataPoly *a, const RataPoly *b, RataPoly *out, RataArena *arena);

/* Euclidean division: a = q * b + r with deg(r) < deg(b).
 * Allocates q and r coeffs in `arena` (owned by `arena`).
 * Returns RATA_ERR_DIV_ZERO if b is zero. */
RataStatus poly_divmod(const RataPoly *a,
                       const RataPoly *b,
                       RataPoly *q_out,
                       RataPoly *r_out,
                       RataArena *arena);

/* Expands real polynomial gain * prod_{k=0..count-1} (x - roots[k]).
 * Roots must be real or occur in complex-conjugate pairs; returns RATA_ERR_INVALID_ARG
 * if resulting coefficients have significant imaginary residuals.
 * Result coeffs are allocated in `arena` (owned by `arena`). */
RataStatus poly_from_roots_cplx(const cplx *roots,
                                size_t count,
                                double gain,
                                RataPoly *out,
                                RataArena *arena);

/* Finds all complex roots of P(z) = 0 using closed-form (deg <= 2) and overflow-safe
 * Aberth-Ehrlich iteration (deg >= 3) after zero-root deflation.
 * Writes roots into caller-owned `roots_out` (capacity `roots_cap`) and stores root count
 * in `*count_out`. Uses `scratch` for temporary allocations (restores scratch mark before return).
 * Roots are sorted deterministically by descending real part, then descending imaginary part. */
RataStatus poly_roots(const RataPoly *p,
                      cplx *roots_out,
                      size_t roots_cap,
                      size_t *count_out,
                      RataArena *scratch);

#endif /* RATA_CORE_POLY_H */

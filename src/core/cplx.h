#ifndef RATA_CORE_CPLX_H
#define RATA_CORE_CPLX_H

#include <stdbool.h>
#include <stdint.h>

#include "core/status.h"

/* Plain C ABI complex number (do not use <complex.h>). */
typedef struct cplx {
    double re;
    double im;
} cplx;

cplx cplx_make(double re, double im);
cplx cplx_zero(void);
cplx cplx_from_polar(double mag, double phase_rad);

cplx cplx_add(cplx a, cplx b);
cplx cplx_sub(cplx a, cplx b);
cplx cplx_mul(cplx a, cplx b);
cplx cplx_neg(cplx a);
cplx cplx_conj(cplx a);
cplx cplx_scale(cplx a, double s);

/* Overflow-safe division (Smith's formula). Returns RATA_ERR_DIV_ZERO if b == 0. */
RataStatus cplx_div_safe(cplx a, cplx b, cplx *out);

/* Overflow-safe division; returns {0, 0} if b == 0 (prefer cplx_div_safe when checking errors). */
cplx cplx_div(cplx a, cplx b);

/* Overflow-safe magnitude |a|. */
double cplx_abs(cplx a);
/* Squared magnitude |a|^2. */
double cplx_abs2(cplx a);
/* Principal phase angle in radians in [-pi, pi]. */
double cplx_arg(cplx a);

/* Complex elementary functions. */
cplx cplx_exp(cplx z);
cplx cplx_sqrt(cplx z);
cplx cplx_pow_int(cplx z, int32_t exp);
RataStatus cplx_log_safe(cplx z, cplx *out);

bool cplx_is_finite(cplx z);
bool cplx_near(cplx a, cplx b, double tol);

#endif /* RATA_CORE_CPLX_H */

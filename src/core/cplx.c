#include "core/cplx.h"

#include <math.h>
#include <stddef.h>

cplx cplx_make(double re, double im) {
    cplx z;
    z.re = re;
    z.im = im;
    return z;
}

cplx cplx_zero(void) {
    return cplx_make(0.0, 0.0);
}

cplx cplx_from_polar(double mag, double phase_rad) {
    return cplx_make(mag * cos(phase_rad), mag * sin(phase_rad));
}

cplx cplx_add(cplx a, cplx b) {
    return cplx_make(a.re + b.re, a.im + b.im);
}

cplx cplx_sub(cplx a, cplx b) {
    return cplx_make(a.re - b.re, a.im - b.im);
}

cplx cplx_mul(cplx a, cplx b) {
    return cplx_make(a.re * b.re - a.im * b.im, a.re * b.im + a.im * b.re);
}

cplx cplx_neg(cplx a) {
    return cplx_make(-a.re, -a.im);
}

cplx cplx_conj(cplx a) {
    return cplx_make(a.re, -a.im);
}

cplx cplx_scale(cplx a, double s) {
    return cplx_make(a.re * s, a.im * s);
}

RataStatus cplx_div_safe(cplx a, cplx b, cplx *out) {
    if (out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    double br = fabs(b.re);
    double bi = fabs(b.im);
    if (br == 0.0 && bi == 0.0) {
        *out = cplx_zero();
        return RATA_ERR_DIV_ZERO;
    }

    /* Smith's formula avoids intermediate overflow/underflow. */
    if (br >= bi) {
        double r = b.im / b.re;
        double denom = b.re + b.im * r;
        out->re = (a.re + a.im * r) / denom;
        out->im = (a.im - a.re * r) / denom;
    } else {
        double r = b.re / b.im;
        double denom = b.im + b.re * r;
        out->re = (a.re * r + a.im) / denom;
        out->im = (a.im * r - a.re) / denom;
    }
    return RATA_OK;
}

cplx cplx_div(cplx a, cplx b) {
    cplx res;
    if (cplx_div_safe(a, b, &res) != RATA_OK) {
        return cplx_zero();
    }
    return res;
}

double cplx_abs(cplx a) {
    double ar = fabs(a.re);
    double ai = fabs(a.im);
    if (ar == 0.0) {
        return ai;
    }
    if (ai == 0.0) {
        return ar;
    }
    if (ar >= ai) {
        double r = ai / ar;
        return ar * sqrt(1.0 + r * r);
    }
    double r = ar / ai;
    return ai * sqrt(1.0 + r * r);
}

double cplx_abs2(cplx a) {
    return a.re * a.re + a.im * a.im;
}

double cplx_arg(cplx a) {
    return atan2(a.im, a.re);
}

cplx cplx_exp(cplx z) {
    double r = exp(z.re);
    return cplx_make(r * cos(z.im), r * sin(z.im));
}

cplx cplx_sqrt(cplx z) {
    double r = cplx_abs(z);
    if (r == 0.0) {
        return cplx_zero();
    }
    if (z.re >= 0.0) {
        double re = sqrt(0.5 * (r + z.re));
        double im = (re > 0.0) ? (0.5 * z.im / re) : 0.0;
        return cplx_make(re, im);
    }
    double im_abs = sqrt(0.5 * (r - z.re));
    double im = (z.im < 0.0) ? -im_abs : im_abs;
    double re = (im_abs > 0.0) ? (0.5 * fabs(z.im) / im_abs) : 0.0;
    return cplx_make(re, im);
}

cplx cplx_pow_int(cplx z, int32_t exp_val) {
    if (exp_val == 0) {
        return cplx_make(1.0, 0.0);
    }
    bool neg = exp_val < 0;
    uint32_t n = neg ? (uint32_t)(-(int64_t)exp_val) : (uint32_t)exp_val;

    cplx base = z;
    cplx acc = cplx_make(1.0, 0.0);
    while (n > 0u) {
        if ((n & 1u) != 0u) {
            acc = cplx_mul(acc, base);
        }
        n >>= 1u;
        if (n > 0u) {
            base = cplx_mul(base, base);
        }
    }
    if (neg) {
        return cplx_div(cplx_make(1.0, 0.0), acc);
    }
    return acc;
}

RataStatus cplx_log_safe(cplx z, cplx *out) {
    if (out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    double r = cplx_abs(z);
    if (r == 0.0) {
        *out = cplx_zero();
        return RATA_ERR_RANGE;
    }
    out->re = log(r);
    out->im = atan2(z.im, z.re);
    return RATA_OK;
}

bool cplx_is_finite(cplx z) {
    return isfinite(z.re) && isfinite(z.im);
}

bool cplx_near(cplx a, cplx b, double tol) {
    return cplx_abs(cplx_sub(a, b)) <= tol;
}

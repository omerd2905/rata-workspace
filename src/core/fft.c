#include "core/fft.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

bool fft_is_power_of_two(size_t n) {
    return n != 0u && (n & (n - 1u)) == 0u;
}

size_t fft_next_power_of_two(size_t n) {
    if (n == 0u) {
        return 0u;
    }
    size_t p = 1u;
    while (p < n) {
        if (p > (((size_t)-1) >> 1u)) {
            return 0u;
        }
        p <<= 1u;
    }
    return p;
}

RataStatus dft_direct_forward(const cplx *in, cplx *out, size_t n) {
    if (in == NULL || out == NULL || n == 0u || in == out) {
        return RATA_ERR_INVALID_ARG;
    }
    for (size_t k = 0u; k < n; ++k) {
        cplx sum = cplx_zero();
        for (size_t m = 0u; m < n; ++m) {
            uint64_t km_mod = ((uint64_t)k * (uint64_t)m) % (uint64_t)n;
            double angle = -RATA_TWO_PI * (double)km_mod / (double)n;
            cplx tw = cplx_make(cos(angle), sin(angle));
            sum = cplx_add(sum, cplx_mul(in[m], tw));
        }
        out[k] = sum;
    }
    return RATA_OK;
}

RataStatus dft_direct_inverse(const cplx *in, cplx *out, size_t n) {
    if (in == NULL || out == NULL || n == 0u || in == out) {
        return RATA_ERR_INVALID_ARG;
    }
    double inv_n = 1.0 / (double)n;
    for (size_t m = 0u; m < n; ++m) {
        cplx sum = cplx_zero();
        for (size_t k = 0u; k < n; ++k) {
            uint64_t km_mod = ((uint64_t)k * (uint64_t)m) % (uint64_t)n;
            double angle = RATA_TWO_PI * (double)km_mod / (double)n;
            cplx tw = cplx_make(cos(angle), sin(angle));
            sum = cplx_add(sum, cplx_mul(in[k], tw));
        }
        out[m] = cplx_scale(sum, inv_n);
    }
    return RATA_OK;
}

/* In-place Cooley-Tukey radix-2 DIT forward FFT (n must be a power of 2). */
static void fft_radix2_inplace_forward(cplx *buf, size_t n) {
    /* Bit-reversal permutation. */
    size_t j = 0u;
    for (size_t i = 1u; i < n; ++i) {
        size_t bit = n >> 1u;
        while ((j & bit) != 0u) {
            j ^= bit;
            bit >>= 1u;
        }
        j ^= bit;
        if (i < j) {
            cplx tmp = buf[i];
            buf[i] = buf[j];
            buf[j] = tmp;
        }
    }

    /* Butterfly stages. */
    for (size_t len = 2u; len <= n; len <<= 1u) {
        size_t half = len >> 1u;
        double base_angle = -RATA_TWO_PI / (double)len;
        for (size_t start = 0u; start < n; start += len) {
            for (size_t k = 0u; k < half; ++k) {
                double angle = base_angle * (double)k;
                cplx w = cplx_make(cos(angle), sin(angle));
                cplx u = buf[start + k];
                cplx v = cplx_mul(buf[start + k + half], w);
                buf[start + k] = cplx_add(u, v);
                buf[start + k + half] = cplx_sub(u, v);
            }
        }
    }
}

static void fft_radix2_inplace_inverse(cplx *buf, size_t n) {
    for (size_t i = 0u; i < n; ++i) {
        buf[i] = cplx_conj(buf[i]);
    }
    fft_radix2_inplace_forward(buf, n);
    double inv_n = 1.0 / (double)n;
    for (size_t i = 0u; i < n; ++i) {
        buf[i] = cplx_scale(cplx_conj(buf[i]), inv_n);
    }
}

/* Bluestein chirp-z forward FFT for arbitrary length n >= 2. */
static RataStatus fft_bluestein_forward(const cplx *in, cplx *out, size_t n, RataArena *scratch) {
    if (scratch == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (n > (((size_t)-1) - 1u) / 2u) {
        return RATA_ERR_RANGE;
    }
    size_t m = fft_next_power_of_two(2u * n - 1u);
    if (m == 0u) {
        return RATA_ERR_RANGE;
    }

    size_t mark = rata_arena_mark(scratch);
    cplx *chirp = (cplx *)rata_arena_alloc(scratch, n * sizeof(cplx), 8u);
    cplx *a = (cplx *)rata_arena_calloc(scratch, m, sizeof(cplx), 8u);
    cplx *b = (cplx *)rata_arena_calloc(scratch, m, sizeof(cplx), 8u);
    if (chirp == NULL || a == NULL || b == NULL) {
        rata_arena_restore(scratch, mark);
        return RATA_ERR_OUT_OF_MEMORY;
    }

    uint64_t two_n = 2u * (uint64_t)n;
    for (size_t k = 0u; k < n; ++k) {
        uint64_t sq_mod = ((uint64_t)k * (uint64_t)k) % two_n;
        double angle = -RATA_PI * (double)sq_mod / (double)n;
        chirp[k] = cplx_make(cos(angle), sin(angle));
        a[k] = cplx_mul(in[k], chirp[k]);
    }

    b[0] = cplx_conj(chirp[0]);
    for (size_t k = 1u; k < n; ++k) {
        cplx bc = cplx_conj(chirp[k]);
        b[k] = bc;
        b[m - k] = bc;
    }

    fft_radix2_inplace_forward(a, m);
    fft_radix2_inplace_forward(b, m);
    for (size_t i = 0u; i < m; ++i) {
        a[i] = cplx_mul(a[i], b[i]);
    }
    fft_radix2_inplace_inverse(a, m);

    for (size_t k = 0u; k < n; ++k) {
        out[k] = cplx_mul(a[k], chirp[k]);
    }

    rata_arena_restore(scratch, mark);
    return RATA_OK;
}

RataStatus fft_forward(const cplx *in, cplx *out, size_t n, RataArena *scratch) {
    if (in == NULL || out == NULL || n == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    if (n == 1u) {
        out[0] = in[0];
        return RATA_OK;
    }
    if (fft_is_power_of_two(n)) {
        if (in != out) {
            memcpy(out, in, n * sizeof(cplx));
        }
        fft_radix2_inplace_forward(out, n);
        return RATA_OK;
    }
    return fft_bluestein_forward(in, out, n, scratch);
}

RataStatus fft_inverse(const cplx *in, cplx *out, size_t n, RataArena *scratch) {
    if (in == NULL || out == NULL || n == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    if (n == 1u) {
        out[0] = in[0];
        return RATA_OK;
    }
    if (fft_is_power_of_two(n)) {
        if (in != out) {
            memcpy(out, in, n * sizeof(cplx));
        }
        fft_radix2_inplace_inverse(out, n);
        return RATA_OK;
    }

    if (scratch == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t mark = rata_arena_mark(scratch);
    cplx *conj_in = (cplx *)rata_arena_alloc(scratch, n * sizeof(cplx), 8u);
    if (conj_in == NULL) {
        rata_arena_restore(scratch, mark);
        return RATA_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0u; i < n; ++i) {
        conj_in[i] = cplx_conj(in[i]);
    }
    RataStatus st = fft_bluestein_forward(conj_in, out, n, scratch);
    rata_arena_restore(scratch, mark);
    if (st != RATA_OK) {
        return st;
    }
    double inv_n = 1.0 / (double)n;
    for (size_t i = 0u; i < n; ++i) {
        out[i] = cplx_scale(cplx_conj(out[i]), inv_n);
    }
    return RATA_OK;
}

RataStatus fft_real_forward(const double *in, cplx *out, size_t n, RataArena *scratch) {
    if (in == NULL || out == NULL || n == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    for (size_t i = 0u; i < n; ++i) {
        out[i] = cplx_make(in[i], 0.0);
    }
    return fft_forward(out, out, n, scratch);
}

RataStatus fft_magnitude(const cplx *spec, double *mag, size_t n) {
    if (spec == NULL || mag == NULL || n == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    for (size_t i = 0u; i < n; ++i) {
        mag[i] = cplx_abs(spec[i]);
    }
    return RATA_OK;
}

RataStatus fft_magnitude_db(const cplx *spec, double *mag_db, size_t n, double floor_db) {
    if (spec == NULL || mag_db == NULL || n == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    for (size_t i = 0u; i < n; ++i) {
        double m = cplx_abs(spec[i]);
        if (m <= 0.0) {
            mag_db[i] = floor_db;
        } else {
            double db = 20.0 * log10(m);
            mag_db[i] = (db < floor_db) ? floor_db : db;
        }
    }
    return RATA_OK;
}

RataStatus fft_phase(const cplx *spec, double *phase_rad, size_t n, double mag_thresh) {
    if (spec == NULL || phase_rad == NULL || n == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    for (size_t i = 0u; i < n; ++i) {
        if (cplx_abs(spec[i]) < mag_thresh) {
            phase_rad[i] = 0.0;
        } else {
            phase_rad[i] = cplx_arg(spec[i]);
        }
    }
    return RATA_OK;
}

RataStatus fft_shift(const cplx *in, cplx *out, size_t n) {
    if (in == NULL || out == NULL || n == 0u || in == out) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t half = n / 2u;
    size_t pivot = n - half;
    for (size_t i = 0u; i < n; ++i) {
        size_t dst_idx = (i < pivot) ? (i + half) : (i - pivot);
        out[dst_idx] = in[i];
    }
    return RATA_OK;
}

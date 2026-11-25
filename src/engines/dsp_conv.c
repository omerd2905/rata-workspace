#include "engines/dsp_conv.h"

#include <limits.h>
#include <math.h>
#include <string.h>

RataStatus dsp_conv_linear(const RataSignal *x,
                           const RataSignal *h,
                           RataSignal *y_out,
                           RataArena *arena) {
    if (x == NULL || h == NULL || x->samples == NULL || h->samples == NULL ||
        x->len == 0u || h->len == 0u || y_out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    size_t y_len = x->len + h->len - 1u;
    if (y_len > RATA_DSP_MAX_SAMPLES) {
        return RATA_ERR_RANGE;
    }
    int64_t y_n0_64 = (int64_t)x->n0 + (int64_t)h->n0;
    if (y_n0_64 < (int64_t)INT32_MIN || y_n0_64 > (int64_t)INT32_MAX) {
        return RATA_ERR_RANGE;
    }
    RataStatus st = dsp_signal_alloc(y_out, y_len, (int32_t)y_n0_64, x->fs, arena);
    if (st != RATA_OK) {
        return st;
    }

    for (size_t i = 0u; i < x->len; ++i) {
        double xi = x->samples[i];
        if (xi == 0.0) {
            continue;
        }
        for (size_t j = 0u; j < h->len; ++j) {
            y_out->samples[i + j] += xi * h->samples[j];
        }
    }
    return RATA_OK;
}

RataStatus dsp_conv_step(const RataSignal *x,
                         const RataSignal *h,
                         int32_t n_shift,
                         RataConvStep *step_out,
                         RataArena *arena) {
    if (x == NULL || h == NULL || x->samples == NULL || h->samples == NULL ||
        x->len == 0u || h->len == 0u || step_out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }

    /* Fixed sweep axis k covers x[k] and the full sweep of h[n - k] for all valid output n. */
    int64_t x_min = (int64_t)x->n0;
    int64_t x_max = (int64_t)x->n0 + (int64_t)x->len - 1;
    int64_t h_min = (int64_t)h->n0;
    int64_t h_max = (int64_t)h->n0 + (int64_t)h->len - 1;

    int64_t y_min = x_min + h_min;
    int64_t y_max = x_max + h_max;

    /* At n = y_min, h[n-k] spans [y_min - h_max, y_min - h_min] = [x_min - (Lh-1), x_min].
     * At n = y_max, h[n-k] spans [y_max - h_max, y_max - h_min] = [x_max, x_max + (Lh-1)]. */
    int64_t sweep_k_min = y_min - h_max;
    int64_t sweep_k_max = y_max - h_min;

    /* Also cover current n_shift if caller passed an out-of-support shift index. */
    int64_t cur_h_k_min = (int64_t)n_shift - h_max;
    int64_t cur_h_k_max = (int64_t)n_shift - h_min;

    int64_t k_min = (x_min < sweep_k_min) ? x_min : sweep_k_min;
    if (cur_h_k_min < k_min) {
        k_min = cur_h_k_min;
    }
    int64_t k_max = (x_max > sweep_k_max) ? x_max : sweep_k_max;
    if (cur_h_k_max > k_max) {
        k_max = cur_h_k_max;
    }

    size_t k_len = (size_t)(k_max - k_min + 1);
    if (k_len == 0u || k_len > RATA_DSP_MAX_SAMPLES) {
        return RATA_ERR_RANGE;
    }

    double *k_axis = (double *)rata_arena_alloc(arena, k_len * sizeof(double), 8u);
    double *x_al = (double *)rata_arena_calloc(arena, k_len, sizeof(double), 8u);
    double *h_sh = (double *)rata_arena_calloc(arena, k_len, sizeof(double), 8u);
    double *prod = (double *)rata_arena_calloc(arena, k_len, sizeof(double), 8u);
    if (k_axis == NULL || x_al == NULL || h_sh == NULL || prod == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }

    double sum = 0.0;
    for (size_t idx = 0u; idx < k_len; ++idx) {
        int64_t k = k_min + (int64_t)idx;
        k_axis[idx] = (double)k;

        double xv = 0.0;
        if (k >= x_min && k <= x_max) {
            xv = x->samples[(size_t)(k - x_min)];
        }
        int64_t h_arg = (int64_t)n_shift - k;
        double hv = 0.0;
        if (h_arg >= h_min && h_arg <= h_max) {
            hv = h->samples[(size_t)(h_arg - h_min)];
        }
        double pv = xv * hv;
        x_al[idx] = xv;
        h_sh[idx] = hv;
        prod[idx] = pv;
        sum += pv;
    }

    step_out->n_shift = n_shift;
    step_out->k_min = (int32_t)k_min;
    step_out->k_len = k_len;
    step_out->k_axis = k_axis;
    step_out->x_aligned = x_al;
    step_out->h_shifted = h_sh;
    step_out->product = prod;
    step_out->sum = sum;
    return RATA_OK;
}

RataStatus dsp_deconv_linear(const RataSignal *y,
                             const RataSignal *known,
                             size_t max_out_len,
                             RataSignal *out,
                             RataDeconvInfo *info_out,
                             RataArena *arena) {
    if (info_out != NULL) {
        memset(info_out, 0, sizeof(*info_out));
    }
    if (y == NULL || known == NULL || y->samples == NULL || known->samples == NULL ||
        y->len == 0u || known->len == 0u || out == NULL || arena == NULL ||
        max_out_len == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    if (max_out_len > RATA_DSP_MAX_SAMPLES) {
        max_out_len = RATA_DSP_MAX_SAMPLES;
    }

    double max_k = 0.0;
    for (size_t i = 0u; i < known->len; ++i) {
        double v = known->samples[i];
        if (!isfinite(v)) {
            return RATA_ERR_RANGE;
        }
        if (fabs(v) > max_k) {
            max_k = fabs(v);
        }
    }
    if (max_k <= 1.0e-15) {
        return RATA_ERR_DIV_ZERO;
    }

    double max_y = 0.0;
    for (size_t i = 0u; i < y->len; ++i) {
        double v = y->samples[i];
        if (!isfinite(v)) {
            return RATA_ERR_RANGE;
        }
        if (fabs(v) > max_y) {
            max_y = fabs(v);
        }
    }

    double tol_k = 1.0e-12 * max_k;
    size_t k_first = 0u;
    while (k_first < known->len && fabs(known->samples[k_first]) <= tol_k) {
        k_first++;
    }
    size_t k_last = known->len - 1u;
    while (k_last > k_first && fabs(known->samples[k_last]) <= tol_k) {
        k_last--;
    }
    size_t L_k = k_last - k_first + 1u;
    const double *k_trim = known->samples + k_first;
    int64_t k_n0 = (int64_t)known->n0 + (int64_t)k_first;

    double fs = (y->fs > 0.0 && isfinite(y->fs)) ? y->fs : known->fs;

    /* Identically zero output y[n] => zero solution. */
    if (max_y <= 1.0e-15) {
        int64_t zero_n0 = (int64_t)y->n0 - k_n0;
        if (zero_n0 < (int64_t)INT32_MIN || zero_n0 > (int64_t)INT32_MAX) {
            return RATA_ERR_RANGE;
        }
        RataStatus st = dsp_signal_alloc(out, 1u, (int32_t)zero_n0, fs, arena);
        if (st == RATA_OK && info_out != NULL) {
            info_out->exact_finite = true;
            info_out->max_residual = 0.0;
        }
        return st;
    }

    double tol_y = 1.0e-12 * max_y;
    size_t y_first = 0u;
    while (y_first < y->len && fabs(y->samples[y_first]) <= tol_y) {
        y_first++;
    }
    size_t y_last = y->len - 1u;
    while (y_last > y_first && fabs(y->samples[y_last]) <= tol_y) {
        y_last--;
    }
    size_t L_y = y_last - y_first + 1u;
    const double *y_trim = y->samples + y_first;
    int64_t y_n0 = (int64_t)y->n0 + (int64_t)y_first;

    int64_t out_n0_64 = y_n0 - k_n0;
    if (out_n0_64 < (int64_t)INT32_MIN || out_n0_64 > (int64_t)INT32_MAX) {
        return RATA_ERR_RANGE;
    }
    int32_t out_n0 = (int32_t)out_n0_64;

    /* 1. Test exact finite-support deconvolution when L_y >= L_k. */
    if (L_y >= L_k) {
        size_t L_exact = L_y - L_k + 1u;
        size_t mark = rata_arena_mark(arena);
        double *q_ex = (double *)rata_arena_calloc(arena, L_exact, sizeof(double), 8u);
        if (q_ex == NULL) {
            return RATA_ERR_OUT_OF_MEMORY;
        }
        bool ok_ex = true;
        double max_q = 0.0;
        for (size_t m = 0u; m < L_exact; ++m) {
            double rhs = y_trim[m];
            size_t j_max = (m < L_k - 1u) ? m : (L_k - 1u);
            for (size_t j = 1u; j <= j_max; ++j) {
                rhs -= k_trim[j] * q_ex[m - j];
            }
            double qm = rhs / k_trim[0];
            if (!isfinite(qm) || fabs(qm) > 1.0e15) {
                ok_ex = false;
                break;
            }
            q_ex[m] = qm;
            if (fabs(qm) > max_q) {
                max_q = fabs(qm);
            }
        }

        double max_tail_err = 0.0;
        if (ok_ex) {
            for (size_t m = L_exact; m < L_y; ++m) {
                double conv_m = 0.0;
                size_t j_start = m - (L_exact - 1u);
                size_t j_end = (m < L_k - 1u) ? m : (L_k - 1u);
                for (size_t j = j_start; j <= j_end; ++j) {
                    conv_m += k_trim[j] * q_ex[m - j];
                }
                double err = fabs(y_trim[m] - conv_m);
                if (err > max_tail_err) {
                    max_tail_err = err;
                }
            }
        }

        if (ok_ex && max_tail_err <= 1.0e-6 * max_y) {
            size_t trim_len = L_exact;
            while (trim_len > 1u && fabs(q_ex[trim_len - 1u]) <= 1.0e-10 * max_q) {
                trim_len--;
            }
            /* Copy out via temporary stack/arena safe path: allocate final signal after copy. */
            rata_arena_restore(arena, mark);
            RataStatus st = dsp_signal_alloc(out, trim_len, out_n0, fs, arena);
            if (st != RATA_OK) {
                return st;
            }
            for (size_t m = 0u; m < trim_len; ++m) {
                double rhs = y_trim[m];
                size_t j_max = (m < L_k - 1u) ? m : (L_k - 1u);
                for (size_t j = 1u; j <= j_max; ++j) {
                    rhs -= k_trim[j] * out->samples[m - j];
                }
                out->samples[m] = rhs / k_trim[0];
            }
            if (info_out != NULL) {
                info_out->exact_finite = true;
                info_out->truncated_iir = false;
                info_out->max_residual = max_tail_err;
            }
            return RATA_OK;
        }
        rata_arena_restore(arena, mark);
    }

    /* 2. Causal ascending-power long division Y(z) / Known(z) up to max_out_len samples. */
    RataStatus st = dsp_signal_alloc(out, max_out_len, out_n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }

    bool diverged = false;
    size_t valid_len = max_out_len;
    for (size_t m = 0u; m < max_out_len; ++m) {
        double rhs = (m < L_y) ? y_trim[m] : 0.0;
        size_t j_max = (m < L_k - 1u) ? m : (L_k - 1u);
        for (size_t j = 1u; j <= j_max; ++j) {
            rhs -= k_trim[j] * out->samples[m - j];
        }
        double qm = rhs / k_trim[0];
        if (!isfinite(qm) || fabs(qm) > 1.0e15) {
            diverged = true;
            valid_len = (m > 0u) ? m : 1u;
            if (info_out != NULL) {
                info_out->diverged_at = m;
            }
            break;
        }
        out->samples[m] = qm;
    }
    out->len = valid_len;

    /* Compute full finite-convolution residual max_m |y_trim[m] - (k_trim * out)[m]|. */
    double max_res = 0.0;
    size_t conv_len = L_k + valid_len - 1u;
    size_t check_len = (conv_len > L_y) ? conv_len : L_y;
    for (size_t m = 0u; m < check_len; ++m) {
        double conv_m = 0.0;
        if (m < conv_len) {
            size_t j_start = (m >= valid_len) ? (m - valid_len + 1u) : 0u;
            size_t j_end = (m < L_k - 1u) ? m : (L_k - 1u);
            for (size_t j = j_start; j <= j_end; ++j) {
                conv_m += k_trim[j] * out->samples[m - j];
            }
        }
        double ym = (m < L_y) ? y_trim[m] : 0.0;
        double err = fabs(ym - conv_m);
        if (err > max_res) {
            max_res = err;
        }
    }

    if (info_out != NULL) {
        info_out->exact_finite = false;
        info_out->truncated_iir = true;
        info_out->max_residual = max_res;
    }
    return diverged ? RATA_ERR_UNSTABLE : RATA_OK;
}

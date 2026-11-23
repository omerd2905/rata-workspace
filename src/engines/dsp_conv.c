#include "engines/dsp_conv.h"

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
    int32_t y_n0 = x->n0 + h->n0;
    RataStatus st = dsp_signal_alloc(y_out, y_len, y_n0, x->fs, arena);
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

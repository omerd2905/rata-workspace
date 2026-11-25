#include "engines/dsp_signal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

RataStatus dsp_signal_alloc(RataSignal *out, size_t len, int32_t n0, double fs, RataArena *arena) {
    if (out == NULL || arena == NULL || len == 0u || len > RATA_DSP_MAX_SAMPLES ||
        !isfinite(fs) || fs <= 0.0) {
        return RATA_ERR_INVALID_ARG;
    }
    double *buf = (double *)rata_arena_calloc(arena, len, sizeof(double), 8u);
    if (buf == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }
    out->samples = buf;
    out->len = len;
    out->n0 = n0;
    out->fs = fs;
    return RATA_OK;
}

RataStatus dsp_signal_from_expr(const char *expr_str,
                                int32_t n0,
                                size_t len,
                                double fs,
                                RataSignal *out,
                                RataExprError *err_out,
                                RataArena *arena) {
    if (expr_str == NULL || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(out, len, n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }

    size_t mark = rata_arena_mark(arena);
    RataExpr expr;
    st = expr_parse_cstr(expr_str, &expr, err_out, arena);
    if (st != RATA_OK) {
        rata_arena_restore(arena, mark);
        return st;
    }

    RataExprVar vars[3] = {
        {"n", 0.0},
        {"t", 0.0},
        {"fs", fs}
    };
    for (size_t i = 0u; i < len; ++i) {
        int32_t n = n0 + (int32_t)i;
        vars[0].value = (double)n;
        vars[1].value = (double)n / fs;
        double val = 0.0;
        st = expr_eval(&expr, vars, 3u, &val);
        if (st != RATA_OK) {
            if (err_out != NULL && err_out->message[0] == '\0') {
                snprintf(err_out->message, sizeof(err_out->message),
                         "evaluation failed at n=%d (%s)", n, rata_status_str(st));
            }
            rata_arena_restore(arena, mark);
            return st;
        }
        out->samples[i] = val;
    }

    rata_arena_restore(arena, mark);
    return RATA_OK;
}

static bool is_ws_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

RataStatus dsp_signal_from_text(const char *input_str,
                                int32_t n0,
                                size_t expr_len,
                                double fs,
                                RataSignal *out,
                                bool *was_list_out,
                                RataExprError *err_out,
                                RataArena *arena) {
    if (was_list_out != NULL) {
        *was_list_out = false;
    }
    if (err_out != NULL) {
        err_out->col = 0u;
        err_out->message[0] = '\0';
    }
    if (input_str == NULL || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (!isfinite(fs) || fs <= 0.0) {
        return RATA_ERR_INVALID_ARG;
    }

    const char *s = input_str;
    while (*s != '\0' && is_ws_char(*s)) {
        s++;
    }
    size_t slen = strlen(s);
    while (slen > 0u && is_ws_char(s[slen - 1u])) {
        slen--;
    }
    if (slen == 0u) {
        if (err_out != NULL) {
            snprintf(err_out->message, sizeof(err_out->message), "empty signal input");
        }
        return RATA_ERR_PARSE;
    }

    bool bracketed = false;
    if (s[0] == '[' || s[0] == '{') {
        char close_ch = (s[0] == '[') ? ']' : '}';
        if (s[slen - 1u] != close_ch) {
            if (err_out != NULL) {
                snprintf(err_out->message, sizeof(err_out->message),
                         "unclosed '%c' in sample list", s[0]);
            }
            return RATA_ERR_PARSE;
        }
        s++;
        slen -= 2u;
        bracketed = true;
    }

    bool has_top_sep = false;
    int depth = 0;
    for (size_t i = 0u; i < slen; ++i) {
        if (s[i] == '(') {
            depth++;
        } else if (s[i] == ')' && depth > 0) {
            depth--;
        } else if (depth == 0 && (s[i] == ',' || s[i] == ';')) {
            has_top_sep = true;
        }
    }

    if (!bracketed && !has_top_sep) {
        return dsp_signal_from_expr(input_str, n0, expr_len, fs, out, err_out, arena);
    }

    if (was_list_out != NULL) {
        *was_list_out = true;
    }

    /* Count comma/semicolon-separated sample items at parenthesis depth 0. */
    size_t item_count = 0u;
    size_t seg_start = 0u;
    depth = 0;
    for (size_t i = 0u; i <= slen; ++i) {
        bool at_end = (i == slen);
        bool is_sep = (!at_end && depth == 0 && (s[i] == ',' || s[i] == ';'));
        if (!at_end) {
            if (s[i] == '(') {
                depth++;
            } else if (s[i] == ')' && depth > 0) {
                depth--;
            }
        }
        if (at_end || is_sep) {
            size_t a = seg_start;
            size_t b = i;
            while (a < b && is_ws_char(s[a])) {
                a++;
            }
            while (b > a && is_ws_char(s[b - 1u])) {
                b--;
            }
            if (a == b) {
                if (at_end && item_count > 0u) {
                    /* Allow trailing comma inside sample list (e.g. "[1, 2, 3, ]"). */
                    break;
                }
                if (err_out != NULL) {
                    snprintf(err_out->message, sizeof(err_out->message),
                             "empty sample at position %zu in list", item_count + 1u);
                }
                return RATA_ERR_PARSE;
            }
            item_count++;
            if (item_count > RATA_DSP_MAX_SAMPLES) {
                return RATA_ERR_RANGE;
            }
            seg_start = i + 1u;
        }
    }

    if (item_count == 0u) {
        if (err_out != NULL) {
            snprintf(err_out->message, sizeof(err_out->message), "empty sample list");
        }
        return RATA_ERR_PARSE;
    }

    RataStatus st = dsp_signal_alloc(out, item_count, n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }

    RataExprVar vars[3] = {
        {"n", 0.0},
        {"t", 0.0},
        {"fs", fs}
    };

    size_t idx = 0u;
    seg_start = 0u;
    depth = 0;
    for (size_t i = 0u; i <= slen && idx < item_count; ++i) {
        bool at_end = (i == slen);
        bool is_sep = (!at_end && depth == 0 && (s[i] == ',' || s[i] == ';'));
        if (!at_end) {
            if (s[i] == '(') {
                depth++;
            } else if (s[i] == ')' && depth > 0) {
                depth--;
            }
        }
        if (at_end || is_sep) {
            size_t a = seg_start;
            size_t b = i;
            while (a < b && is_ws_char(s[a])) {
                a++;
            }
            while (b > a && is_ws_char(s[b - 1u])) {
                b--;
            }
            if (a == b) {
                break;
            }
            size_t mark = rata_arena_mark(arena);
            RataExpr expr;
            st = expr_parse(s + a, b - a, &expr, err_out, arena);
            if (st != RATA_OK) {
                rata_arena_restore(arena, mark);
                return st;
            }
            int32_t n = n0 + (int32_t)idx;
            vars[0].value = (double)n;
            vars[1].value = (double)n / fs;
            double val = 0.0;
            st = expr_eval(&expr, vars, 3u, &val);
            rata_arena_restore(arena, mark);
            if (st != RATA_OK) {
                if (err_out != NULL && err_out->message[0] == '\0') {
                    snprintf(err_out->message, sizeof(err_out->message),
                             "invalid sample #%zu (%s)", idx + 1u, rata_status_str(st));
                }
                return st;
            }
            out->samples[idx++] = val;
            seg_start = i + 1u;
        }
    }

    return RATA_OK;
}

RataStatus dsp_signal_impulse(int32_t n0, size_t len, double fs,
                              double amp, int32_t shift,
                              RataSignal *out, RataArena *arena) {
    if (!isfinite(amp)) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(out, len, n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i < len; ++i) {
        int32_t n = n0 + (int32_t)i;
        out->samples[i] = (n == shift) ? amp : 0.0;
    }
    return RATA_OK;
}

RataStatus dsp_signal_step(int32_t n0, size_t len, double fs,
                           double amp, int32_t shift,
                           RataSignal *out, RataArena *arena) {
    if (!isfinite(amp)) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(out, len, n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i < len; ++i) {
        int32_t n = n0 + (int32_t)i;
        out->samples[i] = (n >= shift) ? amp : 0.0;
    }
    return RATA_OK;
}

RataStatus dsp_signal_sinusoid(int32_t n0, size_t len, double fs,
                               double amp, double freq_hz, double phase_rad,
                               RataSignal *out, RataArena *arena) {
    if (!isfinite(amp) || !isfinite(freq_hz) || !isfinite(phase_rad)) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(out, len, n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i < len; ++i) {
        int32_t n = n0 + (int32_t)i;
        double arg = RATA_TWO_PI * freq_hz * ((double)n / fs) + phase_rad;
        out->samples[i] = amp * cos(arg);
    }
    return RATA_OK;
}

RataStatus dsp_signal_exp(int32_t n0, size_t len, double fs,
                          double amp, double base, int32_t shift,
                          RataSignal *out, RataArena *arena) {
    if (!isfinite(amp) || !isfinite(base)) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(out, len, n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i < len; ++i) {
        int32_t n = n0 + (int32_t)i;
        if (n >= shift) {
            double v = amp * pow(base, (double)(n - shift));
            if (!isfinite(v)) {
                return RATA_ERR_RANGE;
            }
            out->samples[i] = v;
        } else {
            out->samples[i] = 0.0;
        }
    }
    return RATA_OK;
}

RataStatus dsp_signal_rect(int32_t n0, size_t len, double fs,
                           double amp, int32_t start_n, size_t width,
                           RataSignal *out, RataArena *arena) {
    if (!isfinite(amp)) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(out, len, n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    int64_t end_excl = (int64_t)start_n + (int64_t)width;
    for (size_t i = 0u; i < len; ++i) {
        int64_t n = (int64_t)n0 + (int64_t)i;
        out->samples[i] = (n >= (int64_t)start_n && n < end_excl) ? amp : 0.0;
    }
    return RATA_OK;
}

RataStatus dsp_signal_piecewise(int32_t n0, size_t len, double fs,
                                const RataPiecewiseSeg *segs, size_t seg_count,
                                RataSignal *out, RataExprError *err_out, RataArena *arena) {
    if (out == NULL || arena == NULL || (seg_count > 0u && segs == NULL)) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(out, len, n0, fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    if (seg_count == 0u) {
        return RATA_OK;
    }

    size_t mark = rata_arena_mark(arena);
    RataExpr *parsed = (RataExpr *)rata_arena_calloc(arena, seg_count, sizeof(RataExpr), 8u);
    if (parsed == NULL) {
        return RATA_ERR_OUT_OF_MEMORY;
    }
    for (size_t s = 0u; s < seg_count; ++s) {
        if (segs[s].expr == NULL || segs[s].n_start > segs[s].n_end) {
            rata_arena_restore(arena, mark);
            return RATA_ERR_INVALID_ARG;
        }
        st = expr_parse_cstr(segs[s].expr, &parsed[s], err_out, arena);
        if (st != RATA_OK) {
            rata_arena_restore(arena, mark);
            return st;
        }
    }

    RataExprVar vars[3] = {
        {"n", 0.0},
        {"t", 0.0},
        {"fs", fs}
    };
    for (size_t i = 0u; i < len; ++i) {
        int32_t n = n0 + (int32_t)i;
        vars[0].value = (double)n;
        vars[1].value = (double)n / fs;
        out->samples[i] = 0.0;
        for (size_t s = 0u; s < seg_count; ++s) {
            if (n >= segs[s].n_start && n <= segs[s].n_end) {
                double val = 0.0;
                st = expr_eval(&parsed[s], vars, 3u, &val);
                if (st != RATA_OK) {
                    rata_arena_restore(arena, mark);
                    return st;
                }
                out->samples[i] = val;
                break;
            }
        }
    }

    rata_arena_restore(arena, mark);
    return RATA_OK;
}

RataStatus dsp_window_coeffs(RataWindowKind kind, double *win_out, size_t len) {
    if (win_out == NULL || len == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    if (len == 1u) {
        win_out[0] = 1.0;
        return RATA_OK;
    }
    double denom = (double)(len - 1u);
    for (size_t n = 0u; n < len; ++n) {
        double phase = RATA_TWO_PI * (double)n / denom;
        switch (kind) {
            case WIN_RECT:
                win_out[n] = 1.0;
                break;
            case WIN_HANN:
                win_out[n] = 0.5 * (1.0 - cos(phase));
                break;
            case WIN_HAMMING:
                win_out[n] = 0.54 - 0.46 * cos(phase);
                break;
            case WIN_BLACKMAN:
                win_out[n] = 0.42 - 0.5 * cos(phase) + 0.08 * cos(2.0 * phase);
                break;
            default:
                return RATA_ERR_INVALID_ARG;
        }
    }
    return RATA_OK;
}

RataStatus dsp_signal_apply_window(const RataSignal *in,
                                   RataWindowKind kind,
                                   RataSignal *out,
                                   RataArena *arena) {
    if (in == NULL || in->samples == NULL || in->len == 0u || out == NULL || arena == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = dsp_signal_alloc(out, in->len, in->n0, in->fs, arena);
    if (st != RATA_OK) {
        return st;
    }
    st = dsp_window_coeffs(kind, out->samples, in->len);
    if (st != RATA_OK) {
        return st;
    }
    for (size_t i = 0u; i < in->len; ++i) {
        out->samples[i] *= in->samples[i];
    }
    return RATA_OK;
}

RataStatus dsp_signal_stats(const RataSignal *sig, RataSignalStats *stats_out) {
    if (sig == NULL || sig->samples == NULL || sig->len == 0u || stats_out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    double sum = 0.0;
    double energy = 0.0;
    double mn = sig->samples[0];
    double mx = sig->samples[0];
    for (size_t i = 0u; i < sig->len; ++i) {
        double v = sig->samples[i];
        sum += v;
        energy += v * v;
        if (v < mn) {
            mn = v;
        }
        if (v > mx) {
            mx = v;
        }
    }
    double n = (double)sig->len;
    stats_out->energy = energy;
    stats_out->avg_power = energy / n;
    stats_out->mean = sum / n;
    stats_out->rms = sqrt(stats_out->avg_power);
    stats_out->min_val = mn;
    stats_out->max_val = mx;
    return RATA_OK;
}

const char *dsp_window_name(RataWindowKind kind) {
    switch (kind) {
        case WIN_RECT:     return "Rectangular";
        case WIN_HANN:     return "Hann";
        case WIN_HAMMING:  return "Hamming";
        case WIN_BLACKMAN: return "Blackman";
        default:           return "Unknown";
    }
}

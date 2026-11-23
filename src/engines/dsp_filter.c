#include "engines/dsp_filter.h"

#include <math.h>
#include <string.h>

#include "core/cplx.h"
#include "core/poly.h"

static bool valid_cutoffs(RataFilterBand band, double fs, double fc1, double fc2) {
    if (!isfinite(fs) || fs <= 0.0 || !isfinite(fc1)) {
        return false;
    }
    double nyq = 0.5 * fs;
    if (fc1 <= 0.0 || fc1 >= nyq) {
        return false;
    }
    if (band == FILTER_BANDPASS || band == FILTER_BANDSTOP) {
        if (!isfinite(fc2) || fc2 <= fc1 || fc2 >= nyq) {
            return false;
        }
    }
    return true;
}

static double ideal_lp_impulse(double wc, double m) {
    if (fabs(m) < 1.0e-12) {
        return wc / RATA_PI;
    }
    return sin(wc * m) / (RATA_PI * m);
}

RataStatus dsp_design_fir(const RataFirSpec *spec, RataLti *out_sys, RataArena *scratch) {
    if (spec == NULL || out_sys == NULL || scratch == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (spec->taps < 3u || spec->taps > 129u || (spec->taps % 2u) == 0u) {
        return RATA_ERR_RANGE;
    }
    if (!valid_cutoffs(spec->band, spec->fs_hz, spec->fc1_hz, spec->fc2_hz)) {
        return RATA_ERR_RANGE;
    }

    size_t n = spec->taps;
    size_t mid = (n - 1u) / 2u;
    double wc1 = RATA_TWO_PI * spec->fc1_hz / spec->fs_hz;
    double wc2 = RATA_TWO_PI * spec->fc2_hz / spec->fs_hz;

    double w[RATA_LTI_MAX_COEFFS];
    double b[RATA_LTI_MAX_COEFFS];
    RataStatus st = dsp_window_coeffs(spec->window, w, n);
    if (st != RATA_OK) {
        return st;
    }

    for (size_t i = 0u; i < n; ++i) {
        double m = (double)i - (double)mid;
        double delta = (i == mid) ? 1.0 : 0.0;
        double hd = 0.0;
        switch (spec->band) {
            case FILTER_LOWPASS:
                hd = ideal_lp_impulse(wc1, m);
                break;
            case FILTER_HIGHPASS:
                hd = delta - ideal_lp_impulse(wc1, m);
                break;
            case FILTER_BANDPASS:
                hd = ideal_lp_impulse(wc2, m) - ideal_lp_impulse(wc1, m);
                break;
            case FILTER_BANDSTOP:
                hd = delta - (ideal_lp_impulse(wc2, m) - ideal_lp_impulse(wc1, m));
                break;
            default:
                return RATA_ERR_INVALID_ARG;
        }
        b[i] = hd * w[i];
    }

    /* Normalize unity passband gain at reference frequency w_ref. */
    double w_ref = 0.0;
    if (spec->band == FILTER_HIGHPASS) {
        w_ref = RATA_PI;
    } else if (spec->band == FILTER_BANDPASS) {
        w_ref = 0.5 * (wc1 + wc2);
    }

    cplx resp = cplx_zero();
    for (size_t i = 0u; i < n; ++i) {
        double ang = -w_ref * (double)i;
        resp = cplx_add(resp, cplx_make(b[i] * cos(ang), b[i] * sin(ang)));
    }
    double ref_mag = cplx_abs(resp);
    if (ref_mag < 1.0e-12) {
        return RATA_ERR_SINGULAR;
    }
    for (size_t i = 0u; i < n; ++i) {
        b[i] /= ref_mag;
    }

    double a[1] = {1.0};
    return dsp_lti_init(out_sys, b, n, a, 1u, spec->fs_hz);
}

static RataStatus build_analog_prototype(RataIirFamily family,
                                         size_t order,
                                         double ripple_db,
                                         cplx *proto_poles) {
    if (family == IIR_BUTTERWORTH) {
        for (size_t k = 0u; k < order; ++k) {
            double theta = RATA_PI * (double)(2u * k + order + 1u) / (2.0 * (double)order);
            proto_poles[k] = cplx_make(cos(theta), sin(theta));
        }
        return RATA_OK;
    }
    if (family == IIR_CHEBYSHEV1) {
        if (!isfinite(ripple_db) || ripple_db <= 0.0 || ripple_db > 40.0) {
            return RATA_ERR_RANGE;
        }
        double eps = sqrt(pow(10.0, ripple_db / 10.0) - 1.0);
        if (!isfinite(eps) || eps <= 0.0) {
            return RATA_ERR_RANGE;
        }
        double beta = asinh(1.0 / eps) / (double)order;
        double sh = sinh(beta);
        double ch = cosh(beta);
        for (size_t k = 0u; k < order; ++k) {
            double theta = RATA_PI * (double)(2u * k + 1u) / (2.0 * (double)order);
            proto_poles[k] = cplx_make(-sh * sin(theta), ch * cos(theta));
        }
        return RATA_OK;
    }
    return RATA_ERR_INVALID_ARG;
}

static cplx bilinear_map(cplx s, double k_bilin) {
    /* z = (K + s) / (K - s) */
    cplx num = cplx_add(cplx_make(k_bilin, 0.0), s);
    cplx den = cplx_sub(cplx_make(k_bilin, 0.0), s);
    return cplx_div(num, den);
}

RataStatus dsp_design_iir(const RataIirSpec *spec, RataLti *out_sys, RataArena *scratch) {
    if (spec == NULL || out_sys == NULL || scratch == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    bool is_bp_bs = (spec->band == FILTER_BANDPASS || spec->band == FILTER_BANDSTOP);
    size_t max_order = is_bp_bs ? 4u : 8u;
    if (spec->order < 1u || spec->order > max_order) {
        return RATA_ERR_RANGE;
    }
    if (!valid_cutoffs(spec->band, spec->fs_hz, spec->fc1_hz, spec->fc2_hz)) {
        return RATA_ERR_RANGE;
    }

    size_t n = spec->order;
    cplx proto_poles[8];
    RataStatus st = build_analog_prototype(spec->family, n, spec->ripple_db, proto_poles);
    if (st != RATA_OK) {
        return st;
    }

    double k_bilin = 2.0 * spec->fs_hz;
    double om1 = k_bilin * tan(RATA_PI * spec->fc1_hz / spec->fs_hz);
    double om2 = is_bp_bs ? (k_bilin * tan(RATA_PI * spec->fc2_hz / spec->fs_hz)) : 0.0;
    double om0 = is_bp_bs ? sqrt(om1 * om2) : om1;
    double bw = is_bp_bs ? (om2 - om1) : 0.0;

    cplx s_poles[16];
    cplx s_zeros[16];
    size_t np = 0u;
    size_t nz_finite = 0u;

    if (spec->band == FILTER_LOWPASS) {
        for (size_t k = 0u; k < n; ++k) {
            s_poles[np++] = cplx_scale(proto_poles[k], om1);
        }
    } else if (spec->band == FILTER_HIGHPASS) {
        for (size_t k = 0u; k < n; ++k) {
            s_poles[np++] = cplx_div(cplx_make(om1, 0.0), proto_poles[k]);
            s_zeros[nz_finite++] = cplx_zero();
        }
    } else if (spec->band == FILTER_BANDPASS) {
        cplx four_om0_sq = cplx_make(4.0 * om0 * om0, 0.0);
        for (size_t k = 0u; k < n; ++k) {
            cplx bp = cplx_scale(proto_poles[k], bw);
            cplx disc = cplx_sqrt(cplx_sub(cplx_mul(bp, bp), four_om0_sq));
            s_poles[np++] = cplx_scale(cplx_add(bp, disc), 0.5);
            s_poles[np++] = cplx_scale(cplx_sub(bp, disc), 0.5);
            s_zeros[nz_finite++] = cplx_zero();
        }
    } else if (spec->band == FILTER_BANDSTOP) {
        cplx four_om0_sq = cplx_make(4.0 * om0 * om0, 0.0);
        for (size_t k = 0u; k < n; ++k) {
            cplx bp = cplx_div(cplx_make(bw, 0.0), proto_poles[k]);
            cplx disc = cplx_sqrt(cplx_sub(cplx_mul(bp, bp), four_om0_sq));
            s_poles[np++] = cplx_scale(cplx_add(bp, disc), 0.5);
            s_poles[np++] = cplx_scale(cplx_sub(bp, disc), 0.5);
            s_zeros[nz_finite++] = cplx_make(0.0, om0);
            s_zeros[nz_finite++] = cplx_make(0.0, -om0);
        }
    } else {
        return RATA_ERR_INVALID_ARG;
    }

    /* Bilinear transform of poles and zeros (s = inf zeros map to z = -1). */
    cplx z_poles[16];
    cplx z_zeros[16];
    for (size_t i = 0u; i < np; ++i) {
        z_poles[i] = bilinear_map(s_poles[i], k_bilin);
    }
    for (size_t i = 0u; i < nz_finite; ++i) {
        z_zeros[i] = bilinear_map(s_zeros[i], k_bilin);
    }
    for (size_t i = nz_finite; i < np; ++i) {
        z_zeros[i] = cplx_make(-1.0, 0.0);
    }

    size_t mark = rata_arena_mark(scratch);
    RataPoly p_num, p_den;
    st = poly_from_roots_cplx(z_zeros, np, 1.0, &p_num, scratch);
    if (st != RATA_OK) {
        rata_arena_restore(scratch, mark);
        return st;
    }
    st = poly_from_roots_cplx(z_poles, np, 1.0, &p_den, scratch);
    if (st != RATA_OK) {
        rata_arena_restore(scratch, mark);
        return st;
    }

    /* poly_from_roots_cplx produces ascending powers of z (c[0] + ... + c[np]*z^np).
     * Reversing gives powers of z^-1 (b[0] + b[1]*z^-1 + ... + b[np]*z^-np) with a[0] = 1. */
    double b[17];
    double a[17];
    for (size_t i = 0u; i <= np; ++i) {
        b[i] = (np - i <= p_num.deg) ? p_num.coeffs[np - i] : 0.0;
        a[i] = (np - i <= p_den.deg) ? p_den.coeffs[np - i] : 0.0;
    }
    rata_arena_restore(scratch, mark);

    if (fabs(a[0]) < 1.0e-12) {
        return RATA_ERR_SINGULAR;
    }
    for (size_t i = 0u; i <= np; ++i) {
        b[i] /= a[0];
        a[i] /= a[0];
    }

    /* Reference frequency in passband where prototype maps to s = 0. */
    double w_ref = 0.0;
    if (spec->band == FILTER_HIGHPASS) {
        w_ref = RATA_PI;
    } else if (spec->band == FILTER_BANDPASS) {
        w_ref = 2.0 * atan(om0 / k_bilin);
    }

    RataLti raw;
    st = dsp_lti_init(&raw, b, np + 1u, a, np + 1u, spec->fs_hz);
    if (st != RATA_OK) {
        return st;
    }
    cplx h_ref;
    st = dsp_lti_eval_freq(&raw, w_ref, &h_ref);
    if (st != RATA_OK) {
        return st;
    }
    double raw_mag = cplx_abs(h_ref);
    if (raw_mag < 1.0e-12) {
        return RATA_ERR_SINGULAR;
    }

    /* Even-order Chebyshev Type I has passband gain 1/sqrt(1 + eps^2) at prototype DC. */
    double target_gain = 1.0;
    if (spec->family == IIR_CHEBYSHEV1 && (n % 2u) == 0u) {
        double eps_sq = pow(10.0, spec->ripple_db / 10.0) - 1.0;
        target_gain = 1.0 / sqrt(1.0 + eps_sq);
    }

    double gain_scale = target_gain / raw_mag;
    for (size_t i = 0u; i <= np; ++i) {
        b[i] *= gain_scale;
    }

    return dsp_lti_init(out_sys, b, np + 1u, a, np + 1u, spec->fs_hz);
}

const char *dsp_filter_band_name(RataFilterBand band) {
    switch (band) {
        case FILTER_LOWPASS:  return "Low-Pass";
        case FILTER_HIGHPASS: return "High-Pass";
        case FILTER_BANDPASS: return "Band-Pass";
        case FILTER_BANDSTOP: return "Band-Stop";
        default:              return "Unknown";
    }
}

const char *dsp_iir_family_name(RataIirFamily family) {
    switch (family) {
        case IIR_BUTTERWORTH: return "Butterworth";
        case IIR_CHEBYSHEV1:  return "Chebyshev I";
        default:              return "Unknown";
    }
}

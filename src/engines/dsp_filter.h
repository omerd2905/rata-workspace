#ifndef RATA_ENGINES_DSP_FILTER_H
#define RATA_ENGINES_DSP_FILTER_H

#include <stddef.h>

#include "core/arena.h"
#include "core/status.h"
#include "engines/dsp_lti.h"
#include "engines/dsp_signal.h"

typedef enum RataFilterBand {
    FILTER_LOWPASS = 0,
    FILTER_HIGHPASS = 1,
    FILTER_BANDPASS = 2,
    FILTER_BANDSTOP = 3
} RataFilterBand;

typedef enum RataIirFamily {
    IIR_BUTTERWORTH = 0,
    IIR_CHEBYSHEV1 = 1
} RataIirFamily;

typedef struct RataFirSpec {
    RataFilterBand band;
    size_t taps;          /* Must be odd in [3, 129] (Type I linear-phase FIR) */
    double fs_hz;         /* Sampling frequency (Hz), > 0 */
    double fc1_hz;        /* Cutoff frequency (or lower cutoff for BP/BS) in (0, fs/2) */
    double fc2_hz;        /* Upper cutoff for BP/BS in (fc1, fs/2) */
    RataWindowKind window;
} RataFirSpec;

typedef struct RataIirSpec {
    RataIirFamily family;
    RataFilterBand band;
    size_t order;         /* Prototype order: 1..8 for LP/HP, 1..4 for BP/BS (2*order poles) */
    double fs_hz;         /* Sampling frequency (Hz), > 0 */
    double fc1_hz;        /* Cutoff (or lower cutoff for BP/BS) in (0, fs/2) */
    double fc2_hz;        /* Upper cutoff for BP/BS in (fc1, fs/2) */
    double ripple_db;     /* Passband ripple in dB for Chebyshev Type I (> 0, e.g. 0.5) */
} RataIirSpec;

/* Designs a Type-I linear-phase windowed-sinc FIR filter and writes it to `out_sys`.
 * Uses `scratch` for temporary allocations (restores mark). */
RataStatus dsp_design_fir(const RataFirSpec *spec, RataLti *out_sys, RataArena *scratch);

/* Designs a digital IIR Butterworth or Chebyshev Type I filter via s-plane frequency
 * transformation and pre-warped Bilinear Transform, writing it to `out_sys`.
 * Uses `scratch` for temporary allocations (restores mark). */
RataStatus dsp_design_iir(const RataIirSpec *spec, RataLti *out_sys, RataArena *scratch);

const char *dsp_filter_band_name(RataFilterBand band);
const char *dsp_iir_family_name(RataIirFamily family);

#endif /* RATA_ENGINES_DSP_FILTER_H */

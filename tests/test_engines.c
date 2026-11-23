#include "test_runner.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "core/arena.h"
#include "core/cplx.h"
#include "core/fft.h"
#include "core/status.h"
#include "engines/dsp_conv.h"
#include "engines/dsp_filter.h"
#include "engines/dsp_lti.h"
#include "engines/dsp_sampling.h"
#include "engines/dsp_signal.h"
#include "engines/expr.h"
#include "engines/formula_parse.h"
#include "engines/project_file.h"
#include "engines/user_config.h"

static void test_expr_parser_and_primitives(TestContext *t) {
    uint8_t mem[65536];
    RataArena a;
    rata_arena_init(&a, mem, sizeof(mem));

    RataExpr e;
    RataExprError err;
    double val = 0.0;

    /* Precedence & right-associative power: 2 + 3 * 4 = 14, 2^3^2 = 512, -2^2 = -4 */
    test_expect_true(t, expr_parse_cstr("2 + 3 * 4", &e, &err, &a) == RATA_OK, "p1", __LINE__);
    test_expect_true(t, expr_eval(&e, NULL, 0u, &val) == RATA_OK, "e1", __LINE__);
    test_expect_near(t, val, 14.0, 1.0e-12, "2+3*4", __LINE__);

    test_expect_true(t, expr_parse_cstr("2^3^2", &e, &err, &a) == RATA_OK, "p2", __LINE__);
    test_expect_true(t, expr_eval(&e, NULL, 0u, &val) == RATA_OK, "e2", __LINE__);
    test_expect_near(t, val, 512.0, 1.0e-12, "2^3^2", __LINE__);

    test_expect_true(t, expr_parse_cstr("-2^2", &e, &err, &a) == RATA_OK, "p3", __LINE__);
    test_expect_true(t, expr_eval(&e, NULL, 0u, &val) == RATA_OK, "e3", __LINE__);
    test_expect_near(t, val, -4.0, 1.0e-12, "-2^2", __LINE__);

    /* EE primitives: delta, u, sinc, short-circuit if */
    test_expect_true(t, expr_parse_cstr("delta(n-2) + 2*u(n-1) + sinc(n/2)", &e, &err, &a) == RATA_OK,
                     "p4", __LINE__);
    RataExprVar v = {"n", 2.0};
    test_expect_true(t, expr_eval(&e, &v, 1u, &val) == RATA_OK, "e4", __LINE__);
    /* delta(0)=1, 2*u(1)=2, sinc(1)=0 => 3.0 */
    test_expect_near(t, val, 3.0, 1.0e-12, "primitives at n=2", __LINE__);

    v.value = 0.0;
    test_expect_true(t, expr_eval(&e, &v, 1u, &val) == RATA_OK, "e4b", __LINE__);
    /* delta(-2)=0, 2*u(-1)=0, sinc(0)=1 => 1.0 */
    test_expect_near(t, val, 1.0, 1.0e-12, "primitives at n=0", __LINE__);

    test_expect_true(t, expr_parse_cstr("if(n != 0, 10 / n, -1)", &e, &err, &a) == RATA_OK,
                     "if short circuit parse", __LINE__);
    v.value = 0.0;
    test_expect_true(t, expr_eval(&e, &v, 1u, &val) == RATA_OK, "if short circuit eval", __LINE__);
    test_expect_near(t, val, -1.0, 1.0e-12, "if n==0", __LINE__);
    v.value = 2.0;
    test_expect_true(t, expr_eval(&e, &v, 1u, &val) == RATA_OK, "if branch eval", __LINE__);
    test_expect_near(t, val, 5.0, 1.0e-12, "if n==2", __LINE__);

    /* Rejection of invalid inputs */
    test_expect_true(t, expr_parse_cstr("", &e, &err, &a) == RATA_ERR_PARSE, "empty", __LINE__);
    test_expect_true(t, expr_parse_cstr("1 + * 2", &e, &err, &a) == RATA_ERR_PARSE, "bad op", __LINE__);
    test_expect_true(t, expr_parse_cstr("sin(1, 2)", &e, &err, &a) == RATA_ERR_PARSE, "arity", __LINE__);
    test_expect_true(t, expr_parse_cstr("1 / 0", &e, &err, &a) == RATA_OK, "div0 parse", __LINE__);
    test_expect_true(t, expr_eval(&e, NULL, 0u, &val) == RATA_ERR_DIV_ZERO, "div0 eval", __LINE__);
}

static void test_project_file_and_formula_parsers(TestContext *t) {
    uint8_t mem[65536];
    RataArena a;
    rata_arena_init(&a, mem, sizeof(mem));

    RataProjectDoc doc;
    test_expect_true(t, project_doc_init(&doc, 32u, &a) == RATA_OK, "doc init", __LINE__);
    test_expect_true(t, project_doc_set(&doc, "ratawave", "fs", "1000.0") == RATA_OK, "set fs", __LINE__);
    test_expect_true(t, project_doc_set(&doc, "ratawave", "n0", "-4") == RATA_OK, "set n0", __LINE__);
    test_expect_true(t, project_doc_set(&doc, "ratawave", "expr_x", "u(n) - u(n-5)") == RATA_OK,
                     "set expr", __LINE__);

    char buf[4096];
    size_t written = 0u;
    test_expect_true(t, project_file_serialize(&doc, buf, sizeof(buf), &written) == RATA_OK,
                     "serialize", __LINE__);

    RataProjectDoc loaded;
    test_expect_true(t, project_file_parse(buf, written, &loaded, &a) == RATA_OK, "parse back", __LINE__);
    double fs = 0.0;
    int32_t n0 = 0;
    test_expect_true(t, project_doc_get_double(&loaded, "ratawave", "fs", &fs), "get fs", __LINE__);
    test_expect_near(t, fs, 1000.0, 1.0e-12, "fs roundtrip", __LINE__);
    test_expect_true(t, project_doc_get_i32(&loaded, "ratawave", "n0", &n0), "get n0", __LINE__);
    test_expect_true(t, n0 == -4, "n0 roundtrip", __LINE__);
    const char *ex = project_doc_get(&loaded, "ratawave", "expr_x");
    test_expect_true(t, ex != NULL && strcmp(ex, "u(n) - u(n-5)") == 0, "expr roundtrip", __LINE__);

    /* Newer project versions return explicit RATA_ERR_NEWER_VERSION with version number. */
    const char *bad_ver = "RATA_PROJECT 99\n[ratawave]\nfs 1000\n";
    test_expect_true(t, project_file_parse(bad_ver, strlen(bad_ver), &loaded, &a) ==
                     RATA_ERR_NEWER_VERSION, "reject newer version", __LINE__);
    test_expect_true(t, loaded.version == 99u, "report newer version number", __LINE__);
    const char *no_hdr = "[ratawave]\nfs 1000\n";
    test_expect_true(t, project_file_parse(no_hdr, strlen(no_hdr), &loaded, &a) == RATA_ERR_FORMAT,
                     "reject missing header", __LINE__);

    /* Committed v1 golden fixture must always load cleanly. */
    RataProjectDoc v1doc;
    test_expect_true(t, project_file_load_path("tests/fixtures/sample_v1.rata", &v1doc, &a) == RATA_OK,
                     "load committed v1 fixture", __LINE__);
    test_expect_true(t, v1doc.version == 1u, "v1 version == 1", __LINE__);
    const char *v1_ws = project_doc_get(&v1doc, "app", "active_workspace");
    test_expect_true(t, v1_ws != NULL && strcmp(v1_ws, "ratawave") == 0, "v1 workspace", __LINE__);
    double v1_fs = 0.0;
    test_expect_true(t, project_doc_get_double(&v1doc, "ratawave", "fs_hz", &v1_fs), "v1 fs", __LINE__);
    test_expect_near(t, v1_fs, 1000.0, 1.0e-12, "v1 fs==1000", __LINE__);

    /* User config directory resolution (Linux XDG, Linux HOME fallback, Windows APPDATA). */
    char dir[256], path[256];
    test_expect_true(t, user_config_resolve_dir(false, "/tmp/xdg", "/home/u", NULL, dir, sizeof(dir)) == RATA_OK,
                     "xdg dir", __LINE__);
    test_expect_true(t, strcmp(dir, "/tmp/xdg/rata-workspace") == 0, "xdg path exact", __LINE__);
    test_expect_true(t, user_config_resolve_dir(false, NULL, "/home/u/", NULL, dir, sizeof(dir)) == RATA_OK,
                     "home dir", __LINE__);
    test_expect_true(t, strcmp(dir, "/home/u/.config/rata-workspace") == 0, "home fallback", __LINE__);
    test_expect_true(t, user_config_resolve_dir(true, NULL, NULL, "C:\\Users\\Me\\AppData\\Roaming\\",
                     dir, sizeof(dir)) == RATA_OK, "win dir", __LINE__);
    test_expect_true(t, strcmp(dir, "C:\\Users\\Me\\AppData\\Roaming\\RataWorkspace") == 0,
                     "win appdata", __LINE__);
    test_expect_true(t, user_config_join_path(true, dir, "settings.rata", path, sizeof(path)) == RATA_OK,
                     "win join", __LINE__);
    test_expect_true(t, strcmp(path, "C:\\Users\\Me\\AppData\\Roaming\\RataWorkspace\\settings.rata") == 0,
                     "win join exact", __LINE__);

    /* Formula parser valid + malformed tests. */
    FormulaTree ft;
    test_expect_true(t,
        formula_parse_cstr("H(z) = frac{b_0 + b_1 z^{-1}}{1 + a_1 z^{-1}}", &ft, &a) == RATA_OK,
        "formula valid", __LINE__);
    test_expect_true(t,
        formula_parse_cstr("y[n] = \\sum_{k=-\\infty}^{\\infty} x[k] h[n-k]", &ft, &a) == RATA_OK,
        "formula sum", __LINE__);
    test_expect_true(t, formula_parse_cstr("frac{1}{2", &ft, &a) == RATA_ERR_PARSE,
                     "formula unclosed brace", __LINE__);
    test_expect_true(t, formula_parse_cstr("x^2^3", &ft, &a) == RATA_ERR_PARSE,
                     "formula double sup", __LINE__);
}

static void test_convolution_textbook_and_steps(TestContext *t) {
    uint8_t mem[131072];
    RataArena a;
    rata_arena_init(&a, mem, sizeof(mem));

    /* Textbook: [1, 2, 3] * [4, 5] = [4, 13, 22, 15] */
    RataSignal x, h, y, y_comm;
    dsp_signal_alloc(&x, 3u, 0, 1000.0, &a);
    x.samples[0] = 1.0;
    x.samples[1] = 2.0;
    x.samples[2] = 3.0;

    dsp_signal_alloc(&h, 2u, 0, 1000.0, &a);
    h.samples[0] = 4.0;
    h.samples[1] = 5.0;

    test_expect_true(t, dsp_conv_linear(&x, &h, &y, &a) == RATA_OK, "conv ok", __LINE__);
    test_expect_true(t, y.len == 4u && y.n0 == 0, "conv len/n0", __LINE__);
    const double exp_y[4] = {4.0, 13.0, 22.0, 15.0};
    for (size_t i = 0u; i < 4u; ++i) {
        test_expect_near(t, y.samples[i], exp_y[i], 1.0e-12, "conv[i]", __LINE__);
    }

    /* Commutativity: h * x == x * h */
    test_expect_true(t, dsp_conv_linear(&h, &x, &y_comm, &a) == RATA_OK, "comm ok", __LINE__);
    for (size_t i = 0u; i < 4u; ++i) {
        test_expect_near(t, y_comm.samples[i], y.samples[i], 1.0e-12, "comm[i]", __LINE__);
    }

    /* Non-causal index tracking + step-by-step flip-and-shift consistency. */
    RataSignal x2, h2, y2;
    RataExprError err;
    test_expect_true(t, dsp_signal_from_expr("u(n+2) - u(n-2)", -3, 8u, 1000.0, &x2, &err, &a) == RATA_OK,
                     "x2 expr", __LINE__);
    test_expect_true(t, dsp_signal_from_expr("0.5^n * u(n)", -1, 6u, 1000.0, &h2, &err, &a) == RATA_OK,
                     "h2 expr", __LINE__);
    test_expect_true(t, dsp_conv_linear(&x2, &h2, &y2, &a) == RATA_OK, "conv2 ok", __LINE__);
    test_expect_true(t, y2.n0 == -4 && y2.len == 13u, "conv2 n0/len", __LINE__);

    for (size_t i = 0u; i < y2.len; ++i) {
        int32_t n_shift = y2.n0 + (int32_t)i;
        size_t mark = rata_arena_mark(&a);
        RataConvStep step;
        test_expect_true(t, dsp_conv_step(&x2, &h2, n_shift, &step, &a) == RATA_OK,
                         "conv_step ok", __LINE__);
        test_expect_near(t, step.sum, y2.samples[i], 1.0e-12, "step.sum == y[n]", __LINE__);
        rata_arena_restore(&a, mark);
    }

    /* Convolution theorem check against zero-padded FFT (N = 16). */
    cplx X[16], H[16], Y[16], y_ifft[16];
    memset(X, 0, sizeof(X));
    memset(H, 0, sizeof(H));
    for (size_t i = 0u; i < x2.len; ++i) {
        X[i] = cplx_make(x2.samples[i], 0.0);
    }
    for (size_t i = 0u; i < h2.len; ++i) {
        H[i] = cplx_make(h2.samples[i], 0.0);
    }
    fft_forward(X, X, 16u, &a);
    fft_forward(H, H, 16u, &a);
    for (size_t i = 0u; i < 16u; ++i) {
        Y[i] = cplx_mul(X[i], H[i]);
    }
    fft_inverse(Y, y_ifft, 16u, &a);
    for (size_t i = 0u; i < y2.len; ++i) {
        test_expect_near(t, y_ifft[i].re, y2.samples[i], 1.0e-11, "conv theorem", __LINE__);
    }
}

static void test_lti_pole_zero_freqz_and_stability(TestContext *t) {
    uint8_t mem[131072];
    RataArena a;
    rata_arena_init(&a, mem, sizeof(mem));

    /* 1. First-order IIR: H(z) = 1 / (1 - 0.5*z^-1) = z / (z - 0.5) */
    double b1[1] = {1.0};
    double a1[2] = {1.0, -0.5};
    RataLti sys1;
    test_expect_true(t, dsp_lti_init(&sys1, b1, 1u, a1, 2u, 1000.0) == RATA_OK, "lti1 init", __LINE__);

    RataPoleZero pz1;
    test_expect_true(t, dsp_lti_poles_zeros(&sys1, &pz1, &a) == RATA_OK, "pz1 ok", __LINE__);
    test_expect_true(t, pz1.num_poles == 1u && pz1.num_zeros == 1u, "1 pole 1 origin zero", __LINE__);
    test_expect_near(t, pz1.poles[0].re, 0.5, 1.0e-12, "pole re=0.5", __LINE__);
    test_expect_near(t, pz1.poles[0].im, 0.0, 1.0e-12, "pole im=0", __LINE__);
    test_expect_near(t, pz1.zeros[0].re, 0.0, 1.0e-12, "origin zero", __LINE__);
    test_expect_true(t, pz1.stability == LTI_STABLE, "stable", __LINE__);
    test_expect_near(t, pz1.dc_gain, 2.0, 1.0e-12, "DC gain=2", __LINE__);

    /* Analytic impulse h[n] = 0.5^n and step s[n] = 2*(1 - 0.5^(n+1)) */
    RataSignal h1, s1;
    size_t div_at = 0u;
    test_expect_true(t, dsp_lti_impulse_response(&sys1, 20u, &h1, &div_at, &a) == RATA_OK,
                     "imp1", __LINE__);
    test_expect_true(t, dsp_lti_step_response(&sys1, 20u, &s1, &div_at, &a) == RATA_OK,
                     "step1", __LINE__);
    for (size_t n = 0u; n < 20u; ++n) {
        test_expect_near(t, h1.samples[n], pow(0.5, (double)n), 1.0e-12, "h1[n]", __LINE__);
        double expected_s = 2.0 * (1.0 - pow(0.5, (double)(n + 1u)));
        test_expect_near(t, s1.samples[n], expected_s, 1.0e-12, "s1[n]", __LINE__);
    }

    /* Frequency response at omega = pi/2: |H| = 1 / sqrt(1 + 0.25) = 2/sqrt(5) */
    cplx h_half;
    test_expect_true(t, dsp_lti_eval_freq(&sys1, RATA_HALF_PI, &h_half) == RATA_OK, "eval pi/2", __LINE__);
    test_expect_near(t, cplx_abs(h_half), 2.0 / sqrt(5.0), 1.0e-12, "|H(j)|", __LINE__);

    /* 2. 4-point moving average FIR: constant group delay = 1.5 samples, 3 origin poles */
    double b2[4] = {0.25, 0.25, 0.25, 0.25};
    double a2[1] = {1.0};
    RataLti sys2;
    dsp_lti_init(&sys2, b2, 4u, a2, 1u, 1000.0);
    RataPoleZero pz2;
    test_expect_true(t, dsp_lti_poles_zeros(&sys2, &pz2, &a) == RATA_OK, "pz2 ok", __LINE__);
    test_expect_true(t, pz2.num_poles == 3u && pz2.num_zeros == 3u, "ma4 3p 3z", __LINE__);
    for (size_t i = 0u; i < 3u; ++i) {
        test_expect_near(t, cplx_abs(pz2.poles[i]), 0.0, 1.0e-12, "fir origin pole", __LINE__);
        test_expect_near(t, cplx_abs(pz2.zeros[i]), 1.0, 1.0e-11, "ma4 unit circle zero", __LINE__);
    }
    RataFreqz fz2;
    test_expect_true(t, dsp_lti_freqz(&sys2, 65u, false, -100.0, &fz2, &a) == RATA_OK, "fz2", __LINE__);
    /* Check passband bin k=4 (away from zeros at pi/2 and pi): group delay == 1.5 samples */
    test_expect_near(t, fz2.group_delay[4], 1.5, 1.0e-11, "ma4 group delay = 1.5", __LINE__);

    /* 3. Unstable system: H(z) = 1 / (1 - 1.5*z^-1) gracefully caught */
    double a_un[2] = {1.0, -1.5};
    RataLti sys_un;
    dsp_lti_init(&sys_un, b1, 1u, a_un, 2u, 1000.0);
    RataPoleZero pz_un;
    test_expect_true(t, dsp_lti_poles_zeros(&sys_un, &pz_un, &a) == RATA_OK, "pz_un", __LINE__);
    test_expect_true(t, pz_un.stability == LTI_UNSTABLE, "classified unstable", __LINE__);
    RataSignal h_un;
    test_expect_true(t, dsp_lti_impulse_response(&sys_un, 200u, &h_un, &div_at, &a) == RATA_ERR_UNSTABLE,
                     "unstable divergence detected", __LINE__);
    test_expect_true(t, div_at < 200u && isfinite(h_un.samples[0]), "safe clamped output", __LINE__);

    /* 4. Marginally stable oscillator: H(z) = 1 / (1 + z^-2) (poles at +/- j) */
    double a_marg[3] = {1.0, 0.0, 1.0};
    RataLti sys_marg;
    dsp_lti_init(&sys_marg, b1, 1u, a_marg, 3u, 1000.0);
    RataPoleZero pz_marg;
    dsp_lti_poles_zeros(&sys_marg, &pz_marg, &a);
    test_expect_true(t, pz_marg.stability == LTI_MARGINAL, "classified marginal", __LINE__);
    RataFreqz fz_marg;
    test_expect_true(t, dsp_lti_freqz(&sys_marg, 65u, false, -100.0, &fz_marg, &a) == RATA_OK,
                     "marginal freqz safe", __LINE__);
    test_expect_true(t, fz_marg.has_pole_on_grid, "detected pole hit at pi/2", __LINE__);
    for (size_t i = 0u; i < fz_marg.len; ++i) {
        test_expect_true(t, isfinite(fz_marg.mag_db[i]), "finite mag_db on pole hit", __LINE__);
    }
}

static void test_fir_and_iir_filter_design(TestContext *t) {
    uint8_t mem[131072];
    RataArena a;
    rata_arena_init(&a, mem, sizeof(mem));
    const double inv_sqrt2 = 1.0 / sqrt(2.0);

    /* 1. Butterworth 4th-order Low-Pass: fs=1000 Hz, fc=200 Hz */
    RataIirSpec lp_spec = {IIR_BUTTERWORTH, FILTER_LOWPASS, 4u, 1000.0, 200.0, 0.0, 0.0};
    RataLti lp;
    test_expect_true(t, dsp_design_iir(&lp_spec, &lp, &a) == RATA_OK, "butter lp4", __LINE__);
    cplx h0, hfc, hnyq;
    dsp_lti_eval_freq(&lp, 0.0, &h0);
    dsp_lti_eval_freq(&lp, RATA_TWO_PI * 200.0 / 1000.0, &hfc);
    dsp_lti_eval_freq(&lp, RATA_PI, &hnyq);
    test_expect_near(t, cplx_abs(h0), 1.0, 1.0e-10, "butter lp4 DC=1", __LINE__);
    test_expect_near(t, cplx_abs(hfc), inv_sqrt2, 1.0e-9, "butter lp4 -3dB at fc", __LINE__);
    test_expect_near(t, cplx_abs(hnyq), 0.0, 1.0e-10, "butter lp4 nyquist=0", __LINE__);
    RataPoleZero pz_lp;
    dsp_lti_poles_zeros(&lp, &pz_lp, &a);
    test_expect_true(t, pz_lp.stability == LTI_STABLE && pz_lp.num_poles == 4u, "lp4 stable", __LINE__);

    /* 2. Butterworth 3rd-order High-Pass: fs=1000 Hz, fc=250 Hz */
    RataIirSpec hp_spec = {IIR_BUTTERWORTH, FILTER_HIGHPASS, 3u, 1000.0, 250.0, 0.0, 0.0};
    RataLti hp;
    test_expect_true(t, dsp_design_iir(&hp_spec, &hp, &a) == RATA_OK, "butter hp3", __LINE__);
    dsp_lti_eval_freq(&hp, 0.0, &h0);
    dsp_lti_eval_freq(&hp, RATA_TWO_PI * 250.0 / 1000.0, &hfc);
    dsp_lti_eval_freq(&hp, RATA_PI, &hnyq);
    test_expect_near(t, cplx_abs(h0), 0.0, 1.0e-10, "butter hp3 DC=0", __LINE__);
    test_expect_near(t, cplx_abs(hfc), inv_sqrt2, 1.0e-9, "butter hp3 -3dB at fc", __LINE__);
    test_expect_near(t, cplx_abs(hnyq), 1.0, 1.0e-10, "butter hp3 nyquist=1", __LINE__);

    /* 3. Butterworth Band-Pass (order 2 -> 4 poles): fs=1000 Hz, fc1=150 Hz, fc2=300 Hz */
    RataIirSpec bp_spec = {IIR_BUTTERWORTH, FILTER_BANDPASS, 2u, 1000.0, 150.0, 300.0, 0.0};
    RataLti bp;
    test_expect_true(t, dsp_design_iir(&bp_spec, &bp, &a) == RATA_OK, "butter bp2", __LINE__);
    cplx hfc1, hfc2;
    dsp_lti_eval_freq(&bp, RATA_TWO_PI * 150.0 / 1000.0, &hfc1);
    dsp_lti_eval_freq(&bp, RATA_TWO_PI * 300.0 / 1000.0, &hfc2);
    dsp_lti_eval_freq(&bp, 0.0, &h0);
    dsp_lti_eval_freq(&bp, RATA_PI, &hnyq);
    test_expect_near(t, cplx_abs(hfc1), inv_sqrt2, 1.0e-9, "butter bp -3dB at fc1", __LINE__);
    test_expect_near(t, cplx_abs(hfc2), inv_sqrt2, 1.0e-9, "butter bp -3dB at fc2", __LINE__);
    test_expect_near(t, cplx_abs(h0), 0.0, 1.0e-10, "butter bp DC=0", __LINE__);
    test_expect_near(t, cplx_abs(hnyq), 0.0, 1.0e-10, "butter bp nyq=0", __LINE__);

    /* 4. Butterworth Band-Stop (order 2 -> 4 poles): fs=1000 Hz, fc1=150 Hz, fc2=300 Hz */
    RataIirSpec bs_spec = {IIR_BUTTERWORTH, FILTER_BANDSTOP, 2u, 1000.0, 150.0, 300.0, 0.0};
    RataLti bs;
    test_expect_true(t, dsp_design_iir(&bs_spec, &bs, &a) == RATA_OK, "butter bs2", __LINE__);
    dsp_lti_eval_freq(&bs, RATA_TWO_PI * 150.0 / 1000.0, &hfc1);
    dsp_lti_eval_freq(&bs, RATA_TWO_PI * 300.0 / 1000.0, &hfc2);
    dsp_lti_eval_freq(&bs, 0.0, &h0);
    dsp_lti_eval_freq(&bs, RATA_PI, &hnyq);
    test_expect_near(t, cplx_abs(hfc1), inv_sqrt2, 1.0e-9, "butter bs -3dB at fc1", __LINE__);
    test_expect_near(t, cplx_abs(hfc2), inv_sqrt2, 1.0e-9, "butter bs -3dB at fc2", __LINE__);
    test_expect_near(t, cplx_abs(h0), 1.0, 1.0e-10, "butter bs DC=1", __LINE__);
    test_expect_near(t, cplx_abs(hnyq), 1.0, 1.0e-10, "butter bs nyq=1", __LINE__);

    /* 5. Chebyshev Type I 4th-order LP: Rp = 1.0 dB at fc=200 Hz and DC (even order) */
    RataIirSpec ch_spec = {IIR_CHEBYSHEV1, FILTER_LOWPASS, 4u, 1000.0, 200.0, 0.0, 1.0};
    RataLti ch;
    test_expect_true(t, dsp_design_iir(&ch_spec, &ch, &a) == RATA_OK, "cheby1 lp4", __LINE__);
    double expected_rp_mag = pow(10.0, -1.0 / 20.0);
    dsp_lti_eval_freq(&ch, 0.0, &h0);
    dsp_lti_eval_freq(&ch, RATA_TWO_PI * 200.0 / 1000.0, &hfc);
    test_expect_near(t, cplx_abs(h0), expected_rp_mag, 1.0e-9, "cheby1 even DC=-Rp", __LINE__);
    test_expect_near(t, cplx_abs(hfc), expected_rp_mag, 1.0e-8, "cheby1 fc=-Rp", __LINE__);

    /* 6. FIR Hamming LP (31 taps, fs=1000, fc=200): linear phase symmetry & stopband */
    RataFirSpec fir_spec = {FILTER_LOWPASS, 31u, 1000.0, 200.0, 0.0, WIN_HAMMING};
    RataLti fir;
    test_expect_true(t, dsp_design_fir(&fir_spec, &fir, &a) == RATA_OK, "fir lp31", __LINE__);
    for (size_t i = 0u; i < 15u; ++i) {
        test_expect_near(t, fir.b[i], fir.b[30u - i], 1.0e-14, "fir symmetric", __LINE__);
    }
    dsp_lti_eval_freq(&fir, 0.0, &h0);
    cplx hstop;
    dsp_lti_eval_freq(&fir, RATA_TWO_PI * 400.0 / 1000.0, &hstop);
    test_expect_near(t, cplx_abs(h0), 1.0, 1.0e-12, "fir DC=1", __LINE__);
    test_expect_true(t, 20.0 * log10(cplx_abs(hstop)) < -40.0, "fir stopband < -40dB", __LINE__);
}

static void test_sampling_and_aliasing(TestContext *t) {
    uint8_t mem[262144];
    RataArena a;
    rata_arena_init(&a, mem, sizeof(mem));

    double f_alias = 0.0;
    bool is_aliased = false;
    bool phase_neg = false;

    /* 1. Folded pure cosine (phi = 0): f0=800 Hz, fs=1000 Hz => cos(2*pi*200*n/fs), NOT inverted */
    test_expect_true(t, dsp_alias_frequency(800.0, 1000.0, &f_alias, &is_aliased, &phase_neg) == RATA_OK,
                     "alias 800/1000", __LINE__);
    test_expect_true(t, is_aliased && phase_neg, "aliased + phase_negated", __LINE__);
    test_expect_near(t, f_alias, 200.0, 1.0e-12, "f_alias=200", __LINE__);
    RataSamplingConfig cfg_cos = {800.0, 1.0, 0.0, 1000.0, 0.01, 101u};
    RataSamplingResult res_cos;
    test_expect_true(t, dsp_sampling_simulate(&cfg_cos, &res_cos, &a) == RATA_OK, "sim cos", __LINE__);
    test_expect_near(t, res_cos.aliased_phase_rad, 0.0, 1.0e-12, "cos aliased phase==0", __LINE__);
    for (size_t n = 0u; n < res_cos.sample_len; ++n) {
        double tn = res_cos.t_samples[n];
        double expected = cos(RATA_TWO_PI * 200.0 * tn);
        test_expect_near(t, res_cos.x_samples[n], expected, 1.0e-12, "folded cos sample", __LINE__);
    }

    /* 2. Folded sinusoid with nonzero, non-(-pi/2) phase: phi = 0.7 rad, f0 = 800 Hz, fs = 1000 Hz.
     * Alias MUST be cos(2*pi*200*t - 0.7), NOT +0.7 and NOT (0.7 + pi). */
    const double phi = 0.7;
    RataSamplingConfig cfg_phi = {800.0, 1.5, phi, 1000.0, 0.01, 101u};
    RataSamplingResult res_phi;
    test_expect_true(t, dsp_sampling_simulate(&cfg_phi, &res_phi, &a) == RATA_OK, "sim phi", __LINE__);
    test_expect_true(t, res_phi.is_aliased && res_phi.phase_negated, "phi aliased+negated", __LINE__);
    test_expect_near(t, res_phi.aliased_freq_hz, 200.0, 1.0e-12, "phi f_alias=200", __LINE__);
    test_expect_near(t, res_phi.aliased_phase_rad, -phi, 1.0e-12, "phi -> -phi", __LINE__);
    bool differed_from_plus_phi = false;
    bool differed_from_pi_shift = false;
    for (size_t n = 0u; n < res_phi.sample_len; ++n) {
        double tn = res_phi.t_samples[n];
        double expected = 1.5 * cos(RATA_TWO_PI * 200.0 * tn - phi);
        double wrong_plus = 1.5 * cos(RATA_TWO_PI * 200.0 * tn + phi);
        double wrong_pi = 1.5 * cos(RATA_TWO_PI * 200.0 * tn + phi + RATA_PI);
        test_expect_near(t, res_phi.x_samples[n], expected, 1.0e-12, "folded phi sample", __LINE__);
        if (fabs(res_phi.x_samples[n] - wrong_plus) > 0.1) {
            differed_from_plus_phi = true;
        }
        if (fabs(res_phi.x_samples[n] - wrong_pi) > 0.1) {
            differed_from_pi_shift = true;
        }
    }
    test_expect_true(t, differed_from_plus_phi, "not +phi", __LINE__);
    test_expect_true(t, differed_from_pi_shift, "not pi-shifted", __LINE__);

    /* 3. Arbitrary higher-zone fold: f0 = 2700 Hz, fs = 1000 Hz => rem = 700 > 500 => 300 Hz, -phi. */
    RataSamplingConfig cfg_hi = {2700.0, 1.0, phi, 1000.0, 0.01, 101u};
    RataSamplingResult res_hi;
    test_expect_true(t, dsp_sampling_simulate(&cfg_hi, &res_hi, &a) == RATA_OK, "sim 2700", __LINE__);
    test_expect_true(t, res_hi.is_aliased && res_hi.phase_negated, "2700 negated", __LINE__);
    test_expect_near(t, res_hi.aliased_freq_hz, 300.0, 1.0e-12, "2700 -> 300", __LINE__);
    test_expect_near(t, res_hi.aliased_phase_rad, -phi, 1.0e-12, "2700 -> -phi", __LINE__);
    for (size_t n = 0u; n < res_hi.sample_len; ++n) {
        double tn = res_hi.t_samples[n];
        test_expect_near(t, res_hi.x_samples[n], cos(RATA_TWO_PI * 300.0 * tn - phi),
                         1.0e-11, "2700 sample match", __LINE__);
    }

    /* 4. Even-zone fold: f0=1300 Hz, fs=1000 Hz => aliased to 300 Hz, phase NOT negated (+phi). */
    test_expect_true(t, dsp_alias_frequency(1300.0, 1000.0, &f_alias, &is_aliased, &phase_neg) == RATA_OK,
                     "alias 1300/1000", __LINE__);
    test_expect_true(t, is_aliased && !phase_neg, "aliased, not negated", __LINE__);
    test_expect_near(t, f_alias, 300.0, 1.0e-12, "f_alias=300", __LINE__);

    /* 5. Well-sampled sinc reconstruction matches analog signal cleanly. */
    RataSamplingConfig cfg = {100.0, 1.0, 0.25, 1000.0, 0.02, 201u};
    RataSamplingResult res;
    test_expect_true(t, dsp_sampling_simulate(&cfg, &res, &a) == RATA_OK, "sim ok", __LINE__);
    test_expect_true(t, !res.is_aliased, "not aliased", __LINE__);
    test_expect_near(t, res.nyquist_freq_hz, 500.0, 1.0e-12, "nyq=500", __LINE__);
    for (size_t i = 20u; i < 180u; i += 10u) {
        test_expect_near(t, res.x_recon_sinc[i], res.x_analog[i], 2.0e-2, "sinc recon", __LINE__);
    }
}

void run_engine_tests(TestContext *t) {
    printf("=== Engine & DSP Suite ===\n");
    test_run(t, "expr_parser_and_primitives", test_expr_parser_and_primitives);
    test_run(t, "project_file_and_formula_parsers", test_project_file_and_formula_parsers);
    test_run(t, "convolution_textbook_and_steps", test_convolution_textbook_and_steps);
    test_run(t, "lti_pole_zero_freqz_and_stability", test_lti_pole_zero_freqz_and_stability);
    test_run(t, "fir_and_iir_filter_design", test_fir_and_iir_filter_design);
    test_run(t, "sampling_and_aliasing", test_sampling_and_aliasing);
}

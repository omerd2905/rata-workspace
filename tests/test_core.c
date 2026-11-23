#include "test_runner.h"

#include <math.h>
#include <stdint.h>

#include "core/arena.h"
#include "core/cplx.h"
#include "core/fft.h"
#include "core/poly.h"
#include "core/status.h"

static void test_arena_basic_and_marks(TestContext *t) {
    uint8_t buf[256];
    RataArena a;
    test_expect_true(t, rata_arena_init(&a, buf, sizeof(buf)) == RATA_OK, "init", __LINE__);
    test_expect_true(t, rata_arena_remaining(&a) == 256u, "rem 256", __LINE__);

    size_t m0 = rata_arena_mark(&a);
    double *p1 = (double *)rata_arena_calloc(&a, 4u, sizeof(double), 8u);
    test_expect_true(t, p1 != NULL, "alloc p1", __LINE__);
    test_expect_true(t, ((uintptr_t)p1 % 8u) == 0u, "aligned 8", __LINE__);
    test_expect_near(t, p1[3], 0.0, 0.0, "calloc zeroed", __LINE__);

    size_t m1 = rata_arena_mark(&a);
    void *too_big = rata_arena_alloc(&a, 1024u, 8u);
    test_expect_true(t, too_big == NULL, "oom returns NULL", __LINE__);
    test_expect_true(t, rata_arena_mark(&a) == m1, "oom preserves offset", __LINE__);

    rata_arena_restore(&a, m0);
    test_expect_true(t, rata_arena_used(&a) == 0u, "restore mark", __LINE__);

    RataArena ha;
    test_expect_true(t, rata_arena_init_heap(&ha, 128u) == RATA_OK, "heap init", __LINE__);
    test_expect_true(t, rata_arena_alloc(&ha, 64u, 16u) != NULL, "heap alloc", __LINE__);
    rata_arena_destroy(&ha);
    test_expect_true(t, ha.base == NULL && ha.cap == 0u, "heap destroy", __LINE__);
}

static void test_cplx_arithmetic_and_elementary(TestContext *t) {
    const double tol = 1.0e-12;
    cplx a = cplx_make(3.0, 4.0);
    cplx b = cplx_make(1.0, -2.0);

    test_expect_near(t, cplx_abs(a), 5.0, tol, "|3+4j| == 5", __LINE__);
    test_expect_near(t, cplx_abs2(a), 25.0, tol, "|3+4j|^2 == 25", __LINE__);

    /* (3+4j)*(1-2j) = (3+8) + (-6+4)j = 11 - 2j */
    cplx prod = cplx_mul(a, b);
    test_expect_near(t, prod.re, 11.0, tol, "mul re", __LINE__);
    test_expect_near(t, prod.im, -2.0, tol, "mul im", __LINE__);

    /* (3+4j)/(1-2j) = (3-8)/5 + (4+6)/5 j = -1 + 2j */
    cplx quot;
    test_expect_true(t, cplx_div_safe(a, b, &quot) == RATA_OK, "div ok", __LINE__);
    test_expect_near(t, quot.re, -1.0, tol, "div re", __LINE__);
    test_expect_near(t, quot.im, 2.0, tol, "div im", __LINE__);

    /* Division by zero safely caught. */
    test_expect_true(t, cplx_div_safe(a, cplx_zero(), &quot) == RATA_ERR_DIV_ZERO, "div0", __LINE__);

    /* Overflow-safe magnitude and division for huge values (1e200). */
    cplx huge1 = cplx_make(3.0e200, 4.0e200);
    cplx huge2 = cplx_make(1.0e200, 0.0);
    test_expect_near(t, cplx_abs(huge1) / 1.0e200, 5.0, tol, "huge abs", __LINE__);
    test_expect_true(t, cplx_div_safe(huge1, huge2, &quot) == RATA_OK, "huge div", __LINE__);
    test_expect_near(t, quot.re, 3.0, tol, "huge div re", __LINE__);
    test_expect_near(t, quot.im, 4.0, tol, "huge div im", __LINE__);

    /* Euler identity: exp(j*pi) + 1 = 0 */
    cplx e_jpi = cplx_exp(cplx_make(0.0, RATA_PI));
    test_expect_near(t, e_jpi.re, -1.0, tol, "exp(j*pi) re", __LINE__);
    test_expect_near(t, e_jpi.im, 0.0, tol, "exp(j*pi) im", __LINE__);

    /* sqrt(-4) = 2j, sqrt(3 + 4j) = 2 + j */
    cplx s1 = cplx_sqrt(cplx_make(-4.0, 0.0));
    test_expect_near(t, s1.re, 0.0, tol, "sqrt(-4) re", __LINE__);
    test_expect_near(t, s1.im, 2.0, tol, "sqrt(-4) im", __LINE__);
    cplx s2 = cplx_sqrt(a);
    test_expect_near(t, s2.re, 2.0, tol, "sqrt(3+4j) re", __LINE__);
    test_expect_near(t, s2.im, 1.0, tol, "sqrt(3+4j) im", __LINE__);

    /* Integer powers: (1+j)^4 = -4 */
    cplx p4 = cplx_pow_int(cplx_make(1.0, 1.0), 4);
    test_expect_near(t, p4.re, -4.0, tol, "(1+j)^4 re", __LINE__);
    test_expect_near(t, p4.im, 0.0, tol, "(1+j)^4 im", __LINE__);
}

static void test_poly_arithmetic_and_divmod(TestContext *t) {
    uint8_t mem[8192];
    RataArena a;
    rata_arena_init(&a, mem, sizeof(mem));

    /* P(x) = (x - 1)(x - 2)(x + 3) = x^3 - 7x + 6 -> ascending [6, -7, 0, 1] */
    double pc[4] = {6.0, -7.0, 0.0, 1.0};
    double qc[2] = {-2.0, 1.0}; /* x - 2 */
    double pbuf[4], qbuf[2];
    RataPoly P, Q;
    poly_init(&P, pbuf, 4u, pc, 4u);
    poly_init(&Q, qbuf, 2u, qc, 2u);

    test_expect_near(t, poly_eval_real(&P, 2.0), 0.0, 1.0e-12, "P(2)==0", __LINE__);
    test_expect_near(t, poly_eval_real(&P, -3.0), 0.0, 1.0e-12, "P(-3)==0", __LINE__);

    RataPoly quot, rem;
    test_expect_true(t, poly_divmod(&P, &Q, &quot, &rem, &a) == RATA_OK, "divmod ok", __LINE__);
    /* (x-1)(x+3) = x^2 + 2x - 3 -> ascending [-3, 2, 1] */
    test_expect_true(t, quot.deg == 2u, "quot deg 2", __LINE__);
    test_expect_near(t, quot.coeffs[0], -3.0, 1.0e-12, "q[0]", __LINE__);
    test_expect_near(t, quot.coeffs[1], 2.0, 1.0e-12, "q[1]", __LINE__);
    test_expect_near(t, quot.coeffs[2], 1.0, 1.0e-12, "q[2]", __LINE__);
    test_expect_true(t, poly_is_zero(&rem, 1.0e-12), "rem == 0", __LINE__);

    /* Multiply back: quot * Q == P */
    RataPoly prod;
    test_expect_true(t, poly_mul(&quot, &Q, &prod, &a) == RATA_OK, "mul ok", __LINE__);
    test_expect_true(t, prod.deg == 3u, "prod deg 3", __LINE__);
    for (size_t i = 0u; i <= 3u; ++i) {
        test_expect_near(t, prod.coeffs[i], pc[i], 1.0e-12, "prod coeff", __LINE__);
    }
}

static void test_poly_roots_textbook(TestContext *t) {
    uint8_t mem[16384];
    RataArena scratch;
    rata_arena_init(&scratch, mem, sizeof(mem));

    /* Case 1: Quadratic with complex conjugate roots: z^2 - 2z + 5 = 0 => 1 +/- 2j */
    double c_quad[3] = {5.0, -2.0, 1.0};
    double qbuf[3];
    RataPoly pq;
    poly_init(&pq, qbuf, 3u, c_quad, 3u);
    cplx roots[16];
    size_t nroots = 0u;
    test_expect_true(t, poly_roots(&pq, roots, 16u, &nroots, &scratch) == RATA_OK, "quad ok", __LINE__);
    test_expect_true(t, nroots == 2u, "quad 2 roots", __LINE__);
    test_expect_near(t, roots[0].re, 1.0, 1.0e-12, "q r0 re", __LINE__);
    test_expect_near(t, roots[0].im, 2.0, 1.0e-12, "q r0 im", __LINE__);
    test_expect_near(t, roots[1].re, 1.0, 1.0e-12, "q r1 re", __LINE__);
    test_expect_near(t, roots[1].im, -2.0, 1.0e-12, "q r1 im", __LINE__);

    /* Case 2: 5th roots of unity: z^5 - 1 = 0 (all roots on unit circle, exact residual) */
    double c_u5[6] = {-1.0, 0.0, 0.0, 0.0, 0.0, 1.0};
    double u5buf[6];
    RataPoly pu5;
    poly_init(&pu5, u5buf, 6u, c_u5, 6u);
    test_expect_true(t, poly_roots(&pu5, roots, 16u, &nroots, &scratch) == RATA_OK, "u5 ok", __LINE__);
    test_expect_true(t, nroots == 5u, "u5 5 roots", __LINE__);
    for (size_t i = 0u; i < nroots; ++i) {
        test_expect_near(t, cplx_abs(roots[i]), 1.0, 1.0e-11, "u5 |r|==1", __LINE__);
        test_expect_near(t, cplx_abs(poly_eval_cplx(&pu5, roots[i])), 0.0, 1.0e-11, "u5 P(r)==0", __LINE__);
    }

    /* Case 3: Mixed zero root, real roots, and complex conjugate pair built from known roots:
     * r = {0, 0, -2, 0.5 + 0.5j, 0.5 - 0.5j, 3} */
    cplx expected[6] = {
        {3.0, 0.0}, {0.5, 0.5}, {0.5, -0.5}, {0.0, 0.0}, {0.0, 0.0}, {-2.0, 0.0}
    };
    RataPoly pmix;
    test_expect_true(t, poly_from_roots_cplx(expected, 6u, 2.5, &pmix, &scratch) == RATA_OK,
                     "from_roots", __LINE__);
    test_expect_true(t, poly_roots(&pmix, roots, 16u, &nroots, &scratch) == RATA_OK,
                     "mixed roots ok", __LINE__);
    test_expect_true(t, nroots == 6u, "mixed 6 roots", __LINE__);
    for (size_t i = 0u; i < 6u; ++i) {
        test_expect_near(t, roots[i].re, expected[i].re, 1.0e-10, "mixed re", __LINE__);
        test_expect_near(t, roots[i].im, expected[i].im, 1.0e-10, "mixed im", __LINE__);
    }

    /* Case 4: Repeated root (z - 1)^3 = z^3 - 3z^2 + 3z - 1 (tol 1e-4 for multiplicity 3 in f64) */
    double c_rep[4] = {-1.0, 3.0, -3.0, 1.0};
    double repbuf[4];
    RataPoly prep;
    poly_init(&prep, repbuf, 4u, c_rep, 4u);
    test_expect_true(t, poly_roots(&prep, roots, 16u, &nroots, &scratch) == RATA_OK,
                     "repeated root ok", __LINE__);
    test_expect_true(t, nroots == 3u, "repeated 3 roots", __LINE__);
    for (size_t i = 0u; i < 3u; ++i) {
        test_expect_near(t, roots[i].re, 1.0, 1.0e-4, "triple root re", __LINE__);
        test_expect_near(t, roots[i].im, 0.0, 1.0e-4, "triple root im", __LINE__);
    }
}

static void test_fft_textbook_radix2_and_arbitrary(TestContext *t) {
    uint8_t mem[65536];
    RataArena scratch;
    rata_arena_init(&scratch, mem, sizeof(mem));
    const double tol = 1.0e-11;

    /* Textbook 4-point DFT: x = [1, 2, 3, 4] => X = [10, -2+2j, -2, -2-2j] */
    cplx x4[4] = {{1.0, 0.0}, {2.0, 0.0}, {3.0, 0.0}, {4.0, 0.0}};
    cplx X4[4];
    cplx x4_rec[4];
    test_expect_true(t, fft_forward(x4, X4, 4u, &scratch) == RATA_OK, "fft4 ok", __LINE__);
    test_expect_near(t, X4[0].re, 10.0, tol, "X4[0].re", __LINE__);
    test_expect_near(t, X4[0].im, 0.0, tol, "X4[0].im", __LINE__);
    test_expect_near(t, X4[1].re, -2.0, tol, "X4[1].re", __LINE__);
    test_expect_near(t, X4[1].im, 2.0, tol, "X4[1].im", __LINE__);
    test_expect_near(t, X4[2].re, -2.0, tol, "X4[2].re", __LINE__);
    test_expect_near(t, X4[2].im, 0.0, tol, "X4[2].im", __LINE__);
    test_expect_near(t, X4[3].re, -2.0, tol, "X4[3].re", __LINE__);
    test_expect_near(t, X4[3].im, -2.0, tol, "X4[3].im", __LINE__);

    test_expect_true(t, fft_inverse(X4, x4_rec, 4u, &scratch) == RATA_OK, "ifft4 ok", __LINE__);
    for (size_t i = 0u; i < 4u; ++i) {
        test_expect_near(t, x4_rec[i].re, x4[i].re, tol, "ifft4 re", __LINE__);
        test_expect_near(t, x4_rec[i].im, 0.0, tol, "ifft4 im", __LINE__);
    }

    /* Known cosine DFT: x[n] = cos(2*pi*3*n/16) => X[3] = X[13] = 8, others 0 */
    double cos16[16];
    cplx X16[16];
    for (size_t n = 0u; n < 16u; ++n) {
        cos16[n] = cos(RATA_TWO_PI * 3.0 * (double)n / 16.0);
    }
    test_expect_true(t, fft_real_forward(cos16, X16, 16u, &scratch) == RATA_OK, "cos16 fft", __LINE__);
    for (size_t k = 0u; k < 16u; ++k) {
        double expected_re = (k == 3u || k == 13u) ? 8.0 : 0.0;
        test_expect_near(t, X16[k].re, expected_re, tol, "cos16 re", __LINE__);
        test_expect_near(t, X16[k].im, 0.0, tol, "cos16 im", __LINE__);
    }

    /* Arbitrary non-power-of-two lengths (Bluestein vs direct DFT + Parseval + roundtrip):
     * Test prime N=7, composite N=15, prime N=37, and N=100. */
    const size_t lengths[4] = {7u, 15u, 37u, 100u};
    for (size_t li = 0u; li < 4u; ++li) {
        size_t N = lengths[li];
        cplx in[100], out_fft[100], out_dft[100], rec[100];
        double energy_time = 0.0;
        for (size_t n = 0u; n < N; ++n) {
            in[n] = cplx_make(sin(0.37 * (double)n) + 0.5 * cos(1.1 * (double)n),
                              0.25 * cos(0.73 * (double)n));
            energy_time += cplx_abs2(in[n]);
        }
        test_expect_true(t, dft_direct_forward(in, out_dft, N) == RATA_OK, "dft direct", __LINE__);
        test_expect_true(t, fft_forward(in, out_fft, N, &scratch) == RATA_OK, "bluestein fwd", __LINE__);

        double energy_freq = 0.0;
        for (size_t k = 0u; k < N; ++k) {
            test_expect_near(t, out_fft[k].re, out_dft[k].re, 1.0e-10, "bluestein re vs dft", __LINE__);
            test_expect_near(t, out_fft[k].im, out_dft[k].im, 1.0e-10, "bluestein im vs dft", __LINE__);
            energy_freq += cplx_abs2(out_fft[k]);
        }
        /* Parseval's theorem: sum |x[n]|^2 == (1/N) sum |X[k]|^2 */
        test_expect_near(t, energy_freq / (double)N, energy_time, 1.0e-9, "Parseval", __LINE__);

        test_expect_true(t, fft_inverse(out_fft, rec, N, &scratch) == RATA_OK, "bluestein inv", __LINE__);
        for (size_t n = 0u; n < N; ++n) {
            test_expect_near(t, rec[n].re, in[n].re, 1.0e-10, "roundtrip re", __LINE__);
            test_expect_near(t, rec[n].im, in[n].im, 1.0e-10, "roundtrip im", __LINE__);
        }
    }

    /* Verify scratch arena mark was cleanly restored (0 bytes leaked in arena). */
    test_expect_true(t, rata_arena_used(&scratch) == 0u, "fft scratch restored", __LINE__);
}

void run_core_tests(TestContext *t) {
    printf("=== Core Math Suite ===\n");
    test_run(t, "arena_basic_and_marks", test_arena_basic_and_marks);
    test_run(t, "cplx_arithmetic_and_elementary", test_cplx_arithmetic_and_elementary);
    test_run(t, "poly_arithmetic_and_divmod", test_poly_arithmetic_and_divmod);
    test_run(t, "poly_roots_textbook", test_poly_roots_textbook);
    test_run(t, "fft_textbook_radix2_and_arbitrary", test_fft_textbook_radix2_and_arbitrary);
}

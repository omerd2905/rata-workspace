#ifndef RATA_TEST_RUNNER_H
#define RATA_TEST_RUNNER_H

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct TestContext {
    int passed;
    int failed;
    const char *current_test;
} TestContext;

typedef void (*TestFn)(TestContext *t);

static inline void test_expect_true(TestContext *t, bool cond, const char *expr, int line) {
    if (cond) {
        t->passed++;
    } else {
        t->failed++;
        fprintf(stderr, "  [FAIL] %s (line %d): expected true: %s\n", t->current_test, line, expr);
    }
}

static inline void test_expect_near(TestContext *t,
                                    double actual,
                                    double expected,
                                    double tol,
                                    const char *label,
                                    int line) {
    double diff = fabs(actual - expected);
    if (isfinite(actual) && diff <= tol) {
        t->passed++;
    } else {
        t->failed++;
        fprintf(stderr,
                "  [FAIL] %s (line %d): %s got %.15g, expected %.15g (|diff|=%.3e > tol=%.3e)\n",
                t->current_test,
                line,
                label,
                actual,
                expected,
                diff,
                tol);
    }
}

static inline void test_run(TestContext *t, const char *name, TestFn fn) {
    int before_fail = t->failed;
    t->current_test = name;
    fn(t);
    if (t->failed == before_fail) {
        printf("  [PASS] %s\n", name);
    }
}

/* Suite entry points (implemented in separate test_*.c files). */
void run_core_tests(TestContext *t);
void run_engine_tests(TestContext *t);

#endif /* RATA_TEST_RUNNER_H */

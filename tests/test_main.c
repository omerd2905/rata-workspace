#include "test_runner.h"

#include <stdio.h>

int main(void) {
    TestContext t;
    t.passed = 0;
    t.failed = 0;
    t.current_test = "";

    run_core_tests(&t);
    run_engine_tests(&t);

    printf("\nSummary: %d assertions passed, %d failed.\n", t.passed, t.failed);
    return (t.failed == 0) ? 0 : 1;
}

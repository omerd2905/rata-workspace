#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/arena.h"
#include "engines/expr.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4096u) {
        size = 4096u;
    }
    uint8_t mem[131072];
    RataArena arena;
    if (rata_arena_init(&arena, mem, sizeof(mem)) != RATA_OK) {
        return 0;
    }

    RataExpr expr;
    RataExprError err;
    RataStatus st = expr_parse((const char *)data, size, &expr, &err, &arena);
    if (st == RATA_OK) {
        RataExprVar vars[3] = {
            {"n", 3.0},
            {"t", 0.01},
            {"fs", 1000.0}
        };
        double out = 0.0;
        (void)expr_eval(&expr, vars, 3u, &out);

        vars[0].value = 0.0;
        (void)expr_eval(&expr, vars, 3u, &out);

        vars[0].value = -5.0;
        (void)expr_eval(&expr, vars, 3u, &out);
    }
    return 0;
}

#ifdef FUZZ_STANDALONE
static uint64_t xs64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x << 13u;
    x ^= x >> 7u;
    x ^= x << 17u;
    *state = x;
    return x;
}

int main(void) {
    static const char *seeds[] = {
        "sin(2*pi*50*n/fs) + 0.5*cos(2*pi*120*n/fs)",
        "if(n >= 0, 0.8^n * u(n), 0)",
        "delta(n) + 2*delta(n-1) - rect(n/4) + tri(n/3) + sinc(n/2)",
        "1 / (n - 3)",
        "((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((((1))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))",
        "1e99999 + 0.0^ -1",
        "atan2(n, t) + mod(n, 0) + ln(-1) + sqrt(-2)",
        "",
        "   ",
        "foo(1, 2, 3, 4)",
        "2^3^2^-1",
        "n <= 5 == 1 != 0"
    };
    size_t nseeds = sizeof(seeds) / sizeof(seeds[0]);
    for (size_t i = 0u; i < nseeds; ++i) {
        LLVMFuzzerTestOneInput((const uint8_t *)seeds[i], strlen(seeds[i]));
    }

    uint64_t rng = 0x123456789ABCDEF0ULL;
    uint8_t buf[256];
    for (size_t iter = 0u; iter < 5000u; ++iter) {
        size_t len = (size_t)(xs64(&rng) % sizeof(buf));
        if ((iter & 1u) == 0u) {
            const char *s = seeds[iter % nseeds];
            size_t slen = strlen(s);
            if (slen > sizeof(buf)) {
                slen = sizeof(buf);
            }
            memcpy(buf, s, slen);
            len = slen;
            if (len > 0u) {
                size_t flips = 1u + (size_t)(xs64(&rng) % 4u);
                for (size_t f = 0u; f < flips; ++f) {
                    buf[xs64(&rng) % len] = (uint8_t)(xs64(&rng) & 0xFFu);
                }
            }
        } else {
            for (size_t j = 0u; j < len; ++j) {
                buf[j] = (uint8_t)(xs64(&rng) & 0xFFu);
            }
        }
        LLVMFuzzerTestOneInput(buf, len);
    }
    printf("fuzz_expr: passed %zu seeds + 5000 mutations.\n", nseeds);
    return 0;
}
#endif

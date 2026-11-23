#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/arena.h"
#include "engines/formula_parse.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 4096u) {
        size = 4096u;
    }
    uint8_t mem[65536];
    RataArena arena;
    if (rata_arena_init(&arena, mem, sizeof(mem)) != RATA_OK) {
        return 0;
    }

    FormulaTree tree;
    (void)formula_parse((const char *)data, size, &tree, &arena);
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
        "H(z) = frac{b_0 + b_1 z^{-1}}{a_0 + a_1 z^{-1}}",
        "y[n] = \\sum_{k=-\\infty}^{\\infty} x[k] h[n - k]",
        "\\omega_c = 2 \\pi f_c, \\zeta = frac{1}{sqrt{2}}",
        "frac{frac{frac{1}{2}}{3}}{4}",
        "{{{{{{{{{{{{{{{{{{{{{{{{a}}}}}}}}}}}}}}}}}}}}}}}}",
        "x^2^3",
        "\\unknowncmd + frac{1}",
        "sqrt{1 - \\zeta^2} \\pm \\Delta \\Omega",
        ""
    };
    size_t nseeds = sizeof(seeds) / sizeof(seeds[0]);
    for (size_t i = 0u; i < nseeds; ++i) {
        LLVMFuzzerTestOneInput((const uint8_t *)seeds[i], strlen(seeds[i]));
    }

    uint64_t rng = 0xDEADBEEF87654321ULL;
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
    printf("fuzz_formula: passed %zu seeds + 5000 mutations.\n", nseeds);
    return 0;
}
#endif

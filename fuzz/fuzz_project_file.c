#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/arena.h"
#include "engines/project_file.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size > 16384u) {
        size = 16384u;
    }
    uint8_t mem[131072];
    RataArena arena;
    if (rata_arena_init(&arena, mem, sizeof(mem)) != RATA_OK) {
        return 0;
    }

    RataProjectDoc doc;
    RataStatus st = project_file_parse((const char *)data, size, &doc, &arena);
    if (st == RATA_OK) {
        char out_buf[32768];
        size_t written = 0u;
        (void)project_file_serialize(&doc, out_buf, sizeof(out_buf), &written);
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
        "RATA_PROJECT 1\n[ratawave]\nfs 1000.0\nexpr_x sin(2*pi*50*n/fs)\n",
        "RATA_PROJECT 2\n[ratawave]\nkey val\n",
        "RATA_PROJECT 1\n[bad section]\nk v\n",
        "RATA_PROJECT 1\n[]\n",
        "NOT_A_HEADER\n",
        "# comment\nRATA_PROJECT 1\r\n[sec]\r\nempty_key\r\n",
        ""
    };
    size_t nseeds = sizeof(seeds) / sizeof(seeds[0]);
    for (size_t i = 0u; i < nseeds; ++i) {
        LLVMFuzzerTestOneInput((const uint8_t *)seeds[i], strlen(seeds[i]));
    }

    uint64_t rng = 0xCAFEBABE1337BEEFULL;
    uint8_t buf[512];
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
                size_t flips = 1u + (size_t)(xs64(&rng) % 5u);
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
    printf("fuzz_project_file: passed %zu seeds + 5000 mutations.\n", nseeds);
    return 0;
}
#endif

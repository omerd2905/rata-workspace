#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "engines/update_manifest.h"

static void fuzz_one(const uint8_t *data, size_t size) {
    if (size > 4096u) {
        size = 4096u;
    }
    RataUpdateManifest m;
    RataStatus st = update_manifest_parse((const char *)data, size, &m);
    if (st == RATA_OK) {
        char formatted[RATA_UPDATE_MANIFEST_MAX];
        size_t flen = 0u;
        (void)update_manifest_format(&m, formatted, sizeof(formatted), &flen);

        char payload[RATA_UPDATE_PAYLOAD_MAX];
        size_t plen = 0u;
        (void)update_manifest_signing_payload(&m, payload, sizeof(payload), &plen);

        uint8_t pub[RATA_ED25519_PUBKEY_LEN];
        memset(pub, 0x42, sizeof(pub));
        (void)update_manifest_verify_signature(&m, pub);
    }
}

#ifdef FUZZ_STANDALONE
static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13u;
    x ^= x >> 17u;
    x ^= x << 5u;
    *state = x;
    return x;
}

int main(void) {
    static const char *SEEDS[] = {
        "",
        "RATA_UPDATE_MANIFEST 1\n",
        "RATA_UPDATE_MANIFEST 99\nversion=1.0.0\n",
        "RATA_UPDATE_MANIFEST 1\nversion=0.1.0\nasset_id=123\nsize=456\n"
        "sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
        "signature=0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000\n",
        "RATA_UPDATE_MANIFEST 1\r\nversion=1.2.3\r\nasset_id=0\r\nsize=0\r\n",
        "RATA_UPDATE_MANIFEST 0\nversion=0.1.0\n",
        "version=0.1.0\nunknown=foo\n"
    };
    const size_t nseeds = sizeof(SEEDS) / sizeof(SEEDS[0]);
    for (size_t i = 0u; i < nseeds; ++i) {
        fuzz_one((const uint8_t *)SEEDS[i], strlen(SEEDS[i]));
    }

    uint32_t rng = 0x51A7C0DEu;
    uint8_t buf[512];
    for (size_t iter = 0u; iter < 5000u; ++iter) {
        const char *base = SEEDS[xorshift32(&rng) % nseeds];
        size_t len = strlen(base);
        if (len >= sizeof(buf)) {
            len = sizeof(buf) - 1u;
        }
        memcpy(buf, base, len);
        size_t muts = 1u + (xorshift32(&rng) % 6u);
        for (size_t m = 0u; m < muts; ++m) {
            uint32_t op = xorshift32(&rng) % 3u;
            if (op == 0u && len > 0u) {
                buf[xorshift32(&rng) % len] = (uint8_t)(xorshift32(&rng) & 0xFFu);
            } else if (op == 1u && len + 1u < sizeof(buf)) {
                buf[len++] = (uint8_t)(xorshift32(&rng) & 0xFFu);
            } else if (len > 0u) {
                len--;
            }
        }
        fuzz_one(buf, len);
    }

    printf("fuzz_update_manifest: passed %zu seeds + 5000 mutations.\n", nseeds);
    return 0;
}
#else
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzz_one(data, size);
    return 0;
}
#endif

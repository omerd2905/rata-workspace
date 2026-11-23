#include "engines/update_pubkey.h"

#include <stddef.h>
#include <string.h>

/* Compiled-in Ed25519 public key used to verify signed Windows release updates.
 *
 * ONE-TIME SETUP:
 *   1. Run `make sign-tool && ./build/linux-debug/rata-sign keygen` (or release build).
 *   2. Store the printed 64-hex-char seed ONLY in GitHub Actions secret
 *      `RATA_UPDATE_SIGNING_KEY` (never commit the seed).
 *   3. Replace the 32 bytes below with the printed public key bytes and commit.
 *
 * Until replaced, this all-zero placeholder causes signature verification to
 * fail closed (rejecting all updates rather than accepting unsigned binaries). */
static const uint8_t RATA_UPDATE_ED25519_PUBKEY[RATA_ED25519_PUBKEY_LEN] = {
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
    0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
};

void update_pubkey_get(uint8_t pubkey_out[RATA_ED25519_PUBKEY_LEN]) {
    if (pubkey_out == NULL) {
        return;
    }
    memcpy(pubkey_out, RATA_UPDATE_ED25519_PUBKEY, RATA_ED25519_PUBKEY_LEN);
}

bool update_pubkey_is_configured(const uint8_t pubkey[RATA_ED25519_PUBKEY_LEN]) {
    if (pubkey == NULL) {
        return false;
    }
    uint8_t acc = 0u;
    for (size_t i = 0u; i < RATA_ED25519_PUBKEY_LEN; ++i) {
        acc |= pubkey[i];
    }
    return acc != 0u;
}

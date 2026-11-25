#include "engines/update_pubkey.h"

#include <stddef.h>
#include <string.h>

/* Compiled-in Ed25519 public key used to verify signed Windows release updates.
 * Corresponding private seed is stored solely in the GitHub Actions repository
 * secret `RATA_UPDATE_SIGNING_KEY` (never in git).
 *
 * Public key (hex): 40b09fd2689046324ee35daceabd6f2ea98498c6c79e04535b044c18f5d535ab */
static const uint8_t RATA_UPDATE_ED25519_PUBKEY[RATA_ED25519_PUBKEY_LEN] = {
    0x40u, 0xb0u, 0x9fu, 0xd2u, 0x68u, 0x90u, 0x46u, 0x32u,
    0x4eu, 0xe3u, 0x5du, 0xacu, 0xeau, 0xbdu, 0x6fu, 0x2eu,
    0xa9u, 0x84u, 0x98u, 0xc6u, 0xc7u, 0x9eu, 0x04u, 0x53u,
    0x5bu, 0x04u, 0x4cu, 0x18u, 0xf5u, 0xd5u, 0x35u, 0xabu
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

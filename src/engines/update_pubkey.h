#ifndef RATA_ENGINES_UPDATE_PUBKEY_H
#define RATA_ENGINES_UPDATE_PUBKEY_H

#include <stdbool.h>
#include <stdint.h>

#define RATA_ED25519_SEED_LEN   32u
#define RATA_ED25519_SECKEY_LEN 64u
#define RATA_ED25519_PUBKEY_LEN 32u
#define RATA_ED25519_SIG_LEN    64u

/* Copies the compiled-in 32-byte Ed25519 release signing public key into pubkey_out.
 * Caller owns pubkey_out[32]. */
void update_pubkey_get(uint8_t pubkey_out[RATA_ED25519_PUBKEY_LEN]);

/* Returns true iff pubkey is non-zero (configured). All-zero placeholder keys
 * are always rejected by manifest signature verification. */
bool update_pubkey_is_configured(const uint8_t pubkey[RATA_ED25519_PUBKEY_LEN]);

#endif /* RATA_ENGINES_UPDATE_PUBKEY_H */

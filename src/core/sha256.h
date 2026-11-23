#ifndef RATA_CORE_SHA256_H
#define RATA_CORE_SHA256_H

#include <stddef.h>
#include <stdint.h>

#include "core/status.h"

#define RATA_SHA256_DIGEST_LEN 32u
#define RATA_SHA256_HEX_LEN    64u

/* Caller-owned streaming SHA-256 context (FIPS 180-4, zero global state). */
typedef struct RataSha256Ctx {
    uint32_t state[8];
    uint64_t total_bytes;
    uint8_t buffer[64];
    size_t buffer_len;
} RataSha256Ctx;

void sha256_init(RataSha256Ctx *ctx);
void sha256_update(RataSha256Ctx *ctx, const uint8_t *data, size_t len);
void sha256_final(RataSha256Ctx *ctx, uint8_t digest_out[RATA_SHA256_DIGEST_LEN]);

/* One-shot SHA-256 helper. Caller owns digest_out[32]. */
void sha256_hash(const uint8_t *data, size_t len, uint8_t digest_out[RATA_SHA256_DIGEST_LEN]);

/* Encodes 32-byte digest into 64 lowercase hex characters + NUL terminator.
 * Caller owns hex_out[65]. */
void sha256_to_hex(const uint8_t digest[RATA_SHA256_DIGEST_LEN],
                   char hex_out[RATA_SHA256_HEX_LEN + 1u]);

/* Decodes 64 hex characters (case-insensitive) into 32 bytes.
 * Rejects wrong length or non-hex characters with RATA_ERR_PARSE. */
RataStatus sha256_from_hex(const char *hex, uint8_t digest_out[RATA_SHA256_DIGEST_LEN]);

#endif /* RATA_CORE_SHA256_H */

#ifndef RATA_ENGINES_UPDATE_MANIFEST_H
#define RATA_ENGINES_UPDATE_MANIFEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core/sha256.h"
#include "core/status.h"
#include "engines/update_pubkey.h"

#define RATA_UPDATE_MANIFEST_VERSION 1u
#define RATA_UPDATE_VERSION_MAX      32u
#define RATA_UPDATE_PAYLOAD_MAX      256u
#define RATA_UPDATE_MANIFEST_MAX     512u

/* Strict semantic version MAJOR.MINOR.PATCH. */
typedef struct RataSemver {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} RataSemver;

/* Parsed line-based update manifest (no JSON library).
 *
 * Format v1:
 *   RATA_UPDATE_MANIFEST 1
 *   version=0.1.0
 *   size=1234567
 *   sha256=<64 lowercase hex>
 *   signature=<128 lowercase hex>
 */
typedef struct RataUpdateManifest {
    uint32_t format_version;
    char version[RATA_UPDATE_VERSION_MAX];
    RataSemver semver;
    uint64_t size_bytes;
    uint8_t sha256[RATA_SHA256_DIGEST_LEN];
    uint8_t signature[RATA_ED25519_SIG_LEN];
} RataUpdateManifest;

/* Parses strict "X.Y.Z" decimal semantic version string. */
RataStatus update_semver_parse(const char *str, RataSemver *out);

/* Compares two semantic versions. Returns -1 if a < b, 0 if a == b, +1 if a > b. */
int32_t update_semver_compare(RataSemver a, RataSemver b);

/* Parses a line-based update manifest buffer.
 * Returns RATA_ERR_NEWER_VERSION if format_version > RATA_UPDATE_MANIFEST_VERSION.
 * Returns RATA_ERR_FORMAT on syntax error or missing required fields. */
RataStatus update_manifest_parse(const char *text, size_t text_len, RataUpdateManifest *out);

/* Serializes a manifest into caller-owned buf_out (NUL-terminated).
 * If len_out != NULL, writes byte length excluding NUL. */
RataStatus update_manifest_format(const RataUpdateManifest *m,
                                  char *buf_out,
                                  size_t buf_cap,
                                  size_t *len_out);

/* Builds the canonical domain-separated byte payload signed by Ed25519:
 *   "rata-update-v1\n<version>\n<size>\n<sha256_hex>\n"
 * Caller owns buf_out. */
RataStatus update_manifest_signing_payload(const RataUpdateManifest *m,
                                           char *buf_out,
                                           size_t buf_cap,
                                           size_t *len_out);

/* Derives an Ed25519 secret_key[64] and pubkey[32] from a 32-byte random seed.
 * Does not modify `seed`. Caller owns output buffers and should wipe secrets after use. */
void update_ed25519_keypair(const uint8_t seed[RATA_ED25519_SEED_LEN],
                            uint8_t secret_key_out[RATA_ED25519_SECKEY_LEN],
                            uint8_t pubkey_out[RATA_ED25519_PUBKEY_LEN]);

/* Overwrites sensitive memory in a way the compiler cannot optimize away. */
void update_secure_wipe(void *buf, size_t len);

/* Signs `m`'s canonical payload using Ed25519 secret_key[64] and stores signature in `m`. */
RataStatus update_manifest_sign(RataUpdateManifest *m,
                                const uint8_t secret_key[RATA_ED25519_SECKEY_LEN]);

/* Verifies `m->signature` against `pubkey[32]` over `m`'s canonical payload.
 * Fails closed (RATA_ERR_VERIFY) if pubkey is all-zero or signature does not match. */
RataStatus update_manifest_verify_signature(const RataUpdateManifest *m,
                                            const uint8_t pubkey[RATA_ED25519_PUBKEY_LEN]);

/* Streams `file_path` computing exact byte size and SHA-256 digest. */
RataStatus update_hash_file(const char *file_path,
                            uint64_t *size_out,
                            uint8_t sha256_out[RATA_SHA256_DIGEST_LEN]);

/* Verifies Ed25519 signature, exact byte size, AND streaming SHA-256 of `file_path`.
 * Returns RATA_OK only when all three checks pass. */
RataStatus update_manifest_verify_file(const RataUpdateManifest *m,
                                       const char *file_path,
                                       const uint8_t pubkey[RATA_ED25519_PUBKEY_LEN]);

/* Hex encoding/decoding helpers for keys, digests, and signatures.
 * hex_out must have capacity >= 2 * in_len + 1. */
void update_hex_encode(const uint8_t *in, size_t in_len, char *hex_out);
RataStatus update_hex_decode(const char *hex, size_t hex_len, uint8_t *out, size_t out_len);

#endif /* RATA_ENGINES_UPDATE_MANIFEST_H */

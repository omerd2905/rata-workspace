#include "engines/update_manifest.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "monocypher-ed25519.h"

static int hex_val(char c) {
    if (c >= '0' && c <= '9') {
        return (int)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (int)(c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return (int)(c - 'A') + 10;
    }
    return -1;
}

void update_hex_encode(const uint8_t *in, size_t in_len, char *hex_out) {
    static const char HEX[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    if (in == NULL || hex_out == NULL) {
        return;
    }
    for (size_t i = 0u; i < in_len; ++i) {
        hex_out[i * 2u] = HEX[(in[i] >> 4u) & 0x0Fu];
        hex_out[i * 2u + 1u] = HEX[in[i] & 0x0Fu];
    }
    hex_out[in_len * 2u] = '\0';
}

RataStatus update_hex_decode(const char *hex, size_t hex_len, uint8_t *out, size_t out_len) {
    if (hex == NULL || out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (hex_len != out_len * 2u) {
        return RATA_ERR_PARSE;
    }
    for (size_t i = 0u; i < out_len; ++i) {
        int hi = hex_val(hex[i * 2u]);
        int lo = hex_val(hex[i * 2u + 1u]);
        if (hi < 0 || lo < 0) {
            return RATA_ERR_PARSE;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return RATA_OK;
}

static RataStatus parse_u64_strict(const char *s, size_t len, uint64_t *out) {
    if (s == NULL || out == NULL || len == 0u) {
        return RATA_ERR_PARSE;
    }
    uint64_t v = 0u;
    for (size_t i = 0u; i < len; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return RATA_ERR_PARSE;
        }
        uint64_t d = (uint64_t)(s[i] - '0');
        if (v > (UINT64_MAX - d) / 10u) {
            return RATA_ERR_RANGE;
        }
        v = v * 10u + d;
    }
    *out = v;
    return RATA_OK;
}

RataStatus update_semver_parse(const char *str, RataSemver *out) {
    if (str == NULL || out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    const char *p = str;
    uint32_t parts[3] = {0u, 0u, 0u};

    for (int idx = 0; idx < 3; ++idx) {
        if (*p < '0' || *p > '9') {
            return RATA_ERR_PARSE;
        }
        uint64_t val = 0u;
        while (*p >= '0' && *p <= '9') {
            val = val * 10u + (uint64_t)(*p - '0');
            if (val > 0xFFFFFFFFu) {
                return RATA_ERR_RANGE;
            }
            ++p;
        }
        parts[idx] = (uint32_t)val;
        if (idx < 2) {
            if (*p != '.') {
                return RATA_ERR_PARSE;
            }
            ++p;
        }
    }
    if (*p != '\0') {
        return RATA_ERR_PARSE;
    }

    out->major = parts[0];
    out->minor = parts[1];
    out->patch = parts[2];
    return RATA_OK;
}

int32_t update_semver_compare(RataSemver a, RataSemver b) {
    if (a.major != b.major) {
        return (a.major < b.major) ? -1 : 1;
    }
    if (a.minor != b.minor) {
        return (a.minor < b.minor) ? -1 : 1;
    }
    if (a.patch != b.patch) {
        return (a.patch < b.patch) ? -1 : 1;
    }
    return 0;
}

static size_t trim_line(const char *src, size_t len, const char **start_out) {
    size_t b = 0u;
    while (b < len && (src[b] == ' ' || src[b] == '\t' || src[b] == '\r')) {
        ++b;
    }
    size_t e = len;
    while (e > b && (src[e - 1u] == ' ' || src[e - 1u] == '\t' || src[e - 1u] == '\r')) {
        --e;
    }
    *start_out = src + b;
    return e - b;
}

RataStatus update_manifest_parse(const char *text, size_t text_len, RataUpdateManifest *out) {
    if (text == NULL || out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    bool saw_header = false;
    bool has_ver = false;
    bool has_size = false;
    bool has_sha = false;
    bool has_sig = false;

    size_t pos = 0u;
    while (pos < text_len) {
        size_t line_start = pos;
        while (pos < text_len && text[pos] != '\n') {
            if (text[pos] == '\0') {
                return RATA_ERR_FORMAT;
            }
            ++pos;
        }
        size_t raw_len = pos - line_start;
        if (pos < text_len && text[pos] == '\n') {
            ++pos;
        }

        const char *line = NULL;
        size_t line_len = trim_line(text + line_start, raw_len, &line);
        if (line_len == 0u || line[0] == '#') {
            continue;
        }

        if (!saw_header) {
            static const char PREFIX[] = "RATA_UPDATE_MANIFEST ";
            const size_t plen = sizeof(PREFIX) - 1u;
            if (line_len <= plen || memcmp(line, PREFIX, plen) != 0) {
                return RATA_ERR_FORMAT;
            }
            uint64_t fver = 0u;
            if (parse_u64_strict(line + plen, line_len - plen, &fver) != RATA_OK ||
                fver == 0u || fver > 0xFFFFFFFFu) {
                return RATA_ERR_FORMAT;
            }
            out->format_version = (uint32_t)fver;
            if (out->format_version > RATA_UPDATE_MANIFEST_VERSION) {
                return RATA_ERR_NEWER_VERSION;
            }
            saw_header = true;
            continue;
        }

        const char *eq = (const char *)memchr(line, '=', line_len);
        if (eq == NULL) {
            return RATA_ERR_FORMAT;
        }
        size_t klen = (size_t)(eq - line);
        const char *val = eq + 1;
        size_t vlen = line_len - klen - 1u;
        if (klen == 0u || vlen == 0u) {
            return RATA_ERR_FORMAT;
        }

        if (klen == 7u && memcmp(line, "version", 7u) == 0) {
            if (has_ver || vlen >= sizeof(out->version)) {
                return RATA_ERR_FORMAT;
            }
            memcpy(out->version, val, vlen);
            out->version[vlen] = '\0';
            if (update_semver_parse(out->version, &out->semver) != RATA_OK) {
                return RATA_ERR_FORMAT;
            }
            has_ver = true;
        } else if (klen == 4u && memcmp(line, "size", 4u) == 0) {
            if (has_size || parse_u64_strict(val, vlen, &out->size_bytes) != RATA_OK ||
                out->size_bytes == 0u) {
                return RATA_ERR_FORMAT;
            }
            has_size = true;
        } else if (klen == 6u && memcmp(line, "sha256", 6u) == 0) {
            if (has_sha || update_hex_decode(val, vlen, out->sha256, sizeof(out->sha256)) != RATA_OK) {
                return RATA_ERR_FORMAT;
            }
            has_sha = true;
        } else if (klen == 9u && memcmp(line, "signature", 9u) == 0) {
            if (has_sig || update_hex_decode(val, vlen, out->signature, sizeof(out->signature)) != RATA_OK) {
                return RATA_ERR_FORMAT;
            }
            has_sig = true;
        } else {
            return RATA_ERR_FORMAT;
        }
    }

    if (!saw_header || !has_ver || !has_size || !has_sha || !has_sig) {
        return RATA_ERR_FORMAT;
    }
    return RATA_OK;
}

RataStatus update_manifest_signing_payload(const RataUpdateManifest *m,
                                           char *buf_out,
                                           size_t buf_cap,
                                           size_t *len_out) {
    if (m == NULL || buf_out == NULL || buf_cap == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    RataSemver sv;
    if (update_semver_parse(m->version, &sv) != RATA_OK || m->size_bytes == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    char sha_hex[RATA_SHA256_HEX_LEN + 1u];
    sha256_to_hex(m->sha256, sha_hex);

    int n = snprintf(buf_out, buf_cap,
                     "rata-update-v1\n%s\n%" PRIu64 "\n%s\n",
                     m->version, m->size_bytes, sha_hex);
    if (n < 0 || (size_t)n >= buf_cap) {
        return RATA_ERR_RANGE;
    }
    if (len_out != NULL) {
        *len_out = (size_t)n;
    }
    return RATA_OK;
}

RataStatus update_manifest_format(const RataUpdateManifest *m,
                                  char *buf_out,
                                  size_t buf_cap,
                                  size_t *len_out) {
    if (m == NULL || buf_out == NULL || buf_cap == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    RataSemver sv;
    if (update_semver_parse(m->version, &sv) != RATA_OK || m->size_bytes == 0u) {
        return RATA_ERR_INVALID_ARG;
    }
    char sha_hex[RATA_SHA256_HEX_LEN + 1u];
    char sig_hex[RATA_ED25519_SIG_LEN * 2u + 1u];
    sha256_to_hex(m->sha256, sha_hex);
    update_hex_encode(m->signature, sizeof(m->signature), sig_hex);

    int n = snprintf(buf_out, buf_cap,
                     "RATA_UPDATE_MANIFEST %u\n"
                     "version=%s\n"
                     "size=%" PRIu64 "\n"
                     "sha256=%s\n"
                     "signature=%s\n",
                     (unsigned)RATA_UPDATE_MANIFEST_VERSION,
                     m->version,
                     m->size_bytes,
                     sha_hex,
                     sig_hex);
    if (n < 0 || (size_t)n >= buf_cap) {
        return RATA_ERR_RANGE;
    }
    if (len_out != NULL) {
        *len_out = (size_t)n;
    }
    return RATA_OK;
}

void update_ed25519_keypair(const uint8_t seed[RATA_ED25519_SEED_LEN],
                            uint8_t secret_key_out[RATA_ED25519_SECKEY_LEN],
                            uint8_t pubkey_out[RATA_ED25519_PUBKEY_LEN]) {
    if (seed == NULL || secret_key_out == NULL || pubkey_out == NULL) {
        return;
    }
    uint8_t seed_copy[RATA_ED25519_SEED_LEN];
    memcpy(seed_copy, seed, sizeof(seed_copy));
    crypto_ed25519_key_pair(secret_key_out, pubkey_out, seed_copy);
    crypto_wipe(seed_copy, sizeof(seed_copy));
}

void update_secure_wipe(void *buf, size_t len) {
    if (buf != NULL && len > 0u) {
        crypto_wipe(buf, len);
    }
}

RataStatus update_manifest_sign(RataUpdateManifest *m,
                                const uint8_t secret_key[RATA_ED25519_SECKEY_LEN]) {
    if (m == NULL || secret_key == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    char payload[RATA_UPDATE_PAYLOAD_MAX];
    size_t payload_len = 0u;
    RataStatus st = update_manifest_signing_payload(m, payload, sizeof(payload), &payload_len);
    if (st != RATA_OK) {
        return st;
    }
    crypto_ed25519_sign(m->signature, secret_key, (const uint8_t *)payload, payload_len);
    m->format_version = RATA_UPDATE_MANIFEST_VERSION;
    return RATA_OK;
}

RataStatus update_manifest_verify_signature(const RataUpdateManifest *m,
                                            const uint8_t pubkey[RATA_ED25519_PUBKEY_LEN]) {
    if (m == NULL || pubkey == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (!update_pubkey_is_configured(pubkey)) {
        return RATA_ERR_VERIFY;
    }
    char payload[RATA_UPDATE_PAYLOAD_MAX];
    size_t payload_len = 0u;
    RataStatus st = update_manifest_signing_payload(m, payload, sizeof(payload), &payload_len);
    if (st != RATA_OK) {
        return RATA_ERR_VERIFY;
    }
    int ok = crypto_ed25519_check(m->signature, pubkey, (const uint8_t *)payload, payload_len);
    return (ok == 0) ? RATA_OK : RATA_ERR_VERIFY;
}

RataStatus update_hash_file(const char *file_path,
                            uint64_t *size_out,
                            uint8_t sha256_out[RATA_SHA256_DIGEST_LEN]) {
    if (file_path == NULL || size_out == NULL || sha256_out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        return RATA_ERR_IO;
    }

    RataSha256Ctx ctx;
    sha256_init(&ctx);
    uint8_t chunk[16384];
    uint64_t total = 0u;
    for (;;) {
        size_t n = fread(chunk, 1u, sizeof(chunk), fp);
        if (n > 0u) {
            sha256_update(&ctx, chunk, n);
            total += (uint64_t)n;
        }
        if (n < sizeof(chunk)) {
            if (ferror(fp)) {
                fclose(fp);
                return RATA_ERR_IO;
            }
            break;
        }
    }
    fclose(fp);
    sha256_final(&ctx, sha256_out);
    *size_out = total;
    return RATA_OK;
}

RataStatus update_manifest_verify_file(const RataUpdateManifest *m,
                                       const char *file_path,
                                       const uint8_t pubkey[RATA_ED25519_PUBKEY_LEN]) {
    if (m == NULL || file_path == NULL || pubkey == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    RataStatus st = update_manifest_verify_signature(m, pubkey);
    if (st != RATA_OK) {
        return st;
    }

    uint64_t file_size = 0u;
    uint8_t file_sha[RATA_SHA256_DIGEST_LEN];
    st = update_hash_file(file_path, &file_size, file_sha);
    if (st != RATA_OK) {
        return st;
    }
    if (file_size != m->size_bytes) {
        return RATA_ERR_VERIFY;
    }
    if (crypto_verify32(file_sha, m->sha256) != 0) {
        return RATA_ERR_VERIFY;
    }
    return RATA_OK;
}

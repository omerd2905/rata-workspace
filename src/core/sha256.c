#include "core/sha256.h"

#include <string.h>

static const uint32_t SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32u - n));
}

static uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24u) |
           ((uint32_t)p[1] << 16u) |
           ((uint32_t)p[2] << 8u)  |
           ((uint32_t)p[3]);
}

static void store_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24u) & 0xFFu);
    p[1] = (uint8_t)((v >> 16u) & 0xFFu);
    p[2] = (uint8_t)((v >> 8u) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

static void sha256_compress(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (size_t i = 0u; i < 16u; ++i) {
        w[i] = load_be32(block + (i * 4u));
    }
    for (size_t i = 16u; i < 64u; ++i) {
        uint32_t s0 = rotr32(w[i - 15u], 7u) ^ rotr32(w[i - 15u], 18u) ^ (w[i - 15u] >> 3u);
        uint32_t s1 = rotr32(w[i - 2u], 17u) ^ rotr32(w[i - 2u], 19u) ^ (w[i - 2u] >> 10u);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    uint32_t e = state[4];
    uint32_t f = state[5];
    uint32_t g = state[6];
    uint32_t h = state[7];

    for (size_t i = 0u; i < 64u; ++i) {
        uint32_t ep1 = rotr32(e, 6u) ^ rotr32(e, 11u) ^ rotr32(e, 25u);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + ep1 + ch + SHA256_K[i] + w[i];
        uint32_t ep0 = rotr32(a, 2u) ^ rotr32(a, 13u) ^ rotr32(a, 22u);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = ep0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void sha256_init(RataSha256Ctx *ctx) {
    if (ctx == NULL) {
        return;
    }
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
    ctx->total_bytes = 0u;
    ctx->buffer_len = 0u;
    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void sha256_update(RataSha256Ctx *ctx, const uint8_t *data, size_t len) {
    if (ctx == NULL || (data == NULL && len > 0u)) {
        return;
    }
    ctx->total_bytes += (uint64_t)len;

    size_t pos = 0u;
    if (ctx->buffer_len > 0u) {
        size_t need = 64u - ctx->buffer_len;
        size_t take = (len < need) ? len : need;
        memcpy(ctx->buffer + ctx->buffer_len, data, take);
        ctx->buffer_len += take;
        pos += take;
        if (ctx->buffer_len == 64u) {
            sha256_compress(ctx->state, ctx->buffer);
            ctx->buffer_len = 0u;
        }
    }

    while (pos + 64u <= len) {
        sha256_compress(ctx->state, data + pos);
        pos += 64u;
    }

    if (pos < len) {
        size_t rem = len - pos;
        memcpy(ctx->buffer, data + pos, rem);
        ctx->buffer_len = rem;
    }
}

void sha256_final(RataSha256Ctx *ctx, uint8_t digest_out[RATA_SHA256_DIGEST_LEN]) {
    if (ctx == NULL || digest_out == NULL) {
        return;
    }

    uint64_t total_bits = ctx->total_bytes * 8u;
    ctx->buffer[ctx->buffer_len++] = 0x80u;

    if (ctx->buffer_len > 56u) {
        memset(ctx->buffer + ctx->buffer_len, 0, 64u - ctx->buffer_len);
        sha256_compress(ctx->state, ctx->buffer);
        ctx->buffer_len = 0u;
    }

    memset(ctx->buffer + ctx->buffer_len, 0, 56u - ctx->buffer_len);
    store_be32(ctx->buffer + 56u, (uint32_t)(total_bits >> 32u));
    store_be32(ctx->buffer + 60u, (uint32_t)(total_bits & 0xFFFFFFFFu));
    sha256_compress(ctx->state, ctx->buffer);

    for (size_t i = 0u; i < 8u; ++i) {
        store_be32(digest_out + (i * 4u), ctx->state[i]);
    }
    memset(ctx, 0, sizeof(*ctx));
}

void sha256_hash(const uint8_t *data, size_t len, uint8_t digest_out[RATA_SHA256_DIGEST_LEN]) {
    RataSha256Ctx ctx;
    sha256_init(&ctx);
    if (len > 0u) {
        sha256_update(&ctx, data, len);
    }
    sha256_final(&ctx, digest_out);
}

void sha256_to_hex(const uint8_t digest[RATA_SHA256_DIGEST_LEN],
                   char hex_out[RATA_SHA256_HEX_LEN + 1u]) {
    static const char HEX[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
    };
    if (digest == NULL || hex_out == NULL) {
        return;
    }
    for (size_t i = 0u; i < RATA_SHA256_DIGEST_LEN; ++i) {
        hex_out[i * 2u] = HEX[(digest[i] >> 4u) & 0x0Fu];
        hex_out[i * 2u + 1u] = HEX[digest[i] & 0x0Fu];
    }
    hex_out[RATA_SHA256_HEX_LEN] = '\0';
}

static int hex_nibble(char c) {
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

RataStatus sha256_from_hex(const char *hex, uint8_t digest_out[RATA_SHA256_DIGEST_LEN]) {
    if (hex == NULL || digest_out == NULL) {
        return RATA_ERR_INVALID_ARG;
    }
    if (strlen(hex) != RATA_SHA256_HEX_LEN) {
        return RATA_ERR_PARSE;
    }
    for (size_t i = 0u; i < RATA_SHA256_DIGEST_LEN; ++i) {
        int hi = hex_nibble(hex[i * 2u]);
        int lo = hex_nibble(hex[i * 2u + 1u]);
        if (hi < 0 || lo < 0) {
            return RATA_ERR_PARSE;
        }
        digest_out[i] = (uint8_t)((hi << 4) | lo);
    }
    return RATA_OK;
}

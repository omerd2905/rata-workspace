#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/sha256.h"
#include "engines/update_manifest.h"
#include "engines/update_pubkey.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#endif

/* Pure C11 Ed25519 release signing tool for Rata Workspace (`make sign-tool`).
 * Private signing seed is NEVER passed via argv — it is read from the
 * RATA_UPDATE_SIGNING_KEY environment variable or stdin. */

static bool read_csprng_bytes(uint8_t *out, size_t len) {
#ifdef _WIN32
    HCRYPTPROV prov = 0;
    if (!CryptAcquireContextW(&prov, NULL, NULL, PROV_RSA_FULL,
                              CRYPT_VERIFYCONTEXT | CRYPT_SILENT)) {
        return false;
    }
    BOOL ok = CryptGenRandom(prov, (DWORD)len, out);
    CryptReleaseContext(prov, 0);
    return ok != FALSE;
#else
    FILE *fp = fopen("/dev/urandom", "rb");
    if (fp == NULL) {
        return false;
    }
    size_t got = fread(out, 1u, len, fp);
    fclose(fp);
    return got == len;
#endif
}

static bool read_seed_from_env_or_stdin(uint8_t seed_out[RATA_ED25519_SEED_LEN]) {
    char buf[128];
    memset(buf, 0, sizeof(buf));

    const char *env = getenv("RATA_UPDATE_SIGNING_KEY");
    if (env != NULL && env[0] != '\0') {
        size_t len = strlen(env);
        while (len > 0u && (env[len - 1u] == '\n' || env[len - 1u] == '\r' ||
                            env[len - 1u] == ' '  || env[len - 1u] == '\t')) {
            --len;
        }
        RataStatus st = update_hex_decode(env, len, seed_out, RATA_ED25519_SEED_LEN);
        return st == RATA_OK;
    }

    if (fgets(buf, (int)sizeof(buf), stdin) == NULL) {
        return false;
    }
    size_t len = strlen(buf);
    while (len > 0u && (buf[len - 1u] == '\n' || buf[len - 1u] == '\r' ||
                        buf[len - 1u] == ' '  || buf[len - 1u] == '\t')) {
        buf[--len] = '\0';
    }
    RataStatus st = update_hex_decode(buf, len, seed_out, RATA_ED25519_SEED_LEN);
    update_secure_wipe(buf, sizeof(buf));
    return st == RATA_OK;
}

static void print_pubkey_c_snippet(const uint8_t pub[RATA_ED25519_PUBKEY_LEN]) {
    char pub_hex[RATA_ED25519_PUBKEY_LEN * 2u + 1u];
    update_hex_encode(pub, RATA_ED25519_PUBKEY_LEN, pub_hex);
    printf("Public key (hex): %s\n\n", pub_hex);
    printf("Paste into src/engines/update_pubkey.c:\n");
    printf("static const uint8_t RATA_UPDATE_ED25519_PUBKEY[RATA_ED25519_PUBKEY_LEN] = {\n");
    for (size_t i = 0u; i < RATA_ED25519_PUBKEY_LEN; ++i) {
        if ((i % 8u) == 0u) {
            printf("    ");
        }
        printf("0x%02xu", (unsigned)pub[i]);
        if (i + 1u < RATA_ED25519_PUBKEY_LEN) {
            printf(",");
            if ((i % 8u) != 7u) {
                printf(" ");
            }
        }
        if ((i % 8u) == 7u) {
            printf("\n");
        }
    }
    printf("};\n");
}

static int cmd_keygen(void) {
    uint8_t seed[RATA_ED25519_SEED_LEN];
    uint8_t seckey[RATA_ED25519_SECKEY_LEN];
    uint8_t pubkey[RATA_ED25519_PUBKEY_LEN];
    if (!read_csprng_bytes(seed, sizeof(seed))) {
        fprintf(stderr, "rata-sign: failed to read OS CSPRNG bytes.\n");
        return 1;
    }

    update_ed25519_keypair(seed, seckey, pubkey);
    char seed_hex[RATA_ED25519_SEED_LEN * 2u + 1u];
    update_hex_encode(seed, sizeof(seed), seed_hex);

    printf("Private signing seed (store ONLY in GitHub Actions secret RATA_UPDATE_SIGNING_KEY;\n"
           "NEVER commit this value to git):\n");
    printf("%s\n\n", seed_hex);
    print_pubkey_c_snippet(pubkey);

    update_secure_wipe(seed_hex, sizeof(seed_hex));
    update_secure_wipe(seed, sizeof(seed));
    update_secure_wipe(seckey, sizeof(seckey));
    return 0;
}

static int cmd_pubkey(void) {
    uint8_t seed[RATA_ED25519_SEED_LEN];
    uint8_t seckey[RATA_ED25519_SECKEY_LEN];
    uint8_t pubkey[RATA_ED25519_PUBKEY_LEN];
    if (!read_seed_from_env_or_stdin(seed)) {
        fprintf(stderr, "rata-sign: expected 64-hex-char seed in RATA_UPDATE_SIGNING_KEY or stdin.\n");
        return 1;
    }
    update_ed25519_keypair(seed, seckey, pubkey);
    print_pubkey_c_snippet(pubkey);

    update_secure_wipe(seed, sizeof(seed));
    update_secure_wipe(seckey, sizeof(seckey));
    return 0;
}

static bool parse_u64_arg(const char *s, uint64_t *out) {
    if (s == NULL || s[0] == '\0') {
        return false;
    }
    uint64_t v = 0u;
    for (size_t i = 0u; s[i] != '\0'; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        uint64_t d = (uint64_t)(s[i] - '0');
        if (v > (UINT64_MAX - d) / 10u) {
            return false;
        }
        v = v * 10u + d;
    }
    *out = v;
    return v > 0u;
}

static int cmd_sign_manifest(const char *version, const char *asset_id_str, const char *file_path) {
    RataUpdateManifest m;
    memset(&m, 0, sizeof(m));

    if (strlen(version) >= sizeof(m.version) ||
        update_semver_parse(version, &m.semver) != RATA_OK) {
        fprintf(stderr, "rata-sign: invalid semantic version '%s' (expected X.Y.Z).\n", version);
        return 1;
    }
    snprintf(m.version, sizeof(m.version), "%s", version);

    if (!parse_u64_arg(asset_id_str, &m.asset_id)) {
        fprintf(stderr, "rata-sign: invalid asset_id '%s'.\n", asset_id_str);
        return 1;
    }

    RataStatus st = update_hash_file(file_path, &m.size_bytes, m.sha256);
    if (st != RATA_OK || m.size_bytes == 0u) {
        fprintf(stderr, "rata-sign: failed to read installer '%s': %s\n",
                file_path, rata_status_str(st));
        return 1;
    }

    uint8_t seed[RATA_ED25519_SEED_LEN];
    uint8_t seckey[RATA_ED25519_SECKEY_LEN];
    uint8_t pubkey[RATA_ED25519_PUBKEY_LEN];
    if (!read_seed_from_env_or_stdin(seed)) {
        fprintf(stderr, "rata-sign: expected 64-hex-char seed in RATA_UPDATE_SIGNING_KEY or stdin.\n");
        return 1;
    }
    update_ed25519_keypair(seed, seckey, pubkey);
    st = update_manifest_sign(&m, seckey);
    update_secure_wipe(seed, sizeof(seed));
    update_secure_wipe(seckey, sizeof(seckey));
    if (st != RATA_OK) {
        fprintf(stderr, "rata-sign: signing failed: %s\n", rata_status_str(st));
        return 1;
    }

    /* Self-check signature against derived pubkey before printing. */
    if (update_manifest_verify_signature(&m, pubkey) != RATA_OK) {
        fprintf(stderr, "rata-sign: internal self-verification failed!\n");
        return 1;
    }

    /* Warn on stderr if compiled-in pubkey is configured and differs from signing key. */
    uint8_t compiled_pub[RATA_ED25519_PUBKEY_LEN];
    update_pubkey_get(compiled_pub);
    if (update_pubkey_is_configured(compiled_pub) &&
        memcmp(compiled_pub, pubkey, sizeof(pubkey)) != 0) {
        fprintf(stderr,
                "rata-sign: ERROR: RATA_UPDATE_SIGNING_KEY does not match compiled-in public key "
                "in src/engines/update_pubkey.c!\n");
        return 1;
    }

    char out[RATA_UPDATE_MANIFEST_MAX];
    size_t out_len = 0u;
    st = update_manifest_format(&m, out, sizeof(out), &out_len);
    if (st != RATA_OK) {
        fprintf(stderr, "rata-sign: failed to format manifest: %s\n", rata_status_str(st));
        return 1;
    }
    fwrite(out, 1u, out_len, stdout);
    return 0;
}

static int cmd_verify_manifest(const char *manifest_path, const char *file_path) {
    FILE *fp = fopen(manifest_path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "rata-sign: cannot open manifest '%s'.\n", manifest_path);
        return 1;
    }
    char text[4096];
    size_t n = fread(text, 1u, sizeof(text) - 1u, fp);
    fclose(fp);
    text[n] = '\0';

    RataUpdateManifest m;
    RataStatus st = update_manifest_parse(text, n, &m);
    if (st != RATA_OK) {
        fprintf(stderr, "rata-sign: invalid manifest: %s\n", rata_status_str(st));
        return 1;
    }

    uint8_t pubkey[RATA_ED25519_PUBKEY_LEN];
    const char *env_pub = getenv("RATA_UPDATE_PUBKEY");
    if (env_pub != NULL && env_pub[0] != '\0') {
        size_t plen = strlen(env_pub);
        while (plen > 0u && (env_pub[plen - 1u] == '\n' || env_pub[plen - 1u] == '\r')) {
            --plen;
        }
        if (update_hex_decode(env_pub, plen, pubkey, sizeof(pubkey)) != RATA_OK) {
            fprintf(stderr, "rata-sign: invalid RATA_UPDATE_PUBKEY hex.\n");
            return 1;
        }
    } else {
        update_pubkey_get(pubkey);
    }

    st = update_manifest_verify_file(&m, file_path, pubkey);
    if (st != RATA_OK) {
        fprintf(stderr, "rata-sign: verification FAILED: %s\n", rata_status_str(st));
        return 1;
    }
    printf("rata-sign: VERIFIED v%s (asset_id=%" PRIu64 ", size=%" PRIu64 " bytes)\n",
           m.version, m.asset_id, m.size_bytes);
    return 0;
}

static void print_usage(void) {
    fprintf(stderr,
            "Usage:\n"
            "  rata-sign keygen\n"
            "  rata-sign pubkey                       (reads seed from RATA_UPDATE_SIGNING_KEY or stdin)\n"
            "  rata-sign sign-manifest <ver> <asset_id> <installer_path>\n"
            "  rata-sign verify-manifest <manifest_path> <installer_path>\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    if (strcmp(argv[1], "keygen") == 0 && argc == 2) {
        return cmd_keygen();
    }
    if (strcmp(argv[1], "pubkey") == 0 && argc == 2) {
        return cmd_pubkey();
    }
    if (strcmp(argv[1], "sign-manifest") == 0 && argc == 5) {
        return cmd_sign_manifest(argv[2], argv[3], argv[4]);
    }
    if (strcmp(argv[1], "verify-manifest") == 0 && argc == 4) {
        return cmd_verify_manifest(argv[2], argv[3]);
    }
    print_usage();
    return 1;
}

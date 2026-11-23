#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincred.h>
#include <winhttp.h>
#include <direct.h>
#include <wchar.h>
#endif

#include "app/app_updater.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engines/update_manifest.h"
#include "engines/update_pubkey.h"
#include "engines/user_config.h"

#ifndef RATA_VERSION
#define RATA_VERSION "0.1.0"
#endif

#define RATA_AUTH_ERR_MSG \
    "GitHub token missing, expired or lacks access \xe2\x80\x94 run rata-workspace --set-token"

#ifdef _WIN32

#define RATA_CRED_TARGET       L"RataWorkspace/GitHubPAT"
#define RATA_GITHUB_HOST       L"api.github.com"
#define RATA_MANIFEST_PATH     L"/repos/omerd2905/rata-workspace/contents/update-manifest.txt?ref=stable"
#define RATA_MAX_DOWNLOAD_SIZE (512ULL * 1024ULL * 1024ULL)

struct AppUpdater {
    CRITICAL_SECTION lock;
    HANDLE thread;
    volatile LONG cancel_requested;
    bool checking;
    bool next_manual;
    AppUpdateSnapshot snap;
};

static bool std_handle_is_valid(DWORD std_id) {
    HANDLE h = GetStdHandle(std_id);
    if (h == NULL || h == INVALID_HANDLE_VALUE) {
        return false;
    }
    return GetFileType(h) != FILE_TYPE_UNKNOWN;
}

void app_console_attach_for_cli(bool need_stdin) {
    bool has_out = std_handle_is_valid(STD_OUTPUT_HANDLE);
    bool has_err = std_handle_is_valid(STD_ERROR_HANDLE);
    bool has_in = std_handle_is_valid(STD_INPUT_HANDLE);

    if (has_out && has_err && (!need_stdin || has_in)) {
        return;
    }
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        return;
    }
    if (!has_out) {
        (void)freopen("CONOUT$", "w", stdout);
    }
    if (!has_err) {
        (void)freopen("CONOUT$", "w", stderr);
    }
    if (need_stdin && !has_in) {
        (void)freopen("CONIN$", "r", stdin);
    }
}

static bool acp_to_wide(const char *src, wchar_t *dst, size_t dst_cap) {
    if (src == NULL || dst == NULL || dst_cap == 0u) {
        return false;
    }
    int n = MultiByteToWideChar(CP_ACP, 0, src, -1, dst, (int)dst_cap);
    return n > 0;
}

static RataStatus ensure_config_dir(char *dir_out, size_t dir_cap) {
    RataStatus st = user_config_resolve_dir(true, NULL, NULL, getenv("APPDATA"), dir_out, dir_cap);
    if (st != RATA_OK) {
        return st;
    }
    size_t len = strlen(dir_out);
    if (len >= RATA_PATH_MAX) {
        return RATA_ERR_RANGE;
    }
    char tmp[RATA_PATH_MAX];
    memcpy(tmp, dir_out, len + 1u);
    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] == '\\' || tmp[i] == '/') {
            if (i == 2u && tmp[1] == ':') {
                continue;
            }
            char saved = tmp[i];
            tmp[i] = '\0';
            if (_mkdir(tmp) != 0 && errno != EEXIST) {
                return RATA_ERR_IO;
            }
            tmp[i] = saved;
        }
    }
    if (_mkdir(tmp) != 0 && errno != EEXIST) {
        return RATA_ERR_IO;
    }
    return RATA_OK;
}

static bool config_file_path(const char *name, char *out, size_t cap) {
    char dir[RATA_PATH_MAX];
    if (ensure_config_dir(dir, sizeof(dir)) != RATA_OK) {
        return false;
    }
    return user_config_join_path(true, dir, name, out, cap) == RATA_OK;
}

/* Append diagnostic line to updater.log (NEVER pass tokens here!). */
static void updater_log(const char *msg) {
    char path[RATA_PATH_MAX];
    if (!config_file_path("updater.log", path, sizeof(path))) {
        return;
    }
    FILE *fp = fopen(path, "ab");
    if (fp == NULL) {
        return;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(fp, "[%04u-%02u-%02u %02u:%02u:%02u] (v%s) %s\r\n",
            (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
            (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond,
            RATA_VERSION, msg);
    fclose(fp);
}

static void delete_narrow_file(const char *path) {
    wchar_t wpath[RATA_PATH_MAX];
    if (acp_to_wide(path, wpath, RATA_PATH_MAX)) {
        (void)DeleteFileW(wpath);
    }
}

static bool atomic_rename_file(const char *src, const char *dst) {
    wchar_t wsrc[RATA_PATH_MAX];
    wchar_t wdst[RATA_PATH_MAX];
    if (!acp_to_wide(src, wsrc, RATA_PATH_MAX) || !acp_to_wide(dst, wdst, RATA_PATH_MAX)) {
        return false;
    }
    return MoveFileExW(wsrc, wdst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

static bool read_pat_from_cred_manager(char *token_out, size_t cap) {
    if (token_out == NULL || cap < 2u) {
        return false;
    }
    PCREDENTIALW pcred = NULL;
    if (!CredReadW(RATA_CRED_TARGET, CRED_TYPE_GENERIC, 0, &pcred) || pcred == NULL) {
        return false;
    }
    bool ok = false;
    if (pcred->CredentialBlob != NULL && pcred->CredentialBlobSize > 0u &&
        (size_t)pcred->CredentialBlobSize < cap) {
        memcpy(token_out, pcred->CredentialBlob, (size_t)pcred->CredentialBlobSize);
        token_out[pcred->CredentialBlobSize] = '\0';
        ok = (token_out[0] != '\0');
    }
    if (pcred->CredentialBlob != NULL && pcred->CredentialBlobSize > 0u) {
        SecureZeroMemory(pcred->CredentialBlob, pcred->CredentialBlobSize);
    }
    CredFree(pcred);
    return ok;
}

static bool write_pat_to_cred_manager(const char *token) {
    if (token == NULL || token[0] == '\0') {
        return false;
    }
    size_t len = strlen(token);
    if (len > 4096u) {
        return false;
    }
    CREDENTIALW cred;
    memset(&cred, 0, sizeof(cred));
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = (LPWSTR)RATA_CRED_TARGET;
    cred.CredentialBlobSize = (DWORD)len;
    cred.CredentialBlob = (LPBYTE)token;
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = (LPWSTR)L"rata-workspace";
    return CredWriteW(&cred, 0) != FALSE;
}

static bool utf8_to_wide(const char *src, wchar_t *dst, size_t dst_cap) {
    if (src == NULL || dst == NULL || dst_cap == 0u) {
        return false;
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int)dst_cap);
    return n > 0;
}

static HINTERNET open_winhttp_session(void) {
    char ua_narrow[64];
    wchar_t ua[64];
    snprintf(ua_narrow, sizeof(ua_narrow), "RataWorkspace/%s", RATA_VERSION);
    if (!utf8_to_wide(ua_narrow, ua, 64u)) {
        return NULL;
    }
    HINTERNET hSession = WinHttpOpen(ua,
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS,
                                     0);
    if (hSession == NULL) {
        hSession = WinHttpOpen(ua,
                               WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                               WINHTTP_NO_PROXY_NAME,
                               WINHTTP_NO_PROXY_BYPASS,
                               0);
    }
    if (hSession != NULL) {
        (void)WinHttpSetTimeouts(hSession, 5000, 5000, 8000, 15000);
    }
    return hSession;
}

static bool query_http_status(HINTERNET hReq, DWORD *status_out) {
    DWORD code = 0u;
    DWORD sz = sizeof(code);
    if (!WinHttpQueryHeaders(hReq,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             WINHTTP_HEADER_NAME_BY_INDEX,
                             &code, &sz, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    *status_out = code;
    return true;
}

static bool add_github_headers(HINTERNET hReq, const char *accept_val, const char *token) {
    char narrow[1024];
    wchar_t hdrs[1024];
    int n = snprintf(narrow, sizeof(narrow),
                     "Accept: %s\r\n"
                     "X-GitHub-Api-Version: 2022-11-28\r\n"
                     "Authorization: Bearer %s\r\n",
                     accept_val, token);
    if (n <= 0 || (size_t)n >= sizeof(narrow)) {
        SecureZeroMemory(narrow, sizeof(narrow));
        return false;
    }
    bool ok = false;
    if (utf8_to_wide(narrow, hdrs, 1024u)) {
        ok = WinHttpAddRequestHeaders(hReq, hdrs, (DWORD)-1L,
                                      WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE) != FALSE;
    }
    SecureZeroMemory(narrow, sizeof(narrow));
    SecureZeroMemory(hdrs, sizeof(hdrs));
    return ok;
}

static RataStatus fetch_manifest_text(HINTERNET hSession,
                                      const char *token,
                                      char *buf_out,
                                      size_t buf_cap,
                                      size_t *len_out) {
    HINTERNET hConn = WinHttpConnect(hSession, RATA_GITHUB_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConn == NULL) {
        return RATA_ERR_IO;
    }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", RATA_MANIFEST_PATH, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (hReq == NULL) {
        WinHttpCloseHandle(hConn);
        return RATA_ERR_IO;
    }

    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    (void)WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));

    RataStatus st = RATA_ERR_IO;
    if (add_github_headers(hReq, "application/vnd.github.raw", token) &&
        WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, NULL)) {
        DWORD status = 0u;
        if (query_http_status(hReq, &status)) {
            if (status == 401u || status == 403u) {
                st = RATA_ERR_AUTH;
            } else if (status == 404u) {
                st = RATA_ERR_RANGE;
            } else if (status == 200u) {
                size_t total = 0u;
                bool read_ok = true;
                for (;;) {
                    DWORD avail = 0u;
                    if (!WinHttpQueryDataAvailable(hReq, &avail)) {
                        read_ok = false;
                        break;
                    }
                    if (avail == 0u) {
                        break;
                    }
                    if (total + (size_t)avail >= buf_cap) {
                        read_ok = false;
                        break;
                    }
                    DWORD got = 0u;
                    if (!WinHttpReadData(hReq, buf_out + total, avail, &got)) {
                        read_ok = false;
                        break;
                    }
                    total += (size_t)got;
                }
                if (read_ok && total > 0u) {
                    buf_out[total] = '\0';
                    *len_out = total;
                    st = RATA_OK;
                }
            }
        }
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    return st;
}

/* Downloads release asset to tmp_path.
 * Handles GitHub API HTTP 302 redirect MANUALLY and strips the Authorization header
 * before connecting to the redirect host. */
static RataStatus download_asset_to_file(HINTERNET hSession,
                                         const char *token,
                                         uint64_t asset_id,
                                         uint64_t expected_size,
                                         const char *tmp_path,
                                         volatile LONG *cancel_flag) {
    if (expected_size == 0u || expected_size > RATA_MAX_DOWNLOAD_SIZE) {
        return RATA_ERR_RANGE;
    }

    char asset_path_narrow[160];
    wchar_t asset_path[160];
    snprintf(asset_path_narrow, sizeof(asset_path_narrow),
             "/repos/omerd2905/rata-workspace/releases/assets/%" PRIu64, asset_id);
    if (!utf8_to_wide(asset_path_narrow, asset_path, 160u)) {
        return RATA_ERR_RANGE;
    }

    HINTERNET hConn = WinHttpConnect(hSession, RATA_GITHUB_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConn == NULL) {
        return RATA_ERR_IO;
    }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", asset_path, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (hReq == NULL) {
        WinHttpCloseHandle(hConn);
        return RATA_ERR_IO;
    }

    /* CRITICAL: disable automatic redirects so Authorization is NEVER forwarded. */
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    (void)WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));

    if (!add_github_headers(hReq, "application/octet-stream", token) ||
        !WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, NULL)) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        return RATA_ERR_IO;
    }

    DWORD status = 0u;
    if (!query_http_status(hReq, &status)) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        return RATA_ERR_IO;
    }

    if (status == 401u || status == 403u) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        return RATA_ERR_AUTH;
    }

    HINTERNET dl_conn = hConn;
    HINTERNET dl_req = hReq;

    if (status == 301u || status == 302u || status == 303u || status == 307u || status == 308u) {
        wchar_t loc[2048];
        DWORD loc_bytes = sizeof(loc);
        BOOL has_loc = WinHttpQueryHeaders(hReq, WINHTTP_QUERY_LOCATION,
                                           WINHTTP_HEADER_NAME_BY_INDEX,
                                           loc, &loc_bytes, WINHTTP_NO_HEADER_INDEX);
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        dl_req = NULL;
        dl_conn = NULL;
        if (!has_loc) {
            return RATA_ERR_IO;
        }

        wchar_t host[256];
        wchar_t url_path[2048];
        wchar_t extra[2048];
        URL_COMPONENTSW uc;
        memset(&uc, 0, sizeof(uc));
        uc.dwStructSize = sizeof(uc);
        uc.lpszHostName = host;
        uc.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(host[0]));
        uc.lpszUrlPath = url_path;
        uc.dwUrlPathLength = (DWORD)(sizeof(url_path) / sizeof(url_path[0]));
        uc.lpszExtraInfo = extra;
        uc.dwExtraInfoLength = (DWORD)(sizeof(extra) / sizeof(extra[0]));
        if (!WinHttpCrackUrl(loc, 0, 0, &uc)) {
            return RATA_ERR_IO;
        }
        /* Refuse downgrade to plaintext HTTP on redirect. */
        if (uc.nScheme != INTERNET_SCHEME_HTTPS || uc.dwHostNameLength == 0u) {
            return RATA_ERR_VERIFY;
        }

        size_t plen = wcslen(url_path);
        size_t elen = wcslen(extra);
        if (plen + elen >= 4096u) {
            return RATA_ERR_RANGE;
        }
        wchar_t full_path[4096];
        memcpy(full_path, url_path, plen * sizeof(wchar_t));
        memcpy(full_path + plen, extra, (elen + 1u) * sizeof(wchar_t));

        dl_conn = WinHttpConnect(hSession, host, uc.nPort, 0);
        if (dl_conn == NULL) {
            return RATA_ERR_IO;
        }
        dl_req = WinHttpOpenRequest(dl_conn, L"GET", full_path, NULL,
                                    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                    WINHTTP_FLAG_SECURE);
        if (dl_req == NULL) {
            WinHttpCloseHandle(dl_conn);
            return RATA_ERR_IO;
        }

        /* No Authorization header on redirect request! Also disable further redirects. */
        (void)WinHttpSetOption(dl_req, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
        (void)WinHttpSetTimeouts(dl_req, 5000, 5000, 10000, 30000);

        if (!WinHttpSendRequest(dl_req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(dl_req, NULL) ||
            !query_http_status(dl_req, &status) ||
            status != 200u) {
            WinHttpCloseHandle(dl_req);
            WinHttpCloseHandle(dl_conn);
            return RATA_ERR_IO;
        }
    } else if (status != 200u) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        return RATA_ERR_IO;
    }

    FILE *fp = fopen(tmp_path, "wb");
    if (fp == NULL) {
        WinHttpCloseHandle(dl_req);
        WinHttpCloseHandle(dl_conn);
        return RATA_ERR_IO;
    }

    uint8_t chunk[32768];
    uint64_t total = 0u;
    RataStatus st = RATA_OK;
    for (;;) {
        if (cancel_flag != NULL && InterlockedCompareExchange(cancel_flag, 0, 0) != 0) {
            st = RATA_ERR_IO;
            break;
        }
        DWORD avail = 0u;
        if (!WinHttpQueryDataAvailable(dl_req, &avail)) {
            st = RATA_ERR_IO;
            break;
        }
        if (avail == 0u) {
            break;
        }
        DWORD to_read = (avail > (DWORD)sizeof(chunk)) ? (DWORD)sizeof(chunk) : avail;
        DWORD got = 0u;
        if (!WinHttpReadData(dl_req, chunk, to_read, &got)) {
            st = RATA_ERR_IO;
            break;
        }
        if (got == 0u) {
            break;
        }
        if (total + (uint64_t)got > expected_size) {
            st = RATA_ERR_VERIFY;
            break;
        }
        if (fwrite(chunk, 1u, (size_t)got, fp) != (size_t)got) {
            st = RATA_ERR_IO;
            break;
        }
        total += (uint64_t)got;
    }

    fclose(fp);
    WinHttpCloseHandle(dl_req);
    WinHttpCloseHandle(dl_conn);

    if (st != RATA_OK || total != expected_size) {
        delete_narrow_file(tmp_path);
        return (st == RATA_OK) ? RATA_ERR_VERIFY : st;
    }
    return RATA_OK;
}

static bool load_and_verify_pending(RataUpdateManifest *m_out,
                                    char *exe_path_out,
                                    size_t exe_cap,
                                    bool delete_if_stale_or_bad) {
    char txt_path[RATA_PATH_MAX];
    if (!config_file_path("pending-update.txt", txt_path, sizeof(txt_path)) ||
        !config_file_path("pending-update.exe", exe_path_out, exe_cap)) {
        return false;
    }
    FILE *fp = fopen(txt_path, "rb");
    if (fp == NULL) {
        return false;
    }
    char text[2048];
    size_t n = fread(text, 1u, sizeof(text) - 1u, fp);
    fclose(fp);
    text[n] = '\0';

    RataUpdateManifest m;
    if (update_manifest_parse(text, n, &m) != RATA_OK) {
        updater_log("Rejected malformed pending-update.txt.");
        if (delete_if_stale_or_bad) {
            delete_narrow_file(txt_path);
            delete_narrow_file(exe_path_out);
        }
        return false;
    }

    RataSemver cur;
    if (update_semver_parse(RATA_VERSION, &cur) != RATA_OK ||
        update_semver_compare(m.semver, cur) <= 0) {
        /* Already upgraded to this version or newer: clean up staged files silently. */
        if (delete_if_stale_or_bad) {
            delete_narrow_file(txt_path);
            delete_narrow_file(exe_path_out);
        }
        return false;
    }

    uint8_t pubkey[RATA_ED25519_PUBKEY_LEN];
    update_pubkey_get(pubkey);
    if (update_manifest_verify_file(&m, exe_path_out, pubkey) != RATA_OK) {
        updater_log("Rejected pending-update.exe: signature/SHA-256/size verification failed.");
        if (delete_if_stale_or_bad) {
            delete_narrow_file(txt_path);
            delete_narrow_file(exe_path_out);
        }
        return false;
    }

    if (m_out != NULL) {
        *m_out = m;
    }
    return true;
}

static bool spawn_installer_silent(const char *exe_path) {
    wchar_t wexe[RATA_PATH_MAX];
    if (!acp_to_wide(exe_path, wexe, RATA_PATH_MAX)) {
        return false;
    }
    static const wchar_t ARGS[] = L"\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART";
    size_t elen = wcslen(wexe);
    size_t alen = wcslen(ARGS);
    if (1u + elen + alen >= RATA_PATH_MAX + 96u) {
        return false;
    }
    wchar_t cmd[RATA_PATH_MAX + 96];
    cmd[0] = L'"';
    memcpy(cmd + 1u, wexe, elen * sizeof(wchar_t));
    memcpy(cmd + 1u + elen, ARGS, (alen + 1u) * sizeof(wchar_t));

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if (!CreateProcessW(wexe, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        updater_log("CreateProcessW failed when launching verified installer.");
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static RataStatus run_check_and_stage(volatile LONG *cancel_flag,
                                      bool verbose_cli,
                                      RataUpdateManifest *staged_out,
                                      bool *has_update_out) {
    *has_update_out = false;

    uint8_t pubkey[RATA_ED25519_PUBKEY_LEN];
    update_pubkey_get(pubkey);
    if (!update_pubkey_is_configured(pubkey)) {
        updater_log("Update check skipped: signing public key is not configured in this build.");
        if (verbose_cli) {
            fprintf(stderr, "Error: update signing public key is not configured in this build.\n");
        }
        return RATA_ERR_VERIFY;
    }

    char token[1024];
    memset(token, 0, sizeof(token));
    if (!read_pat_from_cred_manager(token, sizeof(token))) {
        return RATA_ERR_AUTH;
    }

    HINTERNET hSession = open_winhttp_session();
    if (hSession == NULL) {
        SecureZeroMemory(token, sizeof(token));
        return RATA_ERR_IO;
    }

    if (verbose_cli) {
        printf("Checking for updates (current v%s)...\n", RATA_VERSION);
        fflush(stdout);
    }

    char manifest_buf[4096];
    size_t manifest_len = 0u;
    RataStatus st = fetch_manifest_text(hSession, token, manifest_buf, sizeof(manifest_buf), &manifest_len);
    if (st == RATA_ERR_RANGE) {
        /* 404 on stable branch: no manifest published yet. */
        WinHttpCloseHandle(hSession);
        SecureZeroMemory(token, sizeof(token));
        return RATA_OK;
    }
    if (st != RATA_OK) {
        WinHttpCloseHandle(hSession);
        SecureZeroMemory(token, sizeof(token));
        if (st == RATA_ERR_AUTH) {
            updater_log("Manifest fetch failed with HTTP 401/403 (token invalid or lacks access).");
        }
        return st;
    }

    RataUpdateManifest m;
    st = update_manifest_parse(manifest_buf, manifest_len, &m);
    if (st != RATA_OK) {
        updater_log("Rejected malformed update-manifest.txt from server.");
        WinHttpCloseHandle(hSession);
        SecureZeroMemory(token, sizeof(token));
        return st;
    }

    if (update_manifest_verify_signature(&m, pubkey) != RATA_OK) {
        updater_log("Rejected update-manifest.txt: Ed25519 signature verification failed.");
        WinHttpCloseHandle(hSession);
        SecureZeroMemory(token, sizeof(token));
        return RATA_ERR_VERIFY;
    }

    RataSemver cur;
    if (update_semver_parse(RATA_VERSION, &cur) != RATA_OK) {
        WinHttpCloseHandle(hSession);
        SecureZeroMemory(token, sizeof(token));
        return RATA_ERR_FORMAT;
    }
    if (update_semver_compare(m.semver, cur) <= 0) {
        WinHttpCloseHandle(hSession);
        SecureZeroMemory(token, sizeof(token));
        return RATA_OK;
    }

    /* Check if this exact newer version is already staged & verified. */
    RataUpdateManifest existing;
    char existing_exe[RATA_PATH_MAX];
    if (load_and_verify_pending(&existing, existing_exe, sizeof(existing_exe), false) &&
        strcmp(existing.version, m.version) == 0 &&
        memcmp(existing.sha256, m.sha256, sizeof(m.sha256)) == 0) {
        WinHttpCloseHandle(hSession);
        SecureZeroMemory(token, sizeof(token));
        *staged_out = m;
        *has_update_out = true;
        return RATA_OK;
    }

    char tmp_exe[RATA_PATH_MAX];
    char tmp_txt[RATA_PATH_MAX];
    char dst_exe[RATA_PATH_MAX];
    char dst_txt[RATA_PATH_MAX];
    if (!config_file_path("pending-update.tmp", tmp_exe, sizeof(tmp_exe)) ||
        !config_file_path("pending-update.txt.tmp", tmp_txt, sizeof(tmp_txt)) ||
        !config_file_path("pending-update.exe", dst_exe, sizeof(dst_exe)) ||
        !config_file_path("pending-update.txt", dst_txt, sizeof(dst_txt))) {
        WinHttpCloseHandle(hSession);
        SecureZeroMemory(token, sizeof(token));
        return RATA_ERR_IO;
    }

    if (verbose_cli) {
        printf("Downloading Rata Workspace v%s (%" PRIu64 " bytes)...\n", m.version, m.size_bytes);
        fflush(stdout);
    }

    st = download_asset_to_file(hSession, token, m.asset_id, m.size_bytes, tmp_exe, cancel_flag);
    WinHttpCloseHandle(hSession);
    SecureZeroMemory(token, sizeof(token));
    if (st != RATA_OK) {
        if (st == RATA_ERR_VERIFY) {
            updater_log("Downloaded installer rejected (unexpected size or downgrade attempt).");
        }
        return st;
    }

    if (verbose_cli) {
        printf("Verifying SHA-256 and Ed25519 signature...\n");
        fflush(stdout);
    }

    st = update_manifest_verify_file(&m, tmp_exe, pubkey);
    if (st != RATA_OK) {
        updater_log("Downloaded installer FAILED SHA-256 or Ed25519 verification; deleted.");
        delete_narrow_file(tmp_exe);
        return RATA_ERR_VERIFY;
    }

    char formatted[RATA_UPDATE_MANIFEST_MAX];
    size_t flen = 0u;
    if (update_manifest_format(&m, formatted, sizeof(formatted), &flen) != RATA_OK) {
        delete_narrow_file(tmp_exe);
        return RATA_ERR_FORMAT;
    }
    FILE *tfp = fopen(tmp_txt, "wb");
    if (tfp == NULL || fwrite(formatted, 1u, flen, tfp) != flen) {
        if (tfp != NULL) {
            fclose(tfp);
        }
        delete_narrow_file(tmp_exe);
        delete_narrow_file(tmp_txt);
        return RATA_ERR_IO;
    }
    fclose(tfp);

    if (!atomic_rename_file(tmp_exe, dst_exe) || !atomic_rename_file(tmp_txt, dst_txt)) {
        delete_narrow_file(tmp_exe);
        delete_narrow_file(tmp_txt);
        return RATA_ERR_IO;
    }

    char logline[128];
    snprintf(logline, sizeof(logline), "Staged verified update v%s.", m.version);
    updater_log(logline);

    *staged_out = m;
    *has_update_out = true;
    return RATA_OK;
}

static DWORD WINAPI updater_thread_proc(LPVOID param) {
    AppUpdater *u = (AppUpdater *)param;
    bool manual = false;
    EnterCriticalSection(&u->lock);
    manual = u->next_manual;
    LeaveCriticalSection(&u->lock);

    RataUpdateManifest staged;
    bool has_update = false;
    RataStatus st = run_check_and_stage(&u->cancel_requested, false, &staged, &has_update);

    EnterCriticalSection(&u->lock);
    u->checking = false;
    u->snap.from_manual = manual;
    if (st == RATA_OK && has_update) {
        u->snap.state = APP_UPD_READY;
        snprintf(u->snap.ready_version, sizeof(u->snap.ready_version), "%s", staged.version);
        snprintf(u->snap.message, sizeof(u->snap.message),
                 "v%s ready \xe2\x80\x94 restart to apply.", staged.version);
    } else if (st == RATA_OK) {
        u->snap.state = APP_UPD_UP_TO_DATE;
        u->snap.ready_version[0] = '\0';
        snprintf(u->snap.message, sizeof(u->snap.message),
                 "Up to date (v%s).", RATA_VERSION);
    } else if (st == RATA_ERR_AUTH) {
        u->snap.state = APP_UPD_AUTH_ERROR;
        snprintf(u->snap.message, sizeof(u->snap.message), "%s", RATA_AUTH_ERR_MSG);
    } else if (manual) {
        /* Surface error only on explicit manual check; background network errors stay silent. */
        u->snap.state = APP_UPD_ERROR;
        if (st == RATA_ERR_VERIFY) {
            snprintf(u->snap.message, sizeof(u->snap.message),
                     "Update rejected: signature or integrity check failed.");
        } else {
            snprintf(u->snap.message, sizeof(u->snap.message),
                     "Could not check for updates (offline or network error).");
        }
    } else {
        u->snap.state = APP_UPD_IDLE;
        u->snap.message[0] = '\0';
    }
    LeaveCriticalSection(&u->lock);
    return 0;
}

AppUpdater *app_updater_create(const char *argv0) {
    (void)argv0;
    AppUpdater *u = (AppUpdater *)calloc(1u, sizeof(AppUpdater));
    if (u == NULL) {
        return NULL;
    }
    InitializeCriticalSection(&u->lock);
    u->snap.supported = true;
    u->snap.is_nix_store = false;
    u->snap.state = APP_UPD_IDLE;
    return u;
}

void app_updater_destroy(AppUpdater *u) {
    if (u == NULL) {
        return;
    }
    InterlockedExchange(&u->cancel_requested, 1);
    HANDLE th = NULL;
    EnterCriticalSection(&u->lock);
    th = u->thread;
    u->thread = NULL;
    LeaveCriticalSection(&u->lock);
    if (th != NULL) {
        (void)WaitForSingleObject(th, 10000);
        CloseHandle(th);
    }
    DeleteCriticalSection(&u->lock);
    free(u);
}

bool app_updater_apply_pending_on_startup(AppUpdater *u) {
    if (u == NULL) {
        return false;
    }
    RataUpdateManifest m;
    char exe_path[RATA_PATH_MAX];
    if (!load_and_verify_pending(&m, exe_path, sizeof(exe_path), true)) {
        return false;
    }
    char logline[128];
    snprintf(logline, sizeof(logline), "Applying verified pending update v%s on startup.", m.version);
    updater_log(logline);
    return spawn_installer_silent(exe_path);
}

bool app_updater_launch_pending(AppUpdater *u) {
    if (u == NULL) {
        return false;
    }
    RataUpdateManifest m;
    char exe_path[RATA_PATH_MAX];
    if (!load_and_verify_pending(&m, exe_path, sizeof(exe_path), true)) {
        return false;
    }
    char logline[128];
    snprintf(logline, sizeof(logline), "Launching verified pending update v%s on restart.", m.version);
    updater_log(logline);
    return spawn_installer_silent(exe_path);
}

void app_updater_start_background_check(AppUpdater *u, bool manual_request) {
    if (u == NULL) {
        return;
    }
    EnterCriticalSection(&u->lock);
    if (u->checking) {
        if (manual_request) {
            u->next_manual = true;
        }
        LeaveCriticalSection(&u->lock);
        return;
    }
    if (u->thread != NULL) {
        CloseHandle(u->thread);
        u->thread = NULL;
    }
    u->checking = true;
    u->next_manual = manual_request;
    u->snap.state = APP_UPD_CHECKING;
    u->snap.from_manual = manual_request;
    snprintf(u->snap.message, sizeof(u->snap.message), "Checking for updates...");
    HANDLE th = CreateThread(NULL, 0, updater_thread_proc, u, 0, NULL);
    if (th == NULL) {
        u->checking = false;
        u->snap.state = APP_UPD_IDLE;
        u->snap.message[0] = '\0';
    } else {
        u->thread = th;
    }
    LeaveCriticalSection(&u->lock);
}

void app_updater_get_snapshot(AppUpdater *u, AppUpdateSnapshot *out) {
    if (u == NULL || out == NULL) {
        return;
    }
    EnterCriticalSection(&u->lock);
    *out = u->snap;
    LeaveCriticalSection(&u->lock);
}

int app_updater_cli_set_token(AppUpdater *u, int argc, char **argv) {
    (void)u;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--set-token") != 0) {
            fprintf(stderr,
                    "Error: do not pass tokens on the command line.\n"
                    "Run 'rata-workspace --set-token' with no extra arguments.\n");
            fflush(stderr);
            return 1;
        }
    }

    fprintf(stdout,
            "Enter GitHub fine-grained PAT (single repo, Contents: read-only)\n"
            "Token (input hidden): ");
    fflush(stdout);

    HANDLE hin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD orig_mode = 0u;
    bool has_console = (hin != NULL && hin != INVALID_HANDLE_VALUE &&
                        GetConsoleMode(hin, &orig_mode) != FALSE);
    if (has_console) {
        (void)SetConsoleMode(hin, orig_mode & ~((DWORD)ENABLE_ECHO_INPUT));
    }

    char token[1024];
    memset(token, 0, sizeof(token));
    char *line = fgets(token, (int)sizeof(token), stdin);

    if (has_console) {
        (void)SetConsoleMode(hin, orig_mode);
        fputc('\n', stdout);
        fflush(stdout);
    }

    if (line == NULL) {
        SecureZeroMemory(token, sizeof(token));
        fprintf(stderr, "Error: failed to read token from input.\n");
        fflush(stderr);
        return 1;
    }

    size_t len = strlen(token);
    while (len > 0u && (token[len - 1u] == '\n' || token[len - 1u] == '\r' ||
                        token[len - 1u] == ' '  || token[len - 1u] == '\t')) {
        token[--len] = '\0';
    }
    if (len == 0u) {
        SecureZeroMemory(token, sizeof(token));
        fprintf(stderr, "Error: empty token rejected.\n");
        fflush(stderr);
        return 1;
    }

    bool saved = write_pat_to_cred_manager(token);
    SecureZeroMemory(token, sizeof(token));
    if (!saved) {
        fprintf(stderr, "Error: failed to store token in Windows Credential Manager.\n");
        fflush(stderr);
        return 1;
    }

    fprintf(stdout, "GitHub token stored securely in Windows Credential Manager.\n");
    fflush(stdout);
    return 0;
}

int app_updater_cli_update(AppUpdater *u) {
    if (u == NULL) {
        return 1;
    }
    RataUpdateManifest staged;
    bool has_update = false;
    RataStatus st = run_check_and_stage(NULL, true, &staged, &has_update);
    if (st == RATA_ERR_AUTH) {
        fprintf(stderr, "%s\n", RATA_AUTH_ERR_MSG);
        fflush(stderr);
        return 1;
    }
    if (st == RATA_ERR_VERIFY) {
        fprintf(stderr, "Error: update rejected (Ed25519 signature, size, or SHA-256 mismatch).\n");
        fflush(stderr);
        return 1;
    }
    if (st != RATA_OK) {
        fprintf(stderr, "Error: update failed (%s).\n", rata_status_str(st));
        fflush(stderr);
        return 1;
    }
    if (!has_update) {
        printf("Rata Workspace %s is already up to date.\n", RATA_VERSION);
        fflush(stdout);
        return 0;
    }

    printf("Signature and SHA-256 verified. Installing v%s...\n", staged.version);
    fflush(stdout);
    if (!app_updater_launch_pending(u)) {
        fprintf(stderr, "Error: failed to launch verified installer.\n");
        fflush(stderr);
        return 1;
    }
    printf("Installer started. Rata Workspace will relaunch when installation completes.\n");
    fflush(stdout);
    return 0;
}

#else /* Non-Windows (Linux / NixOS): no self-updater */

struct AppUpdater {
    bool is_nix_store;
};

static bool detect_nix_store(const char *argv0) {
    if (argv0 != NULL && strncmp(argv0, "/nix/store/", 11u) == 0) {
        return true;
    }
    /* Pure ISO C11 check of the main executable mapping (first line of /proc/self/maps). */
    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        return false;
    }
    char line[512];
    bool in_nix = false;
    if (fgets(line, (int)sizeof(line), fp) != NULL) {
        const char *slash = strchr(line, '/');
        if (slash != NULL && strncmp(slash, "/nix/store/", 11u) == 0) {
            in_nix = true;
        }
    }
    fclose(fp);
    return in_nix;
}

void app_console_attach_for_cli(bool need_stdin) {
    (void)need_stdin;
}

AppUpdater *app_updater_create(const char *argv0) {
    AppUpdater *u = (AppUpdater *)calloc(1u, sizeof(AppUpdater));
    if (u != NULL) {
        u->is_nix_store = detect_nix_store(argv0);
    }
    return u;
}

void app_updater_destroy(AppUpdater *u) {
    free(u);
}

bool app_updater_apply_pending_on_startup(AppUpdater *u) {
    (void)u;
    return false;
}

bool app_updater_launch_pending(AppUpdater *u) {
    (void)u;
    return false;
}

void app_updater_start_background_check(AppUpdater *u, bool manual_request) {
    (void)u;
    (void)manual_request;
}

void app_updater_get_snapshot(AppUpdater *u, AppUpdateSnapshot *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->supported = false;
    out->is_nix_store = (u != NULL) ? u->is_nix_store : false;
    out->state = APP_UPD_IDLE;
}

int app_updater_cli_set_token(AppUpdater *u, int argc, char **argv) {
    (void)argc;
    (void)argv;
    if (u != NULL && u->is_nix_store) {
        fprintf(stderr,
                "--set-token is only used by the Windows auto-updater.\n"
                "On Nix, update with: nix profile upgrade rata-workspace\n");
    } else {
        fprintf(stderr, "--set-token is only used by the Windows auto-updater.\n");
    }
    return 1;
}

int app_updater_cli_update(AppUpdater *u) {
    if (u != NULL && u->is_nix_store) {
        fprintf(stderr,
                "Self-update is disabled for Nix builds.\n"
                "Update with: nix profile upgrade rata-workspace\n");
    } else {
        fprintf(stderr,
                "Self-update is only available on Windows.\n"
                "On Linux, update via 'nix profile upgrade rata-workspace' or a newer release tarball.\n");
    }
    return 1;
}

#endif

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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

#ifdef _WIN32

#define RATA_RELEASE_HOST      L"github.com"
#define RATA_MANIFEST_URL_PATH L"/omerd2905/rata-workspace/releases/latest/download/update-manifest.txt"
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

static bool utf8_to_wide(const char *src, wchar_t *dst, size_t dst_cap) {
    if (src == NULL || dst == NULL || dst_cap == 0u) {
        return false;
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, (int)dst_cap);
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

static RataStatus fetch_manifest_text(HINTERNET hSession,
                                      char *buf_out,
                                      size_t buf_cap,
                                      size_t *len_out) {
    HINTERNET hConn = WinHttpConnect(hSession, RATA_RELEASE_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConn == NULL) {
        return RATA_ERR_IO;
    }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", RATA_MANIFEST_URL_PATH, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (hReq == NULL) {
        WinHttpCloseHandle(hConn);
        return RATA_ERR_IO;
    }

    /* Follow HTTPS->HTTPS release CDN redirects only; block any downgrade to HTTP. */
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    (void)WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));

    RataStatus st = RATA_ERR_IO;
    if (WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hReq, NULL)) {
        DWORD status = 0u;
        if (query_http_status(hReq, &status)) {
            if (status == 404u) {
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

static RataStatus download_installer_to_file(HINTERNET hSession,
                                             const char *verified_version,
                                             uint64_t expected_size,
                                             const char *tmp_path,
                                             volatile LONG *cancel_flag) {
    if (expected_size == 0u || expected_size > RATA_MAX_DOWNLOAD_SIZE) {
        return RATA_ERR_RANGE;
    }

    char rel_narrow[256];
    wchar_t rel_wide[256];
    int n = snprintf(rel_narrow, sizeof(rel_narrow),
                     "/omerd2905/rata-workspace/releases/download/v%s/"
                     "rata-workspace-v%s-windows-x86_64-setup.exe",
                     verified_version, verified_version);
    if (n <= 0 || (size_t)n >= sizeof(rel_narrow) || !utf8_to_wide(rel_narrow, rel_wide, 256u)) {
        return RATA_ERR_RANGE;
    }

    HINTERNET hConn = WinHttpConnect(hSession, RATA_RELEASE_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (hConn == NULL) {
        return RATA_ERR_IO;
    }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", rel_wide, NULL,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                        WINHTTP_FLAG_SECURE);
    if (hReq == NULL) {
        WinHttpCloseHandle(hConn);
        return RATA_ERR_IO;
    }

    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    (void)WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
    (void)WinHttpSetTimeouts(hReq, 5000, 5000, 10000, 30000);

    DWORD status = 0u;
    if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hReq, NULL) ||
        !query_http_status(hReq, &status) ||
        status != 200u) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
        return RATA_ERR_IO;
    }

    FILE *fp = fopen(tmp_path, "wb");
    if (fp == NULL) {
        WinHttpCloseHandle(hReq);
        WinHttpCloseHandle(hConn);
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
        if (!WinHttpQueryDataAvailable(hReq, &avail)) {
            st = RATA_ERR_IO;
            break;
        }
        if (avail == 0u) {
            break;
        }
        DWORD to_read = (avail > (DWORD)sizeof(chunk)) ? (DWORD)sizeof(chunk) : avail;
        DWORD got = 0u;
        if (!WinHttpReadData(hReq, chunk, to_read, &got)) {
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
    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);

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

    HINTERNET hSession = open_winhttp_session();
    if (hSession == NULL) {
        return RATA_ERR_IO;
    }

    if (verbose_cli) {
        printf("Checking for updates (current v%s)...\n", RATA_VERSION);
        fflush(stdout);
    }

    char manifest_buf[4096];
    size_t manifest_len = 0u;
    RataStatus st = fetch_manifest_text(hSession, manifest_buf, sizeof(manifest_buf), &manifest_len);
    if (st == RATA_ERR_RANGE) {
        /* 404 on latest release: no signed manifest published yet. */
        WinHttpCloseHandle(hSession);
        return RATA_OK;
    }
    if (st != RATA_OK) {
        WinHttpCloseHandle(hSession);
        return st;
    }

    RataUpdateManifest m;
    st = update_manifest_parse(manifest_buf, manifest_len, &m);
    if (st != RATA_OK) {
        updater_log("Rejected malformed update-manifest.txt from server.");
        WinHttpCloseHandle(hSession);
        return st;
    }

    if (update_manifest_verify_signature(&m, pubkey) != RATA_OK) {
        updater_log("Rejected update-manifest.txt: Ed25519 signature verification failed.");
        WinHttpCloseHandle(hSession);
        return RATA_ERR_VERIFY;
    }

    RataSemver cur;
    if (update_semver_parse(RATA_VERSION, &cur) != RATA_OK) {
        WinHttpCloseHandle(hSession);
        return RATA_ERR_FORMAT;
    }
    if (update_semver_compare(m.semver, cur) <= 0) {
        WinHttpCloseHandle(hSession);
        return RATA_OK;
    }

    /* Check if this exact newer version is already staged & verified. */
    RataUpdateManifest existing;
    char existing_exe[RATA_PATH_MAX];
    if (load_and_verify_pending(&existing, existing_exe, sizeof(existing_exe), false) &&
        strcmp(existing.version, m.version) == 0 &&
        memcmp(existing.sha256, m.sha256, sizeof(m.sha256)) == 0) {
        WinHttpCloseHandle(hSession);
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
        return RATA_ERR_IO;
    }

    if (verbose_cli) {
        printf("Downloading Rata Workspace v%s (%" PRIu64 " bytes)...\n", m.version, m.size_bytes);
        fflush(stdout);
    }

    st = download_installer_to_file(hSession, m.version, m.size_bytes, tmp_exe, cancel_flag);
    WinHttpCloseHandle(hSession);
    if (st != RATA_OK) {
        if (st == RATA_ERR_VERIFY) {
            updater_log("Downloaded installer rejected (unexpected byte size).");
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

int app_updater_cli_update(AppUpdater *u) {
    if (u == NULL) {
        return 1;
    }
    RataUpdateManifest staged;
    bool has_update = false;
    RataStatus st = run_check_and_stage(NULL, true, &staged, &has_update);
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

#ifndef RATA_APP_UPDATER_H
#define RATA_APP_UPDATER_H

#include <stdbool.h>
#include <stddef.h>

#include "core/status.h"

/* Opaque caller-owned updater controller (zero hidden global state).
 * Windows: WinHTTP + Credential Manager + Ed25519 signature verification + background thread.
 * Linux/Nix: no self-updater; detects /nix/store installation for the About dialog. */
typedef struct AppUpdater AppUpdater;

typedef enum AppUpdateState {
    APP_UPD_IDLE = 0,
    APP_UPD_CHECKING = 1,
    APP_UPD_UP_TO_DATE = 2,
    APP_UPD_READY = 3,
    APP_UPD_AUTH_ERROR = 4,
    APP_UPD_ERROR = 5
} AppUpdateState;

typedef struct AppUpdateSnapshot {
    bool supported;       /* True on Windows; false on Linux/Nix */
    bool is_nix_store;    /* True when running from /nix/store on Linux */
    bool from_manual;     /* True if last result came from explicit user check */
    AppUpdateState state;
    char ready_version[32];
    char message[192];
} AppUpdateSnapshot;

/* On Windows GUI subsystem (-mwindows), preserves already-redirected stdout/stderr/stdin
 * handles (e.g. CI pipes/files), and otherwise attaches to the parent console (PowerShell/cmd)
 * and reopens CONIN$/CONOUT$. No-op on non-Windows. */
void app_console_attach_for_cli(bool need_stdin);

/* Allocates and initializes updater state. Caller owns returned pointer and must
 * call app_updater_destroy() on exit. Returns NULL on allocation failure. */
AppUpdater *app_updater_create(const char *argv0);
void app_updater_destroy(AppUpdater *u);

/* Startup hook called BEFORE InitWindow().
 * On Windows: if a previously downloaded pending update exists, is strictly newer
 * than RATA_VERSION, and passes exact size + SHA-256 + Ed25519 signature checks,
 * launches it silently (/VERYSILENT /SUPPRESSMSGBOXES /NORESTART) and returns true
 * so main() can exit immediately. Returns false otherwise. */
bool app_updater_apply_pending_on_startup(AppUpdater *u);

/* Re-verifies and launches the staged pending update silently (used by "Restart & Update").
 * Returns true if the installer process was started. */
bool app_updater_launch_pending(AppUpdater *u);

/* Starts a non-blocking background update check + download (Windows only).
 * If a check is already running, this is a safe no-op. */
void app_updater_start_background_check(AppUpdater *u, bool manual_request);

/* Thread-safe snapshot of current updater state for UI polling. */
void app_updater_get_snapshot(AppUpdater *u, AppUpdateSnapshot *out);

/* Synchronous CLI command: `rata-workspace --set-token`.
 * Reads GitHub PAT from a hidden console prompt (NEVER from argv) and stores it
 * in Windows Credential Manager. Returns process exit code (0 on success). */
int app_updater_cli_set_token(AppUpdater *u, int argc, char **argv);

/* Synchronous CLI command: `rata-workspace --update`.
 * Runs check -> download -> verify -> install with progress text and clear errors.
 * Returns process exit code (0 on up-to-date or installer launched). */
int app_updater_cli_update(AppUpdater *u);

#endif /* RATA_APP_UPDATER_H */

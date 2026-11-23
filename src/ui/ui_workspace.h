#ifndef RATA_UI_WORKSPACE_H
#define RATA_UI_WORKSPACE_H

#include <stdbool.h>
#include <stddef.h>

#include "core/arena.h"
#include "core/status.h"
#include "engines/project_file.h"
#include "raylib.h"
#include "ui/ui_theme.h"
#include "ui/ui_widgets.h"

typedef enum RataWorkspaceId {
    WS_RATAWAVE = 0,
    WS_RATACALC = 1,
    WS_RATALOOP = 2,
    WS_RATAGATE = 3,
    WS_RATAFORGE = 4,
    WS_RATASIM = 5,
    WS_COUNT = 6
} RataWorkspaceId;

typedef struct RataWorkspaceCtx {
    const RataTheme *theme;
    RataArena *frame_arena;
    UiInputFocus *focus;
    double dt;
} RataWorkspaceCtx;

typedef void (*WorkspaceInitFn)(void *state);
typedef void (*WorkspaceDrawFn)(void *state, Rectangle viewport, RataWorkspaceCtx *ctx);
typedef RataStatus (*WorkspaceSaveFn)(const void *state, RataProjectDoc *doc);
typedef RataStatus (*WorkspaceLoadFn)(void *state, const RataProjectDoc *doc);

typedef struct RataWorkspaceEntry {
    RataWorkspaceId id;
    const char *id_key;
    const char *name;
    const char *subtitle;
    const char *planned_summary;
    int phase_num;
    bool implemented;
    void *state;
    WorkspaceInitFn init_fn;
    WorkspaceDrawFn draw_fn;
    WorkspaceSaveFn save_fn;
    WorkspaceLoadFn load_fn;
} RataWorkspaceEntry;

typedef struct RataWorkspaceRegistry {
    RataWorkspaceEntry entries[WS_COUNT];
} RataWorkspaceRegistry;

/* Initializes registry with all six EE workspaces (unbuilt ones show coming-soon view). */
void ui_workspace_registry_init(RataWorkspaceRegistry *reg);

/* Registers an implemented workspace module into the registry. */
void ui_workspace_register(RataWorkspaceRegistry *reg,
                           RataWorkspaceId id,
                           void *state,
                           WorkspaceInitFn init_fn,
                           WorkspaceDrawFn draw_fn,
                           WorkspaceSaveFn save_fn,
                           WorkspaceLoadFn load_fn);

/* Draws the default coming-soon placeholder screen for an unbuilt workspace. */
void ui_workspace_draw_coming_soon(const RataWorkspaceEntry *entry,
                                   Rectangle viewport,
                                   const RataTheme *theme);

#endif /* RATA_UI_WORKSPACE_H */

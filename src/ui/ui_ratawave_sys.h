#ifndef RATA_UI_RATAWAVE_SYS_H
#define RATA_UI_RATAWAVE_SYS_H

#include "raylib.h"
#include "ui/ui_ratawave.h"
#include "ui/ui_workspace.h"

void ui_ratawave_draw_lti_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx);
void ui_ratawave_draw_filter_tab(RataWaveState *st, Rectangle area, RataWorkspaceCtx *ctx);

#endif /* RATA_UI_RATAWAVE_SYS_H */

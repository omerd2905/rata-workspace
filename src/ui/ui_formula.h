#ifndef RATA_UI_FORMULA_H
#define RATA_UI_FORMULA_H

#include "core/arena.h"
#include "raylib.h"
#include "ui/ui_theme.h"

/* Measures and renders minimal EE formula markup (fractions, square roots, sub/superscripts,
 * Greek letters, common operators) inside `bounds`.
 * Uses `scratch` for temporary AST allocation and restores its mark before returning.
 * On malformed markup, gracefully displays the raw string as fallback. */
void ui_formula_draw(Rectangle bounds,
                     const char *markup,
                     float font_size,
                     Color color,
                     RataArena *scratch);

/* Convenience panel wrapper drawing a titled formula card. */
void ui_formula_card(Rectangle bounds,
                     const char *caption,
                     const char *markup,
                     float font_size,
                     const RataTheme *theme,
                     RataArena *scratch);

#endif /* RATA_UI_FORMULA_H */

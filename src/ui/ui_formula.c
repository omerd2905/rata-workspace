#include "ui/ui_formula.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#include "engines/formula_parse.h"

typedef struct FormulaBox {
    float w;
    float asc;
    float desc;
} FormulaBox;

static float measure_symbol_width(FormulaSymKind sym, float sz) {
    switch (sym) {
        case FSYM_SUM:
        case FSYM_OMEGA_UPPER:
        case FSYM_SIGMA_UPPER:
        case FSYM_INFTY:
        case FSYM_ARROW:
            return sz * 0.90f;
        case FSYM_PI:
        case FSYM_OMEGA_LOWER:
        case FSYM_DELTA_UPPER:
        case FSYM_PHI_UPPER:
        case FSYM_PHI_LOWER:
        case FSYM_THETA:
        case FSYM_MU:
        case FSYM_APPROX:
        case FSYM_PM:
        case FSYM_LE:
        case FSYM_GE:
        case FSYM_NE:
        case FSYM_ANGLE:
            return sz * 0.72f;
        case FSYM_CDOT:
        case FSYM_DEG:
            return sz * 0.40f;
        default:
            return sz * 0.65f;
    }
}

/* Vector rendering for Greek letters & math operators (independent of TTF/system fonts). */
static void draw_vector_symbol(FormulaSymKind sym, float x, float baseline_y, float sz, Color c) {
    float w = measure_symbol_width(sym, sz);
    float top = baseline_y - sz * 0.78f;
    float mid = baseline_y - sz * 0.36f;
    float bot = baseline_y;
    float cx = x + w * 0.5f;
    float thick = (sz >= 16.0f) ? 1.8f : 1.3f;

    switch (sym) {
        case FSYM_PI:
            DrawLineEx((Vector2){x + 1.0f, top + sz * 0.22f},
                       (Vector2){x + w - 1.0f, top + sz * 0.22f}, thick, c);
            DrawLineEx((Vector2){x + w * 0.30f, top + sz * 0.22f},
                       (Vector2){x + w * 0.26f, bot}, thick, c);
            DrawLineEx((Vector2){x + w * 0.72f, top + sz * 0.22f},
                       (Vector2){x + w * 0.74f, bot}, thick, c);
            break;

        case FSYM_OMEGA_LOWER: {
            float y0 = top + sz * 0.24f;
            float y1 = bot - sz * 0.04f;
            DrawLineEx((Vector2){x + w * 0.14f, y0}, (Vector2){x + w * 0.24f, y1}, thick, c);
            DrawLineEx((Vector2){x + w * 0.24f, y1}, (Vector2){cx, mid + sz * 0.06f}, thick, c);
            DrawLineEx((Vector2){cx, mid + sz * 0.06f}, (Vector2){x + w * 0.76f, y1}, thick, c);
            DrawLineEx((Vector2){x + w * 0.76f, y1}, (Vector2){x + w * 0.86f, y0}, thick, c);
            break;
        }

        case FSYM_OMEGA_UPPER: {
            float r = sz * 0.32f;
            DrawCircleLines((int)cx, (int)(mid - sz * 0.04f), r, c);
            DrawLineEx((Vector2){x + 1.0f, bot}, (Vector2){x + w * 0.34f, bot}, thick, c);
            DrawLineEx((Vector2){x + w * 0.66f, bot}, (Vector2){x + w - 1.0f, bot}, thick, c);
            break;
        }

        case FSYM_SUM:
        case FSYM_SIGMA_UPPER: {
            float y0 = top - sz * 0.10f;
            float y1 = bot + sz * 0.10f;
            DrawLineEx((Vector2){x + w - 1.0f, y0}, (Vector2){x + 1.0f, y0}, thick, c);
            DrawLineEx((Vector2){x + 1.0f, y0}, (Vector2){x + w * 0.56f, mid}, thick, c);
            DrawLineEx((Vector2){x + w * 0.56f, mid}, (Vector2){x + 1.0f, y1}, thick, c);
            DrawLineEx((Vector2){x + 1.0f, y1}, (Vector2){x + w - 1.0f, y1}, thick, c);
            break;
        }

        case FSYM_DELTA_UPPER:
            DrawLineEx((Vector2){cx, top}, (Vector2){x + 1.0f, bot}, thick, c);
            DrawLineEx((Vector2){cx, top}, (Vector2){x + w - 1.0f, bot}, thick, c);
            DrawLineEx((Vector2){x + 1.0f, bot}, (Vector2){x + w - 1.0f, bot}, thick, c);
            break;

        case FSYM_DELTA_LOWER: {
            float r = sz * 0.22f;
            DrawCircleLines((int)cx, (int)(bot - r), r, c);
            DrawLineEx((Vector2){cx, bot - 2.0f * r}, (Vector2){x + w * 0.30f, top + sz * 0.10f}, thick, c);
            DrawLineEx((Vector2){x + w * 0.30f, top + sz * 0.10f},
                       (Vector2){x + w * 0.78f, top + sz * 0.08f}, thick, c);
            break;
        }

        case FSYM_INFTY: {
            float r = sz * 0.17f;
            DrawCircleLines((int)(cx - r), (int)mid, r, c);
            DrawCircleLines((int)(cx + r), (int)mid, r, c);
            break;
        }

        case FSYM_PM:
            DrawLineEx((Vector2){cx, top + sz * 0.14f}, (Vector2){cx, mid + sz * 0.10f}, thick, c);
            DrawLineEx((Vector2){x + 2.0f, mid - sz * 0.08f},
                       (Vector2){x + w - 2.0f, mid - sz * 0.08f}, thick, c);
            DrawLineEx((Vector2){x + 2.0f, bot - sz * 0.10f},
                       (Vector2){x + w - 2.0f, bot - sz * 0.10f}, thick, c);
            break;

        case FSYM_TIMES:
            DrawLineEx((Vector2){x + 2.0f, mid - sz * 0.20f},
                       (Vector2){x + w - 2.0f, mid + sz * 0.20f}, thick, c);
            DrawLineEx((Vector2){x + w - 2.0f, mid - sz * 0.20f},
                       (Vector2){x + 2.0f, mid + sz * 0.20f}, thick, c);
            break;

        case FSYM_CDOT:
            DrawCircleV((Vector2){cx, mid}, (sz >= 16.0f) ? 2.2f : 1.7f, c);
            break;

        case FSYM_DEG:
            DrawCircleLines((int)cx, (int)(top + sz * 0.18f), sz * 0.12f, c);
            break;

        case FSYM_LE:
            DrawLineEx((Vector2){x + w - 2.0f, top + sz * 0.16f}, (Vector2){x + 2.0f, mid}, thick, c);
            DrawLineEx((Vector2){x + 2.0f, mid}, (Vector2){x + w - 2.0f, bot - sz * 0.18f}, thick, c);
            DrawLineEx((Vector2){x + 2.0f, bot - sz * 0.02f},
                       (Vector2){x + w - 2.0f, bot - sz * 0.02f}, thick, c);
            break;

        case FSYM_GE:
            DrawLineEx((Vector2){x + 2.0f, top + sz * 0.16f}, (Vector2){x + w - 2.0f, mid}, thick, c);
            DrawLineEx((Vector2){x + w - 2.0f, mid}, (Vector2){x + 2.0f, bot - sz * 0.18f}, thick, c);
            DrawLineEx((Vector2){x + 2.0f, bot - sz * 0.02f},
                       (Vector2){x + w - 2.0f, bot - sz * 0.02f}, thick, c);
            break;

        case FSYM_NE:
            DrawLineEx((Vector2){x + 2.0f, mid - sz * 0.12f},
                       (Vector2){x + w - 2.0f, mid - sz * 0.12f}, thick, c);
            DrawLineEx((Vector2){x + 2.0f, mid + sz * 0.12f},
                       (Vector2){x + w - 2.0f, mid + sz * 0.12f}, thick, c);
            DrawLineEx((Vector2){x + w - 3.0f, top + sz * 0.10f},
                       (Vector2){x + 3.0f, bot - sz * 0.06f}, thick, c);
            break;

        case FSYM_APPROX: {
            float dy = sz * 0.12f;
            for (int row = 0; row < 2; ++row) {
                float yb = mid + ((row == 0) ? -dy : dy);
                DrawLineEx((Vector2){x + 1.0f, yb + 2.0f}, (Vector2){x + w * 0.35f, yb - 2.0f}, thick, c);
                DrawLineEx((Vector2){x + w * 0.35f, yb - 2.0f}, (Vector2){x + w * 0.65f, yb + 2.0f}, thick, c);
                DrawLineEx((Vector2){x + w * 0.65f, yb + 2.0f}, (Vector2){x + w - 1.0f, yb - 2.0f}, thick, c);
            }
            break;
        }

        case FSYM_ARROW:
            DrawLineEx((Vector2){x + 1.0f, mid}, (Vector2){x + w - 2.0f, mid}, thick, c);
            DrawLineEx((Vector2){x + w - 6.0f, mid - 4.0f}, (Vector2){x + w - 1.0f, mid}, thick, c);
            DrawLineEx((Vector2){x + w - 6.0f, mid + 4.0f}, (Vector2){x + w - 1.0f, mid}, thick, c);
            break;

        case FSYM_ANGLE:
            DrawLineEx((Vector2){x + w - 2.0f, top + sz * 0.14f}, (Vector2){x + 2.0f, bot}, thick, c);
            DrawLineEx((Vector2){x + 2.0f, bot}, (Vector2){x + w - 2.0f, bot}, thick, c);
            break;

        case FSYM_THETA: {
            float r = sz * 0.28f;
            DrawCircleLines((int)cx, (int)mid, r, c);
            DrawLineEx((Vector2){cx - r, mid}, (Vector2){cx + r, mid}, thick, c);
            break;
        }

        case FSYM_PHI_LOWER:
        case FSYM_PHI_UPPER: {
            float r = sz * 0.24f;
            DrawCircleLines((int)cx, (int)mid, r, c);
            DrawLineEx((Vector2){cx, top}, (Vector2){cx, bot + sz * 0.08f}, thick, c);
            break;
        }

        case FSYM_ZETA:
            DrawLineEx((Vector2){x + w * 0.20f, top + sz * 0.08f},
                       (Vector2){x + w * 0.80f, top + sz * 0.08f}, thick, c);
            DrawLineEx((Vector2){x + w * 0.80f, top + sz * 0.08f},
                       (Vector2){x + w * 0.22f, bot - sz * 0.08f}, thick, c);
            DrawLineEx((Vector2){x + w * 0.22f, bot - sz * 0.08f},
                       (Vector2){x + w * 0.74f, bot + sz * 0.04f}, thick, c);
            break;

        case FSYM_MU:
            DrawLineEx((Vector2){x + w * 0.24f, top + sz * 0.25f},
                       (Vector2){x + w * 0.24f, bot + sz * 0.18f}, thick, c);
            DrawLineEx((Vector2){x + w * 0.24f, bot - sz * 0.04f},
                       (Vector2){x + w * 0.72f, bot - sz * 0.04f}, thick, c);
            DrawLineEx((Vector2){x + w * 0.72f, top + sz * 0.25f},
                       (Vector2){x + w * 0.72f, bot}, thick, c);
            break;

        case FSYM_TAU:
            DrawLineEx((Vector2){x + 1.0f, top + sz * 0.26f},
                       (Vector2){x + w - 1.0f, top + sz * 0.26f}, thick, c);
            DrawLineEx((Vector2){cx, top + sz * 0.26f}, (Vector2){cx, bot}, thick, c);
            break;

        case FSYM_INT:
            DrawLineEx((Vector2){x + w * 0.65f, top - sz * 0.10f},
                       (Vector2){x + w * 0.35f, bot + sz * 0.14f}, thick, c);
            break;

        default: {
            const char *txt = formula_sym_ascii(sym);
            DrawText(txt, (int)x, (int)(baseline_y - sz * 0.82f), (int)sz, c);
            break;
        }
    }
}

static FormulaBox measure_node(const FormulaTree *tree,
                               int32_t id,
                               float sz,
                               const RataTheme *theme,
                               size_t depth) {
    FormulaBox box = {0.0f, sz * 0.8f, sz * 0.2f};
    if (id < 0 || (size_t)id >= tree->node_count || depth > RATA_FORMULA_MAX_DEPTH + 4u) {
        return box;
    }
    const FormulaNode *nd = &tree->nodes[id];
    switch (nd->kind) {
        case FNODE_ROW: {
            float total_w = 0.0f;
            float max_asc = sz * 0.75f;
            float max_desc = sz * 0.20f;
            int32_t ch = nd->first_child;
            while (ch >= 0) {
                FormulaBox cb = measure_node(tree, ch, sz, theme, depth + 1u);
                total_w += cb.w;
                if (cb.asc > max_asc) {
                    max_asc = cb.asc;
                }
                if (cb.desc > max_desc) {
                    max_desc = cb.desc;
                }
                ch = tree->nodes[ch].next_sibling;
            }
            box.w = total_w;
            box.asc = max_asc;
            box.desc = max_desc;
            return box;
        }
        case FNODE_TEXT: {
            float fsz = (sz < 9.0f) ? 9.0f : sz;
            box.w = (float)ui_measure_text(theme, nd->text, fsz);
            box.asc = sz * 0.82f;
            box.desc = sz * 0.18f;
            return box;
        }
        case FNODE_SYMBOL:
            box.w = measure_symbol_width(nd->sym, sz) + 2.0f;
            box.asc = sz * 0.85f;
            box.desc = sz * 0.20f;
            return box;

        case FNODE_FRAC: {
            float sub_sz = sz * 0.82f;
            if (sub_sz < 10.0f) {
                sub_sz = 10.0f;
            }
            FormulaBox num = measure_node(tree, nd->child_a, sub_sz, theme, depth + 1u);
            FormulaBox den = measure_node(tree, nd->child_b, sub_sz, theme, depth + 1u);
            float max_w = (num.w > den.w) ? num.w : den.w;
            float bar_off = sz * 0.32f;
            box.w = max_w + 10.0f;
            box.asc = bar_off + num.asc + num.desc + 4.0f;
            box.desc = (den.asc + den.desc + 4.0f) - bar_off;
            if (box.desc < sz * 0.25f) {
                box.desc = sz * 0.25f;
            }
            return box;
        }

        case FNODE_SQRT: {
            FormulaBox inner = measure_node(tree, nd->child_a, sz, theme, depth + 1u);
            box.w = inner.w + sz * 0.70f + 4.0f;
            box.asc = inner.asc + 4.0f;
            box.desc = inner.desc + 1.0f;
            return box;
        }

        case FNODE_SCRIPT: {
            float sc_sz = sz * 0.72f;
            if (sc_sz < 9.0f) {
                sc_sz = 9.0f;
            }
            FormulaBox base = measure_node(tree, nd->child_a, sz, theme, depth + 1u);
            FormulaBox sub = {0.0f, 0.0f, 0.0f};
            FormulaBox sup = {0.0f, 0.0f, 0.0f};
            if (nd->child_b >= 0) {
                sub = measure_node(tree, nd->child_b, sc_sz, theme, depth + 1u);
            }
            if (nd->child_c >= 0) {
                sup = measure_node(tree, nd->child_c, sc_sz, theme, depth + 1u);
            }
            float script_w = (sub.w > sup.w) ? sub.w : sup.w;
            box.w = base.w + script_w + 2.0f;
            box.asc = base.asc;
            if (nd->child_c >= 0) {
                float sup_top = sz * 0.44f + sup.asc;
                if (sup_top > box.asc) {
                    box.asc = sup_top;
                }
            }
            box.desc = base.desc;
            if (nd->child_b >= 0) {
                float sub_bot = sz * 0.28f + sub.desc;
                if (sub_bot > box.desc) {
                    box.desc = sub_bot;
                }
            }
            return box;
        }
        default:
            return box;
    }
}

static void draw_node(const FormulaTree *tree,
                      int32_t id,
                      float x,
                      float baseline_y,
                      float sz,
                      Color color,
                      const RataTheme *theme,
                      size_t depth) {
    if (id < 0 || (size_t)id >= tree->node_count || depth > RATA_FORMULA_MAX_DEPTH + 4u) {
        return;
    }
    const FormulaNode *nd = &tree->nodes[id];
    switch (nd->kind) {
        case FNODE_ROW: {
            float cx = x;
            int32_t ch = nd->first_child;
            while (ch >= 0) {
                FormulaBox cb = measure_node(tree, ch, sz, theme, depth + 1u);
                draw_node(tree, ch, cx, baseline_y, sz, color, theme, depth + 1u);
                cx += cb.w;
                ch = tree->nodes[ch].next_sibling;
            }
            break;
        }
        case FNODE_TEXT: {
            float fsz = (sz < 9.0f) ? 9.0f : sz;
            ui_draw_text(theme, nd->text, x, baseline_y - sz * 0.82f, fsz, color);
            break;
        }
        case FNODE_SYMBOL:
            draw_vector_symbol(nd->sym, x + 1.0f, baseline_y, sz, color);
            break;

        case FNODE_FRAC: {
            float sub_sz = sz * 0.82f;
            if (sub_sz < 10.0f) {
                sub_sz = 10.0f;
            }
            FormulaBox num = measure_node(tree, nd->child_a, sub_sz, theme, depth + 1u);
            FormulaBox den = measure_node(tree, nd->child_b, sub_sz, theme, depth + 1u);
            float max_w = (num.w > den.w) ? num.w : den.w;
            float total_w = max_w + 10.0f;
            float bar_y = baseline_y - sz * 0.32f;

            float num_x = x + (total_w - num.w) * 0.5f;
            float num_base = bar_y - 3.0f - num.desc;
            draw_node(tree, nd->child_a, num_x, num_base, sub_sz, color, theme, depth + 1u);

            DrawLineEx((Vector2){x + 2.0f, bar_y},
                       (Vector2){x + total_w - 2.0f, bar_y}, 1.5f, color);

            float den_x = x + (total_w - den.w) * 0.5f;
            float den_base = bar_y + 4.0f + den.asc;
            draw_node(tree, nd->child_b, den_x, den_base, sub_sz, color, theme, depth + 1u);
            break;
        }

        case FNODE_SQRT: {
            FormulaBox inner = measure_node(tree, nd->child_a, sz, theme, depth + 1u);
            float rad_w = sz * 0.70f;
            float top_y = baseline_y - inner.asc - 3.0f;
            float bot_y = baseline_y + inner.desc * 0.5f;
            float mid_y = baseline_y - sz * 0.25f;

            DrawLineEx((Vector2){x + 1.0f, mid_y}, (Vector2){x + rad_w * 0.35f, bot_y}, 1.5f, color);
            DrawLineEx((Vector2){x + rad_w * 0.35f, bot_y}, (Vector2){x + rad_w * 0.85f, top_y}, 1.5f, color);
            DrawLineEx((Vector2){x + rad_w * 0.85f, top_y},
                       (Vector2){x + rad_w + inner.w + 3.0f, top_y}, 1.5f, color);

            draw_node(tree, nd->child_a, x + rad_w + 1.0f, baseline_y, sz, color, theme, depth + 1u);
            break;
        }

        case FNODE_SCRIPT: {
            float sc_sz = sz * 0.72f;
            if (sc_sz < 9.0f) {
                sc_sz = 9.0f;
            }
            FormulaBox base = measure_node(tree, nd->child_a, sz, theme, depth + 1u);
            draw_node(tree, nd->child_a, x, baseline_y, sz, color, theme, depth + 1u);
            float sx = x + base.w + 1.0f;
            if (nd->child_c >= 0) {
                draw_node(tree, nd->child_c, sx, baseline_y - sz * 0.44f, sc_sz, color, theme, depth + 1u);
            }
            if (nd->child_b >= 0) {
                draw_node(tree, nd->child_b, sx, baseline_y + sz * 0.28f, sc_sz, color, theme, depth + 1u);
            }
            break;
        }
        default:
            break;
    }
}

void ui_formula_draw(Rectangle bounds,
                     const char *markup,
                     float font_size,
                     Color color,
                     const RataTheme *theme,
                     RataArena *scratch) {
    if (markup == NULL || scratch == NULL) {
        return;
    }
    ui_begin_scissor(theme, bounds.x, bounds.y, bounds.width, bounds.height);

    size_t mark = rata_arena_mark(scratch);
    FormulaTree tree;
    if (formula_parse_cstr(markup, &tree, scratch) == RATA_OK) {
        FormulaBox box = measure_node(&tree, tree.root, font_size, theme, 0u);
        float total_h = box.asc + box.desc;
        float start_x = bounds.x + 8.0f;
        if (box.w + 16.0f < bounds.width) {
            start_x = bounds.x + (bounds.width - box.w) * 0.5f;
        }
        float baseline_y = bounds.y + (bounds.height - total_h) * 0.5f + box.asc;
        draw_node(&tree, tree.root, start_x, baseline_y, font_size, color, theme, 0u);
    } else {
        ui_draw_text(theme, markup, bounds.x + 8.0f,
                     bounds.y + (bounds.height - font_size) * 0.5f, font_size, color);
    }
    rata_arena_restore(scratch, mark);
    EndScissorMode();
}

void ui_formula_card(Rectangle bounds,
                     const char *caption,
                     const char *markup,
                     float font_size,
                     const RataTheme *theme,
                     RataArena *scratch) {
    if (theme == NULL) {
        return;
    }
    DrawRectangleRec(bounds, theme->bg_plot);
    DrawRectangleLinesEx(bounds, 1.0f, theme->border);
    float top_off = 0.0f;
    if (caption != NULL && caption[0] != '\0') {
        ui_draw_text(theme, caption, bounds.x + 8.0f, bounds.y + 4.0f, 12.0f, theme->text_muted);
        top_off = 15.0f;
    }
    Rectangle inner = {bounds.x + 2.0f, bounds.y + top_off,
                       bounds.width - 4.0f, bounds.height - top_off - 2.0f};
    ui_formula_draw(inner, markup, font_size, theme->text_primary, theme, scratch);
}

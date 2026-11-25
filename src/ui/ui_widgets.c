#include "ui/ui_widgets.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "raygui.h"

bool ui_text_field(Rectangle bounds,
                   const char *label,
                   char *buf,
                   int buf_size,
                   int32_t field_id,
                   UiInputFocus *focus,
                   const RataTheme *theme) {
    if (buf == NULL || buf_size <= 1 || focus == NULL || theme == NULL) {
        return false;
    }
    float label_h = (label != NULL && label[0] != '\0') ? 17.0f : 0.0f;
    if (label_h > 0.0f) {
        ui_draw_text(theme, label, bounds.x, bounds.y, 13.0f, theme->text_secondary);
    }

    Rectangle box = {bounds.x, bounds.y + label_h, bounds.width, bounds.height - label_h};
    if (box.height < 22.0f) {
        box.height = 22.0f;
    }

    bool editing = (focus->active_id == field_id);
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mp = GetMousePosition();
        if (CheckCollisionPointRec(mp, box)) {
            focus->active_id = field_id;
            editing = true;
        } else if (editing) {
            focus->active_id = 0;
            return true;
        }
    }

    ui_begin_scissor(theme, box.x, box.y, box.width, box.height);
    int res = GuiTextBox(box, buf, buf_size, editing);
    EndScissorMode();
    if (res != 0) {
        if (editing) {
            focus->active_id = 0;
            return true;
        }
        focus->active_id = field_id;
    }
    return false;
}

bool ui_slider_row(Rectangle bounds,
                   const char *label,
                   const char *unit,
                   double *val,
                   double min_v,
                   double max_v,
                   const RataTheme *theme) {
    if (val == NULL || theme == NULL || max_v <= min_v) {
        return false;
    }
    char hdr[96];
    if (unit != NULL && unit[0] != '\0') {
        snprintf(hdr, sizeof(hdr), "%s: %.4g %s", (label != NULL) ? label : "", *val, unit);
    } else {
        snprintf(hdr, sizeof(hdr), "%s: %.4g", (label != NULL) ? label : "", *val);
    }
    ui_draw_text(theme, hdr, bounds.x, bounds.y, 13.0f, theme->text_secondary);

    Rectangle srec = {bounds.x, bounds.y + 16.0f, bounds.width, bounds.height - 18.0f};
    if (srec.height < 16.0f) {
        srec.height = 16.0f;
    }

    float fval = (float)(*val);
    float prev = fval;
    GuiSliderBar(srec, NULL, NULL, &fval, (float)min_v, (float)max_v);
    if (fabs((double)fval - (double)prev) > 1.0e-7) {
        *val = (double)fval;
        return true;
    }
    return false;
}

bool ui_segmented_tabs(Rectangle bounds,
                       const char *const *labels,
                       int count,
                       int *selected,
                       const RataTheme *theme) {
    if (labels == NULL || count <= 0 || selected == NULL || theme == NULL) {
        return false;
    }
    bool changed = false;
    float gap = 4.0f;
    float w = (bounds.width - gap * (float)(count - 1)) / (float)count;
    for (int i = 0; i < count; ++i) {
        Rectangle r = {bounds.x + (float)i * (w + gap), bounds.y, w, bounds.height};
        bool active = (*selected == i);
        if (ui_button(r, labels[i], active, theme)) {
            if (*selected != i) {
                *selected = i;
                changed = true;
            }
        }
    }
    return changed;
}

bool ui_tab_bar(Rectangle bounds,
                const char *const *labels,
                int count,
                int *selected,
                const RataTheme *theme) {
    if (labels == NULL || count <= 0 || selected == NULL || theme == NULL) {
        return false;
    }
    bool changed = false;
    DrawLine((int)bounds.x, (int)(bounds.y + bounds.height - 1.0f),
             (int)(bounds.x + bounds.width), (int)(bounds.y + bounds.height - 1.0f),
             theme->border);

    Vector2 mp = GetMousePosition();
    float cx = bounds.x;
    float fsz = 14.0f;
    for (int i = 0; i < count; ++i) {
        const char *lbl = (labels[i] != NULL) ? labels[i] : "";
        int tw = ui_measure_text(theme, lbl, fsz);
        float tab_w = (float)tw + 28.0f;
        if (cx + tab_w > bounds.x + bounds.width) {
            tab_w = (bounds.x + bounds.width) - cx;
        }
        if (tab_w < 24.0f) {
            break;
        }
        Rectangle tr = {cx, bounds.y, tab_w, bounds.height};
        bool active = (*selected == i);
        bool hovered = CheckCollisionPointRec(mp, tr);
        if (hovered && !active) {
            DrawRectangle((int)tr.x, (int)tr.y, (int)tr.width, (int)tr.height - 1,
                          (Color){70, 66, 52, 110});
        }
        Color tc = active ? theme->text_primary : (hovered ? theme->text_secondary : theme->text_muted);
        float tx = tr.x + (tr.width - (float)tw) * 0.5f;
        float ty = tr.y + (tr.height - fsz) * 0.5f - 1.0f;
        ui_begin_scissor(theme, tr.x + 2.0f, tr.y, tr.width - 4.0f, tr.height);
        ui_draw_text(theme, lbl, tx, ty, fsz, tc);
        EndScissorMode();

        if (active) {
            DrawRectangle((int)tr.x + 6, (int)(tr.y + tr.height - 2.0f),
                          (int)tr.width - 12, 2, theme->accent);
        }
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && *selected != i) {
            *selected = i;
            changed = true;
        }
        cx += tab_w + 2.0f;
    }
    return changed;
}

bool ui_button(Rectangle bounds, const char *label, bool active, const RataTheme *theme) {
    if (theme == NULL || label == NULL) {
        return false;
    }
    Vector2 mp = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mp, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color bg = active ? theme->bg_button_active : theme->bg_button;
    if (hovered && !active) {
        bg = theme->bg_button_hover;
    }
    Color border = active ? theme->border_strong : (hovered ? theme->border_strong : theme->border);
    Color text_col = active ? theme->text_primary : theme->text_secondary;
    if (hovered) {
        text_col = theme->text_primary;
    }

    DrawRectangleRec(bounds, bg);
    DrawRectangleLinesEx(bounds, 1.0f, border);

    float fsz = 13.5f;
    int tw = ui_measure_text(theme, label, fsz);
    float tx = bounds.x + (bounds.width - (float)tw) * 0.5f;
    float ty = bounds.y + (bounds.height - fsz) * 0.5f;
    ui_begin_scissor(theme, bounds.x + 2.0f, bounds.y + 1.0f,
                     bounds.width - 4.0f, bounds.height - 2.0f);
    ui_draw_text(theme, label, tx, ty, fsz, text_col);
    EndScissorMode();
    return clicked;
}

bool ui_toggle_chip(Rectangle bounds, const char *label, bool *state, const RataTheme *theme) {
    if (state == NULL) {
        return false;
    }
    if (ui_button(bounds, label, *state, theme)) {
        *state = !(*state);
        return true;
    }
    return false;
}

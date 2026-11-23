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
    float label_h = (label != NULL && label[0] != '\0') ? 16.0f : 0.0f;
    if (label_h > 0.0f) {
        DrawText(label, (int)bounds.x, (int)bounds.y, 12, theme->text_secondary);
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

    BeginScissorMode((int)box.x, (int)box.y, (int)box.width, (int)box.height);
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
    DrawText(hdr, (int)bounds.x, (int)bounds.y, 12, theme->text_secondary);

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

bool ui_button(Rectangle bounds, const char *label, bool active, const RataTheme *theme) {
    if (theme == NULL || label == NULL) {
        return false;
    }
    Vector2 mp = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mp, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    Color bg = active ? theme->bg_button_active : theme->bg_button;
    if (hovered && !active) {
        bg = (Color){44, 58, 82, 255};
    }
    Color border = active ? theme->accent : (hovered ? theme->border_strong : theme->border);
    Color text_col = active ? theme->text_primary : theme->text_secondary;
    if (hovered) {
        text_col = theme->text_primary;
    }

    DrawRectangleRec(bounds, bg);
    DrawRectangleLinesEx(bounds, active ? 1.5f : 1.0f, border);

    int tw = MeasureText(label, 12);
    int tx = (int)(bounds.x + (bounds.width - (float)tw) * 0.5f);
    int ty = (int)(bounds.y + (bounds.height - 12.0f) * 0.5f);
    BeginScissorMode((int)bounds.x + 2, (int)bounds.y + 1,
                     (int)bounds.width - 4, (int)bounds.height - 2);
    DrawText(label, tx, ty, 12, text_col);
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

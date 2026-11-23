#include "ui/ui_workspace.h"

#include <stdio.h>
#include <string.h>

void ui_workspace_registry_init(RataWorkspaceRegistry *reg) {
    if (reg == NULL) {
        return;
    }
    memset(reg, 0, sizeof(*reg));

    reg->entries[WS_RATAWAVE] = (RataWorkspaceEntry){
        WS_RATAWAVE,
        "ratawave",
        "RataWave",
        "DSP & Signals",
        "Signals by expression/primitives, step-by-step convolution, radix-2 & arbitrary-N FFT, "
        "z-transform pole-zero plots, sampling/aliasing, LTI response, and FIR/IIR filter design.",
        1,
        false,
        NULL, NULL, NULL, NULL, NULL
    };
    reg->entries[WS_RATACALC] = (RataWorkspaceEntry){
        WS_RATACALC,
        "ratacalc",
        "RataCalc",
        "Comms & EE Solver",
        "AM/FM/PM, modulation index, Carson bandwidth, SNR/dB, link budget, quantization/PCM, "
        "line codes, BPSK/QPSK/QAM BER, matched filter, and Shannon capacity with step-by-step work.",
        2,
        false,
        NULL, NULL, NULL, NULL, NULL
    };
    reg->entries[WS_RATALOOP] = (RataWorkspaceEntry){
        WS_RATALOOP,
        "rataloop",
        "RataLoop",
        "Control Systems",
        "TF/ZPK/state-space/spec generation, block-diagram reduction via rational polynomials, "
        "step/impulse response, Bode, Nyquist, root locus, pole-zero map, Routh table, and SSE.",
        3,
        false,
        NULL, NULL, NULL, NULL, NULL
    };
    reg->entries[WS_RATAGATE] = (RataWorkspaceEntry){
        WS_RATAGATE,
        "ratagate",
        "RataGate",
        "Digital Logic",
        "Event-driven logic simulator with gates, D/JK/T/SR flip-flops, muxes, counters, "
        "interactive inputs/LEDs/clock, automatic truth-table generation, and timing diagrams.",
        4,
        false,
        NULL, NULL, NULL, NULL, NULL
    };
    reg->entries[WS_RATAFORGE] = (RataWorkspaceEntry){
        WS_RATAFORGE,
        "rataforge",
        "RataForge",
        "VHDL & Waveforms",
        "In-house VHDL code editor with syntax highlighting, GHDL compile & simulate runner, "
        "testbench support, line-mapped compiler errors, and VCD waveform viewer.",
        5,
        false,
        NULL, NULL, NULL, NULL, NULL
    };
    reg->entries[WS_RATASIM] = (RataWorkspaceEntry){
        WS_RATASIM,
        "ratasim",
        "RataSim",
        "Analog Circuits",
        "Drag-and-drop schematic editor (R, L, C, sources, diode, BJT, MOSFET, op-amp) -> "
        "SPICE netlist -> libngspice (OP, DC sweep, AC, transient), probes, and scope plots.",
        6,
        false,
        NULL, NULL, NULL, NULL, NULL
    };
}

void ui_workspace_register(RataWorkspaceRegistry *reg,
                           RataWorkspaceId id,
                           void *state,
                           WorkspaceInitFn init_fn,
                           WorkspaceDrawFn draw_fn,
                           WorkspaceSaveFn save_fn,
                           WorkspaceLoadFn load_fn) {
    if (reg == NULL || (int)id < 0 || id >= WS_COUNT) {
        return;
    }
    RataWorkspaceEntry *e = &reg->entries[id];
    e->implemented = (draw_fn != NULL);
    e->state = state;
    e->init_fn = init_fn;
    e->draw_fn = draw_fn;
    e->save_fn = save_fn;
    e->load_fn = load_fn;
    if (e->init_fn != NULL) {
        e->init_fn(e->state);
    }
}

void ui_workspace_draw_coming_soon(const RataWorkspaceEntry *entry,
                                   Rectangle viewport,
                                   const RataTheme *theme) {
    if (entry == NULL || theme == NULL) {
        return;
    }
    float card_w = (viewport.width > 680.0f) ? 620.0f : (viewport.width - 40.0f);
    float card_h = 220.0f;
    Rectangle card = {
        viewport.x + (viewport.width - card_w) * 0.5f,
        viewport.y + (viewport.height - card_h) * 0.5f,
        card_w,
        card_h
    };
    ui_draw_panel(card, theme);

    DrawRectangle((int)card.x, (int)card.y, (int)card.width, 4, theme->accent);

    char badge[48];
    snprintf(badge, sizeof(badge), "PHASE %d  |  COMING SOON", entry->phase_num);
    ui_draw_text(theme, badge, card.x + 24.0f, card.y + 22.0f, 13.0f, theme->warning);

    ui_draw_text(theme, entry->name, card.x + 24.0f, card.y + 44.0f, 28.0f, theme->text_primary);
    ui_draw_text(theme, entry->subtitle, card.x + 24.0f, card.y + 78.0f, 16.0f, theme->accent);

    /* Wrap summary text neatly across lines. */
    const char *s = entry->planned_summary;
    float y = card.y + 112.0f;
    int max_w = (int)card.width - 48;
    while (*s != '\0' && y < card.y + card.height - 20.0f) {
        char line[160];
        size_t len = 0u;
        size_t last_sp = 0u;
        while (s[len] != '\0' && len < sizeof(line) - 1u) {
            line[len] = s[len];
            line[len + 1u] = '\0';
            if (s[len] == ' ') {
                last_sp = len;
            }
            if (ui_measure_text(theme, line, 14.0f) > max_w) {
                break;
            }
            len++;
        }
        if (s[len] != '\0' && last_sp > 0u) {
            len = last_sp;
        }
        if (len == 0u) {
            break;
        }
        line[len] = '\0';
        ui_draw_text(theme, line, card.x + 24.0f, y, 14.0f, theme->text_secondary);
        y += 20.0f;
        s += len;
        while (*s == ' ') {
            s++;
        }
    }
}

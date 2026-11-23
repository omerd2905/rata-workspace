# AGENTS.md — Rata Workspace Engineering Contract

This document defines the mandatory architectural, language, portability, numerical-correctness, and
testing rules for **Rata Workspace**. All contributors and coding sessions MUST follow these rules.

---

## 1. Product Overview

**Rata Workspace** is a native desktop multi-workspace application for electrical-engineering
students, built around one shared shell (sidebar switcher, shared theme, shared plot and formula
widgets) hosting six workspaces:

1. **RataWave** — DSP (signals, step-by-step convolution, DFT/FFT, z-transform pole-zero, sampling/aliasing, LTI response, FIR/IIR filter design).
2. **RataCalc** — General + communications solver (AM/FM/PM, modulation index, Carson bandwidth, SNR/dB, link budget, quantization, PCM, line codes, BER, matched filter, Shannon capacity) with given → formula → substitution → result and units.
3. **RataLoop** — Control systems (TF/ZPK/state-space/spec generation, block-diagram reduction via rational polynomials, step/impulse, Bode, Nyquist, root locus, pole-zero, Routh table, steady-state error).
4. **RataGate** — Event-driven digital logic simulator (gates, flip-flops, muxes, counters, truth tables, timing diagrams).
5. **RataForge** — VHDL editor with syntax highlighting, GHDL compile/simulate child process, VCD waveform viewer, line-mapped compiler errors.
6. **RataSim** — Schematic editor → netlist → libngspice (OP, DC, AC, transient), probes, scope, plots.

---

## 2. Hard Language Constraint

- **C11 strictly, with NO GNU extensions**: compile all project code with
  `-std=c11 -Wall -Wextra -Wpedantic -Werror -Wvla`.
- **No other languages**: no Python, JavaScript/TypeScript, Java, C++, Electron, web views, or
  scripting runtimes — neither in the application nor as build-time generators.
- Plain GNU Make and POSIX shell are permitted for building and automation.
- Avoid POSIX/GNU-only C functions (`strdup`, `strtok_r`, `strncasecmp`, `M_PI`, etc.). Use C11
  standard library facilities or explicit in-house helpers.

---

## 3. Portability & C-ABI Constraint (CRITICAL)

Every project module under `src/` must remain mechanically portable one file at a time to another
low-level language interoperating over the plain C ABI, re-running the exact same public-header
test suite after each swap:

- **One module = one `.c` + one `.h`**.
- **Header = plain C ABI contract**:
  - Allowed in public headers: functions, plain `struct`s, `enum`s, fixed-width integers
    (`int32_t`, `uint32_t`, `size_t`, `bool`, etc.), `double`, `float`, pointers, simple constant
    definitions (`#define` numeric/string constants or `enum` values).
  - **Forbidden in our code**:
    - Code-generating function-like macros
    - `_Generic`
    - Variable-length arrays (VLAs)
    - Variadic functions (`...`) in our own APIs
    - Bitfields in structs
    - Clever pointer tricks (pointer tagging, non-trivial type punning, intrusive container-of tricks)
- **No `<complex.h>`**: use our own `typedef struct cplx { double re; double im; } cplx;` in
  `src/core/cplx.h` and its explicit functions.
- **No hidden global state**: all state lives in caller-owned structs passed explicitly by pointer.
  No mutable file-scope static globals in `src/core`, `src/engines`, or `src/ui` logic.
- **Explicit memory ownership**: prefer caller-provided buffers or `RataArena *` arenas
  (`src/core/arena.h`). Every function in a public header must document who owns any returned or
  output memory.
- **Small, readable files**: target `< ~600` lines per file. Prefer clear, straightforward code over
  clever abstractions.

---

## 4. Strict Layering

Dependencies flow strictly downward; lower layers MUST NEVER include headers from higher layers:

```text
src/core/      Pure math, arena allocator, status codes. Standard C11 headers ONLY.
     ↑
src/engines/   Solvers, DSP/control/logic simulators, parsers, file formats.
     ↑         Depends ONLY on src/core/ and C11 stdlib (zero UI or OS/platform deps).
src/ui/        raylib + raygui widgets, theme, plot widget, formula widget, shell, workspace views.
     ↑         Depends on src/core/, src/engines/, raylib, raygui.
src/app/       Application entry point (main.c) and top-level lifecycle.
```

- `src/core/` and `src/engines/` must have **zero** UI, OpenGL, X11, Win32, or platform-specific
  dependencies (the sole future exception for OS process launching in `RataForge` will be isolated
  behind a dedicated platform module).
- Enforce layering via `make check-layering` and by linking the unit test binary exclusively against
  `src/core/` and `src/engines/` (`-lm` only).

---

## 5. Third-Party Dependencies

- Only approved C dependencies:
  - **raylib** (via Nix / system pkg-config / `RAYLIB_DIR`)
  - **raygui** (vendored unmodified in `vendor/raygui/`)
  - **libngspice** (C API, for Phase 6 / RataSim)
  - **GHDL** (external CLI process, for Phase 5 / RataForge)
- **Never** silently add another GUI, math, JSON, or testing library. Justify any new dependency
  with the user first.
- Everything under `vendor/` is third-party code and MUST NEVER be modified. Only our modules under
  `src/` follow the portability rules. Third-party headers are included with `-isystem` so our own
  code remains under `-Wall -Wextra -Wpedantic -Werror`.

---

## 6. Numerical Correctness, Units, & Safety

- **Results must be numerically correct**: a wrong Bode plot, wrong pole location, or wrong FFT is
  worse than none.
- **Units everywhere**: every axis, readout, and parameter displays its units (Hz, s, rad, dB,
  samples, etc.).
- **Visible assumptions**: state implicit assumptions directly in the UI (e.g., normalized
  `sinc(x) = sin(pi*x)/(pi*x)`, phase zeroed below magnitude threshold, assuming unity feedback).
- **Graceful edge-case handling**: never crash, produce NaN/Inf silently, or hang on unstable
  systems (`|p| >= 1`), singular matrices, division by zero, non-positive log-axis values, or
  malformed user input.
- **Project files**: simple, versioned, line-based text format (`src/engines/project_file.h`), no
  JSON library. All parsers must reject malformed input safely with bounded recursion and bounded
  allocations.

---

## 7. Testing & Fuzzing Rules

- **Plain C test runner** in `tests/` — no external test framework.
- **Public-header-only tests**: tests MUST `#include` only public `.h` headers (never `.c` files or
  internal symbols) so tests remain valid unchanged when a module's `.c` implementation is swapped.
- **Textbook ground truth**: every numerical module must be tested against independently known
  textbook/analytic results with explicitly stated tolerances.
- **Sanitizers**: debug builds and test runs on Linux compile with `-fsanitize=address,undefined`
  (`ASan + UBSan`).
- **Fuzz harnesses**: every parser (expression parser, project file parser, formula markup parser,
  and later VCD and netlist parsers) has a fuzz harness under `fuzz/` supporting both libFuzzer and
  a standalone runner (`-DFUZZ_STANDALONE`).

---

## 8. Building & Platforms

- Primary development platform: **NixOS** (`nix develop` via `flake.nix`).
- Secondary target: **Windows 11 with MinGW gcc** (MSYS2 MinGW64 or standalone MinGW + raylib).
- Common targets:
  - `make` — build the desktop app (`build/rata-workspace` or `build/rata-workspace.exe`)
  - `make test` — build and run unit tests (with ASan+UBSan on Linux debug builds)
  - `make fuzz-test` — build and run all parser fuzz harnesses against seed/mutation inputs
  - `make check-layering` — verify strict include layering rules
  - `make BUILD=release` — optimized build without sanitizers
  - `make clean` — remove build artifacts

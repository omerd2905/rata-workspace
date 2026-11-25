# Changelog

All notable changes to **Rata Workspace** are documented in this file.
This project adheres to [Semantic Versioning](https://semver.org/).

## [0.1.3] - 2026-11-25

### Added
- **Bidirectional `x[n]` / `h[n]` / `y[n]` Convolution & Deconvolution (`src/engines/dsp_conv.c`, `src/ui/ui_ratawave_conv.c`)**: enter any two of `x[n]`, `h[n]`, and `y[n]` to solve for the third (`Solve y[n]`, `Solve h[n]`, `Solve x[n]`), or enter all three to verify `y[n] == (x * h)[n]` (`Verify`). Supports exact finite-support FIR deconvolution with automatic `n0` shift tracking, causal ascending-power IIR long division, and safe divergence clamping.
- **Direct Sample-List Signal Input (`src/engines/dsp_signal.c`)**: `dsp_signal_from_text` accepts comma/semicolon-separated sample vectors (e.g. `1, 2, 1` or `[1, -0.5, 0.25]`) alongside symbolic expressions across Signals, Convolution, FFT, and LTI tabs, plus one-click `Copy Solved -> Input`.

### Changed
- **Column-Snapped Sample Probing (`src/ui/ui_plot.c`)**: hovering anywhere above or below a signal's amplitude snaps directly onto the exact sample at that horizontal column (and snaps to nearby poles/zeros on Z-plane plots), highlighting the sample dot and displaying `n = k  x[n] = val` instead of raw empty-space cursor coordinates.

## [0.1.2] - 2026-11-25

### Added
- **Top Menu Bar (`File`, `View`, `Workspace`, `Help`)**: full-width Coffee Bean (`#261C16`) desktop menu bar with dropdown menus, keyboard shortcuts (`Ctrl+=` / `Ctrl+-` / `Ctrl+0` zoom, `Ctrl+B` sidebar toggle, `Ctrl+O` / `Ctrl+S` project open/save, `Alt+1..6` workspace switch), clickable top-right `View N%` scale indicator, and a dedicated `Project File Path...` dialog.
- **High-DPI & User-Controlled Viewing Scale**: automatic OS monitor content-scale detection (`GetWindowScaleDPI`) combined with user-selectable viewing scales (`75%`–`200%` presets and `±10%` steps under `View`), DPI-aware initial window sizing on High-DPI Windows 11 displays, `Camera2D` logical viewport scaling, and `ui_begin_scissor` physical-framebuffer clipping, persisted across sessions in `settings.rata`.
- **Embedded Lora Typography (`src/ui/ui_font_lora.c`, `vendor/lora/OFL.txt`)**: embedded SIL OFL 1.1 **Lora Regular** as the general UI font with trilinear GPU mipmapping and seamless glyph fallback to JetBrains Mono NL for Greek and mathematical symbols.

### Changed
- **Minimalist Warm Studio Theme (`src/ui/ui_theme.c`, `src/ui/ui_shell.c`, `src/ui/ui_widgets.c`)**: replaced high-saturation neon palette and boxy chrome with **Pale Oak** (`#E8DDCB`) text, **Coffee Bean** (`#261C16`) top bar, **Dark Khaki** (`#3F3C2E`) surfaces, low-saturation plot traces, underline workspace tabs (`ui_tab_bar`), subdued faders, and an uncluttered collapsible sidebar.

## [0.1.1] - 2026-11-25

### Added
- **Signed Windows Auto-Updater (`src/app/app_updater.c`, `src/engines/update_manifest.c`)**: background & CLI (`rata-workspace --update`) update checks over HTTPS via WinHTTP, verifying exact byte size, streaming FIPS 180-4 SHA-256 (`src/core/sha256.c`), and Ed25519 signatures (vendored Monocypher 4.0.3) before staging or silently launching the installer; non-blocking `"vX.Y.Z ready — Restart & update"` banner and save-before-restart prompt in the shell.
- **Release Signing Tool & Manifest Fuzzer**: pure-C11 `rata-sign` CLI (`tools/rata_sign.c`, `make sign-tool`) and `fuzz_update_manifest` fuzz harness.
- **Icon Generator Target**: committed pure-C11 icon generator (`tools/gen_icon.c`) under `make icons`.
- **MIT License**: added root `LICENSE` and bundled license metadata across Linux, Nix, and Windows packages.

### Changed
- **Windows GUI Subsystem & `PATH`**: linked `rata-workspace.exe` with `-mwindows` (no stray console window when launched from Start Menu/Explorer) while preserving piped stdout/stderr and attaching to parent PowerShell/cmd consoles for `--version`, `--help`, and `--update`; per-user Inno Setup installer now registers `{app}` in `HKCU\Environment\Path` and relaunches after silent upgrades.
- **Nyquist-Boundary Sampling (`src/engines/dsp_sampling.c`)**: explicitly detects $f \equiv 0$ and $f \equiv f_s/2 \pmod{f_s}$, reporting effective amplitude $A\cos(\phi)$ and unrecoverable phase in RataWave instead of `phi -> -phi`.
- **Nix Flake & `stable` Branch**: locked `flake.lock` against `nixos-26.05`, wrapped runtime `LD_LIBRARY_PATH` (`libGL`, X11 libs, `libngspice`) and `PATH` (`ghdl`), added `/nix/store` update guidance in the About dialog, and configured releases to fast-forward the `stable` branch (`github:omerd2905/rata-workspace/stable`).

## [0.1.0] - 2026-11-23

### Added
- **Core Math (`src/core/`)**: alignment-safe bump arena allocator, explicit status codes, plain-struct complex math (`cplx`), ascending-power polynomials with overflow-safe Aberth–Ehrlich root finder, and radix-2 Cooley–Tukey + arbitrary-length Bluestein chirp-z FFT.
- **Parsers & Fuzz Harnesses (`src/engines/`, `fuzz/`)**: mathematical/EE expression parser (`delta`, `u`, `ramp`, `rect`, `tri`, normalized `sinc`, `sgn`, short-circuit `if`), versioned line-based `.rata` project files with forward-version detection, formula markup AST parser, and standalone/libFuzzer fuzz harnesses.
- **Shared Desktop Shell & Widgets (`src/ui/`)**: raylib + vendored raygui 4.0 shell with embedded OFL JetBrains Mono NL font (ASCII + Greek + math symbols), interactive linear/log/stem/pole-zero scientific plot widget, formula display widget, About dialog, `--version` CLI flag, per-user config storage (`$XDG_CONFIG_HOME/rata-workspace` on Linux, `%APPDATA%\RataWorkspace` on Windows), and six-workspace registry.
- **RataWave (Phase 1 — DSP Workspace)**:
  - Signal generator (expressions, primitives, piecewise presets, energy/power/RMS stats).
  - Discrete linear convolution with interactive animated step-by-step flip-and-shift playback.
  - DFT/FFT magnitude (linear/dB) and phase spectrum supporting both power-of-two and arbitrary transform lengths, windowing, log-frequency axis, and two-sided `fftshift`.
  - Z-transform pole-zero map (equal-aspect unit circle, origin poles/zeros, BIBO stability verdict), frequency response with unit-circle pole clamping warning, analytic group delay, and impulse/step/filtered LTI time responses.
  - Sampling & aliasing demo with exact phase negation (`phi -> -phi` in odd Nyquist zones) and Whittaker–Shannon sinc reconstruction.
  - FIR (windowed-sinc Type-I linear phase) and IIR (Butterworth and Chebyshev Type I via pre-warped Bilinear Transform) filter designer with one-click handoff to the LTI tab.
- **Packaging & CI**: Nix flake package/app (`nix profile install` / `nix run`), static-linked Windows 11 single-`.exe` + per-user Inno Setup installer + portable zip, static-raylib Linux release tarball with `make install`/`make uninstall`, SHA-256 verified CI builds, Xvfb UI smoke test, and automated tag-driven GitHub Releases.

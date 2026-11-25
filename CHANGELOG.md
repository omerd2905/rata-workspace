# Changelog

All notable changes to **Rata Workspace** are documented in this file.
This project adheres to [Semantic Versioning](https://semver.org/).

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

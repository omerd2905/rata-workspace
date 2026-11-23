# Changelog

All notable changes to **Rata Workspace** are documented in this file.
This project adheres to [Semantic Versioning](https://semver.org/).

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

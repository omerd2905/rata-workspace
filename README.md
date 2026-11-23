# Rata Workspace

Native desktop multi-workspace application in pure ISO **C11** for electrical-engineering students.

---

## Install & Update

### NixOS / Nix (Recommended)

Install:
```sh
nix profile install git+ssh://git@github.com/omerd2905/rata-workspace
```

Update:
```sh
nix profile upgrade rata-workspace
```

Run without installing:
```sh
nix run git+ssh://git@github.com/omerd2905/rata-workspace
```

### Windows 11

Install (per-user, no administrator rights required):
1. Download `rata-workspace-vX.Y.Z-windows-x86_64-setup.exe` from [GitHub Releases](https://github.com/omerd2905/rata-workspace/releases) (verify with `SHA256SUMS`).
2. Run the installer (installs to `%LOCALAPPDATA%\Programs\RataWorkspace`, Start Menu, optional Desktop shortcut).

Update:
1. Download and run the newer `setup.exe` — it upgrades in place without touching your settings or projects in `%APPDATA%\RataWorkspace`.

Portable (no installer): download `rata-workspace-vX.Y.Z-windows-x86_64-portable.zip`, unzip, and run `rata-workspace.exe`.

### Linux (Without Nix)

Install (or update by repeating with the newer tarball):
```sh
tar -xzf rata-workspace-vX.Y.Z-linux-x86_64.tar.gz
cd rata-workspace-vX.Y.Z-linux-x86_64
make install PREFIX="$HOME/.local"
```

Uninstall (leaves `$XDG_CONFIG_HOME/rata-workspace` user data untouched):
```sh
make uninstall PREFIX="$HOME/.local"
```

---

## Workspaces

1. **RataWave** *(Implemented — Phase 1)* — DSP & Signals:
   - Signal generation by expression (`delta(n)`, `u(n)`, `ramp(n)`, `rect(x)`, `tri(x)`, `sinc(x)`, `if(...)`) or primitives/piecewise builder
   - Discrete linear convolution with animated step-by-step flip-and-shift view
   - DFT / FFT (Cooley–Tukey radix-2 + Bluestein chirp-z for arbitrary lengths $N$), windowing, linear/dB magnitude, phase, `fftshift`
   - Z-transform pole-zero map (equal-aspect unit circle, origin poles/zeros, BIBO stability verdict), frequency response with unit-circle pole clamping indicator, analytic group delay, and impulse/step/filtered LTI responses
   - Sampling & aliasing demo with exact phase negation (`phi -> -phi` in odd Nyquist zones) and Whittaker–Shannon sinc reconstruction
   - FIR (windowed-sinc Type-I linear phase) and IIR (Butterworth & Chebyshev Type I via pre-warped Bilinear Transform) filter designer
2. **RataCalc** *(Coming Soon — Phase 2)* — General + communications solver
3. **RataLoop** *(Coming Soon — Phase 3)* — Control systems & block-diagram reduction
4. **RataGate** *(Coming Soon — Phase 4)* — Event-driven digital logic simulator
5. **RataForge** *(Coming Soon — Phase 5)* — VHDL editor, GHDL runner, VCD waveform viewer
6. **RataSim** *(Coming Soon — Phase 6)* — Schematic editor & libngspice analog simulation

---

## Building from Source

### NixOS
```sh
nix develop
make check                # Layering checks + unit tests + parser fuzz tests (ASan + UBSan)
make ui-smoke             # Headless/Xvfb UI render test
make BUILD=release        # Optimized release binary -> build/linux-release/rata-workspace
```

### Linux (Non-NixOS)
Install X11/OpenGL development headers and download `raylib-5.5_linux_amd64.tar.gz` (verify SHA-256 `3d95ef03d5b38dfa55c0a16ca122d382134b078f0e5b270b52fe7eae0549c000`):
```sh
RAYLIB_DIR=/path/to/raylib-5.5_linux_amd64 RAYLIB_STATIC=1 make BUILD=release
make install PREFIX="$HOME/.local"
```

### Windows 11 (MSYS2 MinGW 64-bit)
```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils make curl unzip
curl -fsSL -o raylib.zip https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip
unzip -q raylib.zip -d deps
RAYLIB_DIR=deps/raylib-5.5_win64_mingw-w64 make BUILD=release
make BUILD=release test fuzz-test
```

## Architecture & Portability

See [AGENTS.md](AGENTS.md) for the complete C11 portability, plain-C-ABI, strict layering (`core -> engines -> ui -> app`), arena ownership, packaging, and testing contract.

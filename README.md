# Rata Workspace

Native desktop multi-workspace application in pure ISO **C11** (no GNU extensions) for
electrical-engineering students.

## Workspaces

1. **RataWave** *(Implemented — Phase 1)* — DSP & Signals:
   - Signal generation by expression (`delta(n)`, `u(n)`, `ramp(n)`, `rect(x)`, `tri(x)`, `sinc(x)`, `if(...)`) or primitives/piecewise builder
   - Discrete linear convolution with interactive animated step-by-step flip-and-shift view
   - DFT / FFT (Cooley–Tukey radix-2 + Bluestein chirp-z for arbitrary lengths $N$), windowing, linear/dB magnitude, phase, fftshift
   - Z-transform pole-zero map (equal-aspect unit circle, origin poles/zeros, BIBO stability verdict), frequency response, analytic group delay, and impulse/step/filtered LTI time responses
   - Sampling & aliasing demo with Nyquist analysis and Whittaker–Shannon sinc reconstruction
   - FIR (windowed-sinc Type-I linear phase) and IIR (Butterworth & Chebyshev Type I via pre-warped Bilinear Transform) filter designer
2. **RataCalc** *(Coming Soon — Phase 2)* — General + communications solver
3. **RataLoop** *(Coming Soon — Phase 3)* — Control systems & block-diagram reduction
4. **RataGate** *(Coming Soon — Phase 4)* — Event-driven digital logic simulator
5. **RataForge** *(Coming Soon — Phase 5)* — VHDL editor, GHDL runner, VCD waveform viewer
6. **RataSim** *(Coming Soon — Phase 6)* — Schematic editor & libngspice analog simulation

## Building & Running

### NixOS (Primary Development Environment)

```sh
nix develop
make                # Build debug binary with ASan + UBSan -> build/linux-debug/rata-workspace
make check          # Layering checks + unit tests + parser fuzz tests (ASan + UBSan)
make BUILD=release  # Optimized release binary -> build/linux-release/rata-workspace
./build/linux-release/rata-workspace
```

### Linux (Non-NixOS)

Install X11/OpenGL development headers and `raylib` (either via system `pkg-config`, or download the
official `raylib-5.5_linux_amd64.tar.gz` release and pass `RAYLIB_DIR`):

```sh
RAYLIB_DIR=/path/to/raylib-5.5_linux_amd64 make BUILD=release
./build/linux-release/rata-workspace
```

### Windows 11 (MinGW-w64 GCC via MSYS2)

In an **MSYS2 MinGW 64-bit** shell:

```sh
pacman -S mingw-w64-x86_64-gcc make curl unzip
curl -fsSL -o raylib.zip https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip
unzip -q raylib.zip -d deps
RAYLIB_DIR=deps/raylib-5.5_win64_mingw-w64 make BUILD=release
make BUILD=release test fuzz-test
./build/windows-release/rata-workspace.exe
```

Or pass `RAYLIB_DIR=/path/to/raylib-5.5_win64_mingw-w64` if raylib is already installed elsewhere.

## Architecture & Portability

See [AGENTS.md](AGENTS.md) for the complete C11 portability, plain-C-ABI, strict layering
(`core -> engines -> ui -> app`), arena ownership, and testing contract.

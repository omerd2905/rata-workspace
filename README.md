# Rata Workspace

Native desktop multi-workspace application in pure ISO **C11** for electrical-engineering students — double-precision in-house solvers, zero scripting runtimes, and strict plain-C-ABI modularity.

---

## Install & Update

### NixOS / Nix (Recommended — Tracks Published Releases)

Install (tracks the `stable` release branch):
```sh
nix profile install github:omerd2905/rata-workspace/stable
```

Update:
```sh
nix profile upgrade rata-workspace
```

Run without installing:
```sh
nix run github:omerd2905/rata-workspace/stable
```

> **Bleeding edge (`main`)**: to run unreleased commits on `main`, omit `/stable`:
> `nix run github:omerd2905/rata-workspace`

### Windows 11

Install (per-user, no administrator rights required):
1. Download `rata-workspace-vX.Y.Z-windows-x86_64-setup.exe` from [GitHub Releases](https://github.com/omerd2905/rata-workspace/releases) (verify with `SHA256SUMS`).
2. Run the installer (installs to `%LOCALAPPDATA%\Programs\RataWorkspace`, adds it to your per-user `PATH`, creates a Start Menu entry and optional Desktop shortcut).

Updates (zero configuration required):
- **Automatic**: Rata Workspace checks GitHub Releases over HTTPS in the background (`About & Updates` -> `Update automatically`, enabled by default), verifies the Ed25519 signature + SHA-256 + exact byte size, and offers **Restart & update** (or applies silently on the next launch).
- **CLI**: from any PowerShell window, run:
  ```powershell
  rata-workspace --update
  ```
- **Manual**: download and run the newer `setup.exe` — it upgrades in place without touching your settings or projects in `%APPDATA%\RataWorkspace`.

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
   - Sampling & aliasing demo with exact phase negation (`phi -> -phi` in odd Nyquist zones), explicit Nyquist-boundary handling ($f \equiv 0$ or $f_s/2 \pmod{f_s}$ showing effective amplitude $A\cos\phi$ and unrecoverable phase), and Whittaker–Shannon sinc reconstruction
   - FIR (windowed-sinc Type-I linear phase) and IIR (Butterworth & Chebyshev Type I via pre-warped Bilinear Transform) filter designer
2. **RataCalc** *(Coming Soon — Phase 2)* — General + communications solver
3. **RataLoop** *(Coming Soon — Phase 3)* — Control systems & block-diagram reduction
4. **RataGate** *(Coming Soon — Phase 4)* — Event-driven digital logic simulator
5. **RataForge** *(Coming Soon — Phase 5)* — VHDL editor, GHDL runner, VCD waveform viewer
6. **RataSim** *(Coming Soon — Phase 6)* — Schematic editor & libngspice analog simulation

---

## Building from Source

### NixOS / Nix
```sh
nix develop
make check                # Layering checks + unit tests + parser fuzz tests (ASan + UBSan)
make ui-smoke             # Headless/Xvfb UI render test
make BUILD=release        # Optimized release binary -> build/linux-release/rata-workspace
make icons                # Regenerate assets/rata-workspace.png and .ico via tools/gen_icon.c
```

### Linux (Non-NixOS)
Install X11/OpenGL development headers and download `raylib-5.5_linux_amd64.tar.gz` (verify SHA-256 `3d95ef03d5b38dfa55c0a16ca122d382134b078f0e5b270b52fe7eae0549c000`):
```sh
RAYLIB_DIR=/path/to/raylib-5.5_linux_amd64 RAYLIB_STATIC=1 make BUILD=release
make install PREFIX="$HOME/.local"
```

### Windows 11 (MSYS2 MinGW 64-bit)
Open an **MSYS2 MINGW64** shell:
```sh
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-binutils make curl unzip
curl -fsSL -o raylib.zip https://github.com/raysan5/raylib/releases/download/5.5/raylib-5.5_win64_mingw-w64.zip
unzip -q raylib.zip -d deps
RAYLIB_DIR=deps/raylib-5.5_win64_mingw-w64 make BUILD=release
make BUILD=release test fuzz-test
```

---

## Release Signing Setup (Maintainer One-Time Setup)

Windows auto-updates are signed with Ed25519 (via vendored Monocypher) during `.github/workflows/release.yml` and verified against the public key compiled into `src/engines/update_pubkey.c`. Unconfigured builds fail closed and reject unsigned updates.

1. Inside a cloned checkout on **NixOS** (`nix develop`) or **Windows MSYS2 MINGW64**, build `rata-sign` and generate a fresh Ed25519 keypair:
   ```sh
   # On NixOS:
   nix develop -c sh -c 'make BUILD=release sign-tool && ./build/linux-release/rata-sign keygen'

   # Or on Linux / MSYS2 MINGW64:
   make BUILD=release sign-tool
   ./build/linux-release/rata-sign keygen   # (.exe on Windows)
   ```
2. Store the printed 64-character hex **private signing seed** ONLY in the GitHub repository secret named `RATA_UPDATE_SIGNING_KEY` (Settings -> Secrets and variables -> Actions -> New repository secret, or `gh secret set RATA_UPDATE_SIGNING_KEY`). **Never commit the private seed.**
3. Paste the printed `RATA_UPDATE_ED25519_PUBKEY` 32-byte array into `src/engines/update_pubkey.c` and commit it to `main`.
4. Tagging `vX.Y.Z` (matching `VERSION` and `CHANGELOG.md`) verifies that `RATA_UPDATE_SIGNING_KEY` matches `update_pubkey.c`, publishes the signed `update-manifest.txt` and binaries to GitHub Releases, and fast-forwards `stable`.

---

## Architecture & License

- **Architecture & Portability Contract**: see [AGENTS.md](AGENTS.md) for the strict ISO C11, plain-C-ABI, layering (`core -> engines -> ui -> app`), arena ownership, packaging, and testing rules.
- **License**: Rata Workspace is released under the [MIT License](LICENSE). Bundled third-party components under `vendor/` are distributed under their respective permissive licenses (`raygui` — zlib; `JetBrains Mono NL` — SIL OFL 1.1; `Monocypher` — BSD-2-Clause / CC0).

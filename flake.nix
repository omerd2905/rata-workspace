{
  description = "Rata Workspace — native C11 multi-workspace EE desktop application";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      version = builtins.replaceStrings [ "\n" "\r" " " ] [ "" "" "" ] (builtins.readFile ./VERSION);
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs {
        inherit system;
      }));
    in
    {
      packages = forAllSystems (pkgs:
        let
          rata-workspace = pkgs.stdenv.mkDerivation {
            pname = "rata-workspace";
            inherit version;
            src = self;

            nativeBuildInputs = with pkgs; [
              gnumake
              pkg-config
              makeWrapper
            ];

            # Runtime and future-phase dependencies declared here so
            # `nix profile install` needs no separate user installs.
            buildInputs = with pkgs; [
              raylib
              libngspice
              ghdl
              libGL
              xorg.libX11
              xorg.libXcursor
              xorg.libXrandr
              xorg.libXinerama
              xorg.libXi
            ];

            buildPhase = ''
              runHook preBuild
              make BUILD=release app
              runHook postBuild
            '';

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              make BUILD=release check
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              make BUILD=release install PREFIX="$out"
              wrapProgram "$out/bin/rata-workspace" \
                --prefix PATH : "${pkgs.lib.makeBinPath [ pkgs.ghdl ]}" \
                --prefix LD_LIBRARY_PATH : "${pkgs.lib.makeLibraryPath [ pkgs.libngspice ]}"
              runHook postInstall
            '';

            meta = with pkgs.lib; {
              description = "Native C11 multi-workspace toolkit for electrical-engineering students";
              homepage = "https://github.com/omerd2905/rata-workspace";
              platforms = platforms.linux;
              mainProgram = "rata-workspace";
            };
          };
        in
        {
          default = rata-workspace;
          inherit rata-workspace;
        });

      apps = forAllSystems (pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${pkgs.stdenv.hostPlatform.system}.default}/bin/rata-workspace";
        };
      });

      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          name = "rata-workspace-dev";

          nativeBuildInputs = with pkgs; [
            gcc
            gnumake
            pkg-config
            clang
            gdb
          ];

          buildInputs = with pkgs; [
            raylib
            libngspice
            ngspice
            ghdl
            libGL
            xorg.libX11
            xorg.libXcursor
            xorg.libXrandr
            xorg.libXinerama
            xorg.libXi
          ];

          shellHook = ''
            echo "Rata Workspace v${version} dev shell ready (gcc, make, raylib, libngspice, ghdl)."
            echo "Run 'make' to build, 'make check' to run tests + fuzzers with ASan+UBSan."
          '';
        };
      });
    };
}

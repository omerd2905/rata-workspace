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
          ghdlPkg = pkgs.lib.optionals pkgs.stdenv.hostPlatform.isx86_64 [ pkgs.ghdl ];
          runtimeLibs = with pkgs; [
            libGL
            libx11
            libxcursor
            libxrandr
            libxinerama
            libxi
            libxext
            libngspice
          ];

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
            buildInputs = [ pkgs.raylib ] ++ ghdlPkg ++ runtimeLibs;

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
                ${pkgs.lib.optionalString (ghdlPkg != []) ''--prefix PATH : "${pkgs.lib.makeBinPath ghdlPkg}" \''}
                --prefix LD_LIBRARY_PATH : "${pkgs.lib.makeLibraryPath runtimeLibs}"
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

      apps = forAllSystems (pkgs:
        let
          pkg = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
        in
        {
          default = {
            type = "app";
            program = "${pkg}/bin/rata-workspace";
            meta = pkg.meta;
          };
        });

      devShells = forAllSystems (pkgs:
        let
          ghdlPkg = pkgs.lib.optionals pkgs.stdenv.hostPlatform.isx86_64 [ pkgs.ghdl ];
        in
        {
          default = pkgs.mkShell {
            name = "rata-workspace-dev";

            # Avoid _FORTIFY_SOURCE warnings becoming -Werror failures on -O0/debug builds.
            hardeningDisable = [ "fortify" ];

            nativeBuildInputs = with pkgs; [
              gnumake
              pkg-config
              gdb
              clang
            ];

            buildInputs = (with pkgs; [
              raylib
              libngspice
              ngspice
              libGL
              libx11
              libxcursor
              libxrandr
              libxinerama
              libxi
              libxext
            ]) ++ ghdlPkg;

            shellHook = ''
              export CC=gcc
              echo "Rata Workspace v${version} dev shell ready (gcc, make, raylib, libngspice, ghdl)."
              echo "Run 'make' to build, 'make check' to run tests + fuzzers with ASan+UBSan."
            '';
          };
        });
    };
}

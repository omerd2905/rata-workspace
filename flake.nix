{
  description = "Rata Workspace — native C11 multi-workspace EE desktop application";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs {
        inherit system;
      }));
    in
    {
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
            echo "Rata Workspace dev shell ready (gcc, make, raylib, libngspice, ghdl)."
            echo "Run 'make' to build, 'make test' to run unit tests with ASan+UBSan."
          '';
        };
      });
    };
}

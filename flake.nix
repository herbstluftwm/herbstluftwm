{
  description = "Development shell for herbstluftwm";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { nixpkgs, ... }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system:
          f {
            inherit system;
            pkgs = import nixpkgs { inherit system; };
          });
    in {
      devShells = forAllSystems ({ pkgs, ... }:
        let
          pythonEnv = pkgs.python3.withPackages (ps: with ps; [
            ewmh
            pip
            pytest
            pytest-xdist
            tox
            xlib
          ]);
        in {
          default = pkgs.mkShell {
            packages = with pkgs; [
              asciidoc
              cmake
              expat
              fontconfig
              freetype
              gdb
              gnumake
              libx11
              libxcb
              libxdmcp
              libxext
              libxfixes
              libxft
              libxinerama
              libxrandr
              libxrender
              libxslt
              pkg-config
              procps
              pythonEnv
              xdotool
              xorg-server
              xsetroot
              xterm
            ];

            shellHook = ''
              export FONTCONFIG_FILE="${pkgs.fontconfig.out}/etc/fonts/fonts.conf"
              export FONTCONFIG_PATH="${pkgs.fontconfig.out}/etc/fonts"
              export PYTHONPATH="$PWD/python${PYTHONPATH:+:$PYTHONPATH}"
              export XDG_CACHE_HOME="$PWD/.cache"
              mkdir -p "$XDG_CACHE_HOME"
            '';
          };
        });
    };
}

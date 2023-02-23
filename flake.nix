{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";

    hyprland = {
      url = "github:hyprwm/Hyprland";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { nixpkgs, flake-utils, hyprland, ... }: flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs {
        inherit system;
      };

      gbar = (with pkgs; stdenv.mkDerivation {
        nativeBuildInputs = [
          nixpkgs.meson
          nixpkgs.gcc
          nixpkgs.ninja
          nixpkgs.wayland
          nixpkgs.bluez
          nixpkgs.gtk3
          nixpkgs.gtk-layer-shell
          hyprland
        ];

        buildPhase = ''
          meson build --buildtype=release -DWithNvidia=false
          ninja -C build && sudo ninja -C build install
        '';
       
        installPhase = ''
          mkdir -p $out/bin
          cp build/gbar $out/bin/gbar
        '';
      });
    in rec {
      defaultApp = flake-utils.lib.mkApp {
        drv = defaultPackage;
      };
      defaultPackage = gbar;
      devShell = pkgs.mkShell {
        buildInputs = [
          gbar
        ];
      };
    }
  );
}


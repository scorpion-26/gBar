{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils, ... }: flake-utils.lib.eachSystem ["x86_64-linux"] (system:
    let
      pkgs = import nixpkgs {
        inherit system;
      };

      gbar = (with pkgs; stdenv.mkDerivation {

        name = "gbar";

        src = ./.;

        nativeBuildInputs = [
          pkg-config
          meson
          cmake
          ninja
        ];
        buildInputs = [
          wayland
          wayland-protocols
          wayland-scanner
          bluez
          gtk3
          gtk-layer-shell
          libpulseaudio
        ];


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


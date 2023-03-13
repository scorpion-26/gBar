# Based on Hyprland's home-manager and NixOS module

self:
{ config
, lib
, pkgs
, ...
}: let
  cfg = config.programs.gBar;

  defaultGBarPackage = self.defaultPackage.x86_64-linux;

in {
  options.programs.gBar = {
    enable = lib.mkEnableOption "Wether to enable gBar, a blazingly fast statusbar written in c++";

    package = lib.mkOption {
      type = lib.types.nullOr lib.types.package;
      default = defaultGBarPackage;
      defaultText = lib.literalExpression "<gBar flake>.packages.<system>.default";
      example = lib.literalExpression "<gBar flake>.packages.<system>.default.override { }";
      description = ''
        gBar package to use.
      '';
    };

    extraConfig = lib.mkOption {
      type = lib.types.nullOr lib.types.lines;
      default = null;
      description = ''
        Configuration to write to ~/.config/gBar/config, if none nothing happens
      '';
    };

    extraCSS = lib.mkOption {
      type = lib.types.nullOr lib.types.lines;
      default = null;
      description = ''
        Configuration to write to ~/.config/gBar/config, if none nothing happens
      '';
    };
  };

  config = lib.mkIf cfg.enable {
    home.packages = lib.optional (cfg.package != null) cfg.package;

    xdg.configFile."gBar/config" = lib.mkIf (cfg.extraConfig != null) {
      text = cfg.extraConfig;
    };
    xdg.configFile."gBar/style.css" = lib.mkIf (cfg.extraCSS != null) {
      text = cfg.extraCSS;
    };
  };
}

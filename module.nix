# Based on Hyprland's home-manager and NixOS module

self:
{ config
, lib
, pkgs
, ...
}:

with lib;

let
  cfg = config.programs.gBar;

  defaultGBarPackage = self.defaultPackage.x86_64-linux;

in {
  options.programs.gBar = {
    enable = mkEnableOption "Wether to enable gBar, a blazingly fast statusbar written in c++";

    package = mkOption {
      type = types.nullOr types.package;
      default = defaultGBarPackage;
      defaultText = literalExpression "<gBar flake>.packages.<system>.default";
      example = literalExpression "<gBar flake>.packages.<system>.default.override { }";
      description = ''
        gBar package to use.
      '';
    };

    config = mkOption {
        default = {};
        description = "Options to write to gBar config file, named as standard options";
        type = types.submodule {
            options = {
                CPUThermalZone = mkOption {
                    type = types.nullOr types.str;
                    default = "/sys/devices/pci0000:00/0000:00:18.3/hwmon/hwmon2/temp1_input";
                    description = "path to the cpu thermal sensor, probably something in /sys/device";
                };
                SuspendCommand = mkOption {
                    type = types.str;
                    default = "systemctl suspend";
                    description = "";
                };
                LockCommand = mkOption {
                    type = types.nullOr types.str;
                    default = null;
                    description = "";
                };
                ExitCommand = mkOption {
                    type = types.str;
                    default = "killall Hyprland";
                    description = "";
                };
                BatteryFolder = mkOption {
                    type = types.nullOr types.str;
                    default = null;
                    description = "";
                };
                DefaultWorkspaceSymbol = mkOption {
                    type = types.str;
                    default = "";
                    description = "";
                };
                WorkspaceScrollOnMonitor = mkOption {
                    type = types.bool;
                    default = true;
                    description = "";
                };
                WorkspaceScrollInvert = mkOption {
                    type = types.bool;
                    default = false;
                    description = "";
                };
                UseHyprlandIPC = mkOption {
                    type = types.bool;
                    default = false;
                    description = "";
                };
                Location = mkOption {
                    type = types.enum ["T" "B" "L" "R"];
                    default = "T";
                    description = "";
                };
                CenterTime = mkOption {
                    type = types.bool;
                    default = true;
                    description = "";
                };
                TimeSpace = mkOption {
                    type = types.nullOr types.str;
                    default = null;
                    description = "";
                };
                AudioInput = mkOption {
                    type = types.bool;
                    default = false;
                    description = "";
                };
                AudioRevealer = mkOption {
                    type = types.bool;
                    default = false;
                    description = "";
                };
                AudioScrollSpeed = mkOption {
                    type = types.nullOr types.int;
                    default = null;
                    description = "";
                };
                AudioMinVolume = mkOption {
                    type = types.nullOr types.int;
                    default = null;
                    description = "";
                };
                AudioMaxVolume = mkOption {
                    type = types.nullOr types.int;
                    default = null;
                    description = "";
                };
                NetworkAdapter = mkOption {
                    type = types.nullOr types.str;
                    default = null;
                    description = "";
                };
                NetworkWidget = mkOption {
                    type = types.bool;
                    default = true;
                    description = "";
                };
                EnableSNI = mkOption {
                    type = types.bool;
                    default = true;
                    description = "";
                };
                MinDownloadBytes = mkOption {
                    type = types.nullOr types.int;
                    default = null;
                    description = "";
                };
                MaxDownloadBytes = mkOption {
                    type = types.nullOr types.int;
                    default = null;
                    description = "";
                };
                MinUploadBytes = mkOption {
                    type = types.nullOr types.int;
                    default = null;
                    description = "";
                };
                MaxUploadBytes = mkOption {
                    type = types.nullOr types.int;
                    default = null;
                    description = "";
                };
            };
        };
    };

    extraConfig = mkOption {
      type = types.nullOr types.lines;
      default = '' '';
      description = ''
        Configuration to write to ~/.config/gBar/config, if you want to use your own config then set this to null and home manager wont write anything
      '';
    };

    extraCSS = mkOption {
      type = types.nullOr types.lines;
      default = null;
      description = ''
        Configuration to write to ~/.config/gBar/config, if none nothing happens
      '';
    };
  };

  config = let
      # Takes attributeset with an arbitrary value and returns it as an attrset of valid gBar options
      applyVal = x:
          let anyToString = a: if isBool a then boolToString a else toString a;
          in attrsets.mapAttrs (name: value:
                      name + ": " + (anyToString value)) (filterAttrs (n: v: v != null) x);

      gBarConfig = concatMapStrings (x: x + "\n") (attrValues (applyVal cfg.config));

  in mkIf cfg.enable {
    home.packages = optional (cfg.package != null) cfg.package;

    xdg.configFile."gBar/config" = mkIf (cfg.extraConfig != null) {
      text = "# Generated by Home Manager\n" + gBarConfig + cfg.extraConfig;
    };
    xdg.configFile."gBar/style.css" = mkIf (cfg.extraCSS != null) {
      text = cfg.extraCSS;
    };
  };
}

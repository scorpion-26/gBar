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
                DrmAmdCard = mkOption {
                    type = types.nullOr types.str;
                    default = "card0";
                    description = "AMD card to be queried for various system usage and temperature metrics. This can be found in /sys/class/drm";
                };
                AmdGPUThermalZone = mkOption {
                    type = types.nullOr types.str;
                    default = "/device/hwmon/hwmon1/temp1_input";
                    description = "Relative path to AMD gpu thermal sensor, appended after /sys/class/drm/<DrmAmdCard>";
                };
                SuspendCommand = mkOption {
                    type = types.str;
                    default = "systemctl suspend";
                    description = "The command to execute on suspend";
                };
                LockCommand = mkOption {
                    type = types.nullOr types.str;
                    default = "~/.config/scripts/sys.sh lock";
                    description = "The command to execute on lock";
                };
                ExitCommand = mkOption {
                    type = types.str;
                    default = "killall Hyprland";
                    description = "The command to execute on exit";
                };
                BatteryFolder = mkOption {
                    type = types.nullOr types.str;
                    default = "/sys/class/power_supply/BAT1";
                    description = "The folder, where the battery sensors reside";
                };
                DiskPartition = mkOption {
                    type = types.str;
                    default = "/";
                    description = "The partition to monitor with disk sensor";
                };
                # Note: If the type here is null everything blows up
                WorkspaceSymbols = mkOption {
                    type = types.listOf types.str;
                    default = [];
                    description = "A list of strings where the position in the list is the icon to change, overrides the default symbol";
                };
                DefaultWorkspaceSymbol = mkOption {
                    type = types.str;
                    default = "";
                    description = "The default symbol for the workspaces";
                };
                NumWorkspaces = mkOption {
                    type = types.int;
                    default = 9;
                    description = "Number of workspaces to display. Displayed workspace IDs are 1-n (Default: 1-9)";
                };
                WorkspaceScrollOnMonitor = mkOption {
                    type = types.bool;
                    default = true;
                    description = "Scroll through the workspaces of the current monitor instead of all workspaces";
                };
                WorkspaceScrollInvert = mkOption {
                    type = types.bool;
                    default = false;
                    description = "When true: Scroll up -> Next workspace instead of previous workspace. Analogous with scroll down";
                };
                UseHyprlandIPC = mkOption {
                    type = types.bool;
                    default = true;
                    description = ''
                        Use Hyprland IPC instead of the ext_workspace protocol for workspace polling.
                        Hyprland IPC is *slightly* less performant (+0.1% one core), but way less bug prone,
                        since the protocol is not as feature complete as Hyprland IPC.
                    '';
                };
                Location = mkOption {
                    type = types.enum ["T" "B" "L" "R"];
                    default = "T";
                    description = "The location of the bar, enumerates to one capitalised letter as above";
                };
                CenterTime = mkOption {
                    type = types.bool;
                    default = true;
                    description = ''
                        Forces the time to be centered.
                        This can cause the right widget to clip outside, if there is not enough space on screen (e.g. when opening the text)
                        Setting this to false will definitely fix this issue, but it won't look very good, since it will be off-center.
                        So try to decrease "TimeSpace" first, before setting this configuration to false.
                    '';
                };
                TimeSpace = mkOption {
                    type = types.nullOr types.int;
                    default = 300;
                    description = ''
                        How much space should be reserved for the time widget. Setting this too high can cause the right widget to clip outside.
                        Therefore try to set it as low as possible if you experience clipping.
                        Although keep in mind, that a value that is too low can cause the widget to be be off-center,
                        which can also cause clipping.
                        If you can't find an optimal value, consider setting 'CenterTime' to false
                    '';
                };
                DateTimeStyle = mkOption {
                    type = types.str;
                    default = "%a %D - %H:%M:%S %Z";
                    description = "Set datetime style";
                };
                DateTimeLocale = mkOption {
                    type = types.nullOr types.str;
                    default = "";
                    description = "Set datetime locale (defaults to system locale if not set or set to empty string). Example locale: \"de_DE.utf8\"";
                };
                AudioInput = mkOption {
                    type = types.bool;
                    default = false;
                    description = "Adds a microphone volume widget";
                };
                AudioRevealer = mkOption {
                    type = types.bool;
                    default = false;
                    description = "Sets the audio slider to be on reveal (Just like the sensors) when true. Only affects the bar.";
                };
                AudioScrollSpeed = mkOption {
                    type = types.int;
                    default = 5;
                    description = "Sets the rate of change of the slider on each scroll. In Percent";
                };
                AudioNumbers = mkOption {
                    type = types.bool;
                    default = false;
                    description = "Display numbers instead of a slider for the two audio widgets. Doesn't affect the audio flyin";
                };
                AudioMinVolume = mkOption {
                    type = types.int;
                    default = 0;
                    description = "Limits the range of the audio slider. Only works for audio output. Slider 'empty' is AudioMinVolume, Slider 'full' is AudioMaxVolume";
                };
                AudioMaxVolume = mkOption {
                    type = types.int;
                    default = 100;
                    description = "";
                };
                NetworkAdapter = mkOption {
                    type = types.str;
                    default = "eno1";
                    description = "The network adapter to use. You can query /sys/class/net for all possible values";
                };
                NetworkWidget = mkOption {
                    type = types.bool;
                    default = true;
                    description = "Disables the network widget when set to false";
                };
                SensorTooltips = mkOption {
                    type = types.bool;
                    default = true;
                    description = "Use tooltips instead of sliders for the sensors";
                };
                EnableSNI = mkOption {
                    type = types.bool;
                    default = true;
                    description = "Enable tray icons";
                };
                SNIIconSize = mkOption {
                    type = types.attrsOf types.int;
                    default = {};
                    description = "sets the icon size for a SNI icon, an attribute set where, for example you can put Discord = 23 as an attribute and thus make discord slightly smaller Set to * to apply to all";
                };
                SNIIconPaddingTop = mkOption {
                    type = types.attrsOf types.int;
                    default = {};
                    description = "Can be used to push the Icon down. Negative values are allowed same as IconSize with an attribute set";
                };
                # These set the range for the network widget. The widget changes colors at six intervals:
                #    - Below Min...Bytes ("under")
                #    - Between ]0%;25%]. 0% = Min...Bytes; 100% = Max...Bytes ("low")
                #    - Between ]25%;50%]. 0% = Min...Bytes; 100% = Max...Bytes ("mid-low")
                #    - Between ]50%;75%]. 0% = Min...Bytes; 100% = Max...Bytes ("mid-high")
                #    - Between ]75%;100%]. 0% = Min...Bytes; 100% = Max...Bytes ("high")
                #    - Above Max...Bytes ("over")
                MinDownloadBytes = mkOption {
                    type = types.int;
                    default = 0;
                    description = "";
                };
                MaxDownloadBytes = mkOption {
                    type = types.int;
                    default = 10485760;
                    description = "";
                };
                MinUploadBytes = mkOption {
                    type = types.int;
                    default = 0;
                    description = "";
                };
                MaxUploadBytes = mkOption {
                    type = types.int;
                    default = 5242880;
                    description = "";
                };
            };
        };
    };

    extraConfig = mkOption {
      type = types.nullOr types.lines;
      default = '' '';
      description = ''
        Configuration to write to ~/.config/gBar/config, if you want to use your own config then set this to null and home manager wont write anything, neither the config
      '';
    };

    extraCSS = mkOption {
      type = types.nullOr types.lines;
      default = null;
      description = ''
        Configuration to write to ~/.config/gBar/style.css, if none nothing happens
      '';
    };

    extraSCSS = mkOption {
      type = types.nullOr types.lines;
      default = null;
      description = ''
        Configuration to write to ~/.config/gBar/style.scss, if none nothing happens
      '';
    };
  };

  config = let
      # Takes attributeset with an arbitrary value and returns it as an attrset of valid gBar options
      applyVal = x:
          let anyToString = a: if isBool a then boolToString a else toString a;
          in attrsets.mapAttrs (name: value:
                      name + ": " + (anyToString value)) (filterAttrs (n2: v2: (isInt v2 || isString v2 || isBool v2))x);
      extractLists = l:
          (imap1 (i: v: "WorkspaceSymbol: ${toString i}," + v) l.WorkspaceSymbols) ++
          (mapAttrsToList (n: v: "SNIIconSize: ${n}, ${toString v}") l.SNIIconSize) ++
          (mapAttrsToList (n: v: "SNIIconPaddingTop: ${n}, ${toString v}") l.SNIIconPaddingTop);

      gBarConfig = (concatMapStrings (x: x + "\n") (attrValues (applyVal cfg.config)))+(concatMapStrings (x: x+"\n") (extractLists cfg.config));

  in mkIf cfg.enable {
    home.packages = optional (cfg.package != null) cfg.package;

    xdg.configFile."gBar/config" = mkIf (cfg.extraConfig != null) {
      text = "# Generated by Home Manager\n" + gBarConfig + cfg.extraConfig;
    };
    xdg.configFile."gBar/style.css" = mkIf (cfg.extraCSS != null) {
      text = cfg.extraCSS;
    };
    xdg.configFile."gBar/style.scss" = mkIf (cfg.extraSCSS != null) {
      text = cfg.extraSCSS;
    };
  };
}

#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <map>

class Config
{
public:
    std::vector<std::string> widgetsLeft = {"Workspaces"};
    std::vector<std::string> widgetsCenter = {"Time"};
    std::vector<std::string> widgetsRight = {"Tray", "Packages", "Audio", "Bluetooth", "Network", "Disk",
                                             "VRAM", "GPU",      "RAM",   "CPU",       "Battery", "Power"};

    std::string cpuThermalZone = ""; // idk, no standard way of doing this.
    std::string amdGpuThermalZone = "/device/hwmon/hwmon1/temp1_input";
    std::string networkAdapter = "eno1"; // Is this standard?
    std::string drmAmdCard = "card0";    // The card to poll in AMDGPU.
    std::string suspendCommand = "systemctl suspend";
    std::string lockCommand = "";   // idk, no standard way of doing this.
    std::string exitCommand = "";   // idk, no standard way of doing this.
    std::string batteryFolder = ""; // this can be BAT0, BAT1, etc. Usually in /sys/class/power_supply
    std::map<uint32_t, std::string> workspaceSymbols;
    std::string defaultWorkspaceSymbol = "";
    std::string dateTimeStyle = "%a %D - %H:%M:%S %Z"; // A sane default
    std::string dateTimeLocale = "";                   // use system locale
    std::string diskPartition = "/";                   // should be expectable on every linux system

    // Icons
    std::string shutdownIcon = " ";
    std::string rebootIcon = "󰑐";
    std::string sleepIcon = "󰏤";
    std::string lockIcon = "";
    std::string exitIcon = "󰗼";
    std::string btOffIcon = "󰂲";
    std::string btOnIcon = "󰂯";
    std::string btConnectedIcon = "󰂱";
    std::string devKeyboardIcon = "󰌌 ";
    std::string devMouseIcon = "󰍽 ";
    std::string devHeadsetIcon = "󰋋 ";
    std::string devControllerIcon = "󰖺 ";
    std::string devUnknownIcon = " ";
    std::string speakerMutedIcon = "󰝟";
    std::string speakerHighIcon = "󰕾";
    std::string micMutedIcon = "󰍭";
    std::string micHighIcon = "󰍬";
    std::string packageOutOfDateIcon = "󰏔 ";
    // WS is handled by workspaceSymbols
    // TODO: Deprecate workspaceSymbols in favor of e.g. workspaceIcon-x
    // TODO: BluetoothDevices Refresh and close icons

    // Script that returns how many packages are out-of-date. The script should only print a number!
    // See data/update.sh for a human-readable version
    std::string checkPackagesCommand =
        "p=\"$(checkupdates)\"; e=$?; if [ $e -eq 127 ] ; then exit 127; fi; if [ $e -eq 2 ] ; then echo \"0\" && exit 0; fi; echo \"$p\" | wc -l";

    bool forceCSS = false;     // Whether to disable loading SCSS directly.
    bool centerWidgets = true; // Force the center widgets to be in the center.
    bool audioRevealer = false;
    bool audioInput = false;
    bool audioNumbers = false;         // Affects both audio sliders
    bool manualFlyinAnimation = false; // Do the flyin animation via margin updating
    bool networkWidget = true;
    bool workspaceScrollOnMonitor = true; // Scroll through workspaces on monitor instead of all
    bool workspaceScrollInvert = false;   // Up = +1, instead of Up = -1
    bool workspaceHideUnused = false;     // Only display necessary workspaces (i.e. trailing empty workspaces)
    bool useHyprlandIPC = true;           // Use Hyprland IPC instead of ext_workspaces protocol (Less buggy, but also less performant)
    bool enableSNI = true;                // Enable tray icon
    bool sensorTooltips = false;          // Use tooltips instead of sliders for the sensors
    bool iconsAlwaysUp = false;           // Force icons to always point upwards in sidebar mode

    // Controls for color progression of the network widget
    uint32_t minUploadBytes = 0;                  // Bottom limit of the network widgets upload. Everything below it is considered "under"
    uint32_t maxUploadBytes = 4 * 1024 * 1024;    // 4 MiB Top limit of the network widgets upload. Everything above it is considered "over"
    uint32_t minDownloadBytes = 0;                // Bottom limit of the network widgets download. Everything above it is considered "under"
    uint32_t maxDownloadBytes = 10 * 1024 * 1024; // 10 MiB Top limit of the network widgets download. Everything above it is considered "over"

    uint32_t audioScrollSpeed = 5;         // 5% each scroll
    uint32_t checkUpdateInterval = 5 * 60; // Interval to run the "checkPackagesCommand". In seconds
    uint32_t centerSpace = 300;            // How much space should be reserved for the center widgets.
    uint32_t maxTitleLength = 30;          // Maximum chars of the title widget. Longer titles will be shortened
    uint32_t numWorkspaces = 9;            // How many workspaces to display
    uint32_t sensorSize = 24;              // The size of the circular sensors
    uint32_t networkIconSize = 24;         // The size of the two network arrows
    uint32_t batteryWarnThreshold = 20;    // Threshold for color change when on battery

    char location = 'T'; // The Location of the bar. Can be L,R,T,B

    // SNIIconSize: ["Title String"], ["Size"]
    std::unordered_map<std::string, uint32_t> sniIconSizes;
    std::unordered_map<std::string, int32_t> sniPaddingTop;
    std::unordered_map<std::string, std::string> sniIconNames;
    std::unordered_map<std::string, bool> sniDisabled;

    // Only affects outputs (i.e.: speakers, not microphones). This remaps the range of the volume; In percent
    double audioMinVolume = 0.f;   // Map the minimum volume to this value
    double audioMaxVolume = 100.f; // Map the maximum volume to this value

    static void Load(const std::string& overrideConfigLocation);
    static const Config& Get();
};

// Configs, that rely on specific files to be available(e.g. BlueZ running)
class RuntimeConfig
{
public:
#ifdef WITH_NVIDIA
    bool hasNvidia = true;
#else
    bool hasNvidia = false;
#endif
#ifdef WITH_AMD
    bool hasAMD = true;
#else
    bool hasAMD = false;
#endif

#ifdef WITH_WORKSPACES
    bool hasWorkspaces = true;
#else
    bool hasWorkspaces = false;
#endif

#ifdef WITH_BLUEZ
    bool hasBlueZ = true;
#else
    bool hasBlueZ = false;
#endif

#ifdef WITH_SNI
    bool hasSNI = true;
#else
    bool hasSNI = false;
#endif

    bool hasNet = true;

    bool hasPackagesScript = true;

    static RuntimeConfig& Get();
};

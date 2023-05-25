#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

class Config
{
public:
    std::string cpuThermalZone = "";     // idk, no standard way of doing this.
    std::string networkAdapter = "eno1"; // Is this standard?
    std::string suspendCommand = "systemctl suspend";
    std::string lockCommand = "";   // idk, no standard way of doing this.
    std::string exitCommand = "";   // idk, no standard way of doing this.
    std::string batteryFolder = ""; // this can be BAT0, BAT1, etc. Usually in /sys/class/power_supply
    std::vector<std::string> workspaceSymbols = std::vector<std::string>(9, "");
    std::string defaultWorkspaceSymbol = "";
    std::string DateTimeStyle = "%a %D - %H:%M:%S %Z"; // A sane default

    // Script that returns how many packages are out-of-date. The script should only print a number!
    // See data/update.sh for a human-readable version
    std::string checkPackagesCommand = "p=\"$(checkupdates)\"; e=$?; if [ $e -eq 127 ] ; then exit 127; fi; if [ $e -eq 2 ] ; then echo \"0\" && exit 0; fi; echo \"$p\" | wc -l";

    bool centerTime = true;
    bool audioRevealer = false;
    bool audioInput = false;
    bool networkWidget = true;
    bool workspaceScrollOnMonitor = true; // Scroll through workspaces on monitor instead of all
    bool workspaceScrollInvert = false;   // Up = +1, instead of Up = -1
    bool useHyprlandIPC = false;          // Use Hyprland IPC instead of ext_workspaces protocol (Less buggy, but also less performant)
    bool enableSNI = true;                // Enable tray icon

    // Controls for color progression of the network widget
    uint32_t minUploadBytes = 0;                  // Bottom limit of the network widgets upload. Everything below it is considered "under"
    uint32_t maxUploadBytes = 4 * 1024 * 1024;    // 4 MiB Top limit of the network widgets upload. Everything above it is considered "over"
    uint32_t minDownloadBytes = 0;                // Bottom limit of the network widgets download. Everything above it is considered "under"
    uint32_t maxDownloadBytes = 10 * 1024 * 1024; // 10 MiB Top limit of the network widgets download. Everything above it is considered "over"

    uint32_t audioScrollSpeed = 5; // 5% each scroll

    uint32_t checkUpdateInterval = 5 * 60; // Interval to run the "checkPackagesCommand". In seconds

    // SNIIconSize: ["Title String"], ["Size"]
    std::unordered_map<std::string, uint32_t> sniIconSizes;
    std::unordered_map<std::string, int32_t> sniPaddingTop;

    // Only affects outputs (i.e.: speakers, not microphones). This remaps the range of the volume; In percent
    double audioMinVolume = 0.f;   // Map the minimum volume to this value
    double audioMaxVolume = 100.f; // Map the maximum volume to this value

    static void Load();
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

#if defined WITH_SNI && defined HAS_STB
    bool hasSNI = true;
#else
    bool hasSNI = false;
#endif

    bool hasNet = true;

    bool hasPackagesScript = true;

    static RuntimeConfig& Get();
};

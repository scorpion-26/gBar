#pragma once
#include <string>
#include <vector>

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

    bool centerTime = true;
    bool audioRevealer = false;
    bool audioInput = false;
    bool networkWidget = true;
    bool workspaceScrollOnMonitor = true; // Scroll through workspaces on monitor instead of all
    bool workspaceScrollInvert = false;   // Up = +1, instead of Up = -1
    bool useHyprlandIPC = false; // Use Hyprland IPC instead of ext_workspaces protocol (Less buggy, but also less performant)

    // Controls for color progression of the network widget
    uint32_t minUploadBytes = 0;                  // Bottom limit of the network widgets upload. Everything below it is considered "under"
    uint32_t maxUploadBytes = 4 * 1024 * 1024;    // 4 MiB Top limit of the network widgets upload. Everything above it is considered "over"
    uint32_t minDownloadBytes = 0;                // Bottom limit of the network widgets download. Everything above it is considered "under"
    uint32_t maxDownloadBytes = 10 * 1024 * 1024; // 10 MiB Top limit of the network widgets download. Everything above it is considered "over"

    uint32_t audioScrollSpeed = 5; // 5% each scroll

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

    bool hasNet = true;

    static RuntimeConfig& Get();
};

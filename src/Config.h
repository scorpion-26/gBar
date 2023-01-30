#pragma once
#include <string>
#include <vector>

class Config 
{
public:
    std::string cpuThermalZone = ""; // idk, no standard way of doing this.
    std::string suspendCommand = "systemctl suspend";
    std::string lockCommand = ""; // idk, no standard way of doing this.
    std::string exitCommand = ""; // idk, no standard way of doing this.
    std::string batteryFolder = ""; // this can be BAT0, BAT1, etc. Usually in /sys/class/power_supply
    std::vector<std::string> workspaceSymbols = std::vector<std::string>(9, "");
    std::string defaultWorkspaceSymbol = "";

    bool centerTime = true;

    static void Load();
    static const Config& Get();
};

// Configs, that rely on specific files to be available(e.g. Hyprland running)
class RuntimeConfig
{
public:
#ifdef WITH_NVIDIA
    bool hasNvidia = true;
#else
    bool hasNvidia = false;
#endif

#ifdef WITH_HYPRLAND
    bool hasHyprland = true;
#else
    bool hasHyprland = false;
#endif

#ifdef WITH_BLUEZ
    bool hasBlueZ = true;
#else
    bool hasBlueZ = false;
#endif

    static RuntimeConfig& Get();
};

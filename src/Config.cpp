#include "Config.h"
#include "Common.h"

#include <fstream>

static Config config;

const Config& Config::Get()
{
    return config;
}

void Config::Load()
{
    const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
    std::ifstream file;
    if (xdgConfigHome)
    {
        file = std::ifstream(std::string(xdgConfigHome) + "/gBar/config");
    }
    else
    {
        std::string home = getenv("HOME");
        file = std::ifstream(home + "/.config/gBar/config");
    }
    if (!file.is_open())
    {
        LOG("Failed opening config!");
        return;
    }
    std::string line;
    while (std::getline(file, line))
    {
        std::string* prop = nullptr;
        if (line.find("CPUThermalZone: ") != std::string::npos)
        {
            prop = &config.cpuThermalZone;
        }
        else if (line.find("SuspendCommand: ") != std::string::npos)
        {
            prop = &config.suspendCommand;
        }
        else if (line.find("LockCommand: ") != std::string::npos)
        {
            prop = &config.lockCommand;
        }
        else if (line.find("ExitCommand: ") != std::string::npos)
        {
            prop = &config.exitCommand;
        }
        else if (line.find("BatteryFolder: ") != std::string::npos)
        {
            prop = &config.batteryFolder;
        }
        else if (line.find("DefaultWorkspaceSymbol") != std::string::npos)
        {
            prop = &config.defaultWorkspaceSymbol;
        }
        else if (line.find("WorkspaceSymbol") != std::string::npos)
        {
            for (int i = 1; i < 10; i++)
            {
                if (line.find("WorkspaceSymbol-" + std::to_string(i)) != std::string::npos)
                {
                    // Subtract 1 to index from 1 to 9 rather than 0 to 8
                    prop = &(config.workspaceSymbols[i - 1]);
                }
            }
        }
        if (prop == nullptr)
        {
            LOG("Warning: unknown config var: " << line);
            continue;
        }
        *prop = line.substr(line.find(' ') + 1); // Everything after space is data
    }
}

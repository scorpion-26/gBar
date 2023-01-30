#include "Common.h"
#include "Config.h"

#include <fstream>

#ifdef WITH_AMD
namespace AMDGPU
{
    static const char* utilizationFile = "/sys/class/drm/card0/device/gpu_busy_percent";
    static const char* vramTotalFile = "/sys/class/drm/card0/device/mem_info_vram_total";
    static const char* vramUsedFile = "/sys/class/drm/card0/device/mem_info_vram_used";
    // TODO: Make this configurable
    static const char* tempFile = "/sys/class/drm/card0/device/hwmon/hwmon1/temp1_input";

    inline void Init()
    {
        // Test for drm device files
        std::ifstream test(utilizationFile);
        if (!test.is_open())
        {
            LOG("AMD GPU not found, disabling AMD GPU");
            RuntimeConfig::Get().hasAMD = false;
        }
    }

    inline uint32_t GetUtilization()
    {
        if (!RuntimeConfig::Get().hasAMD)
        {
            LOG("Error: Called AMD GetUtilization, but AMD GPU wasn't found!");
            return {};
        }

        std::ifstream file(utilizationFile);
        std::string line;
        std::getline(file, line);
        return atoi(line.c_str());
    }

    inline uint32_t GetTemperature()
    {
        if (!RuntimeConfig::Get().hasAMD)
        {
            LOG("Error: Called AMD GetTemperature, but AMD GPU wasn't found!");
            return {};
        }

        std::ifstream file(tempFile);
        std::string line;
        std::getline(file, line);
        return atoi(line.c_str()) / 1000;
    }

    struct VRAM 
    {
        uint32_t totalB;
        uint32_t usedB;
    };

    inline VRAM GetVRAM()
    {
        if (!RuntimeConfig::Get().hasAMD)
        {
            LOG("Error: Called AMD GetVRAM, but AMD GPU wasn't found!");
            return {};
        }
        VRAM mem{};

        std::ifstream file(vramTotalFile);
        std::string line;
        std::getline(file, line);
        mem.totalB = atoi(line.c_str());
    
        file = std::ifstream(vramUsedFile);
        std::getline(file, line);
        mem.usedB = atoi(line.c_str());

        return mem;
    }
}
#endif

#include "Common.h"
#include "Config.h"

#include <fstream>

#ifdef WITH_AMD
namespace AMDGPU
{
    static const char* drmCardPrefix = "/sys/class/drm/";
    static const char* utilizationFile = "/device/gpu_busy_percent";
    static const char* vramTotalFile = "/device/mem_info_vram_total";
    static const char* vramUsedFile = "/device/mem_info_vram_used";

    inline void Init()
    {
        // Test for drm device files
        std::ifstream test(drmCardPrefix + Config::Get().drmAmdCard + utilizationFile);
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

        std::ifstream file(drmCardPrefix + Config::Get().drmAmdCard + utilizationFile);
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

        std::ifstream file(drmCardPrefix + Config::Get().drmAmdCard + Config::Get().amdGpuThermalZone);

        std::string line;
        std::getline(file, line);
        return atoi(line.c_str()) / 1000;
    }

    struct VRAM
    {
        uint64_t totalB;
        uint64_t usedB;
    };

    inline VRAM GetVRAM()
    {
        if (!RuntimeConfig::Get().hasAMD)
        {
            LOG("Error: Called AMD GetVRAM, but AMD GPU wasn't found!");
            return {};
        }
        VRAM mem{};

        std::ifstream file(drmCardPrefix + Config::Get().drmAmdCard + vramTotalFile);
        std::string line;
        std::getline(file, line);
        mem.totalB = strtoul(line.c_str(), nullptr, 10);

        file = std::ifstream(drmCardPrefix + Config::Get().drmAmdCard + vramUsedFile);
        std::getline(file, line);
        mem.usedB = strtoul(line.c_str(), nullptr, 10);

        return mem;
    }
}
#endif

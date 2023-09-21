#pragma once
#include "Common.h"
#include "Config.h"

#include <dlfcn.h>

#ifdef WITH_NVIDIA
namespace NvidiaGPU
{
    struct GPUUtilization
    {
        uint32_t gpu;
        uint32_t vram;
    };
    struct VRAM
    {
        uint64_t totalB;
        uint64_t freeB;
        uint64_t usedB;
    };

    static void* nvmldl;

    static void* nvmlGPUHandle;

    // Preloaded Query functions 
    typedef int (*PFN_nvmlDeviceGetUtilizationRates)(void*, struct GPUUtilization*);
    static PFN_nvmlDeviceGetUtilizationRates nvmlDeviceGetUtilizationRates;
    typedef int (*PFN_nvmlDeviceGetTemperature)(void*, uint32_t, uint32_t*);
    static PFN_nvmlDeviceGetTemperature nvmlDeviceGetTemperature;
    typedef int (*PFN_nvmlDeviceGetMemoryInfo)(void*, struct VRAM*);
    static PFN_nvmlDeviceGetMemoryInfo nvmlDeviceGetMemoryInfo;

    inline void Init()
    {
        if (nvmldl || !RuntimeConfig::Get().hasNvidia)
            return;

        nvmldl = dlopen("libnvidia-ml.so", RTLD_NOW);
        // nvmldl not found. Nvidia probably not installed
        if (!nvmldl)
        {
            LOG("NVML not found, disabling Nvidia GPU");
            RuntimeConfig::Get().hasNvidia = false;
            return;
        }

        typedef int (*PFN_nvmlInit)();
        auto nvmlInit = (PFN_nvmlInit)dlsym(nvmldl, "nvmlInit");
        int res = nvmlInit();
        if (res != 0)
        {
            LOG("Failed initializing nvml (Error: " << res << "), disabling Nvidia GPU");
            RuntimeConfig::Get().hasNvidia = false;
            return;
        }

        // Get GPU handle
        typedef int (*PFN_nvmlDeviceGetHandle)(uint32_t, void**);
        auto nvmlDeviceGetHandle = (PFN_nvmlDeviceGetHandle)dlsym(nvmldl, "nvmlDeviceGetHandleByIndex");
        res = nvmlDeviceGetHandle(0, &nvmlGPUHandle);
        ASSERT(res == 0, "Failed getting device (Error: " << res << ")!");

        // Dynamically load functions
        nvmlDeviceGetUtilizationRates = (PFN_nvmlDeviceGetUtilizationRates)dlsym(nvmldl, "nvmlDeviceGetUtilizationRates");
        nvmlDeviceGetTemperature = (PFN_nvmlDeviceGetTemperature)dlsym(nvmldl, "nvmlDeviceGetTemperature");
        nvmlDeviceGetMemoryInfo = (PFN_nvmlDeviceGetMemoryInfo)dlsym(nvmldl, "nvmlDeviceGetMemoryInfo");
        
        // Check if all information is available
        GPUUtilization util;
        res = nvmlDeviceGetUtilizationRates(nvmlGPUHandle, &util);
        if (res != 0)
        {
            LOG("Failed querying utilization rates (Error: " << res << "), disabling Nvidia GPU!");
            RuntimeConfig::Get().hasNvidia = false;
            return;
        }
        uint32_t temp;
        res = nvmlDeviceGetTemperature(nvmlGPUHandle, 0, &temp);
        if (res != 0)
        {
            LOG("Failed querying temperature (Error: " << res << "), disabling Nvidia GPU!");
            RuntimeConfig::Get().hasNvidia = false;
            return;
        }
        VRAM mem;
        res = nvmlDeviceGetMemoryInfo(nvmlGPUHandle, &mem);
        if (res != 0)
        {
            LOG("Failed querying VRAM (Error: " << res << "), disabling Nvidia GPU!");
            RuntimeConfig::Get().hasNvidia = false;
            return;
        }
    }

    inline void Shutdown()
    {
        if (nvmldl)
            dlclose(nvmldl);
    }

    inline GPUUtilization GetUtilization()
    {
        if (!RuntimeConfig::Get().hasNvidia)
        {
            LOG("Error: Called Nvidia GetUtilization, but nvml wasn't found!");
            return {};
        }

        GPUUtilization util;
        typedef int (*PFN_nvmlDeviceGetUtilizationRates)(void*, GPUUtilization*);
        auto nvmlDeviceGetUtilizationRates = (PFN_nvmlDeviceGetUtilizationRates)dlsym(nvmldl, "nvmlDeviceGetUtilizationRates");

        int res = nvmlDeviceGetUtilizationRates(nvmlGPUHandle, &util);
        ASSERT(res == 0, "Failed getting utilization (Error: " << res << ")!");
        return util;
    }

    inline uint32_t GetTemperature()
    {
        if (!RuntimeConfig::Get().hasNvidia)
        {
            LOG("Error: Called Nvidia GetTemperature, but nvml wasn't found!");
            return {};
        }

        typedef int (*PFN_nvmlDeviceGetTemperature)(void*, uint32_t, uint32_t*);
        auto nvmlDeviceGetTemperature = (PFN_nvmlDeviceGetTemperature)dlsym(nvmldl, "nvmlDeviceGetTemperature");
        uint32_t temp;
        int res = nvmlDeviceGetTemperature(nvmlGPUHandle, 0, &temp);
        ASSERT(res == 0, "Failed getting temperature (Error: " << res << ")!");
        return temp;
    }

    inline VRAM GetVRAM()
    {
        if (!RuntimeConfig::Get().hasNvidia)
        {
            LOG("Error: Called Nvidia GetVRAM, but nvml wasn't found!");
            return {};
        }

        typedef int (*PFN_nvmlDeviceGetMemoryInfo)(void*, VRAM*);
        auto nvmlDeviceGetMemoryInfo = (PFN_nvmlDeviceGetMemoryInfo)dlsym(nvmldl, "nvmlDeviceGetMemoryInfo");
        VRAM mem;
        int res = nvmlDeviceGetMemoryInfo(nvmlGPUHandle, &mem);
        ASSERT(res == 0, "Failed getting memory (Error: " << res << ")!");
        return mem;
    }
}
#endif

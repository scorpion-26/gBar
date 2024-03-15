#pragma once
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

namespace System
{
    // From 0-1, all cores
    double GetCPUUsage();
    // Tctl
    double GetCPUTemp();

    bool IsBatteryCharging();
    double GetBatteryPercentage();

    struct RAMInfo
    {
        double totalGiB;
        double freeGiB;
    };
    RAMInfo GetRAMInfo();

#if defined WITH_NVIDIA || defined WITH_AMD
    struct GPUInfo
    {
        double utilisation;
        double coreTemp;
    };
    GPUInfo GetGPUInfo();

    struct VRAMInfo
    {
        double totalGiB;
        double usedGiB;
    };
    VRAMInfo GetVRAMInfo();
#endif

    struct DiskInfo
    {
        std::string partition;
        double totalGiB;
        double usedGiB;
    };
    DiskInfo GetDiskInfo();

#ifdef WITH_BLUEZ
    struct BluetoothDevice
    {
        bool connected;
        bool paired;
        std::string mac;
        std::string name;
        // Known types: input-[keyboard,mouse]; audio-headset
        std::string type;
    };

    struct BluetoothInfo
    {
        std::string defaultController;
        std::vector<BluetoothDevice> devices;
    };
    BluetoothInfo GetBluetoothInfo();
    void StartBTScan();
    void StopBTScan();

    // MT functions, callback, is from different thread
    void ConnectBTDevice(BluetoothDevice& device, std::function<void(bool, BluetoothDevice&)> onFinish);
    void DisconnectBTDevice(BluetoothDevice& device, std::function<void(bool, BluetoothDevice&)> onFinish);

    void OpenBTWidget();

    std::string BTTypeToIcon(const BluetoothDevice& dev);
#endif

    struct AudioInfo
    {
        double sinkVolume;
        bool sinkMuted;

        double sourceVolume;
        bool sourceMuted;
    };
    AudioInfo GetAudioInfo();
    void SetVolumeSink(double volume);
    void SetVolumeSource(double volume);

#ifdef WITH_WORKSPACES
    enum class WorkspaceStatus
    {
        Dead,
        Inactive,
        Visible,
        Current,
        Active
    };
    void PollWorkspaces(const std::string& monitor, uint32_t numWorkspaces);
    WorkspaceStatus GetWorkspaceStatus(uint32_t workspace);
    void GotoWorkspace(uint32_t workspace);
    // direction: + or -
    void GotoNextWorkspace(char direction);
    std::string GetWorkspaceSymbol(int index);
#endif

    // Bytes per second upload. dx is time since last call. Will always return 0 on first run
    double GetNetworkBpsUpload(double dt);
    // Bytes per second download. dx is time since last call. Will always return 0 on first run
    double GetNetworkBpsDownload(double dt);

    void GetOutdatedPackagesAsync(std::function<void(uint32_t)>&& returnVal);

    std::string GetTime();

    void Shutdown();
    void Reboot();
    void ExitWM();
    void Lock();
    void Suspend();

    void Init(const std::string& overrideConfigLocation);
    void FreeResources();
}

#pragma once
#include <functional>
#include <string>
#include <vector>

namespace System
{
    // From 0-1, all cores
    double GetCPUUsage();
    // Tctl
    double GetCPUTemp();

    struct RAMInfo
    {
        double totalGiB;
        double freeGiB;
    };
    RAMInfo GetRAMInfo();

#ifdef HAS_NVIDIA
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
        double totalGiB;
        double usedGiB;
    };
    DiskInfo GetDiskInfo();

#ifdef HAS_BLUEZ
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
        double volume;
        bool muted;
    };
    AudioInfo GetAudioInfo();
    void SetVolume(double volume);

#ifdef HAS_HYPRLAND
    enum class WorkspaceStatus
    {
        Dead,
        Inactive,
        Visible,
        Current,
        Active
    };
    WorkspaceStatus GetWorkspaceStatus(uint32_t monitor, uint32_t workspace);
    void GotoWorkspace(uint32_t workspace);
#endif

    std::string GetTime();

    void Shutdown();
    void Reboot();
    void ExitWM();
    void Lock();
    void Suspend();

    void Init();
    void FreeResources();
}

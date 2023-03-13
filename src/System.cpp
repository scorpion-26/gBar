#include "System.h"
#include "Common.h"
#include "NvidiaGPU.h"
#include "AMDGPU.h"
#include "PulseAudio.h"
#include "Workspaces.h"
#include "Config.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>

#include <gio/gio.h>

#include <pulse/pulseaudio.h>

#include <dlfcn.h>
#include <sys/statvfs.h>
#include <unistd.h>

namespace System
{
    struct CPUTimestamp
    {
        size_t total = 0;
        size_t idle = 0;
    };

    static CPUTimestamp curCPUTime;
    static CPUTimestamp prevCPUTime;

    double GetCPUUsage()
    {
        // Gather curCPUTime
        std::ifstream procstat("/proc/stat");
        ASSERT(procstat.is_open(), "Cannot open /proc/stat");

        std::string curLine;
        while (std::getline(procstat, curLine))
        {
            if (curLine.find("cpu ") != std::string::npos)
            {
                // Found it
                std::stringstream lineStr(curLine.substr(5));
                std::string curLine;
                uint32_t idx = 1;
                size_t total = 0;
                size_t idle = 0;
                while (std::getline(lineStr, curLine, ' '))
                {
                    if (idx == 4)
                    {
                        // Fourth col is idle
                        idle = atoi(curLine.c_str());
                    }
                    total += atoi(curLine.c_str());
                    idx++;
                }
                prevCPUTime = curCPUTime;
                curCPUTime.total = total;
                curCPUTime.idle = idle;
                break;
            }
        }

        // Get diffs and percentage of idle time
        size_t diffTotal = curCPUTime.total - prevCPUTime.total;
        size_t diffIdle = curCPUTime.idle - prevCPUTime.idle;
        return 1 - ((double)diffIdle / (double)diffTotal);
    }

    double GetCPUTemp()
    {
        std::ifstream tempFile(Config::Get().cpuThermalZone);
        if (!tempFile.is_open())
        {
            return 0.f;
        }
        std::string tempStr;
        std::getline(tempFile, tempStr);
        uint32_t intTemp = atoi(tempStr.c_str());
        double temp = (double)intTemp / 1000;
        return temp;
    }

    double GetBatteryPercentage()
    {
        std::ifstream fullChargeFile(Config::Get().batteryFolder + "/charge_full");
        std::ifstream currentChargeFile(Config::Get().batteryFolder + "/charge_now");
        if (!fullChargeFile.is_open() || !currentChargeFile.is_open())
        {
            return -1.f;
        }
        std::string fullChargeStr;
        std::string currentChargeStr;
        std::getline(fullChargeFile, fullChargeStr);
        std::getline(currentChargeFile, currentChargeStr);
        uint32_t intFullCharge = atoi(fullChargeStr.c_str());
        uint32_t intCurrentCharge = atoi(currentChargeStr.c_str());
        return ((double)intCurrentCharge / (double)intFullCharge);
    }

    RAMInfo GetRAMInfo()
    {
        RAMInfo out{};
        std::ifstream procstat("/proc/meminfo");
        ASSERT(procstat.is_open(), "Cannot open /proc/meminfo");

        std::string curLine;
        while (std::getline(procstat, curLine))
        {
            if (curLine.find("MemTotal: ") != std::string::npos)
            {
                // Found total
                std::string_view withoutMemTotal = std::string_view(curLine).substr(10);
                size_t begNum = withoutMemTotal.find_first_not_of(' ');
                std::string_view totalKiBStr = withoutMemTotal.substr(begNum, withoutMemTotal.find_last_of(' ') - begNum);
                uint32_t totalKiB = std::stoi(std::string(totalKiBStr));
                out.totalGiB = (double)totalKiB / (1024 * 1024);
            }
            else if (curLine.find("MemAvailable: ") != std::string::npos)
            {
                // Found available
                std::string_view withoutMemAvail = std::string_view(curLine).substr(14);
                size_t begNum = withoutMemAvail.find_first_not_of(' ');
                std::string_view availKiBStr = withoutMemAvail.substr(begNum, withoutMemAvail.find_last_of(' ') - begNum);
                uint32_t availKiB = std::stoi(std::string(availKiBStr));
                out.freeGiB = (double)availKiB / (1024 * 1024);
            }
        }
        return out;
    }

#if defined WITH_NVIDIA || defined WITH_AMD
    GPUInfo GetGPUInfo()
    {
#ifdef WITH_NVIDIA
        if (RuntimeConfig::Get().hasNvidia)
        {
            NvidiaGPU::GPUUtilization util = NvidiaGPU::GetUtilization();
            GPUInfo out;
            out.utilisation = util.gpu;
            out.coreTemp = NvidiaGPU::GetTemperature();
            return out;
        }
#endif
#ifdef WITH_AMD
        if (RuntimeConfig::Get().hasAMD)
        {
            uint32_t util = AMDGPU::GetUtilization();
            GPUInfo out;
            out.utilisation = util;
            out.coreTemp = AMDGPU::GetTemperature();
            return out;
        }
#endif
        return {};
    }

    VRAMInfo GetVRAMInfo()
    {
#ifdef WITH_NVIDIA
        if (RuntimeConfig::Get().hasNvidia)
        {
            NvidiaGPU::VRAM vram = NvidiaGPU::GetVRAM();
            VRAMInfo out;
            out.totalGiB = (double)vram.totalB / (1024 * 1024 * 1024);
            out.usedGiB = out.totalGiB - ((double)vram.freeB / (1024 * 1024 * 1024));
            return out;
        }
#endif
#ifdef WITH_AMD
        if (RuntimeConfig::Get().hasAMD)
        {
            AMDGPU::VRAM vram = AMDGPU::GetVRAM();
            VRAMInfo out;
            out.totalGiB = (double)vram.totalB / (1024 * 1024 * 1024);
            out.usedGiB = (double)vram.usedB / (1024 * 1024 * 1024);
            return out;
        }
#endif
        return {};
    }
#endif

    DiskInfo GetDiskInfo()
    {
        struct statvfs stat;
        int err = statvfs("/", &stat);
        ASSERT(err == 0, "Cannot stat root!");

        DiskInfo out{};
        out.totalGiB = (double)(stat.f_blocks * stat.f_frsize) / (1024 * 1024 * 1024);
        out.usedGiB = (double)((stat.f_blocks - stat.f_bfree) * stat.f_frsize) / (1024 * 1024 * 1024);
        return out;
    }

#ifdef WITH_BLUEZ
    void InitBluetooth()
    {
        // Try connecting to d-bus and org.bluez
        GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
        if (!connection)
        {
            LOG("Can't connect to d-bus! Disabling Bluetooth!");
            // dbus not found, disable bluetooth
            RuntimeConfig::Get().hasBlueZ = false;
        }

        GError* err = nullptr;
        GVariant* objects = g_dbus_connection_call_sync(connection, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects",
                                                        nullptr, G_VARIANT_TYPE("(a{oa{sa{sv}}})"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &err);
        if (!objects)
        {
            LOG("Can't connect to BlueZ d-bus! Disabling Bluetooth!");
            LOG(err->message);
            g_error_free(err);
            // Not found, disable bluetooth
            RuntimeConfig::Get().hasBlueZ = false;
        }
    }
    BluetoothInfo GetBluetoothInfo()
    {
        BluetoothInfo out{};
        if (!RuntimeConfig::Get().hasBlueZ)
        {
            LOG("Error: GetBluetoothInfo called, but bluetooth isn't available");
            return out;
        }
        // Init D-Bus
        GDBusConnection* connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, nullptr, nullptr);
        ASSERT(connection, "Failed to connect to d-bus!");

        GError* err = nullptr;
        GVariant* objects = g_dbus_connection_call_sync(connection, "org.bluez", "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects",
                                                        nullptr, G_VARIANT_TYPE("(a{oa{sa{sv}}})"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &err);
        if (!objects)
        {
            LOG(err->message);
            g_error_free(err);
            exit(-1);
        }

        // First array
        GVariantIter* topArray;
        g_variant_get(objects, "(a{oa{sa{sv}}})", &topArray);

        // Iterate the objects
        GVariantIter* objectDescs;
        while (g_variant_iter_next(topArray, "{oa{sa{sv}}}", NULL, &objectDescs))
        {
            // Iterate the descs
            char* type = nullptr;
            GVariantIter* propIter;
            while (g_variant_iter_next(objectDescs, "{sa{sv}}", &type, &propIter))
            {
                if (strstr(type, "org.bluez.Adapter1"))
                {
                    std::string adapterName;
                    bool powered = false;

                    // This is a controller/adapter -> The "host"
                    char* str = nullptr;
                    GVariant* var = nullptr;
                    while (g_variant_iter_next(propIter, "{sv}", &str, &var))
                    {
                        if (strstr(str, "Name"))
                        {
                            const char* name = g_variant_get_string(var, nullptr);
                            // Copy it for us
                            adapterName = name;
                        }
                        else if (strstr(str, "Powered"))
                        {
                            powered = g_variant_get_boolean(var);
                        }
                        g_free(str);
                        g_variant_unref(var);
                    }
                    if (powered)
                    {
                        out.defaultController = std::move(adapterName);
                    }
                }
                else if (strstr(type, "org.bluez.Device1"))
                {
                    std::string deviceMac;
                    std::string deviceName;
                    std::string deviceType;
                    bool connected = false;
                    bool paired = false;

                    // This is a device -> One "client"
                    char* str = nullptr;
                    GVariant* var = nullptr;
                    while (g_variant_iter_next(propIter, "{sv}", &str, &var))
                    {
                        if (strcmp(str, "Address") == 0)
                        {
                            const char* mac = g_variant_get_string(var, nullptr);
                            // Copy it for us
                            deviceMac = mac;
                        }
                        else if (strstr(str, "Name"))
                        {
                            const char* name = g_variant_get_string(var, nullptr);
                            // Copy it for us
                            deviceName = name;
                        }
                        else if (strstr(str, "Icon"))
                        {
                            const char* icon = g_variant_get_string(var, nullptr);
                            // Copy it for us
                            deviceType = icon;
                        }
                        else if (strstr(str, "Connected"))
                        {
                            connected = g_variant_get_boolean(var);
                        }
                        else if (strstr(str, "Paired"))
                        {
                            paired = g_variant_get_boolean(var);
                        }
                        g_free(str);
                        g_variant_unref(var);
                    }
                    out.devices.push_back(BluetoothDevice{connected, paired, std::move(deviceMac), std::move(deviceName), std::move(deviceType)});
                }
                g_variant_iter_free(propIter);
                g_free(type);
            }
            g_variant_iter_free(objectDescs);
        }

        g_variant_iter_free(topArray);
        g_variant_unref(objects);

        return out;
    }

    static Process btctlProcess{-1};
    void StartBTScan()
    {
        StopBTScan();
        btctlProcess = OpenProcess("/bin/sh -c \"bluetoothctl scan on\"");
    }
    void StopBTScan()
    {
        if (btctlProcess.pid != -1)
        {
            // Ctrl-C stops bluetoothctl
            kill(btctlProcess.pid, SIGINT);
            btctlProcess = {-1};
        }
    }

    void ConnectBTDevice(BluetoothDevice& device, std::function<void(bool, BluetoothDevice&)> onFinish)
    {
        auto thread = [&, mac = device.mac, onFinish]()
        {
            // 1. Pair
            if (!device.paired)
            {
                int success = system(("bluetoothctl pair " + mac).c_str());
                if (success != 0)
                {
                    onFinish(false, device);
                    return;
                }
            }
            // 2. Connect
            if (!device.connected)
            {
                int success = system(("bluetoothctl connect " + mac).c_str());
                if (success != 0)
                {
                    onFinish(false, device);
                    return;
                }
            }
            onFinish(true, device);
        };
        std::thread worker(thread);
        worker.detach();
    }
    void DisconnectBTDevice(BluetoothDevice& device, std::function<void(bool, BluetoothDevice&)> onFinish)
    {
        auto thread = [&, mac = device.mac, onFinish]()
        {
            // 1. Disconnect
            if (device.connected)
            {
                int success = system(("bluetoothctl disconnect " + mac).c_str());
                if (success != 0)
                {
                    onFinish(false, device);
                    return;
                }
            }
            onFinish(true, device);
        };
        std::thread worker(thread);
        worker.detach();
    }

    void OpenBTWidget()
    {
        OpenProcess("/bin/sh -c \"gBar bluetooth\"");
    }

    std::string BTTypeToIcon(const BluetoothDevice& dev)
    {
        if (dev.type == "input-keyboard")
        {
            return " ";
        }
        else if (dev.type == "input-mouse")
        {
            return " ";
        }
        else if (dev.type == "audio-headset")
        {
            return " ";
        }
        else if (dev.type == "input-gaming")
        {
            return "調 ";
        }
        return " ";
    }
#endif

    AudioInfo GetAudioInfo()
    {
        return PulseAudio::GetInfo();
    }
    void SetVolumeSink(double volume)
    {
        PulseAudio::SetVolumeSink(volume);
    }
    void SetVolumeSource(double volume)
    {
        PulseAudio::SetVolumeSource(volume);
    }

#ifdef WITH_WORKSPACES
    void PollWorkspaces(uint32_t monitor, uint32_t numWorkspaces)
    {
        Workspaces::PollStatus(monitor, numWorkspaces);
    }
    WorkspaceStatus GetWorkspaceStatus(uint32_t workspace)
    {
        return Workspaces::GetStatus(workspace);
    }
    void GotoWorkspace(uint32_t workspace)
    {
        return Workspaces::Goto(workspace);
    }
    void GotoNextWorkspace(char direction)
    {
        return Workspaces::GotoNext(direction);
    }
    std::string GetWorkspaceSymbol(int index)
    {
        if (index < 0 || index > 9)
        {
            LOG("Workspace Symbol Index Out Of Bounds: " + std::to_string(index));
            return "";
        }

        if (Config::Get().workspaceSymbols[index].empty())
        {
            return Config::Get().defaultWorkspaceSymbol + " ";
        }

        return Config::Get().workspaceSymbols[index] + " ";
    }
#endif

    void CheckNetwork()
    {
        std::ifstream bytes("/sys/class/net/" + Config::Get().networkAdapter + "/statistics/tx_bytes");
        if (!bytes.is_open())
        {
            LOG("Cannot open network device! Disabling Network widget.");
            RuntimeConfig::Get().hasNet = false;
        }
    }

    double GetNetworkBpsCommon(double dt, uint64_t& prevBytes, const std::string& deviceFile)
    {
        if (!RuntimeConfig::Get().hasNet)
        {
            return 0.f;
        }
        std::ifstream bytes(deviceFile);
        std::string bytesStr;
        std::getline(bytes, bytesStr);

        uint64_t curBytes = std::stoull(bytesStr);

        if (prevBytes == UINT64_MAX)
        {
            prevBytes = curBytes;
            return 0;
        }
        else
        {
            uint64_t diffBytes = curBytes - prevBytes;
            prevBytes = curBytes;
            // Is double precision a problem here?
            return diffBytes / dt;
        }
    }

    double GetNetworkBpsUpload(double dt)
    {
        // Better safe than sorry. Isn't 32bit max only a few GB?
        static uint64_t prevUploadBytes = UINT64_MAX;
        // Apparently /sys/class/net/.../statistics/[t/r]x_bytes is valid for all net devices under Linux
        // https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-class-net-statistics
        return GetNetworkBpsCommon(dt, prevUploadBytes, "/sys/class/net/" + Config::Get().networkAdapter + "/statistics/tx_bytes");
    }

    double GetNetworkBpsDownload(double dt)
    {
        // Better safe than sorry. Isn't 32bit max only a few GB?
        static uint64_t prevDownloadBytes = UINT64_MAX;
        // Apparently /sys/class/net/.../statistics/[t/r]x_bytes is valid for all net devices under Linux
        // https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-class-net-statistics
        return GetNetworkBpsCommon(dt, prevDownloadBytes, "/sys/class/net/" + Config::Get().networkAdapter + "/statistics/rx_bytes");
    }

    std::string GetTime()
    {
        time_t stdTime = time(NULL);
        tm* localTime = localtime(&stdTime);
        std::stringstream str;
        str << std::put_time(localTime, "%a %D - %H:%M:%S %Z");
        return str.str();
    }

    void Shutdown()
    {
        system("shutdown 0");
    }

    void Reboot()
    {
        system("reboot");
    }

    void ExitWM()
    {
        system(Config::Get().exitCommand.c_str());
    }

    void Lock()
    {
        system(Config::Get().lockCommand.c_str());
    }

    void Suspend()
    {
        system(Config::Get().suspendCommand.c_str());
    }

    void Init()
    {
        Logging::Init();

        Config::Load();

#ifdef WITH_NVIDIA
        NvidiaGPU::Init();
#endif

#ifdef WITH_AMD
        AMDGPU::Init();
#endif

#ifdef WITH_WORKSPACES
        Workspaces::Init();
#endif

#ifdef WITH_BLUEZ
        InitBluetooth();
#endif

        PulseAudio::Init();

        CheckNetwork();
    }
    void FreeResources()
    {
#ifdef WITH_NVIDIA
        NvidiaGPU::Shutdown();
#endif
        PulseAudio::Shutdown();

#ifdef WITH_WORKSPACES
        Workspaces::Shutdown();
#endif

#ifdef WITH_BLUEZ
        StopBTScan();
#endif
        Logging::Shutdown();
    }
}

#include "System.h"
#include "Common.h"
#include "NvidiaGPU.h"
#include "PulseAudio.h"
#include "Hyprland.h"

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
    struct Config
    {
        std::string cpuThermalZone = ""; // idk, no standard way of doing this.
        std::string suspendCommand = "systemctl suspend";
        std::string lockCommand = ""; // idk, no standard way of doing this.
        std::string exitCommand = ""; // idk, no standard way of doing this.
        std::string batteryFolder = ""; // this can be BAT0, BAT1, etc. Usually in /sys/class/power_supply
        std::vector<std::string> workspaceSymbols = std::vector<std::string>(9, "");
        std::string defaultWorkspaceSymbol = "";
    };

    static Config config;

    void LoadConfig()
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
            else if (line.find("DefaultWorkspaceSymbol") != std::string::npos) {
                prop = &config.defaultWorkspaceSymbol;
            }
            else if (line.find("WorkspaceSymbol") != std::string::npos) {
                for (int i = 1; i < 10; i++) {
                    if (line.find("WorkspaceSymbol-" + std::to_string(i)) != std::string::npos) {
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
        std::ifstream tempFile(config.cpuThermalZone);
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
        std::ifstream fullChargeFile(config.batteryFolder + "/charge_full");
        std::ifstream currentChargeFile(config.batteryFolder + "/charge_now");
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

#ifdef HAS_NVIDIA
    GPUInfo GetGPUInfo()
    {
        NvidiaGPU::GPUUtilization util = NvidiaGPU::GetUtilization();
        GPUInfo out;
        out.utilisation = util.gpu;
        out.coreTemp = NvidiaGPU::GetTemperature();
        return out;
    }

    VRAMInfo GetVRAMInfo()
    {
        NvidiaGPU::VRAM vram = NvidiaGPU::GetVRAM();
        VRAMInfo out;
        out.totalGiB = (double)vram.totalB / (1024 * 1024 * 1024);
        out.usedGiB = out.totalGiB - ((double)vram.freeB / (1024 * 1024 * 1024));
        return out;
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

#ifdef HAS_BLUEZ
    BluetoothInfo GetBluetoothInfo()
    {
        BluetoothInfo out{};
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
        btctlProcess = OpenProcess("/bin/sh", "/bin/sh", "-c", "bluetoothctl scan on", NULL);
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
        OpenProcess("/bin/sh", "/bin/sh", "-c", "gBar bluetooth");
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
    void SetVolume(double volume)
    {
        PulseAudio::SetVolume(volume);
    }

#ifdef HAS_HYPRLAND
    WorkspaceStatus GetWorkspaceStatus(uint32_t monitor, uint32_t workspace)
    {
        return Hyprland::GetStatus(monitor, workspace);
    }
    void GotoWorkspace(uint32_t workspace)
    {
        return Hyprland::Goto(workspace);
    }
    std::string GetWorkspaceSymbol(int index) {
        if (index < 0 || index > 9) {
            LOG("Workspace Symbol Index Out Of Bounds: " + std::to_string(index));
            return "";
        }

        if (config.workspaceSymbols[index].empty()) {
            return config.defaultWorkspaceSymbol + " ";
        }

        return config.workspaceSymbols[index] + " ";
    }
#endif

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
        system(config.exitCommand.c_str());
    }

    void Lock()
    {
        system(config.lockCommand.c_str());
    }

    void Suspend()
    {
        system(config.suspendCommand.c_str());
    }

    void Init()
    {
        LoadConfig();
#ifdef HAS_NVIDIA
        NvidiaGPU::Init();
#endif
        PulseAudio::Init();
    }
    void FreeResources()
    {
#ifdef HAS_NVIDIA
        NvidiaGPU::Shutdown();
#endif
        PulseAudio::Shutdown();
#ifdef HAS_BLUEZ
        StopBTScan();
#endif
    }
}

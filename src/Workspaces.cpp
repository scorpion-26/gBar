#include "Workspaces.h"
#include "Wayland.h"
#include <ext-workspace-unstable-v1.h>
#include <unordered_map>

#ifdef WITH_WORKSPACES
namespace Workspaces
{
    namespace Wayland
    {
        using WaylandMonitor = ::Wayland::Monitor;
        using WaylandWorkspaceGroup = ::Wayland::WorkspaceGroup;
        using WaylandWorkspace = ::Wayland::Workspace;

        static uint32_t lastPolledMonitor;
        void PollStatus(uint32_t monitorID, uint32_t)
        {
            ::Wayland::PollEvents();
            lastPolledMonitor = monitorID;
        }
        System::WorkspaceStatus GetStatus(uint32_t workspaceId)
        {
            const WaylandMonitor& monitor = ::Wayland::GetMonitors().at(lastPolledMonitor);
            if (!monitor.output)
            {
                LOG("Polled monitor doesn't exist!");
                return System::WorkspaceStatus::Dead;
            }

            auto& workspaces = ::Wayland::GetWorkspaces();
            auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(),
                                            [&](const std::pair<zext_workspace_handle_v1*, WaylandWorkspace>& ws)
                                            {
                                                return ws.second.id == workspaceId;
                                            });
            if (workspaceIt == workspaces.end())
            {
                return System::WorkspaceStatus::Dead;
            }

            const WaylandWorkspaceGroup& group = ::Wayland::GetWorkspaceGroups().at(monitor.workspaceGroup);
            if (group.lastActiveWorkspace)
            {
                const WaylandWorkspace& activeWorkspace = workspaces.at(group.lastActiveWorkspace);
                if (activeWorkspace.id == workspaceId && activeWorkspace.active)
                {
                    return System::WorkspaceStatus::Active;
                }
                else if (activeWorkspace.id == workspaceId)
                {
                    // Last active workspace (Means we can still see it, since no other ws is active and thus is only visible)
                    return System::WorkspaceStatus::Current;
                }
            }

            const WaylandWorkspaceGroup& currentWorkspaceGroup = ::Wayland::GetWorkspaceGroups().at(workspaceIt->second.parent);
            if (currentWorkspaceGroup.lastActiveWorkspace == workspaceIt->first)
            {
                return System::WorkspaceStatus::Visible;
            }
            else
            {
                return System::WorkspaceStatus::Inactive;
            }

            return System::WorkspaceStatus::Dead;
        }
    }

#ifdef WITH_HYPRLAND
    namespace Hyprland
    {
        void Init()
        {
            if (!getenv("HYPRLAND_INSTANCE_SIGNATURE"))
            {
                LOG("Workspaces not running, disabling workspaces");
                // Not available
                RuntimeConfig::Get().hasWorkspaces = false;
            }
        }

        std::string DispatchIPC(const std::string& arg)
        {
            int hyprSocket = socket(AF_UNIX, SOCK_STREAM, 0);
            const char* instanceSignature = getenv("HYPRLAND_INSTANCE_SIGNATURE");
            std::string socketPath = "/tmp/hypr/" + std::string(instanceSignature) + "/.socket.sock";

            sockaddr_un addr = {};
            addr.sun_family = AF_UNIX;
            memcpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path));

            int ret = connect(hyprSocket, (sockaddr*)&addr, SUN_LEN(&addr));
            ASSERT(ret >= 0, "Couldn't connect to hyprland socket");
            ssize_t written = write(hyprSocket, arg.c_str(), arg.size());
            ASSERT(written >= 0, "Couldn't write to socket");
            char buf[2056];
            std::string res;

            while (true)
            {
                ssize_t bytesRead = read(hyprSocket, buf, sizeof(buf));
                if (bytesRead == 0)
                {
                    break;
                }
                ASSERT(bytesRead >= 0, "Couldn't read");
                res += std::string(buf, bytesRead);
            }
            close(hyprSocket);
            return res;
        }

        static std::vector<System::WorkspaceStatus> workspaceStati;

        void PollStatus(uint32_t monitorID, uint32_t numWorkspaces)
        {
            if (RuntimeConfig::Get().hasWorkspaces == false)
            {
                LOG("Error: Polled workspace status, but Workspaces isn't open!");
                return;
            }
            workspaceStati.clear();
            workspaceStati.resize(numWorkspaces, System::WorkspaceStatus::Dead);

            size_t parseIdx = 0;
            // First parse workspaces
            std::string workspaces = DispatchIPC("/workspaces");
            while ((parseIdx = workspaces.find("workspace ID ", parseIdx)) != std::string::npos)
            {
                // Goto (
                size_t begWSNum = workspaces.find('(', parseIdx) + 1;
                size_t endWSNum = workspaces.find(')', begWSNum);

                std::string ws = workspaces.substr(begWSNum, endWSNum - begWSNum);
                int32_t wsId = std::atoi(ws.c_str());
                if (wsId >= 1 && wsId < (int32_t)numWorkspaces)
                {
                    // WS is at least inactive
                    workspaceStati[wsId - 1] = System::WorkspaceStatus::Inactive;
                }
                parseIdx = endWSNum;
            }

            // Parse active workspaces for monitor
            std::string monitors = DispatchIPC("/monitors");
            parseIdx = 0;
            while ((parseIdx = monitors.find("Monitor ", parseIdx)) != std::string::npos)
            {
                // Goto ( and remove ID (=Advance 4 spaces, 1 for (, two for ID, one for space)
                size_t begMonNum = monitors.find('(', parseIdx) + 4;
                size_t endMonNum = monitors.find(')', begMonNum);
                std::string mon = monitors.substr(begMonNum, endMonNum - begMonNum);
                int32_t monIdx = std::atoi(mon.c_str());

                // Parse active workspace
                parseIdx = monitors.find("active workspace: ", parseIdx);
                ASSERT(parseIdx != std::string::npos, "Invalid IPC response!");
                size_t begWSNum = monitors.find('(', parseIdx) + 1;
                size_t endWSNum = monitors.find(')', begWSNum);
                std::string ws = monitors.substr(begWSNum, endWSNum - begWSNum);
                int32_t wsId = std::atoi(ws.c_str());

                // Check if focused
                parseIdx = monitors.find("focused: ", parseIdx);
                ASSERT(parseIdx != std::string::npos, "Invalid IPC response!");
                size_t begFocused = monitors.find(' ', parseIdx) + 1;
                size_t endFocused = monitors.find('\n', begFocused);
                bool focused = std::string_view(monitors).substr(begFocused, endFocused - begFocused) == "yes";

                if (wsId >= 1 && wsId < (int32_t)numWorkspaces)
                {
                    if ((uint32_t)monIdx == monitorID)
                    {
                        if (focused)
                        {
                            workspaceStati[wsId - 1] = System::WorkspaceStatus::Active;
                        }
                        else
                        {
                            workspaceStati[wsId - 1] = System::WorkspaceStatus::Current;
                        }
                    }
                    else
                    {
                        workspaceStati[wsId - 1] = System::WorkspaceStatus::Visible;
                    }
                }
            }
        }

        System::WorkspaceStatus GetStatus(uint32_t workspaceId)
        {
            if (RuntimeConfig::Get().hasWorkspaces == false)
            {
                LOG("Error: Queried for workspace status, but Workspaces isn't open!");
                return System::WorkspaceStatus::Dead;
            }
            ASSERT(workspaceId > 0 && workspaceId <= workspaceStati.size(), "Invalid workspaceId, you need to poll the workspace first!");
            return workspaceStati[workspaceId - 1];
        }
    }
#endif

    void Init()
    {
#ifdef WITH_HYPRLAND
        if (Config::Get().useHyprlandIPC)
        {
            Hyprland::Init();
            return;
        }
#endif
    }

    void PollStatus(uint32_t monitorID, uint32_t numWorkspaces)
    {
#ifdef WITH_HYPRLAND
        if (Config::Get().useHyprlandIPC)
        {
            Hyprland::PollStatus(monitorID, numWorkspaces);
            return;
        }
#endif
        Wayland::PollStatus(monitorID, numWorkspaces);
    }

    System::WorkspaceStatus GetStatus(uint32_t workspaceId)
    {
#ifdef WITH_HYPRLAND
        if (Config::Get().useHyprlandIPC)
        {
            return Hyprland::GetStatus(workspaceId);
        }
#endif
        return Wayland::GetStatus(workspaceId);
    }

    void Shutdown() {}
}
#endif

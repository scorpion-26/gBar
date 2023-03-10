#include "Workspaces.h"

#include <unordered_map>

#include <wayland-client.h>
#include <ext-workspace-unstable-v1.h>

#ifdef WITH_WORKSPACES
namespace Workspaces
{
    namespace Wayland
    {
        struct WaylandMonitor
        {
            std::string name;
            wl_output* output;
            zext_workspace_group_handle_v1* workspaceGroup;
        };

        struct WaylandWorkspace
        {
            zext_workspace_group_handle_v1* parent;
            uint32_t id;
            bool active;
        };
        struct WaylandWorkspaceGroup
        {
            std::vector<zext_workspace_handle_v1*> workspaces;
            zext_workspace_handle_v1* lastActiveWorkspace;
        };

        // There's probably a better way to avoid the LUTs
        static std::unordered_map<uint32_t, WaylandMonitor> monitors;
        static std::unordered_map<zext_workspace_group_handle_v1*, WaylandWorkspaceGroup> workspaceGroups;
        static std::unordered_map<zext_workspace_handle_v1*, WaylandWorkspace> workspaces;

        static uint32_t curID = 0;

        static wl_display* display;
        static wl_registry* registry;
        static zext_workspace_manager_v1* workspaceManager;

        static bool registeredMonitors = false;
        static bool registeredGroup = false;
        static bool registeredWorkspace = false;
        static bool registeredWorkspaceInfo = false;

        // Wayland callbacks

        // Workspace Callbacks
        static void OnWorkspaceName(void*, zext_workspace_handle_v1* workspace, const char* name)
        {
            workspaces[workspace].id = std::stoul(name);
            LOG("Workspace ID: " << workspaces[workspace].id);
            registeredWorkspaceInfo = true;
        }
        static void OnWorkspaceGeometry(void*, zext_workspace_handle_v1*, wl_array*) {}
        static void OnWorkspaceState(void*, zext_workspace_handle_v1* ws, wl_array* arrState)
        {
            WaylandWorkspace& workspace = workspaces[ws];
            ASSERT(workspace.parent, "Wayland: Workspace not registered!");
            WaylandWorkspaceGroup& group = workspaceGroups[workspace.parent];

            workspace.active = false;
            // Manual wl_array_for_each, since that's broken for C++
            for (zext_workspace_handle_v1_state* state = (zext_workspace_handle_v1_state*)arrState->data;
                 (uint8_t*)state < (uint8_t*)arrState->data + arrState->size; state += 1)
            {
                if (*state == ZEXT_WORKSPACE_HANDLE_V1_STATE_ACTIVE)
                {
                    LOG("Wayland: Activate Workspace " << workspace.id);
                    group.lastActiveWorkspace = ws;
                    workspace.active = true;
                }
            }
            if (!workspace.active)
            {
                LOG("Wayland: Deactivate Workspace " << workspace.id);
            }
        }
        static void OnWorkspaceRemove(void*, zext_workspace_handle_v1* ws)
        {
            WaylandWorkspace& workspace = workspaces[ws];
            ASSERT(workspace.parent, "Wayland: Workspace not registered!");
            WaylandWorkspaceGroup& group = workspaceGroups[workspace.parent];
            auto it = std::find(group.workspaces.begin(), group.workspaces.end(), ws);
            group.workspaces.erase(it);

            workspaces.erase(ws);

            LOG("Wayland: Removed workspace!");
        }
        zext_workspace_handle_v1_listener workspaceListener = {OnWorkspaceName, OnWorkspaceGeometry, OnWorkspaceState, OnWorkspaceRemove};

        // Workspace Group callbacks
        static void OnWSGroupOutputEnter(void*, zext_workspace_group_handle_v1* group, wl_output* output)
        {
            auto monitor = std::find_if(monitors.begin(), monitors.end(),
                                        [&](const std::pair<uint32_t, WaylandMonitor>& mon)
                                        {
                                            return mon.second.output == output;
                                        });
            ASSERT(monitor != monitors.end(), "Wayland: Registered WS group before monitor!");
            LOG("Wayland: Added group to monitor");
            monitor->second.workspaceGroup = group;
        }
        static void OnWSGroupOutputLeave(void*, zext_workspace_group_handle_v1*, wl_output* output)
        {
            auto monitor = std::find_if(monitors.begin(), monitors.end(),
                                        [&](const std::pair<uint32_t, WaylandMonitor>& mon)
                                        {
                                            return mon.second.output == output;
                                        });
            ASSERT(monitor != monitors.end(), "Wayland: Registered WS group before monitor!");
            LOG("Wayland: Added group to monitor");
            monitor->second.workspaceGroup = nullptr;
        }
        static void OnWSGroupWorkspaceAdded(void*, zext_workspace_group_handle_v1* workspace, zext_workspace_handle_v1* ws)
        {
            LOG("Wayland: Added workspace!");
            workspaceGroups[workspace].workspaces.push_back(ws);
            workspaces[ws] = {workspace, (uint32_t)-1};
            zext_workspace_handle_v1_add_listener(ws, &workspaceListener, nullptr);
            registeredWorkspace = true;
        }
        static void OnWSGroupRemove(void*, zext_workspace_group_handle_v1* workspaceGroup)
        {
            workspaceGroups.erase(workspaceGroup);
        }
        zext_workspace_group_handle_v1_listener workspaceGroupListener = {OnWSGroupOutputEnter, OnWSGroupOutputLeave, OnWSGroupWorkspaceAdded,
                                                                          OnWSGroupRemove};

        // Workspace Manager Callbacks
        static void OnWSManagerNewGroup(void*, zext_workspace_manager_v1*, zext_workspace_group_handle_v1* group)
        {
            // Register callbacks for the group.
            registeredGroup = true;
            zext_workspace_group_handle_v1_add_listener(group, &workspaceGroupListener, nullptr);
        }
        static void OnWSManagerDone(void*, zext_workspace_manager_v1*) {}
        static void OnWSManagerFinished(void*, zext_workspace_manager_v1*)
        {
            LOG("Wayland: Workspace manager finished. Disabling workspaces!");
            RuntimeConfig::Get().hasWorkspaces = false;
        }
        zext_workspace_manager_v1_listener workspaceManagerListener = {OnWSManagerNewGroup, OnWSManagerDone, OnWSManagerFinished};

        // Output Callbacks
        // Very bloated, indeed
        static void OnOutputGeometry(void*, wl_output*, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t) {}
        static void OnOutputMode(void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {}
        static void OnOutputDone(void*, wl_output*) {}
        static void OnOutputScale(void*, wl_output*, int32_t) {}
        static void OnOutputName(void*, wl_output* output, const char* name)
        {
            LOG("Wayland: Registering monitor " << name << " at ID " << curID);
            registeredMonitors = true;
            monitors.try_emplace(curID++, WaylandMonitor{name, output, nullptr});
        }
        static void OnOutputDescription(void*, wl_output*, const char*) {}
        wl_output_listener outputListener = {OnOutputGeometry, OnOutputMode, OnOutputDone, OnOutputScale, OnOutputName, OnOutputDescription};

        // Registry Callbacks
        static void OnRegistryAdd(void*, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
        {
            if (strcmp(interface, "wl_output") == 0)
            {
                wl_output* output = (wl_output*)wl_registry_bind(registry, name, &wl_output_interface, 4);
                wl_output_add_listener(output, &outputListener, nullptr);
            }
            if (strcmp(interface, "zext_workspace_manager_v1") == 0)
            {
                workspaceManager = (zext_workspace_manager_v1*)wl_registry_bind(registry, name, &zext_workspace_manager_v1_interface, version);
                zext_workspace_manager_v1_add_listener(workspaceManager, &workspaceManagerListener, nullptr);
            }
        }
        static void OnRegistryRemove(void*, wl_registry*, uint32_t) {}
        wl_registry_listener registryListener = {OnRegistryAdd, OnRegistryRemove};

        // Dispatch events.
        static void Dispatch()
        {
            wl_display_roundtrip(display);
        }
        static void WaitFor(bool& condition)
        {
            while (!condition && wl_display_dispatch(display) != -1)
            {
            }
        }

        void Init()
        {
            display = wl_display_connect(nullptr);
            ASSERT(display, "Cannot connect to wayland compositor!");
            registry = wl_display_get_registry(display);
            ASSERT(registry, "Cannot get wayland registry!");

            wl_registry_add_listener(registry, &registryListener, nullptr);
            wl_display_roundtrip(display);

            WaitFor(registeredMonitors);
            registeredMonitors = false;

            if (!workspaceManager)
            {
                LOG("Compositor doesn't implement zext_workspace_manager_v1, disabling workspaces!");
                RuntimeConfig::Get().hasWorkspaces = false;
                return;
            }

            // Hack: manually activate workspace for each monitor
            for (auto& monitor : monitors)
            {
                // Find group
                auto& group = workspaceGroups[monitor.second.workspaceGroup];

                // Find ws with monitor index + 1
                auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(),
                                                [&](const std::pair<zext_workspace_handle_v1*, WaylandWorkspace>& ws)
                                                {
                                                    return ws.second.id == monitor.first + 1;
                                                });
                if (workspaceIt != workspaces.end())
                {
                    LOG("Forcefully activate workspace " << workspaceIt->second.id)
                    if (workspaceIt->second.id == 1)
                    {
                        // Activate first workspace
                        workspaceIt->second.active = true;
                    }
                    // Make it visible
                    group.lastActiveWorkspace = workspaceIt->first;
                }
            }
        }

        // Hack for unified interface
        static uint32_t lastPolledMonitor = 0;
        void PollStatus(uint32_t monitor, uint32_t)
        {
            lastPolledMonitor = monitor;
            // Dispatch events
            Dispatch();
            if (registeredGroup)
            {
                // New Group, wait for workspaces to be registered.
                WaitFor(registeredWorkspace);
            }
            if (registeredWorkspace)
            {
                // New workspace added, need info
                WaitFor(registeredWorkspaceInfo);
            }
            registeredGroup = false;
            registeredWorkspace = false;
            registeredWorkspaceInfo = false;
            return;
        }

        System::WorkspaceStatus GetStatus(uint32_t workspaceId)
        {
            WaylandMonitor& monitor = monitors[lastPolledMonitor];
            if (!monitor.output)
            {
                LOG("Polled monitor doesn't exist!");
                return System::WorkspaceStatus::Dead;
            }

            auto workspaceIt = std::find_if(workspaces.begin(), workspaces.end(),
                                            [&](const std::pair<zext_workspace_handle_v1*, WaylandWorkspace>& ws)
                                            {
                                                return ws.second.id == workspaceId;
                                            });
            if (workspaceIt == workspaces.end())
            {
                return System::WorkspaceStatus::Dead;
            }

            WaylandWorkspaceGroup& group = workspaceGroups[monitor.workspaceGroup];
            if (group.lastActiveWorkspace)
            {
                WaylandWorkspace& activeWorkspace = workspaces[group.lastActiveWorkspace];
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

            WaylandWorkspaceGroup& currentWorkspaceGroup = workspaceGroups[workspaceIt->second.parent];
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

        void Shutdown()
        {
            if (display)
                wl_display_disconnect(display);
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
        Wayland::Init();
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

    void Shutdown()
    {
        Wayland::Shutdown();
    }
}
#endif

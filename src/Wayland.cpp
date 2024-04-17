#include "Wayland.h"

#include "Common.h"
#include "Config.h"
#include <wayland-client-protocol.h>
#include <wayland-client.h>
#include <ext-workspace-unstable-v1.h>
#include <wlr-foreign-toplevel-management-unstable-v1.h>

namespace Wayland
{
    // There's probably a better way to avoid the LUTs
    static std::unordered_map<wl_output*, Monitor> monitors;
    static std::unordered_map<zext_workspace_group_handle_v1*, WorkspaceGroup> workspaceGroups;
    static std::unordered_map<zext_workspace_handle_v1*, Workspace> workspaces;
    static std::unordered_map<zwlr_foreign_toplevel_handle_v1*, Window> windows;

    static wl_display* display;
    static wl_registry* registry;
    static zext_workspace_manager_v1* workspaceManager;
    static zwlr_foreign_toplevel_manager_v1* toplevelManager;

    static bool registeredMonitor = false;
    static bool registeredGroup = false;
    static bool registeredWorkspace = false;
    static bool registeredWorkspaceInfo = false;

    // Wayland callbacks

    // Workspace Callbacks
    static void OnWorkspaceName(void*, zext_workspace_handle_v1* workspace, const char* name)
    {
        try
        {
            workspaces[workspace].id = std::stoul(name);
            LOG("Workspace ID: " << workspaces[workspace].id);
            registeredWorkspaceInfo = true;
        }
        catch (const std::invalid_argument&)
        {
            LOG("Wayland: Invalid WS name: " << name);
        }
    }
    static void OnWorkspaceGeometry(void*, zext_workspace_handle_v1*, wl_array*) {}
    static void OnWorkspaceState(void*, zext_workspace_handle_v1* ws, wl_array* arrState)
    {
        Workspace& workspace = workspaces[ws];
        ASSERT(workspace.parent, "Wayland: Workspace not registered!");
        WorkspaceGroup& group = workspaceGroups[workspace.parent];

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
        Workspace& workspace = workspaces[ws];
        ASSERT(workspace.parent, "Wayland: Workspace not registered!");
        WorkspaceGroup& group = workspaceGroups[workspace.parent];
        auto it = std::find(group.workspaces.begin(), group.workspaces.end(), ws);
        group.workspaces.erase(it);

        workspaces.erase(ws);

        LOG("Wayland: Removed workspace!");
        if (group.lastActiveWorkspace == ws)
        {
            if (group.workspaces.size())
                group.lastActiveWorkspace = group.workspaces[0];
            else
                group.lastActiveWorkspace = nullptr;
        }
    }
    zext_workspace_handle_v1_listener workspaceListener = {OnWorkspaceName, OnWorkspaceGeometry, OnWorkspaceState, OnWorkspaceRemove};

    // Workspace Group callbacks
    static void OnWSGroupOutputEnter(void*, zext_workspace_group_handle_v1* group, wl_output* output)
    {
        auto monitor = monitors.find(output);
        ASSERT(monitor != monitors.end(), "Wayland: Registered WS group before monitor!");
        LOG("Wayland: Added group to monitor");
        monitor->second.workspaceGroup = group;
    }
    static void OnWSGroupOutputLeave(void*, zext_workspace_group_handle_v1*, wl_output* output)
    {
        auto monitor = monitors.find(output);
        ASSERT(monitor != monitors.end(), "Wayland: Registered WS group before monitor!");
        LOG("Wayland: Added group to monitor");
        monitor->second.workspaceGroup = nullptr;
    }
    static void OnWSGroupWorkspaceAdded(void*, zext_workspace_group_handle_v1* workspace, zext_workspace_handle_v1* ws)
    {
        LOG("Wayland: Added workspace!");
        workspaceGroups[workspace].workspaces.push_back(ws);
        workspaces[ws] = {workspace, (uint32_t)-1, false};
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

    // zwlr_foreign_toplevel_handle_v1
    static void OnTLTitle(void*, zwlr_foreign_toplevel_handle_v1* toplevel, const char* title)
    {
        auto window = windows.find(toplevel);
        ASSERT(window != windows.end(), "Wayland: OnTLTile called on unknwon toplevel!");
        window->second.title = title;
    }
    static void OnTLOutputEnter(void*, UNUSED zwlr_foreign_toplevel_handle_v1* toplevel, UNUSED wl_output* output) {}
    static void OnTLOutputLeave(void*, UNUSED zwlr_foreign_toplevel_handle_v1* toplevel, UNUSED wl_output* output) {}
    static void OnTLState(void*, zwlr_foreign_toplevel_handle_v1* toplevel, wl_array* state)
    {
        auto window = windows.find(toplevel);
        ASSERT(window != windows.end(), "Wayland: OnTLState called on unknwon toplevel!");
        // Unrolled from wl_array_for_each, but with types defined to compile under C++
        // There doesn't seem to be any documentation on the element size of the state, so use wlr's internally used uint32_t
        bool activated = false;
        for (uint32_t* curPtr = (uint32_t*)state->data; (uint8_t*)curPtr < (uint8_t*)state->data + state->size; curPtr++)
        {
            switch (*curPtr)
            {
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED: activated = true; break;
            default: break;
            }
        }
        window->second.activated = activated;
    }
    static void OnTLAppID(void*, zwlr_foreign_toplevel_handle_v1* toplevel, const char* appId) {}
    static void OnTLDone(void*, zwlr_foreign_toplevel_handle_v1*) {}
    static void OnTLClosed(void*, zwlr_foreign_toplevel_handle_v1* toplevel)
    {
        windows.erase(toplevel);
        zwlr_foreign_toplevel_handle_v1_destroy(toplevel);
    }
    static void OnTLParent(void*, zwlr_foreign_toplevel_handle_v1*, zwlr_foreign_toplevel_handle_v1*) {}
    zwlr_foreign_toplevel_handle_v1_listener toplevelListener = {OnTLTitle, OnTLAppID, OnTLOutputEnter, OnTLOutputLeave,
                                                                 OnTLState, OnTLDone,  OnTLClosed,      OnTLParent};

    // zwlr_foreign_toplevel_manager_v1
    static void OnToplevel(void*, zwlr_foreign_toplevel_manager_v1*, zwlr_foreign_toplevel_handle_v1* toplevel)
    {
        windows.emplace(toplevel, Window{});
        zwlr_foreign_toplevel_handle_v1_add_listener(toplevel, &toplevelListener, nullptr);
    }
    static void OnTLManagerFinished(void*, zwlr_foreign_toplevel_manager_v1*) {}
    zwlr_foreign_toplevel_manager_v1_listener toplevelManagerListener = {OnToplevel, OnTLManagerFinished};

    // Output Callbacks
    // Very bloated, indeed
    static void OnOutputGeometry(void*, wl_output* output, int32_t, int32_t, int32_t, int32_t, int32_t, const char*, const char*, int32_t transform)
    {
        auto it = monitors.find(output);
        ASSERT(it != monitors.end(), "Error: OnOutputGeometry called on unknown monitor");
        switch (transform)
        {
        case WL_OUTPUT_TRANSFORM_NORMAL:
        case WL_OUTPUT_TRANSFORM_FLIPPED: it->second.rotation = 0; break;

        case WL_OUTPUT_TRANSFORM_90:
        case WL_OUTPUT_TRANSFORM_FLIPPED_90: it->second.rotation = 90; break;

        case WL_OUTPUT_TRANSFORM_180:
        case WL_OUTPUT_TRANSFORM_FLIPPED_180: it->second.rotation = 180; break;

        case WL_OUTPUT_TRANSFORM_270:
        case WL_OUTPUT_TRANSFORM_FLIPPED_270: it->second.rotation = 270; break;
        }
    }
    static void OnOutputMode(void*, wl_output* output, uint32_t, int32_t width, int32_t height, int32_t)
    {
        auto it = monitors.find(output);
        ASSERT(it != monitors.end(), "Error: OnOutputMode called on unknown monitor");
        it->second.width = width;
        it->second.height = height;
    }
    static void OnOutputDone(void*, wl_output* output) {}
    static void OnOutputScale(void*, wl_output* output, int32_t scale)
    {
        auto it = monitors.find(output);
        ASSERT(it != monitors.end(), "Error: OnOutputScale called on unknown monitor");
        it->second.scale = scale;
    }
    static void OnOutputName(void*, wl_output* output, const char* name)
    {
        auto it = monitors.find(output);
        ASSERT(it != monitors.end(), "Error: OnOutputName called on unknown monitor");
        it->second.name = name;
        LOG("Wayland: Monitor at ID " << it->second.ID << " got name " << name);
        registeredMonitor = true;
    }
    static void OnOutputDescription(void*, wl_output*, const char*) {}
    wl_output_listener outputListener = {OnOutputGeometry, OnOutputMode, OnOutputDone, OnOutputScale, OnOutputName, OnOutputDescription};

    // Registry Callbacks
    static void OnRegistryAdd(void*, wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
    {
        if (strcmp(interface, "wl_output") == 0)
        {
            wl_output* output = (wl_output*)wl_registry_bind(registry, name, &wl_output_interface, 4);
            Monitor mon = Monitor{"", name, 0, 0, 0, 0, nullptr, (uint32_t)monitors.size()};
            monitors.emplace(output, mon);

            LOG("Wayland: Register <pending> at ID " << mon.ID);
            wl_output_add_listener(output, &outputListener, nullptr);
        }
        else if (strcmp(interface, "zext_workspace_manager_v1") == 0 && !Config::Get().useHyprlandIPC)
        {
            workspaceManager = (zext_workspace_manager_v1*)wl_registry_bind(registry, name, &zext_workspace_manager_v1_interface, version);
            zext_workspace_manager_v1_add_listener(workspaceManager, &workspaceManagerListener, nullptr);
        }
        else if (strcmp(interface, "zwlr_foreign_toplevel_manager_v1") == 0)
        {
            toplevelManager = (zwlr_foreign_toplevel_manager_v1*)wl_registry_bind(registry, name, &zwlr_foreign_toplevel_manager_v1_interface,
                                                                                  version);
            zwlr_foreign_toplevel_manager_v1_add_listener(toplevelManager, &toplevelManagerListener, nullptr);
        }
    }
    static void OnRegistryRemove(void*, wl_registry*, uint32_t name)
    {
        auto it = std::find_if(monitors.begin(), monitors.end(),
                               [&](const std::pair<wl_output*, const Monitor&>& elem)
                               {
                                   return elem.second.wlName == name;
                               });
        if (it != monitors.end())
        {
            LOG("Wayland: Removing monitor " << it->second.name << " at ID " << it->second.ID);
            // Monitor has been removed. Update the ids of the other accordingly
            for (auto& mon : monitors)
            {
                if (mon.second.ID > it->second.ID)
                {
                    mon.second.ID -= 1;
                    auto name = mon.second.name.empty() ? "<pending>" : mon.second.name;
                    LOG("Wayland: " << name << " got new ID " << mon.second.ID);
                }
            }
            registeredMonitor = true;
            monitors.erase(it);
        }
    }

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

        WaitFor(registeredMonitor);
        registeredMonitor = false;

        if (!workspaceManager && !Config::Get().useHyprlandIPC)
        {
            LOG("Compositor doesn't implement zext_workspace_manager_v1, disabling workspaces!");
            LOG("Note: Hyprland v0.30.0 removed support for zext_workspace_manager_v1, please enable UseHyprlandIPC instead!");
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
                                            [&](const std::pair<zext_workspace_handle_v1*, Workspace>& ws)
                                            {
                                                return ws.second.id == monitor.second.ID + 1;
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

    void PollEvents()
    {
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
    }

    void Shutdown()
    {
        if (display)
            wl_display_disconnect(display);
    }

    std::string GtkMonitorIDToName(int32_t monitorID)
    {
        auto it = std::find_if(monitors.begin(), monitors.end(),
                               [&](const std::pair<wl_output*, Monitor>& el)
                               {
                                   return el.second.ID == (uint32_t)monitorID;
                               });
        if (it == monitors.end())
        {
            LOG("Wayland: No monitor registered with ID " << monitorID);
            return "";
        }
        return it->second.name;
    }
    int32_t NameToGtkMonitorID(const std::string& name)
    {
        auto it = std::find_if(monitors.begin(), monitors.end(),
                               [&](const std::pair<wl_output*, Monitor>& el)
                               {
                                   return el.second.name == name;
                               });
        if (it == monitors.end())
            return -1;
        return it->second.ID;
    }

    const Window* GetActiveWindow()
    {
        auto it = std::find_if(windows.begin(), windows.end(),
                               [&](const std::pair<zwlr_foreign_toplevel_handle_v1*, Window>& el)
                               {
                                   return el.second.activated == true;
                               });
        if (it == windows.end())
            return nullptr;
        return &it->second;
    }

    const std::unordered_map<wl_output*, Monitor>& GetMonitors()
    {
        return monitors;
    }
    const std::unordered_map<zext_workspace_group_handle_v1*, WorkspaceGroup>& GetWorkspaceGroups()
    {
        return workspaceGroups;
    }
    const std::unordered_map<zext_workspace_handle_v1*, Workspace>& GetWorkspaces()
    {
        return workspaces;
    }
}

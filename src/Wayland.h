#pragma once
#include "Common.h"

struct wl_output;
struct zext_workspace_group_handle_v1;
struct zext_workspace_handle_v1;
namespace Wayland
{
    struct Monitor
    {
        std::string name;
        uint32_t wlName;
        int32_t width;
        int32_t height;
        zext_workspace_group_handle_v1* workspaceGroup;
        // The Gdk monitor index. This is only a hacky approximation, since there is no way to get the wl_output from a GdkMonitor
        uint32_t ID;
    };

    struct Workspace
    {
        zext_workspace_group_handle_v1* parent;
        uint32_t id;
        bool active;
    };
    struct WorkspaceGroup
    {
        std::vector<zext_workspace_handle_v1*> workspaces;
        zext_workspace_handle_v1* lastActiveWorkspace;
    };

    void Init();
    void PollEvents();

    const std::unordered_map<wl_output*, Monitor>& GetMonitors();
    const std::unordered_map<zext_workspace_group_handle_v1*, WorkspaceGroup>& GetWorkspaceGroups();
    const std::unordered_map<zext_workspace_handle_v1*, Workspace>& GetWorkspaces();

    // Returns the connector name of the monitor
    std::string GtkMonitorIDToName(int32_t monitorID);
    int32_t NameToGtkMonitorID(const std::string& name);

    void Shutdown();
}

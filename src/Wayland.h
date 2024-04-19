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
        int32_t scale; // TODO: Handle fractional scaling
        uint32_t rotation;
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

    struct Window
    {
        std::string title;
        bool activated;
    };

    void Init();
    void PollEvents();

    const std::unordered_map<wl_output*, Monitor>& GetMonitors();
    const std::unordered_map<zext_workspace_group_handle_v1*, WorkspaceGroup>& GetWorkspaceGroups();
    const std::unordered_map<zext_workspace_handle_v1*, Workspace>& GetWorkspaces();

    // Returns the connector name of the monitor
    std::string GtkMonitorIDToName(int32_t monitorID);
    int32_t NameToGtkMonitorID(const std::string& name);

    template<typename Predicate>
    inline const Monitor* FindMonitor(Predicate&& pred)
    {
        auto& mons = GetMonitors();
        auto it = std::find_if(mons.begin(), mons.end(),
                               [&](const std::pair<wl_output*, Monitor>& mon)
                               {
                                   return pred(mon.second);
                               });
        return it != mons.end() ? &it->second : nullptr;
    }

    inline const Monitor* FindMonitorByName(const std::string& name)
    {
        return FindMonitor(
            [&](const Monitor& mon)
            {
                return mon.name == name;
            });
    }

    const Window* GetActiveWindow();

    void Shutdown();
}

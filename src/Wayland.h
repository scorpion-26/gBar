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
        wl_output* output;
        zext_workspace_group_handle_v1* workspaceGroup;
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

    const std::unordered_map<std::string, Monitor>& GetMonitors();
    const std::unordered_map<zext_workspace_group_handle_v1*, WorkspaceGroup>& GetWorkspaceGroups();
    const std::unordered_map<zext_workspace_handle_v1*, Workspace>& GetWorkspaces();

    void Shutdown();
}

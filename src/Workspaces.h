#pragma once
#include "Common.h"
#include "System.h"
#include "Config.h"

#include <cstdint>
#include <string>
#include <cstdlib>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#ifdef WITH_WORKSPACES
namespace Workspaces
{
    void Init();

    void PollStatus(uint32_t monitorID, uint32_t numWorkspaces);

    System::WorkspaceStatus GetStatus(uint32_t workspaceId);

    void Shutdown();

    // TODO: Use ext_workspaces for this, if applicable
    inline void Goto(uint32_t workspace)
    {
        if (RuntimeConfig::Get().hasWorkspaces == false)
        {
            LOG("Error: Called Go to workspace, but Workspaces isn't open!");
            return;
        }

        system(("hyprctl dispatch workspace " + std::to_string(workspace)).c_str());
    }

    // direction: + or -
    inline void GotoNext(char direction)
    {
        char scrollOp = 'e';
        if (Config::Get().workspaceScrollOnMonitor)
        {
            scrollOp = 'm';
        }
        std::string cmd = std::string("hyprctl dispatch workspace ") + scrollOp + direction + "1";
        system(cmd.c_str());
    }
}
#endif

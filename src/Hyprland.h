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

#ifdef WITH_HYPRLAND
namespace Hyprland
{
    inline void Init()
    {
        if (!getenv("HYPRLAND_INSTANCE_SIGNATURE"))
        {
            LOG("Hyprland not running, disabling workspaces");
            // Not available
            RuntimeConfig::Get().hasHyprland = false;
        }
    }

    inline std::string DispatchIPC(const std::string& arg)
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

    inline System::WorkspaceStatus GetStatus(uint32_t monitorID, uint32_t workspaceId)
    {
        if (RuntimeConfig::Get().hasHyprland == false)
        {
            LOG("Error: Queried for workspace status, but Hyprland isn't open!");
            return System::WorkspaceStatus::Dead;
        }

        std::string workspaces = DispatchIPC("/workspaces");
        if (workspaces.find("workspace ID " + std::to_string(workspaceId)) == std::string::npos)
        {
            // It's dead and there's nothing I can do about it
            // [Doesn't exist, no need to check anything else]
            return System::WorkspaceStatus::Dead;
        }
        std::string monitors = DispatchIPC("/monitors");
        size_t beginMonitor = monitors.find("(ID " + std::to_string(monitorID) + ")");
        ASSERT(beginMonitor != std::string::npos, "Monitor not found!");
        size_t endMonitor = monitors.find("dpmsStatus", beginMonitor);

        std::string_view selectedMon = std::string_view(monitors).substr(beginMonitor, endMonitor - beginMonitor);
        size_t activeWorkspaceLoc = selectedMon.find("active workspace:");
        size_t workspaceBeg = selectedMon.find("(", activeWorkspaceLoc);
        size_t workspaceEnd = selectedMon.find(")", workspaceBeg);

        std::string workspaceNum = std::string(selectedMon.substr(workspaceBeg + 1, workspaceEnd - workspaceBeg - 1));
        // Active workspace
        if (std::stoi(workspaceNum) == (int)workspaceId)
        {
            // Check if focused
            if (selectedMon.find("focused: yes") != std::string::npos)
            {
                return System::WorkspaceStatus::Active;
            }
            else
            {
                return System::WorkspaceStatus::Current;
            }
        }

        if (monitors.find("active workspace: " + std::to_string(workspaceId)) != std::string::npos)
        {
            return System::WorkspaceStatus::Visible;
        }
        else
        {
            return System::WorkspaceStatus::Inactive;
        }
    }

    inline void Goto(uint32_t workspace)
    {
        if (RuntimeConfig::Get().hasHyprland == false)
        {
            LOG("Error: Called Go to workspace, but Hyprland isn't open!");
            return;
        }

        system(("hyprctl dispatch workspace " + std::to_string(workspace)).c_str());
    }
}
#endif

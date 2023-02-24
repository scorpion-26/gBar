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

    static std::vector<System::WorkspaceStatus> workspaceStati;

    inline void PollStatus(uint32_t monitorID, uint32_t numWorkspaces)
    {
        if (RuntimeConfig::Get().hasHyprland == false)
        {
            LOG("Error: Polled workspace status, but Hyprland isn't open!");
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

    inline System::WorkspaceStatus GetStatus(uint32_t workspaceId)
    {
        if (RuntimeConfig::Get().hasHyprland == false)
        {
            LOG("Error: Queried for workspace status, but Hyprland isn't open!");
            return System::WorkspaceStatus::Dead;
        }
        ASSERT(workspaceId > 0 && workspaceId <= workspaceStati.size(), "Invalid workspaceId, you need to poll the workspace first!");
        return workspaceStati[workspaceId - 1];
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

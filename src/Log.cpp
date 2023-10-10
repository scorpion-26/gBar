#include "Log.h"
#include "Common.h"

#include <fstream>

namespace Logging
{
    static std::ofstream logFile;

    void Init()
    {
        pid_t pid = getpid();
        logFile = std::ofstream("/tmp/gBar-" + std::to_string(pid) + ".log");
        if (!logFile.is_open())
        {
            LOG("Cannot open logfile(/tmp/gBar-" << pid << ".log)");
        }
    }

    void Log(const std::string& str)
    {
        if (logFile.is_open())
            logFile << str << std::endl;
    }

    void Shutdown()
    {
        logFile.close();
    }
}

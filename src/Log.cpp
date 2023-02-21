#include "Log.h"
#include "Common.h"

#include <fstream>

namespace Logging
{
    static std::ofstream logFile;

    void Init()
    {
        logFile = std::ofstream("/tmp/gBar.log");
        if (!logFile.is_open())
        {
            LOG("Cannot open logfile(/tmp/gBar.log)");
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

#pragma once
#include <sstream>
#include <iostream>

#ifdef USE_LOGFILE
#define LOG(x)                   \
    std::cout << x << '\n';      \
    {                            \
        std::stringstream str;   \
        str << x;                \
        Logging::Log(str.str()); \
    }
#else
#define LOG(x) std::cout << x << '\n'
#endif
#define ASSERT(x, log)                                  \
    if (!(x))                                           \
    {                                                   \
        LOG(log << "\n[Exiting due to assert failed]"); \
        exit(-1);                                       \
    }
namespace Logging
{
    void Init();

    void Log(const std::string& str);

    void Shutdown();
}

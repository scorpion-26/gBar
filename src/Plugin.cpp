#include "Plugin.h"
#include "Common.h"
#include "Window.h"
#include "src/Log.h"

#include <dlfcn.h>

void Plugin::LoadWidgetFromPlugin(const std::string& pluginName, Window& window, const std::string& monitor)
{
    std::string home = std::getenv("HOME");
    std::array<std::string, 3> paths = {home + "/.local/lib/gBar", "/usr/local/lib/gBar", "/usr/lib/gBar"};
    // 1. Try and load plugin
    void* dl;
    for (auto& path : paths)
    {
        std::string soPath = path + "/lib" + pluginName + ".so";
        dl = dlopen(soPath.c_str(), RTLD_NOW);
        if (dl)
            break;
    }
    ASSERT(dl, "Error: Cannot find plugin \"" << pluginName
                                              << "\"!\n"
                                                 "Note: Did you mean to run \"gBar bar <monitor>\" instead?");

    typedef void (*PFN_InvokeCreateFun)(void*, void*);
    typedef int32_t (*PFN_GetVersion)();
    auto getVersion = (PFN_GetVersion)dlsym(dl, "Plugin_GetVersion");
    ASSERT(getVersion, "DL is not a valid gBar plugin!");
    ASSERT(getVersion() == DL_VERSION, "Mismatching version, please recompile your plugin!");

    auto invokeCreateFun = (PFN_InvokeCreateFun)dlsym(dl, "Plugin_InvokeCreateFun");
    ASSERT(invokeCreateFun, "DL is not a valid gBar plugin!");

    // Execute
    invokeCreateFun(&window, (void*)&monitor);
}

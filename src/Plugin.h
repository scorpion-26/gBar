#pragma once
#include <string>
#include "Window.h"

namespace Plugin
{
    void LoadWidgetFromPlugin(const std::string& pluginName, Window& window, int32_t monitor);
}

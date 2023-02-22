#pragma once
#include "Widget.h"
#include "Window.h"

namespace AudioFlyin
{
    enum class Type
    {
        Speaker,
        Microphone
    };
    void Create(Window& window, int32_t monitor, Type type);
}

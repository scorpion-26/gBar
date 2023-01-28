#include <gBar/Common.h>
#include <gBar/Window.h>

void Create(Window& window, int32_t monitor)
{
    auto mainWidget = Widget::Create<Text>();
    mainWidget->SetText("Hello, World!");

    window = Window(std::move(mainWidget), monitor);
}

DEFINE_PLUGIN(Create);

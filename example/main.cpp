#include <gBar/Common.h>
#include <gBar/Window.h>

void Create(Window& window, int32_t monitor)
{
    auto mainWidget = Widget::Create<Text>();
    mainWidget->SetText("Hello, World!");

    window = Window(monitor);
    window.SetMainWidget(std::move(mainWidget));
}

DEFINE_PLUGIN(Create);

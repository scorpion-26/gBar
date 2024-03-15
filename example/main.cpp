#include <gBar/Common.h>
#include <gBar/Window.h>

void Create(Window& window, const std::string& monitor)
{
    auto mainWidget = Widget::Create<Text>();
    mainWidget->SetText("Hello, World!");

    window.SetMainWidget(std::move(mainWidget));
}

DEFINE_PLUGIN(Create);

#pragma once
#include <gtk/gtk.h>
#include "Widget.h"
#include "Common.h"

enum class Anchor
{
    Top = BIT(0),
    Bottom = BIT(1),
    Left = BIT(2),
    Right = BIT(3)
};
DEFINE_ENUM_FLAGS(Anchor);

class Window
{
public:
    Window() = default;
    Window(std::unique_ptr<Widget>&& mainWidget, int32_t monitor);
    Window(Window&& window) noexcept = default;
    Window& operator=(Window&& other) noexcept = default;
    ~Window();

    void Run(int argc, char** argv);

    void Close();

    void SetAnchor(Anchor anchor) { m_Anchor = anchor; }
    void SetMargin(Anchor anchor, int32_t margin);
    void SetExclusive(bool exclusive) { m_Exclusive = exclusive; }

private:
    void UpdateMargin();

    GtkWindow* m_Window;
    GtkApplication* m_App = nullptr;

    std::unique_ptr<Widget> m_MainWidget;

    Anchor m_Anchor;
    std::array<std::pair<Anchor, int32_t>, 4> m_Margin;
    bool m_Exclusive = true;

    int32_t m_Monitor;
};

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

enum class Layer
{
    Top,
    Overlay
};

class Window
{
public:
    Window() = default;
    Window(int32_t monitor);
    Window(Window&& window) noexcept = default;
    Window& operator=(Window&& other) noexcept = default;
    ~Window();

    void Init(const std::string& overrideConfigLocation);
    void Run();

    void Close();

    void SetAnchor(Anchor anchor) { m_Anchor = anchor; }
    void SetMargin(Anchor anchor, int32_t margin);
    void SetExclusive(bool exclusive) { m_Exclusive = exclusive; }
    void SetLayer(Layer layer) { m_Layer = layer;}

    void SetMainWidget(std::unique_ptr<Widget>&& mainWidget);

    int GetWidth() const;
    int GetHeight() const;
private:
    void UpdateMargin();

    void LoadCSS(GtkCssProvider* provider);

    GtkWindow* m_Window = nullptr;
    GtkApplication* m_App = nullptr;

    std::unique_ptr<Widget> m_MainWidget;

    Anchor m_Anchor;
    std::array<std::pair<Anchor, int32_t>, 4> m_Margin;
    bool m_Exclusive = true;
    Layer m_Layer = Layer::Top;

    int32_t m_MonitorID;
    GdkMonitor* m_Monitor = nullptr;
};

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
    Window(const std::string& monitor);
    Window(Window&& window) noexcept = default;
    Window& operator=(Window&& other) noexcept = default;
    ~Window();

    void Init(const std::string& overrideConfigLocation);
    void Run();

    void Close();

    void SetAnchor(Anchor anchor) { m_Anchor = anchor; }
    void SetMargin(Anchor anchor, int32_t margin);
    void SetExclusive(bool exclusive) { m_Exclusive = exclusive; }
    void SetLayer(Layer layer) { m_Layer = layer; }
    void SetLayerNamespace(const std::string& layerNamespace) { m_LayerNamespace = layerNamespace; }

    void SetMainWidget(std::unique_ptr<Widget>&& mainWidget);

    int GetWidth() const;
    int GetHeight() const;

    // Returns the connector name of the currnet monitor
    std::string GetName() const { return m_MonitorName; }

    // Callback when the widget should be recreated
    std::function<void()> OnWidget;

private:
    void Create();
    void Destroy();

    void UpdateMargin();

    void LoadCSS(GtkCssProvider* provider);

    void MonitorAdded(GdkDisplay* display, GdkMonitor* monitor);
    void MonitorRemoved(GdkDisplay* display, GdkMonitor* monitor);

    GtkWindow* m_Window = nullptr;
    GtkApplication* m_App = nullptr;

    std::unique_ptr<Widget> m_MainWidget;

    Anchor m_Anchor;
    std::array<std::pair<Anchor, int32_t>, 4> m_Margin;
    bool m_Exclusive = true;
    Layer m_Layer = Layer::Top;
    std::string m_LayerNamespace = "gbar";

    // The monitor we are currently on.
    std::string m_MonitorName;

    // The monitor we want to be on.
    std::string m_TargetMonitor;

    GdkMonitor* m_Monitor = nullptr;

    bool bShouldQuit = false;
    bool bHandleMonitorChanges = false;
};

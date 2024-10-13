#include "Window.h"
#include "Common.h"
#include "CSS.h"
#include "Wayland.h"

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>

Window::Window(int32_t monitor) : m_MonitorName(Wayland::GtkMonitorIDToName(monitor)) {}
Window::Window(const std::string& monitor) : m_MonitorName(monitor) {}

Window::~Window()
{
    if (m_App)
    {
        g_object_unref(m_App);
        m_App = nullptr;
    }
}

void Window::Init(const std::string& overideConfigLocation)
{
    m_TargetMonitor = m_MonitorName;

    gtk_init(NULL, NULL);

    // Style
    CSS::Load(overideConfigLocation);

    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), (GtkStyleProvider*)CSS::GetProvider(), GTK_STYLE_PROVIDER_PRIORITY_USER);

    GdkDisplay* defaultDisplay = gdk_display_get_default();
    ASSERT(defaultDisplay != nullptr, "Cannot get display!");
    if (!m_MonitorName.empty())
    {
        m_Monitor = gdk_display_get_monitor(defaultDisplay, Wayland::NameToGtkMonitorID(m_MonitorName));
        ASSERT(m_Monitor, "Cannot get monitor \"" << m_MonitorName << "\"!");
    }
    else
    {
        LOG("Window: Requested monitor not found. Falling back to current monitor!")
        m_Monitor = gdk_display_get_primary_monitor(defaultDisplay);
    }

    // Register monitor added/removed callbacks
    auto monAdded = [](GdkDisplay* display, GdkMonitor* mon, void* window)
    {
        ((Window*)window)->MonitorAdded(display, mon);
    };
    g_signal_connect(defaultDisplay, "monitor-added", G_CALLBACK(+monAdded), this);
    auto monRemoved = [](GdkDisplay* display, GdkMonitor* mon, void* window)
    {
        ((Window*)window)->MonitorRemoved(display, mon);
    };
    g_signal_connect(defaultDisplay, "monitor-removed", G_CALLBACK(+monRemoved), this);
}

void Window::Run()
{
    Create();
    while (!bShouldQuit)
    {
        gtk_main_iteration();
        if (bHandleMonitorChanges)
        {
            // Flush the event loop
            while (gtk_events_pending())
            {
                if (!gtk_main_iteration())
                    break;
            }

            LOG("Window: Handling monitor changes");
            bHandleMonitorChanges = false;

            if (m_MonitorName == m_TargetMonitor)
            {
                // Don't care
                continue;
            }
            // Process Wayland
            Wayland::PollEvents();

            GdkDisplay* display = gdk_display_get_default();

            // HACK: Discrepancies are mostly caused by the HEADLESS monitor. Assume that Gdk ID 0 is headless
            bool bGotHeadless = (size_t)gdk_display_get_n_monitors(display) != Wayland::GetMonitors().size();
            if (bGotHeadless)
            {
                LOG("Window: Found discrepancy between GDK and Wayland!");
            }

            // Try to find our target monitor
            const Wayland::Monitor* targetMonitor = Wayland::FindMonitorByName(m_TargetMonitor);
            if (targetMonitor)
            {
                // Found target monitor, snap back.
                if (m_MainWidget)
                    Destroy();
                m_MonitorName = m_TargetMonitor;
                m_Monitor = gdk_display_get_monitor(display, bGotHeadless ? targetMonitor->ID + 1 : targetMonitor->ID);
                Create();
                continue;
            }

            // We haven't yet created, check if we can.
            if (m_MainWidget == nullptr)
            {
                // Find a non-headless monitor
                const Wayland::Monitor* replacementMonitor = Wayland::FindMonitor(
                    [&](const Wayland::Monitor& mon)
                    {
                        return mon.name.find("HEADLESS") == std::string::npos;
                    });
                if (!replacementMonitor)
                    continue;
                m_MonitorName = replacementMonitor->name;
                m_Monitor = gdk_display_get_monitor(display, bGotHeadless ? replacementMonitor->ID + 1 : replacementMonitor->ID);
                Create();
                continue;
            }
        }
    }
}

void Window::Create()
{
    LOG("Window: Create on monitor " << m_MonitorName);
    m_Window = (GtkWindow*)gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_layer_init_for_window(m_Window);

    // Notify our main method, that we want to init
    OnWidget();

    ASSERT(m_MainWidget, "Main Widget not set!");

    switch (m_Layer)
    {
    case Layer::Top: gtk_layer_set_layer(m_Window, GTK_LAYER_SHELL_LAYER_TOP); break;
    case Layer::Overlay: gtk_layer_set_layer(m_Window, GTK_LAYER_SHELL_LAYER_OVERLAY); break;
    }

    gtk_layer_set_namespace(m_Window, m_LayerNamespace.c_str());

    if (m_Exclusive)
        gtk_layer_auto_exclusive_zone_enable(m_Window);

    gtk_layer_set_monitor(m_Window, m_Monitor);

    if (FLAG_CHECK(m_Anchor, Anchor::Left))
    {
        gtk_layer_set_anchor(m_Window, GTK_LAYER_SHELL_EDGE_LEFT, true);
    }
    if (FLAG_CHECK(m_Anchor, Anchor::Right))
    {
        gtk_layer_set_anchor(m_Window, GTK_LAYER_SHELL_EDGE_RIGHT, true);
    }
    if (FLAG_CHECK(m_Anchor, Anchor::Top))
    {
        gtk_layer_set_anchor(m_Window, GTK_LAYER_SHELL_EDGE_TOP, true);
    }
    if (FLAG_CHECK(m_Anchor, Anchor::Bottom))
    {
        gtk_layer_set_anchor(m_Window, GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    }
    UpdateMargin();

    // Create widgets
    Widget::CreateAndAddWidget(m_MainWidget.get(), (GtkWidget*)m_Window);

    gtk_widget_show_all((GtkWidget*)m_Window);
}

void Window::Destroy()
{
    LOG("Window: Destroy");
    m_MainWidget = nullptr;
    gtk_widget_destroy((GtkWidget*)m_Window);
}

void Window::Close()
{
    Destroy();
    bShouldQuit = true;
}

void Window::UpdateMargin()
{
    for (auto [anchor, margin] : m_Margin)
    {
        if (FLAG_CHECK(anchor, Anchor::Left))
        {
            gtk_layer_set_margin(m_Window, GTK_LAYER_SHELL_EDGE_LEFT, margin);
        }
        if (FLAG_CHECK(anchor, Anchor::Right))
        {
            gtk_layer_set_margin(m_Window, GTK_LAYER_SHELL_EDGE_RIGHT, margin);
        }
        if (FLAG_CHECK(anchor, Anchor::Top))
        {
            gtk_layer_set_margin(m_Window, GTK_LAYER_SHELL_EDGE_TOP, margin);
        }
        if (FLAG_CHECK(anchor, Anchor::Bottom))
        {
            gtk_layer_set_margin(m_Window, GTK_LAYER_SHELL_EDGE_BOTTOM, margin);
        }
    }
}

void Window::SetMainWidget(std::unique_ptr<Widget>&& mainWidget)
{
    m_MainWidget = std::move(mainWidget);
}

void Window::SetMargin(Anchor anchor, int32_t margin)
{
    if (FLAG_CHECK(anchor, Anchor::Left))
    {
        m_Margin[0] = {Anchor::Left, margin};
    }
    if (FLAG_CHECK(anchor, Anchor::Right))
    {
        m_Margin[1] = {Anchor::Right, margin};
    }
    if (FLAG_CHECK(anchor, Anchor::Top))
    {
        m_Margin[2] = {Anchor::Top, margin};
    }
    if (FLAG_CHECK(anchor, Anchor::Bottom))
    {
        m_Margin[2] = {Anchor::Bottom, margin};
    }

    if (m_Window)
    {
        UpdateMargin();
    }
}

int Window::GetWidth() const
{
    // gdk_monitor_get_geometry is really unreliable for some reason.
    // Use our wayland backend instead

    /*GdkRectangle rect{};
    gdk_monitor_get_geometry(m_Monitor, &rect);
    return rect.width;*/

    const Wayland::Monitor* mon = Wayland::FindMonitorByName(m_MonitorName);
    ASSERT(mon, "Window: Couldn't find monitor");

    int viewedWidth = mon->width;

    // A 90/270 rotation should use the height for the viewed width
    switch (mon->rotation)
    {
    case 90:
    case 270: viewedWidth = mon->height; break;
    }
    return viewedWidth / mon->scale;
}

int Window::GetHeight() const
{
    /*GdkRectangle rect{};
    gdk_monitor_get_geometry(m_Monitor, &rect);
    return rect.height;*/

    const Wayland::Monitor* mon = Wayland::FindMonitorByName(m_MonitorName);
    ASSERT(mon, "Window: Couldn't find monitor");

    int viewedHeight = mon->height;

    // A 90/270 rotation should use the width for the viewed height
    switch (mon->rotation)
    {
    case 90:
    case 270: viewedHeight = mon->width; break;
    }
    return viewedHeight / mon->scale;
}

void Window::MonitorAdded(GdkDisplay*, GdkMonitor*)
{
    bHandleMonitorChanges = true;
}

void Window::MonitorRemoved(GdkDisplay*, GdkMonitor* mon)
{
    bHandleMonitorChanges = true;
    // Immediately react
    if (mon == m_Monitor)
    {
        LOG("Window: Current monitor removed!")
        m_Monitor = nullptr;
        m_MonitorName = "";
        Destroy();
    }
}

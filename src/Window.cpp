#include "Window.h"
#include "Common.h"
#include "CSS.h"

#include <tuple>
#include <fstream>

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>

Window::Window(int32_t monitor) : m_MonitorID(monitor) {}

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
    gtk_init(NULL, NULL);

    // Style
    CSS::Load(overideConfigLocation);

    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), (GtkStyleProvider*)CSS::GetProvider(), GTK_STYLE_PROVIDER_PRIORITY_USER);

    GdkDisplay* defaultDisplay = gdk_display_get_default();
    ASSERT(defaultDisplay != nullptr, "Cannot get display!");
    if (m_MonitorID != -1)
    {
        m_Monitor = gdk_display_get_monitor(defaultDisplay, m_MonitorID);
        ASSERT(m_Monitor, "Cannot get monitor!");
    }
    else
    {
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
    ASSERT(m_MainWidget, "Main Widget not set!");

    m_Window = (GtkWindow*)gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_layer_init_for_window(m_Window);

    switch (m_Layer)
    {
    case Layer::Top: gtk_layer_set_layer(m_Window, GTK_LAYER_SHELL_LAYER_TOP); break;
    case Layer::Overlay: gtk_layer_set_layer(m_Window, GTK_LAYER_SHELL_LAYER_OVERLAY); break;
    }

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

    gtk_main();
}

void Window::Close()
{
    gtk_widget_hide((GtkWidget*)m_Window);
    gtk_main_quit();
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
    GdkRectangle rect{};
    gdk_monitor_get_geometry(m_Monitor, &rect);
    return rect.width;
}

int Window::GetHeight() const
{
    GdkRectangle rect{};
    gdk_monitor_get_geometry(m_Monitor, &rect);
    return rect.height;
}

void Window::MonitorAdded(GdkDisplay* display, GdkMonitor* mon)
{
    LOG("Window: Monitor added: " << mon);
    if (!m_Monitor)
    {
        LOG("Window: Activating window");
        gtk_layer_set_monitor(m_Window, mon);
        m_Monitor = mon;
        gtk_widget_show_all((GtkWidget*)m_Window);
    }
}

void Window::MonitorRemoved(GdkDisplay* display, GdkMonitor* mon)
{
    LOG("Window: Monitor removed: " << mon);
    if (mon == m_Monitor)
    {
        // Hide the window, so it doesn't get rendered on an invalid monitor
        gtk_widget_hide((GtkWidget*)m_Window);
        // Notify gtk layer shell and redisplay window
        gtk_layer_set_monitor(m_Window, nullptr);
        m_Monitor = gtk_layer_get_monitor(m_Window);
        LOG("Window: New Monitor: " << m_Monitor);
        if (m_Monitor)
            gtk_widget_show_all((GtkWidget*)m_Window);
    }
}

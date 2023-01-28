#include "Window.h"
#include "Common.h"

#include <tuple>
#include <fstream>

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>

Window::Window(std::unique_ptr<Widget>&& mainWidget, int32_t monitor) : m_MainWidget(std::move(mainWidget)), m_Monitor(monitor) {}

Window::~Window()
{
    if (m_App)
    {
        g_object_unref(m_App);
        m_App = nullptr;
    }
}

void Window::LoadCSS(GtkCssProvider* provider)
{
    struct CSSDir
    {
        std::string xdgEnv;
        std::string fallbackPath;
        std::string relPath;
    };
    std::string home = getenv("HOME");
    std::array<CSSDir, 4> locations = {
        CSSDir{"XDG_CONFIG_HOME", home + "/.config",      "/gBar/style.css"}, // Local config
        CSSDir{"XDG_DATA_HOME",   home + "/.local/share", "/gBar/style.css"}, // local user install
        CSSDir{"",                "/usr/local/share",     "/gBar/style.css"}, // local all install
        CSSDir{"",                "/usr/share",           "/gBar/style.css"}, // package manager all install
    };

    GError* err = nullptr;
    for (auto& dir : locations)
    {
        const char* xdgConfig = dir.xdgEnv.size() ? getenv(dir.xdgEnv.c_str()) : nullptr;
        std::string file;
        if (xdgConfig)
        {
            file = (std::string(xdgConfig) + dir.relPath).c_str();
        }
        else
        {
            file = (dir.fallbackPath + dir.relPath).c_str();
        }
        if (!std::ifstream(file).is_open())
        {
            LOG("Info: No CSS found in " << dir.fallbackPath);
            continue;
        }

        gtk_css_provider_load_from_path(provider, file.c_str(), &err);

        if (!err)
        {
            LOG("CSS found and loaded successfully!");
            return;
        }
        LOG("Warning: Failed loading config for " << dir.fallbackPath << ", trying next one!");
        // Log any errors
        LOG(err->message);
        g_error_free(err);
    }
    ASSERT(false, "No CSS file found!");
}

void Window::Run(int argc, char** argv)
{
    gtk_init(&argc, &argv);

    // Style
    GtkCssProvider* cssprovider = gtk_css_provider_new();
    LoadCSS(cssprovider);

    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), (GtkStyleProvider*)cssprovider, GTK_STYLE_PROVIDER_PRIORITY_USER);

    m_Window = (GtkWindow*)gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_layer_init_for_window(m_Window);
    gtk_layer_set_layer(m_Window, GTK_LAYER_SHELL_LAYER_TOP);
    if (m_Exclusive)
        gtk_layer_auto_exclusive_zone_enable(m_Window);

    GdkDisplay* defaultDisplay = gdk_display_get_default();
    ASSERT(defaultDisplay != nullptr, "Cannot get display!");
    GdkMonitor* selectedMon = nullptr;
    if (m_Monitor != -1)
    {
        selectedMon = gdk_display_get_monitor(defaultDisplay, m_Monitor);
        ASSERT(selectedMon != nullptr, "Cannot get monitor!");
        gtk_layer_set_monitor(m_Window, selectedMon);
    }

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

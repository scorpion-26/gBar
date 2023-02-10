#include "CSS.h"
#include "Common.h"

#include <string>
#include <array>
#include <fstream>

namespace CSS
{
    static GtkCssProvider* sProvider;

    void Load()
    {
        sProvider = gtk_css_provider_new();

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

            gtk_css_provider_load_from_path(sProvider, file.c_str(), &err);

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

    GtkCssProvider* GetProvider()
    {
        return sProvider;
    }
}

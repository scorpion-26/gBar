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

        std::vector<std::string> locations;
        const char* home = std::getenv("HOME");
        const char* env;

        if ((env = std::getenv("XDG_CONFIG_HOME")) && strlen(env))
        {
            locations.push_back(env);
        }
        else if (home)
        {
            std::string s(home);
            locations.push_back(s + "/.config");
        }

        if ((env = std::getenv("XDG_DATA_HOME")) && strlen(env))
        {
            locations.push_back(env);
        }
        else if (home)
        {
            std::string s(home);
            locations.push_back(s + "/.local/share");
        }

        if ((env = std::getenv("XDG_DATA_DIRS")))
        {
            std::stringstream ss(env);
            std::string dir;
            while (std::getline(ss, dir, ':'))
                locations.push_back(dir);
        }
        else
        {
            locations.push_back("/usr/local/share");
            locations.push_back("/usr/share");
        }

        GError* err = nullptr;
        for (auto& dir : locations)
        {
            std::string file = dir + "/gBar/style.css";

            if (!std::ifstream(file).is_open())
            {
                LOG("Info: No CSS found in " << dir);
                continue;
            }

            gtk_css_provider_load_from_path(sProvider, file.c_str(), &err);

            if (!err)
            {
                LOG("CSS found and loaded successfully!");
                return;
            }
            LOG("Warning: Failed loading config for " << dir << ", trying next one!");
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

#include "CSS.h"
#include "Common.h"

#include <string>
#include <array>
#include <fstream>

#include <sass.h>

namespace CSS
{
    static GtkCssProvider* sProvider;

    bool CompileAndLoadSCSS(const std::string& scssFile)
    {
        if (!std::ifstream(scssFile).is_open())
        {
            LOG("Warning: Couldn't open " << scssFile);
            return false;
        }

        LOG("Info: Compiling " << scssFile);
        Sass_File_Context* ctx = sass_make_file_context(scssFile.c_str());
        Sass_Context* ctxout = sass_file_context_get_context(ctx);
        sass_compile_file_context(ctx);
        if (sass_context_get_error_status(ctxout))
        {
            LOG("Error compiling SCSS: " << sass_context_get_error_message(ctxout));
            return false;
        }

        std::string data = sass_context_get_output_string(ctxout);
        GError* err = nullptr;
        gtk_css_provider_load_from_data(sProvider, data.c_str(), data.length(), &err);
        if (err != nullptr)
        {
            LOG("Error loading compiled SCSS: " << err->message);
            g_error_free(err);
            err = nullptr;
            return false;
        }

        sass_delete_file_context(ctx);
        return true;
    }

    bool LoadCSS(const std::string& cssFile)
    {
        if (!std::ifstream(cssFile).is_open())
        {
            LOG("Warning: Couldn't open " << cssFile);
            return false;
        }

        LOG("Info: Loading " << cssFile);
        GError* err = nullptr;
        gtk_css_provider_load_from_path(sProvider, cssFile.c_str(), &err);
        if (err != nullptr)
        {
            LOG("Error loading CSS: " << err->message);
            g_error_free(err);
            return false;
        }
        return true;
    }

    void Load(const std::string& overrideConfigLocation)
    {
        sProvider = gtk_css_provider_new();

        std::vector<std::string> locations;
        const char* home = std::getenv("HOME");

        if (overrideConfigLocation != "")
        {
            locations.push_back(overrideConfigLocation);
        }

        const char* configHome = std::getenv("XDG_CONFIG_HOME");
        if (configHome && strlen(configHome) != 0)
        {
            locations.push_back(std::string(configHome) + "/gBar");
        }
        else if (home)
        {
            locations.push_back(std::string(home) + "/.config/gBar");
        }

        const char* dataHome = std::getenv("XDG_DATA_HOME");
        if (dataHome && strlen(dataHome) != 0)
        {
            locations.push_back(std::string(dataHome) + "/gBar");
        }
        else if (home)
        {
            locations.push_back(std::string(home) + "/.local/share/gBar");
        }

        const char* dataDirs = std::getenv("XDG_DATA_DIRS");
        if (dataDirs && strlen(dataDirs) != 0)
        {
            std::stringstream ss(dataDirs);
            std::string dir;
            while (std::getline(ss, dir, ':'))
                locations.push_back(dir + "/gBar");
        }
        else
        {
            locations.push_back("/usr/local/share/gBar");
            locations.push_back("/usr/share/gBar");
        }

        for (auto& dir : locations)
        {
            if (CompileAndLoadSCSS(dir + "/style.scss"))
            {
                LOG("SCSS found and loaded successfully!");
                return;
            }
            else
            {
                LOG("Warning: Failed loading SCSS, falling back to CSS!");
            }

            if (LoadCSS(dir + "/style.css"))
            {
                LOG("CSS found and loaded successfully!");
                return;
            }
        }
        ASSERT(false, "No CSS file found!");
    }

    GtkCssProvider* GetProvider()
    {
        return sProvider;
    }
}

#include "Window.h"
#include "Common.h"
#include "System.h"
#include "Bar.h"
#include "AudioFlyin.h"

#include <gtk/gtk.h>
#include <gtk-layer-shell.h>
#include <cmath>
#include <cstdio>
#include <unistd.h>

const char* audioTmpFileOpen = "/tmp/gBar__audio";

int main(int argc, char** argv)
{
    System::Init();

    int32_t monitor = -1;
    if (argc >= 3)
    {
        monitor = atoi(argv[2]);
    }

    Window window;
    ASSERT(argc >= 2, "Too little arguments!");
    if (strcmp(argv[1], "bar") == 0)
    {
        Bar::Create(window, monitor);
    }
    else if (strcmp(argv[1], "audio") == 0)
    {
        if (access(audioTmpFileOpen, F_OK) != 0)
        {
            FILE* audioTempFile = fopen(audioTmpFileOpen, "w");
            AudioFlyin::Create(window, monitor);
            fclose(audioTempFile);
        }
        else
        {
            // Already open, close
            LOG("Audio already open");
            exit(0);
        }
    }

    window.Run(argc, argv);

    System::FreeResources();
    if (strcmp(argv[1], "audio") == 0)
    {
        remove(audioTmpFileOpen);
    }
    return 0;
}

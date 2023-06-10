#include "Window.h"
#include "Common.h"
#include "System.h"
#include "Bar.h"
#include "AudioFlyin.h"
#include "BluetoothDevices.h"
#include "Plugin.h"
#include "Config.h"

#include <cmath>
#include <cstdio>
#include <unistd.h>

const char* audioTmpFileOpen = "/tmp/gBar__audio";

static bool flyin = false;
void OpenAudioFlyin(Window& window, int32_t monitor, AudioFlyin::Type type)
{
    flyin = true;
    if (access(audioTmpFileOpen, F_OK) != 0)
    {
        FILE* audioTempFile = fopen(audioTmpFileOpen, "w");
        AudioFlyin::Create(window, monitor, type);
        fclose(audioTempFile);
    }
    else
    {
        // Already open, close
        LOG("Audio flyin already open");
        exit(0);
    }
}

int main(int argc, char** argv)
{
    System::Init();

    int32_t monitor = -1;
    if (argc >= 3)
    {
        monitor = atoi(argv[2]);
    }

    Window window(monitor);
    window.Init(argc, argv);
    ASSERT(argc >= 2, "Too little arguments!");
    if (strcmp(argv[1], "bar") == 0)
    {
        Bar::Create(window, monitor);
    }
    else if (strcmp(argv[1], "audio") == 0)
    {
        OpenAudioFlyin(window, monitor, AudioFlyin::Type::Speaker);
    }
    else if (strcmp(argv[1], "mic") == 0)
    {
        OpenAudioFlyin(window, monitor, AudioFlyin::Type::Microphone);
    }
#ifdef WITH_BLUEZ
    else if (strcmp(argv[1], "bluetooth") == 0)
    {
        if (RuntimeConfig::Get().hasBlueZ)
        {
            BluetoothDevices::Create(window, monitor);
        }
        else
        {
            LOG("Blutooth disabled, cannot open bluetooth widget!");
            exit(1);
        }
    }
#endif
    else
    {
        Plugin::LoadWidgetFromPlugin(argv[1], window, monitor);
    }

    window.Run();

    System::FreeResources();
    if (flyin)
    {
        remove(audioTmpFileOpen);
    }
    return 0;
}

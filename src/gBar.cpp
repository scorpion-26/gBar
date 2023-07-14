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

const char* audioTmpFilePath = "/tmp/gBar__audio";
const char* bluetoothTmpFilePath = "/tmp/gBar__bluetooth";

static bool tmpFileOpen = false;

void OpenAudioFlyin(Window& window, int32_t monitor, AudioFlyin::Type type)
{
    tmpFileOpen = true;
    if (access(audioTmpFilePath, F_OK) != 0)
    {
        FILE* audioTempFile = fopen(audioTmpFilePath, "w");
        AudioFlyin::Create(window, monitor, type);
        fclose(audioTempFile);
    }
    else
    {
        // Already open, close
        LOG("Audio flyin already open (/tmp/gBar__audio exists)! Exiting...");
        exit(0);
    }
}

void CloseTmpFiles(int sig)
{
    if (tmpFileOpen)
    {
        remove(audioTmpFilePath);
        remove(bluetoothTmpFilePath);
    }
    if (sig != 0)
        exit(1);
}

int main(int argc, char** argv)
{
    signal(SIGINT, CloseTmpFiles);
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
            if (access(bluetoothTmpFilePath, F_OK) != 0)
            {
                tmpFileOpen = true;
                FILE* bluetoothTmpFile = fopen(bluetoothTmpFilePath, "w");
                BluetoothDevices::Create(window, monitor);
                fclose(bluetoothTmpFile);
            }
            else
            {
                // Already open, close
                LOG("Bluetooth widget already open (/tmp/gBar__bluetooth exists)! Exiting...");
                exit(0);
            }
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
    CloseTmpFiles(0);
    return 0;
}

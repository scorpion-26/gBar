#pragma once
#include "System.h"
#include "Common.h"

#include <pulse/pulseaudio.h>
#include <stdlib.h>
#include <algorithm>

namespace PulseAudio
{

    static pa_mainloop* mainLoop;
    static pa_context* context;
    static uint32_t pending = 0;

    inline void FlushLoop()
    {
        while (pending)
        {
            pa_mainloop_iterate(mainLoop, 0, nullptr);
        }
    }

    inline void Init()
    {
        mainLoop = pa_mainloop_new();
        pa_mainloop_api* api = pa_mainloop_get_api(mainLoop);

        context = pa_context_new(api, "gBar PA context");
        int res = pa_context_connect(context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);

        auto stateCallback = [](pa_context* c, void*)
        {
            switch (pa_context_get_state(c))
            {
            case PA_CONTEXT_TERMINATED:
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_UNCONNECTED: ASSERT(false, "PA Callback error!"); break;
            case PA_CONTEXT_AUTHORIZING:
            case PA_CONTEXT_SETTING_NAME:
            case PA_CONTEXT_CONNECTING:
                // Don't care
                break;
            case PA_CONTEXT_READY: pending--; break;
            }
        };

        pa_context_set_state_callback(context, +stateCallback, nullptr);
        pending++;

        FlushLoop();

        ASSERT(res >= 0, "pa_context_connect failed!");
    }

    inline System::AudioInfo GetInfo()
    {
        const char* defaultSink = nullptr;
        System::AudioInfo info{};
        // 1. Get default sink
        auto serverInfo = [](pa_context*, const pa_server_info* info, void* sink)
        {
            pending--;
            *(const char**)sink = info->default_sink_name;
        };

        pa_context_get_server_info(context, +serverInfo, &defaultSink);
        pending++;

        FlushLoop();

        auto sinkInfo = [](pa_context*, const pa_sink_info* info, int, void* audioInfo)
        {
            if (!info)
                return;

            System::AudioInfo* out = (System::AudioInfo*)audioInfo;

            double vol = (double)pa_cvolume_avg(&info->volume) / (double)PA_VOLUME_NORM;
            out->volume = vol;
            out->muted = info->mute;

            pending--;
        };
        pa_context_get_sink_info_by_name(context, defaultSink, +sinkInfo, &info);

        pending++;
        FlushLoop();

        return info;
    }

    inline void SetVolume(double value)
    {
        double valClamped = std::clamp(value, 0., 1.);
        // I'm too lazy to implement the c api for this. Since it will only be called when needed and doesn't pipe, it shouldn't be a problem to
        // fallback for a command
        std::string cmd = "pamixer --set-volume " + std::to_string((uint32_t)(valClamped * 100));
        system(cmd.c_str());
    }

    inline void Shutdown()
    {
        pa_mainloop_free(mainLoop);
    }
}

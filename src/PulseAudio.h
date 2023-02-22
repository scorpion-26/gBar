#pragma once
#include "System.h"
#include "Common.h"

#include <cmath>
#include <pulse/pulseaudio.h>
#include <stdlib.h>
#include <algorithm>
#include <list>

namespace PulseAudio
{

    static pa_mainloop* mainLoop;
    static pa_context* context;
    static std::list<pa_operation*> pendingOperations;

    inline void FlushLoop()
    {
        while (pendingOperations.size() > 0)
        {
            pa_mainloop_iterate(mainLoop, 0, nullptr);
            // Remove done or cancelled operations
            pendingOperations.remove_if(
                [](pa_operation* op)
                {
                    pa_operation_state_t state = pa_operation_get_state(op);
                    if (state == PA_OPERATION_DONE)
                    {
                        pa_operation_unref(op);
                        return true;
                    }
                    else if (state == PA_OPERATION_CANCELLED)
                    {
                        LOG("Warning: Cancelled pa_operation!");
                        pa_operation_unref(op);
                        return true;
                    }
                    return false;
                });
        }
    }

    inline void Init()
    {
        mainLoop = pa_mainloop_new();
        pa_mainloop_api* api = pa_mainloop_get_api(mainLoop);

        context = pa_context_new(api, "gBar PA context");
        int res = pa_context_connect(context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);

        bool ready = false;
        auto stateCallback = [](pa_context* c, void* ready)
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
            case PA_CONTEXT_READY:
                LOG("PulseAudio: Context is ready!");
                *(bool*)ready = true;
                break;
            }
        };
        pa_context_set_state_callback(context, +stateCallback, &ready);

        while (!ready)
        {
            pa_mainloop_iterate(mainLoop, 0, nullptr);
        }

        ASSERT(res >= 0, "pa_context_connect failed!");
    }

    inline double PAVolumeToDouble(const pa_cvolume* volume)
    {
        double vol = (double)pa_cvolume_avg(volume) / (double)PA_VOLUME_NORM;
        // Just round to 1% precision, should be enough
        constexpr double precision = 0.01;
        double volRounded = std::round(vol * 1 / precision) * precision;
        return volRounded;
    }

    inline System::AudioInfo GetInfo()
    {
        struct ServerInfo
        {
            const char* defaultSink = nullptr;
            const char* defaultSource = nullptr;
        } serverInfo;

        System::AudioInfo info{};
        // 1. Get default sink
        auto getServerInfo = [](pa_context*, const pa_server_info* info, void* out)
        {
            if (!info)
                return;

            ServerInfo* serverInfo = (ServerInfo*)out;
            serverInfo->defaultSink = info->default_sink_name;
            serverInfo->defaultSource = info->default_source_name;
        };

        pa_operation* op = pa_context_get_server_info(context, +getServerInfo, &serverInfo);
        pa_operation_ref(op);
        pendingOperations.push_back(op);

        FlushLoop();

        auto sinkInfo = [](pa_context*, const pa_sink_info* info, int, void* audioInfo)
        {
            if (!info)
                return;

            System::AudioInfo* out = (System::AudioInfo*)audioInfo;

            double vol = PAVolumeToDouble(&info->volume);
            out->sinkVolume = vol;
            out->sinkMuted = info->mute;
        };
        if (serverInfo.defaultSink)
        {
            op = pa_context_get_sink_info_by_name(context, serverInfo.defaultSink, +sinkInfo, &info);
            pa_operation_ref(op);
            pendingOperations.push_back(op);

            FlushLoop();
        }

        auto sourceInfo = [](pa_context*, const pa_source_info* info, int, void* audioInfo)
        {
            if (!info)
                return;

            System::AudioInfo* out = (System::AudioInfo*)audioInfo;

            double vol = PAVolumeToDouble(&info->volume);
            out->sourceVolume = vol;
            out->sourceMuted = info->mute;
        };
        if (serverInfo.defaultSource)
        {
            op = pa_context_get_source_info_by_name(context, serverInfo.defaultSource, +sourceInfo, &info);
            pa_operation_ref(op);
            pendingOperations.push_back(op);

            FlushLoop();
        }

        return info;
    }

    inline void SetVolumeSink(double value)
    {
        double valClamped = std::clamp(value, 0., 1.);
        // I'm too lazy to implement the c api for this. Since it will only be called when needed and doesn't pipe, it shouldn't be a problem to
        // fallback for a command
        std::string cmd = "pamixer --set-volume " + std::to_string((uint32_t)(valClamped * 100));
        system(cmd.c_str());
    }

    inline void SetVolumeSource(double value)
    {
        double valClamped = std::clamp(value, 0., 1.);
        // I'm too lazy to implement the c api for this. Since it will only be called when needed and doesn't pipe, it shouldn't be a problem to
        // fallback for a command
        std::string cmd = "pamixer --default-source --set-volume " + std::to_string((uint32_t)(valClamped * 100));
        system(cmd.c_str());
    }

    inline void Shutdown()
    {
        pa_mainloop_free(mainLoop);
    }
}

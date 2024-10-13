#include "AudioFlyin.h"
#include "System.h"

namespace AudioFlyin
{
    namespace DynCtx
    {
        Type type;

        Window* win;
        Slider* slider;
        Text* icon;
        double sliderVal;
        bool muted = false;

        int32_t msOpen = 0;
        constexpr int32_t closeTime = 2000;
        constexpr int32_t height = 50;
        int32_t curCloseTime = closeTime;
        int32_t transitionTime = 50;

        void OnChangeVolume(Slider&, double value)
        {
            if (type == Type::Speaker)
            {
                System::SetVolumeSink(value);
            }
            else if (type == Type::Microphone)
            {
                System::SetVolumeSource(value);
            }
        }

        TimerResult Main(Box&)
        {
            System::AudioInfo info = System::GetAudioInfo();
            if (type == Type::Speaker)
            {
                if (sliderVal != info.sinkVolume || muted != info.sinkMuted)
                {
                    // Extend timer
                    curCloseTime = msOpen + closeTime;

                    sliderVal = info.sinkVolume;
                    slider->SetValue(info.sinkVolume);

                    muted = info.sinkMuted;
                    if (info.sinkMuted)
                    {
                        icon->SetText(Config::Get().speakerMutedIcon);
                    }
                    else
                    {
                        icon->SetText(Config::Get().speakerHighIcon);
                    }
                }
            }
            else if (type == Type::Microphone)
            {
                if (sliderVal != info.sourceVolume || muted != info.sourceMuted)
                {
                    // Extend timer
                    curCloseTime = msOpen + closeTime;

                    sliderVal = info.sourceVolume;
                    slider->SetValue(info.sourceVolume);

                    muted = info.sourceMuted;
                    if (info.sourceMuted)
                    {
                        icon->SetText(Config::Get().micMutedIcon);
                    }
                    else
                    {
                        icon->SetText(Config::Get().micHighIcon);
                    }
                }
            }

            msOpen++;

            if (Config::Get().manualFlyinAnimation)
            {
                auto marginFunction = [](int32_t x) -> int32_t
                {
                    // A inverted, cutoff 'V' shape
                    // Fly in -> hover -> fly out
                    double steepness = (double)height / (double)transitionTime;
                    return (int32_t)std::min(-std::abs((double)x - (double)curCloseTime / 2) * steepness + (double)curCloseTime / 2, (double)height);
                };
                win->SetMargin(Anchor::Bottom, marginFunction(msOpen));
            }

            if (msOpen >= curCloseTime)
            {
                win->Close();
            }
            return TimerResult::Ok;
        }
    }
    void WidgetAudio(Widget& parent)
    {
        auto slider = Widget::Create<Slider>();
        slider->SetOrientation(Orientation::Horizontal);
        slider->SetHorizontalTransform({100, true, Alignment::Fill});
        slider->SetInverted(true);
        if (DynCtx::type == Type::Speaker)
        {
            slider->SetClass("audio-volume");
        }
        else if (DynCtx::type == Type::Microphone)
        {
            slider->SetClass("mic-volume");
        }
        slider->OnValueChange(DynCtx::OnChangeVolume);
        slider->SetRange({0, 1, 0.01});
        DynCtx::slider = slider.get();

        auto icon = Widget::Create<Text>();
        icon->SetHorizontalTransform({-1, true, Alignment::Fill, 0, 8});
        if (DynCtx::type == Type::Speaker)
        {
            icon->SetClass("audio-icon");
            icon->SetText(Config::Get().speakerHighIcon);
        }
        else if (DynCtx::type == Type::Microphone)
        {
            icon->SetClass("mic-icon");
            icon->SetText(Config::Get().speakerMutedIcon);
        }

        DynCtx::icon = icon.get();

        parent.AddChild(std::move(slider));
        parent.AddChild(std::move(icon));
    }

    void Create(Window& window, UNUSED const std::string& monitor, Type type)
    {
        DynCtx::win = &window;
        DynCtx::type = type;
        auto mainWidget = Widget::Create<Box>();
        mainWidget->SetSpacing({8, false});
        mainWidget->SetVerticalTransform({16, true, Alignment::Fill});
        mainWidget->SetClass("bar");
        // We update the margin in the timer, so we need late dispatch.
        mainWidget->AddTimer<Box>(DynCtx::Main, 1, TimerDispatchBehaviour::LateDispatch);

        auto padding = Widget::Create<Box>();
        padding->SetHorizontalTransform({8, true, Alignment::Fill});
        mainWidget->AddChild(std::move(padding));

        WidgetAudio(*mainWidget);

        padding = Widget::Create<Box>();
        mainWidget->AddChild(std::move(padding));

        // We want the audio flyin on top of fullscreen windows
        window.SetLayerNamespace("gbar-audio");
        window.SetLayer(Layer::Overlay);
        window.SetExclusive(false);
        window.SetAnchor(Anchor::Bottom);
        window.SetMainWidget(std::move(mainWidget));

        if (!Config::Get().manualFlyinAnimation)
        {
            // Do the margin here, so we don't need to update it every frame
            window.SetMargin(Anchor::Bottom, DynCtx::height);
        }
    }
}

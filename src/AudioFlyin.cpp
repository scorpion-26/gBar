#include "AudioFlyin.h"
#include "System.h"

namespace AudioFlyin
{
    namespace DynCtx
    {
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
            System::SetVolume(value);
        }

        TimerResult Main(Box&)
        {
            System::AudioInfo info = System::GetAudioInfo();
            if (sliderVal != info.volume || muted != info.muted)
            {
                // Extend timer
                curCloseTime = msOpen + closeTime;

                sliderVal = info.volume;
                slider->SetValue(info.volume);

                muted = info.muted;
                if (info.muted)
                {
                    icon->SetText("ﱝ");
                }
                else
                {
                    icon->SetText("墳");
                }
            }

            msOpen++;
            auto marginFunction = [](int32_t x) -> int32_t
            {
                // A inverted, cutoff 'V' shape
                // Fly in -> hover -> fly out
                double steepness = (double)height / (double)transitionTime;
                return (int32_t)std::min(-std::abs((double)x - (double)curCloseTime / 2) * steepness + (double)curCloseTime / 2, (double)height);
            };
            win->SetMargin(Anchor::Bottom, marginFunction(msOpen));
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
        slider->SetClass("audio-volume");
        slider->OnValueChange(DynCtx::OnChangeVolume);
        slider->SetRange({0, 1, 0.01});
        DynCtx::slider = slider.get();

        auto icon = Widget::Create<Text>();
        icon->SetClass("audio-icon");
        icon->SetText("墳");
        DynCtx::icon = icon.get();

        parent.AddChild(std::move(slider));
        parent.AddChild(std::move(icon));
    }

    void Create(Window& window, int32_t monitor)
    {
        DynCtx::win = &window;
        auto mainWidget = Widget::Create<Box>();
        mainWidget->SetSpacing({8, false});
        mainWidget->SetVerticalTransform({16, true, Alignment::Fill});
        mainWidget->SetClass("bar");
        mainWidget->AddTimer<Box>(DynCtx::Main, 1);

        auto padding = Widget::Create<Box>();
        padding->SetHorizontalTransform({8, true, Alignment::Fill});
        mainWidget->AddChild(std::move(padding));

        WidgetAudio(*mainWidget);

        padding = Widget::Create<Box>();
        mainWidget->AddChild(std::move(padding));

        window = Window(std::move(mainWidget), monitor);
        window.SetExclusive(false);
        window.SetAnchor(Anchor::Bottom);
    }
}

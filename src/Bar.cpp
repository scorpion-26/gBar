#include "Bar.h"

#include "System.h"
#include "Common.h"
#include "Config.h"
#include "SNI.h"
#include <cmath>
#include <mutex>

namespace Bar
{
    enum Side
    {
        Left,
        Right,
        Center
    };

    Alignment SideToAlignment(Side side)
    {
        switch (side)
        {
        case Left: return Alignment::Left;
        case Right: return Alignment::Right;
        case Center: return Alignment::Fill;
        }
        return Alignment::Right;
    }

    TransitionType SideToDefaultTransition(Side side)
    {
        switch (side)
        {
        case Right:
        case Center: return TransitionType::SlideLeft;
        case Left: return TransitionType::SlideRight;
        }
        return TransitionType::SlideLeft;
    }

    bool RotatedIcons()
    {
        return (Config::Get().location == 'L' || Config::Get().location == 'R') && Config::Get().iconsAlwaysUp;
    }

    static std::string monitor;

    namespace DynCtx
    {
        constexpr uint32_t updateTime = 1000;
        constexpr uint32_t updateTimeFast = 100;

        static Revealer* powerBoxRevealer;
        static void PowerBoxEvent(EventBox&, bool hovered)
        {
            powerBoxRevealer->SetRevealed(hovered);
        }

        static Text* cpuText;
        static TimerResult UpdateCPU(Sensor& sensor)
        {
            double usage = System::GetCPUUsage();
            double temp = System::GetCPUTemp();

            std::string text = "CPU: " + Utils::ToStringPrecision(usage * 100, "%0.1f") + "% " + Utils::ToStringPrecision(temp, "%0.1f") + "°C";
            if (Config::Get().sensorTooltips)
            {
                sensor.SetTooltip(text);
            }
            else
            {
                cpuText->SetText(text);
            }
            sensor.SetValue(usage);
            return TimerResult::Ok;
        }

        static Text* batteryText;
        static bool wasCharging = false;
        static TimerResult UpdateBattery(Sensor& sensor)
        {
            double percentage = System::GetBatteryPercentage();

            std::string text = "Battery: " + Utils::ToStringPrecision(percentage * 100, "%0.1f") + "%";
            if (Config::Get().sensorTooltips)
            {
                sensor.SetTooltip(text);
            }
            else
            {
                batteryText->SetText(text);
            }
            sensor.SetValue(percentage);

            bool isCharging = System::IsBatteryCharging();
            if (isCharging && !wasCharging && sensor.Get() != nullptr)
            {
                sensor.AddClass("battery-charging");
                if (batteryText)
                    batteryText->AddClass("battery-charging");
                wasCharging = true;
            }
            else if (!isCharging && wasCharging)
            {
                sensor.RemoveClass("battery-charging");
                if (batteryText)
                    batteryText->RemoveClass("battery-charging");
                wasCharging = false;
            }

            // Add warning if color falls below threshold
            if (!isCharging && percentage * 100 <= Config::Get().batteryWarnThreshold)
            {
                sensor.AddClass("battery-warning");
                if (batteryText)
                    batteryText->AddClass("battery-warning");
            }
            else
            {
                sensor.RemoveClass("battery-warning");
                if (batteryText)
                    batteryText->RemoveClass("battery-warning");
            }
            return TimerResult::Ok;
        }

        static Text* ramText;
        static TimerResult UpdateRAM(Sensor& sensor)
        {
            System::RAMInfo info = System::GetRAMInfo();
            double used = info.totalGiB - info.freeGiB;
            double usedPercent = used / info.totalGiB;

            std::string text = "RAM: " + Utils::ToStringPrecision(used, "%0.2f") + "GiB/" + Utils::ToStringPrecision(info.totalGiB, "%0.2f") + "GiB";
            if (Config::Get().sensorTooltips)
            {
                sensor.SetTooltip(text);
            }
            else
            {
                ramText->SetText(text);
            }
            sensor.SetValue(usedPercent);
            return TimerResult::Ok;
        }

#if defined WITH_NVIDIA || defined WITH_AMD
        static Text* gpuText;
        static TimerResult UpdateGPU(Sensor& sensor)
        {
            System::GPUInfo info = System::GetGPUInfo();

            std::string text = "GPU: " + Utils::ToStringPrecision(info.utilisation, "%0.1f") + "% " +
                               Utils::ToStringPrecision(info.coreTemp, "%0.1f") + "°C";
            if (Config::Get().sensorTooltips)
            {
                sensor.SetTooltip(text);
            }
            else
            {
                gpuText->SetText(text);
            }
            sensor.SetValue(info.utilisation / 100);
            return TimerResult::Ok;
        }

        static Text* vramText;
        static TimerResult UpdateVRAM(Sensor& sensor)
        {
            System::VRAMInfo info = System::GetVRAMInfo();

            std::string text = "VRAM: " + Utils::ToStringPrecision(info.usedGiB, "%0.2f") + "GiB/" +
                               Utils::ToStringPrecision(info.totalGiB, "%0.2f") + "GiB";
            if (Config::Get().sensorTooltips)
            {
                sensor.SetTooltip(text);
            }
            else
            {
                vramText->SetText(text);
            }
            sensor.SetValue(info.usedGiB / info.totalGiB);
            return TimerResult::Ok;
        }
#endif

        static Text* diskText;
        static TimerResult UpdateDisk(Sensor& sensor)
        {
            System::DiskInfo info = System::GetDiskInfo();

            std::string text = "Disk " + info.partition + ": " + Utils::ToStringPrecision(info.usedGiB, "%0.2f") + "GiB/" +
                               Utils::ToStringPrecision(info.totalGiB, "%0.2f") + "GiB";
            if (Config::Get().sensorTooltips)
            {
                sensor.SetTooltip(text);
            }
            else
            {
                diskText->SetText(text);
            }
            sensor.SetValue(info.usedGiB / info.totalGiB);
            return TimerResult::Ok;
        }

#ifdef WITH_BLUEZ
        static Button* btIconText;
        static Text* btDevText;
        static TimerResult UpdateBluetooth(Box&)
        {
            System::BluetoothInfo info = System::GetBluetoothInfo();
            if (info.defaultController.empty())
            {
                btIconText->SetClass("bt-label-off");
                btIconText->SetText(Config::Get().btOffIcon);
                btDevText->SetText("");
            }
            else if (info.devices.empty())
            {
                btIconText->SetClass("bt-label-on");
                btIconText->SetText(Config::Get().btOnIcon);
                btDevText->SetText("");
            }
            else
            {
                btIconText->SetClass("bt-label-connected");
                btIconText->SetText(Config::Get().btConnectedIcon);
                std::string btDev;
                std::string tooltip;
                for (auto& dev : info.devices)
                {
                    if (!dev.connected)
                        continue;
                    std::string ico = System::BTTypeToIcon(dev);
                    tooltip += dev.name + " & ";
                    btDev += ico;

                    if (RotatedIcons())
                    {
                        btDev += "\n";
                    }
                }
                // Delete last delim
                if (tooltip.size())
                    tooltip.erase(tooltip.end() - 3, tooltip.end());
                if (btDev.size() && btDev.back() == '\n')
                    btDev.erase(btDev.end() - 1);

                btDevText->SetTooltip(tooltip);
                btDevText->SetText(std::move(btDev));
            }
            return TimerResult::Ok;
        }

        void OnBTClick(Button&)
        {
            System::OpenBTWidget();
        }
#endif

        static std::mutex packageTextLock;
        static TimerResult UpdatePackages(Text& text)
        {
            System::GetOutdatedPackagesAsync(
                [&](uint32_t numOutdatedPackages)
                {
                    packageTextLock.lock();
                    if (numOutdatedPackages)
                    {
                        text.SetText(Config::Get().packageOutOfDateIcon);
                        text.SetVisible(true);
                        text.SetClass("package-outofdate");
                        text.SetTooltip("Updates available! (" + std::to_string(numOutdatedPackages) + " packages)");
                    }
                    else
                    {
                        text.SetText("");
                        text.SetVisible(false);
                        text.SetClass("package-empty");
                        text.SetTooltip("");
                    }
                    packageTextLock.unlock();
                });
            return TimerResult::Ok;
        }

        Widget* audioSlider;
        Widget* micSlider;
        Text* audioIcon;
        Text* micIcon;
        void OnChangeVolumeSink(Slider&, double value)
        {
            System::SetVolumeSink(value);
        }

        void OnChangeVolumeSource(Slider&, double value)
        {
            System::SetVolumeSource(value);
        }

        // For text
        double audioVolume = 0;
        void OnChangeVolumeSinkDelta(double delta)
        {
            audioVolume += delta;
            audioVolume = std::clamp(audioVolume, 0.0, 1.0);
            System::SetVolumeSink(audioVolume);
        }

        double micVolume = 0;
        void OnChangeVolumeSourceDelta(double delta)
        {
            micVolume += delta;
            micVolume = std::clamp(micVolume, 0.0, 1.0);
            System::SetVolumeSource(micVolume);
        }

        TimerResult UpdateAudio(Widget&)
        {
            System::AudioInfo info = System::GetAudioInfo();
            if (Config::Get().audioNumbers)
            {
                audioVolume = info.sinkVolume;
                ((Text*)audioSlider)->SetText(Utils::ToStringPrecision(info.sinkVolume * 100, "%0.0f") + "%");
            }
            else
            {
                ((Slider*)audioSlider)->SetValue(info.sinkVolume);
            }
            if (info.sinkMuted)
            {
                audioIcon->SetText(Config::Get().speakerMutedIcon);
            }
            else
            {
                audioIcon->SetText(Config::Get().speakerHighIcon);
            }
            if (Config::Get().audioInput)
            {
                if (Config::Get().audioNumbers)
                {
                    micVolume = info.sourceVolume;
                    ((Text*)micSlider)->SetText(Utils::ToStringPrecision(info.sourceVolume * 100, "%0.0f") + "%");
                }
                else
                {
                    ((Slider*)micSlider)->SetValue(info.sinkVolume);
                }
                if (info.sourceMuted)
                {
                    micIcon->SetText(Config::Get().micMutedIcon);
                }
                else
                {
                    micIcon->SetText(Config::Get().micHighIcon);
                }
            }
            return TimerResult::Ok;
        }

        Text* networkText;
        TimerResult UpdateNetwork(NetworkSensor& sensor)
        {
            double bpsUp = System::GetNetworkBpsUpload(updateTime / 1000.0);
            double bpsDown = System::GetNetworkBpsDownload(updateTime / 1000.0);

            std::string upload = Utils::StorageUnitDynamic(bpsUp, "%0.1f%s");
            std::string download = Utils::StorageUnitDynamic(bpsDown, "%0.1f%s");

            std::string text = Config::Get().networkAdapter + ": " + upload + " Up/" + download + " Down";
            if (Config::Get().sensorTooltips)
            {
                sensor.SetTooltip(text);
            }
            else
            {
                networkText->SetText(Config::Get().networkAdapter + ": " + upload + " Up/" + download + " Down");
            }

            sensor.SetUp(bpsUp);
            sensor.SetDown(bpsDown);

            return TimerResult::Ok;
        }

        TimerResult UpdateTime(Text& text)
        {
            text.SetText(System::GetTime());
            return TimerResult::Ok;
        }

#ifdef WITH_WORKSPACES
        static std::vector<Button*> workspaces;
        TimerResult UpdateWorkspaces(Box&)
        {
            System::PollWorkspaces(monitor, workspaces.size());
            for (size_t i = 0; i < workspaces.size(); i++)
            {
                switch (System::GetWorkspaceStatus(i + 1))
                {
                case System::WorkspaceStatus::Dead: workspaces[i]->SetClass("ws-dead"); break;
                case System::WorkspaceStatus::Inactive: workspaces[i]->SetClass("ws-inactive"); break;
                case System::WorkspaceStatus::Visible: workspaces[i]->SetClass("ws-visible"); break;
                case System::WorkspaceStatus::Current: workspaces[i]->SetClass("ws-current"); break;
                case System::WorkspaceStatus::Active: workspaces[i]->SetClass("ws-active"); break;
                }
                workspaces[i]->SetText(System::GetWorkspaceSymbol(i));
            }
            return TimerResult::Ok;
        }

        void ScrollWorkspaces(EventBox&, ScrollDirection direction)
        {
            switch (direction)
            {
            case ScrollDirection::Up:
                if (Config::Get().workspaceScrollInvert)
                    System::GotoNextWorkspace('+');
                else
                    System::GotoNextWorkspace('-');
                break;
            case ScrollDirection::Down:
                if (Config::Get().workspaceScrollInvert)
                    System::GotoNextWorkspace('-');
                else
                    System::GotoNextWorkspace('+');
                break;
            default: break;
            }
        }
#endif
    }

    void WidgetSensor(Widget& parent, TimerCallback<Sensor>&& callback, const std::string& sensorName, Text*& textPtr, Side side)
    {
        auto eventBox = Widget::Create<EventBox>();
        Utils::SetTransform(*eventBox, {-1, false, SideToAlignment(side)});
        {
            auto box = Widget::Create<Box>();
            auto widgetClass = sensorName + "-widget";
            box->SetSpacing({0, false});
            box->SetClass(widgetClass);
            box->AddClass("widget");
            box->AddClass("sensor");
            box->SetOrientation(Utils::GetOrientation());
            {
                std::unique_ptr<Revealer> revealer = nullptr;
                if (!Config::Get().sensorTooltips)
                {
                    revealer = Widget::Create<Revealer>();
                    revealer->SetTransition({Utils::GetTransitionType(SideToDefaultTransition(side)), 500});
                    // Add event to eventbox for the revealer to open
                    eventBox->SetHoverFn(
                        [textRevealer = revealer.get()](EventBox&, bool hovered)
                        {
                            textRevealer->SetRevealed(hovered);
                        });
                    {
                        auto text = Widget::Create<Text>();
                        text->SetAngle(Utils::GetAngle());
                        auto textClass = sensorName + "-data-text";
                        text->SetClass(textClass);
                        // Since we don't know, on which side the text is, add padding to both sides.
                        // This creates double padding on the side opposite to the sensor.
                        // TODO: Remove that padding.
                        Utils::SetTransform(*text, {-1, true, Alignment::Fill, 6, 6});
                        textPtr = text.get();
                        revealer->AddChild(std::move(text));
                    }
                }

                auto sensor = Widget::Create<Sensor>();
                double angle = -90;
                switch (Config::Get().location)
                {
                case 'T':
                case 'B': angle = -90; break;
                case 'L':
                case 'R': angle = RotatedIcons() ? -90 : 0; break;
                }
                sensor->SetStyle({angle});
                auto sensorClass = sensorName + "-util-progress";
                sensor->SetClass(sensorClass);
                sensor->AddTimer<Sensor>(std::move(callback), DynCtx::updateTime);
                Utils::SetTransform(*sensor, {(int)Config::Get().sensorSize, true, Alignment::Fill});

                switch (side)
                {
                case Side::Right:
                case Side::Center:
                {
                    if (!Config::Get().sensorTooltips)
                        box->AddChild(std::move(revealer));
                    box->AddChild(std::move(sensor));
                    break;
                }
                case Side::Left:
                {
                    // Invert
                    box->AddChild(std::move(sensor));
                    if (!Config::Get().sensorTooltips)
                        box->AddChild(std::move(revealer));
                    break;
                }
                }
            }
            eventBox->AddChild(std::move(box));
        }

        parent.AddChild(std::move(eventBox));
    }

    // Handles in and out
    void WidgetAudio(Widget& parent, Side side)
    {
        enum class AudioType
        {
            Input,
            Output
        };
        auto widgetAudioVolume = [](Widget& parent, AudioType type)
        {
            if (Config::Get().audioNumbers)
            {
                auto eventBox = Widget::Create<EventBox>();
                auto text = Widget::Create<Text>();
                text->SetAngle(Utils::GetAngle());
                Utils::SetTransform(*text, {-1, true, Alignment::Fill});
                switch (type)
                {
                case AudioType::Input:
                    text->SetClass("mic-volume");
                    DynCtx::micSlider = text.get();
                    break;
                case AudioType::Output:
                    text->SetClass("audio-volume");
                    DynCtx::audioSlider = text.get();
                    break;
                }
                eventBox->SetScrollFn(
                    [type, text = text.get()](EventBox&, ScrollDirection direction)
                    {
                        double delta = (double)Config::Get().audioScrollSpeed / 100.;
                        delta *= direction == ScrollDirection::Down ? -1 : 1;
                        switch (type)
                        {
                        case AudioType::Input: DynCtx::OnChangeVolumeSourceDelta(delta); break;
                        case AudioType::Output: DynCtx::OnChangeVolumeSinkDelta(delta); break;
                        }
                    });
                eventBox->AddChild(std::move(text));
                parent.AddChild(std::move(eventBox));
            }
            else
            {
                auto slider = Widget::Create<Slider>();
                slider->SetOrientation(Utils::GetOrientation());
                Utils::SetTransform(*slider, {100, true, Alignment::Fill});
                slider->SetInverted(true);
                switch (type)
                {
                case AudioType::Input:
                    slider->SetClass("mic-volume");
                    slider->OnValueChange(DynCtx::OnChangeVolumeSource);
                    DynCtx::micSlider = slider.get();
                    break;
                case AudioType::Output:
                    slider->SetClass("audio-volume");
                    slider->OnValueChange(DynCtx::OnChangeVolumeSink);
                    DynCtx::audioSlider = slider.get();
                    break;
                }
                slider->SetRange({0, 1, 0.01});
                slider->SetScrollSpeed((double)Config::Get().audioScrollSpeed / 100.);

                parent.AddChild(std::move(slider));
            }
        };

        auto widgetAudioBody = [&widgetAudioVolume, side](Widget& parent, AudioType type)
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({8, false});
            box->AddClass("widget");
            switch (type)
            {
            case AudioType::Input: box->AddClass("mic"); break;
            case AudioType::Output: box->AddClass("audio"); break;
            }

            Utils::SetTransform(*box, {-1, false, SideToAlignment(side)});
            box->SetOrientation(Utils::GetOrientation());
            {
                auto icon = Widget::Create<Text>();
                icon->SetAngle(Utils::GetAngle());
                switch (type)
                {
                case AudioType::Input:
                    icon->SetClass("mic-icon");
                    icon->SetText(Config::Get().speakerHighIcon);
                    DynCtx::micIcon = icon.get();
                    break;
                case AudioType::Output:
                    icon->SetClass("audio-icon");
                    icon->SetText(Config::Get().micHighIcon);
                    if (!RotatedIcons())
                        Utils::SetTransform(*icon, {-1, true, Alignment::Fill, 0, 6});
                    DynCtx::audioIcon = icon.get();
                    break;
                }

                if (Config::Get().audioRevealer)
                {
                    EventBox& eventBox = (EventBox&)parent;
                    auto revealer = Widget::Create<Revealer>();
                    revealer->SetTransition({Utils::GetTransitionType(SideToDefaultTransition(side)), 500});
                    // Add event to eventbox for the revealer to open
                    eventBox.SetHoverFn(
                        [slideRevealer = revealer.get()](EventBox&, bool hovered)
                        {
                            slideRevealer->SetRevealed(hovered);
                        });
                    {
                        widgetAudioVolume(*revealer, type);
                    }

                    box->AddChild(std::move(revealer));
                }
                else
                {
                    // Straight forward
                    widgetAudioVolume(*box, type);
                }

                box->AddChild(std::move(icon));
            }
            parent.AddChild(std::move(box));
        };

        if (Config::Get().audioRevealer)
        {
            // Need an EventBox
            if (Config::Get().audioInput)
            {
                auto eventBox = Widget::Create<EventBox>();
                widgetAudioBody(*eventBox, AudioType::Input);
                parent.AddChild(std::move(eventBox));
            }
            // Need an EventBox
            auto eventBox = Widget::Create<EventBox>();
            widgetAudioBody(*eventBox, AudioType::Output);
            parent.AddChild(std::move(eventBox));
        }
        else
        {
            // Just invoke it.
            if (Config::Get().audioInput)
            {
                widgetAudioBody(parent, AudioType::Input);
            }
            widgetAudioBody(parent, AudioType::Output);
        }
        parent.AddTimer<Widget>(DynCtx::UpdateAudio, DynCtx::updateTimeFast);
    }

    void WidgetPackages(Widget& parent, Side)
    {
        auto text = Widget::Create<Text>();
        text->SetText("");
        text->SetVisible(false);
        text->SetClass("package-empty");
        text->AddClass("widget");
        text->SetAngle(Utils::GetAngle());
        text->AddTimer<Text>(DynCtx::UpdatePackages, 1000 * Config::Get().checkUpdateInterval, TimerDispatchBehaviour::ImmediateDispatch);

        // Fix alignment when icons are always upside
        if (RotatedIcons())
        {
            Utils::SetTransform(*text, Transform{}, 10, 0);
        }

        parent.AddChild(std::move(text));
    }

#ifdef WITH_BLUEZ
    void WidgetBluetooth(Widget& parent, Side side)
    {
        auto box = Widget::Create<Box>();
        box->SetSpacing({0, false});
        box->AddClass("widget");
        box->AddClass("bluetooth");
        box->SetOrientation(Utils::GetOrientation());
        Utils::SetTransform(*box, {-1, false, SideToAlignment(side)});
        {
            auto devText = Widget::Create<Text>();
            devText->SetAngle(Utils::GetAngle());
            DynCtx::btDevText = devText.get();
            devText->SetClass("bt-num");
            if (side == Side::Left && !RotatedIcons())
            {
                Utils::SetTransform(*devText, {-1, true, Alignment::Fill, 6, 0});
            }
            else if (RotatedIcons())
            {
                Utils::SetTransform(*devText, {}, 6, 0);
            }

            auto iconText = Widget::Create<Button>();
            iconText->OnClick(DynCtx::OnBTClick);
            iconText->SetAngle(Utils::GetAngle());
            DynCtx::btIconText = iconText.get();

            if (!RotatedIcons())
            {
                // Only add end margin when icon is rotated with the bar.
                Utils::SetTransform(*iconText, {-1, true, Alignment::Fill, 0, 6});
            }
            else
            {
                // Align it more properly
                Utils::SetTransform(*iconText, {}, 0, 6);
            }

            switch (side)
            {
            case Side::Right:
            case Side::Center:
            {
                box->AddChild(std::move(devText));
                box->AddChild(std::move(iconText));
                break;
            }
            case Side::Left:
            {
                // Invert
                box->AddChild(std::move(iconText));
                box->AddChild(std::move(devText));
                break;
            }
            }
        }
        box->AddTimer<Box>(DynCtx::UpdateBluetooth, DynCtx::updateTime);

        parent.AddChild(std::move(box));
    }
#endif

    void WidgetNetwork(Widget& parent, Side side)
    {
        auto eventBox = Widget::Create<EventBox>();
        Utils::SetTransform(*eventBox, {-1, false, SideToAlignment(side)});
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({0, false});
            box->AddClass("widget");
            box->AddClass("network");
            box->SetOrientation(Utils::GetOrientation());
            {
                auto revealer = Widget::Create<Revealer>();
                if (!Config::Get().sensorTooltips)
                {
                    revealer->SetTransition({Utils::GetTransitionType(SideToDefaultTransition(side)), 500});
                    // Add event to eventbox for the revealer to open
                    eventBox->SetHoverFn(
                        [textRevealer = revealer.get()](EventBox&, bool hovered)
                        {
                            textRevealer->SetRevealed(hovered);
                        });
                    {
                        auto text = Widget::Create<Text>();
                        text->SetClass("network-data-text");
                        text->SetAngle(Utils::GetAngle());
                        // Margins have the same problem as the WidgetSensor ones...
                        Utils::SetTransform(*text, {-1, true, Alignment::Fill, 6, 6});
                        DynCtx::networkText = text.get();
                        revealer->AddChild(std::move(text));
                    }
                }

                auto sensor = Widget::Create<NetworkSensor>();
                sensor->SetLimitUp({(double)Config::Get().minUploadBytes, (double)Config::Get().maxUploadBytes});
                sensor->SetLimitDown({(double)Config::Get().minDownloadBytes, (double)Config::Get().maxDownloadBytes});
                sensor->SetAngle(Utils::GetAngle());
                sensor->AddTimer<NetworkSensor>(DynCtx::UpdateNetwork, DynCtx::updateTime);
                Utils::SetTransform(*sensor, {(int)Config::Get().networkIconSize, true, Alignment::Fill});

                switch (side)
                {
                case Side::Right:
                case Side::Center:
                {
                    if (!Config::Get().sensorTooltips)
                        box->AddChild(std::move(revealer));
                    box->AddChild(std::move(sensor));
                    break;
                }
                case Side::Left:
                {
                    // Invert
                    box->AddChild(std::move(sensor));
                    if (!Config::Get().sensorTooltips)
                        box->AddChild(std::move(revealer));
                    break;
                }
                }
            }
            eventBox->AddChild(std::move(box));
        }

        parent.AddChild(std::move(eventBox));
    }

    void WidgetSensors(Widget& parent, Side side)
    {
        auto box = Widget::Create<Box>();
        box->SetClass("sensors");
        {
            WidgetSensor(*box, DynCtx::UpdateDisk, "disk", DynCtx::diskText, side);
#if defined WITH_NVIDIA || defined WITH_AMD
            if (RuntimeConfig::Get().hasNvidia || RuntimeConfig::Get().hasAMD)
            {
                WidgetSensor(*box, DynCtx::UpdateVRAM, "vram", DynCtx::vramText, side);
                WidgetSensor(*box, DynCtx::UpdateGPU, "gpu", DynCtx::gpuText, side);
            }
#endif
            WidgetSensor(*box, DynCtx::UpdateRAM, "ram", DynCtx::ramText, side);
            WidgetSensor(*box, DynCtx::UpdateCPU, "cpu", DynCtx::cpuText, side);
            // Only show battery percentage if battery folder is set and exists
            if (System::GetBatteryPercentage() >= 0)
            {
                WidgetSensor(*box, DynCtx::UpdateBattery, "battery", DynCtx::batteryText, side);
            }
        }
        parent.AddChild(std::move(box));
    }

    void WidgetPower(Widget& parent, Side side)
    {
        // TODO: Abstract this (Currently not DRY)
        static bool activatedExit = false;
        static bool activatedLock = false;
        static bool activatedSuspend = false;
        static bool activatedReboot = false;
        static bool activatedShutdown = false;

        auto setActivate = [](Button& button, bool& activeBool, bool activate)
        {
            if (activate)
            {
                button.AddClass("system-confirm");
                button.AddTimer<Button>(
                    [&](Button& button)
                    {
                        button.RemoveClass("system-confirm");
                        activeBool = false;
                        return TimerResult::Delete;
                    },
                    2000, TimerDispatchBehaviour::LateDispatch);
            }
            else
            {
                button.RemoveClass("system-confirm");
            }
            activeBool = activate;
        };

        auto eventBox = Widget::Create<EventBox>();
        eventBox->SetHoverFn(DynCtx::PowerBoxEvent);
        Utils::SetTransform(*eventBox, {-1, false, SideToAlignment(side)});
        {
            auto powerBox = Widget::Create<Box>();
            powerBox->SetClass("power-box");
            powerBox->AddClass("widget");
            powerBox->SetSpacing({0, false});
            powerBox->SetOrientation(Utils::GetOrientation());
            {
                auto revealer = Widget::Create<Revealer>();
                DynCtx::powerBoxRevealer = revealer.get();
                revealer->SetTransition({Utils::GetTransitionType(SideToDefaultTransition(side)), 500});
                {
                    auto powerBoxExpand = Widget::Create<Box>();
                    powerBoxExpand->SetClass("power-box-expand");
                    if (!RotatedIcons())
                    {
                        powerBoxExpand->SetSpacing({8, true});
                        Utils::SetTransform(*powerBoxExpand, {-1, true, Alignment::Fill, 0, 6});
                    }
                    powerBoxExpand->SetOrientation(Utils::GetOrientation());
                    {
                        auto exitButton = Widget::Create<Button>();
                        exitButton->SetClass("exit-button");
                        exitButton->SetText(Config::Get().exitIcon);
                        exitButton->SetAngle(Utils::GetAngle());
                        if (RotatedIcons())
                        {
                            // Align in center
                            Utils::SetTransform(*exitButton, Transform{}, 0, 2);
                        }
                        exitButton->OnClick(
                            [setActivate](Button& but)
                            {
                                if (activatedExit)
                                {
                                    System::ExitWM();
                                    setActivate(but, activatedExit, false);
                                }
                                else
                                {
                                    setActivate(but, activatedExit, true);
                                }
                            });

                        auto lockButton = Widget::Create<Button>();
                        lockButton->SetClass("lock-button");
                        lockButton->SetText(Config::Get().lockIcon);
                        lockButton->SetAngle(Utils::GetAngle());
                        if (RotatedIcons())
                        {
                            // Align in center
                            Utils::SetTransform(*lockButton, Transform{}, 0, 2);
                        }
                        lockButton->OnClick(
                            [setActivate](Button& but)
                            {
                                if (activatedLock)
                                {
                                    System::Lock();
                                    setActivate(but, activatedLock, false);
                                }
                                else
                                {
                                    setActivate(but, activatedLock, true);
                                }
                            });

                        auto sleepButton = Widget::Create<Button>();
                        sleepButton->SetClass("sleep-button");
                        sleepButton->SetText(Config::Get().sleepIcon);
                        sleepButton->SetAngle(Utils::GetAngle());
                        if (RotatedIcons())
                        {
                            // Align in center
                            Utils::SetTransform(*sleepButton, Transform{}, 0, 2);
                        }
                        sleepButton->OnClick(
                            [setActivate](Button& but)
                            {
                                if (activatedSuspend)
                                {
                                    System::Suspend();
                                    setActivate(but, activatedSuspend, false);
                                }
                                else
                                {
                                    setActivate(but, activatedSuspend, true);
                                }
                            });

                        auto rebootButton = Widget::Create<Button>();
                        rebootButton->SetClass("reboot-button");
                        rebootButton->SetText(Config::Get().rebootIcon);
                        rebootButton->SetAngle(Utils::GetAngle());

                        if (!RotatedIcons())
                        {
                            Utils::SetTransform(*rebootButton, {-1, false, Alignment::Fill, 0, 6});
                        }
                        else
                        {
                            // Align in center
                            Utils::SetTransform(*rebootButton, Transform{}, 0, 2);
                        }

                        rebootButton->OnClick(
                            [setActivate](Button& but)
                            {
                                if (activatedReboot)
                                {
                                    System::Reboot();
                                    setActivate(but, activatedReboot, false);
                                }
                                else
                                {
                                    setActivate(but, activatedReboot, true);
                                }
                            });

                        powerBoxExpand->AddChild(std::move(exitButton));
                        powerBoxExpand->AddChild(std::move(lockButton));
                        powerBoxExpand->AddChild(std::move(sleepButton));
                        powerBoxExpand->AddChild(std::move(rebootButton));
                    }

                    revealer->AddChild(std::move(powerBoxExpand));
                }

                auto powerButton = Widget::Create<Button>();
                powerButton->SetClass("power-button");
                powerButton->SetText(Config::Get().shutdownIcon);
                powerButton->SetAngle(Utils::GetAngle());

                Utils::SetTransform(*powerButton, {24, false, Alignment::Fill}, RotatedIcons() ? 10 : 0, 0);

                powerButton->OnClick(
                    [setActivate](Button& but)
                    {
                        if (activatedShutdown)
                        {
                            System::Shutdown();
                            setActivate(but, activatedShutdown, false);
                        }
                        else
                        {
                            setActivate(but, activatedShutdown, true);
                        }
                    });

                switch (side)
                {
                case Side::Right:
                case Side::Center:
                {
                    powerBox->AddChild(std::move(revealer));
                    powerBox->AddChild(std::move(powerButton));
                    break;
                }
                case Side::Left:
                {
                    // Invert
                    powerBox->AddChild(std::move(powerButton));
                    powerBox->AddChild(std::move(revealer));
                    break;
                }
                }
            }
            eventBox->AddChild(std::move(powerBox));
        }

        parent.AddChild(std::move(eventBox));
    }

#ifdef WITH_WORKSPACES
    void WidgetWorkspaces(Widget& parent, Side side)
    {
        auto eventBox = Widget::Create<EventBox>();
        eventBox->SetScrollFn(DynCtx::ScrollWorkspaces);
        Utils::SetTransform(*eventBox, {-1, false, SideToAlignment(side)});
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({8, true});
            box->AddClass("workspaces");
            box->AddClass("widget");
            box->SetOrientation(Utils::GetOrientation());
            {
                DynCtx::workspaces.resize(Config::Get().numWorkspaces);
                for (size_t i = 0; i < DynCtx::workspaces.size(); i++)
                {
                    auto workspace = Widget::Create<Button>();
                    Utils::SetTransform(*workspace, {8, false, Alignment::Fill});
                    workspace->OnClick(
                        [i](Button&)
                        {
                            System::GotoWorkspace((uint32_t)i + 1);
                        });
                    DynCtx::workspaces[i] = workspace.get();
                    box->AddChild(std::move(workspace));
                }
            }
            box->AddTimer<Box>(DynCtx::UpdateWorkspaces, DynCtx::updateTimeFast);
            eventBox->AddChild(std::move(box));
        }
        parent.AddChild(std::move(eventBox));
    }
#endif

    void WidgetTime(Widget& parent, Side side)
    {
        auto time = Widget::Create<Text>();
        Utils::SetTransform(*time, {-1, false, SideToAlignment(side)});
        time->SetAngle(Utils::GetAngle());
        time->SetClass("widget");
        time->AddClass("time-text");
        time->SetText("Uninitialized");
        time->AddTimer<Text>(DynCtx::UpdateTime, 1000);
        parent.AddChild(std::move(time));
    }

    void ChooseWidgetToDraw(const std::string& widgetName, Widget& parent, Side side)
    {
        if (widgetName == "Workspaces")
        {
#ifdef WITH_WORKSPACES
            if (RuntimeConfig::Get().hasWorkspaces)
            {
                WidgetWorkspaces(parent, side);
            }
#endif
            return;
        }
        if (widgetName == "Time")
        {
            WidgetTime(parent, side);
            return;
        }
        if (widgetName == "Tray")
        {
#ifdef WITH_SNI
            SNI::WidgetSNI(parent);
#endif
            return;
        }
        if (widgetName == "Packages")
        {
            WidgetPackages(parent, side);
            return;
        }
        if (widgetName == "Audio")
        {
            WidgetAudio(parent, side);
            return;
        }
        if (widgetName == "Bluetooth")
        {
#ifdef WITH_BLUEZ
            if (RuntimeConfig::Get().hasBlueZ)
                WidgetBluetooth(parent, side);
#endif
            return;
        }
        if (widgetName == "Network")
        {
            if (Config::Get().networkWidget && RuntimeConfig::Get().hasNet)
                WidgetNetwork(parent, side);
            return;
        }
        // Cheeky shorthand for all sensors
        if (widgetName == "Sensors")
        {
            WidgetSensors(parent, side);
            return;
        }
        if (widgetName == "Disk")
        {
            WidgetSensor(parent, DynCtx::UpdateDisk, "disk", DynCtx::diskText, side);
            return;
        }
        if (widgetName == "VRAM")
        {
#if defined WITH_NVIDIA || defined WITH_AMD
            if (RuntimeConfig::Get().hasNvidia || RuntimeConfig::Get().hasAMD)
                WidgetSensor(parent, DynCtx::UpdateVRAM, "vram", DynCtx::vramText, side);
            return;
#endif
        }
        if (widgetName == "GPU")
        {
#if defined WITH_NVIDIA || defined WITH_AMD
            if (RuntimeConfig::Get().hasNvidia || RuntimeConfig::Get().hasAMD)
                WidgetSensor(parent, DynCtx::UpdateGPU, "gpu", DynCtx::gpuText, side);
            return;
#endif
        }
        if (widgetName == "RAM")
        {
            WidgetSensor(parent, DynCtx::UpdateRAM, "ram", DynCtx::ramText, side);
            return;
        }
        if (widgetName == "CPU")
        {
            WidgetSensor(parent, DynCtx::UpdateCPU, "cpu", DynCtx::cpuText, side);
            return;
        }
        if (widgetName == "Battery")
        {
            // Only show battery percentage if battery folder is set and exists
            if (System::GetBatteryPercentage() >= 0)
                WidgetSensor(parent, DynCtx::UpdateBattery, "battery", DynCtx::batteryText, side);
            return;
        }
        if (widgetName == "Power")
        {
            WidgetPower(parent, side);
            return;
        }
        LOG("Warning: Unkwown widget name " << widgetName << "!"
                                            << "\n\tKnown names are: Workspaces, Time, Tray, Packages, Audio, Bluetooth, Network, Sensors, Disk, "
                                               "VRAM, GPU, RAM, CPU, Battery, Power");
    }

    void Create(Window& window, const std::string& monitorName)
    {
        ASSERT(!window.GetName().empty(), "Error: The bar requires a specified monitor. Use 'gBar bar <monitor>' instead!");
        monitor = monitorName;

        auto mainWidget = Widget::Create<Box>();
        mainWidget->SetOrientation(Utils::GetOrientation());
        mainWidget->SetSpacing({0, false});
        mainWidget->SetClass("bar");
        {
            // Calculate how much space we need have for the left widget.
            // The center widget will come directly after that.
            // This ensures that the center widget is centered.
            bool topToBottom = Config::Get().location == 'L' || Config::Get().location == 'R';
            int windowCenter = (topToBottom ? window.GetHeight() : window.GetWidth()) / 2;
            int endLeftWidgets = windowCenter - Config::Get().timeSpace / 2;

            if (!Config::Get().centerTime)
            {
                // Don't care if time is centered or not.
                endLeftWidgets = -1;
            }

            auto left = Widget::Create<Box>();
            left->SetSpacing({6, false});
            left->SetClass("left");
            left->SetOrientation(Utils::GetOrientation());
            // For centerTime the width of the left widget handles the centering.
            // For not centerTime we want to set it as much right as possible. So let this expand as much as possible.
            Utils::SetTransform(*left, {endLeftWidgets, !Config::Get().centerTime, Alignment::Left, 12, 0});

            for (auto& widget : Config::Get().widgetsLeft)
            {
                ChooseWidgetToDraw(widget, *left, Side::Left);
            }

            auto center = Widget::Create<Box>();
            center->SetClass("center");
            center->SetOrientation(Utils::GetOrientation());
            Utils::SetTransform(*center, {(int)Config::Get().timeSpace, false, Alignment::Left});
            center->SetSpacing({6, false});

            for (auto& widget : Config::Get().widgetsCenter)
            {
                ChooseWidgetToDraw(widget, *center, Side::Center);
            }

            auto right = Widget::Create<Box>();
            right->SetClass("right");
            right->SetSpacing({6, false});
            right->SetOrientation(Utils::GetOrientation());
            Utils::SetTransform(*right, {-1, true, Alignment::Right, 0, 10});

            for (auto& widget : Config::Get().widgetsRight)
            {
                ChooseWidgetToDraw(widget, *right, Side::Right);
            }

            mainWidget->AddChild(std::move(left));
            mainWidget->AddChild(std::move(center));
            mainWidget->AddChild(std::move(right));
        }

        Anchor anchor;
        switch (Config::Get().location)
        {
        case 'T': anchor = Anchor::Top | Anchor::Left | Anchor::Right; break;
        case 'B': anchor = Anchor::Bottom | Anchor::Left | Anchor::Right; break;
        case 'L': anchor = Anchor::Left | Anchor::Top | Anchor::Bottom; break;
        case 'R': anchor = Anchor::Right | Anchor::Top | Anchor::Bottom; break;
        default: LOG("Invalid location char \"" << Config::Get().location << "\"!"); anchor = Anchor::Top | Anchor::Left | Anchor::Right;
        }
        window.SetAnchor(anchor);
        window.SetMainWidget(std::move(mainWidget));
    }
}

#include "Bar.h"

#include "System.h"
#include "Common.h"
#include "Config.h"
#include "SNI.h"
#include <cmath>
#include <mutex>

namespace Bar
{
    static int32_t monitorID;

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

            std::string text = "Disk: " + Utils::ToStringPrecision(info.usedGiB, "%0.2f") + "GiB/" +
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
                btIconText->SetText("󰂲");
                btDevText->SetText("");
            }
            else if (info.devices.empty())
            {
                btIconText->SetClass("bt-label-on");
                btIconText->SetText("󰂯");
                btDevText->SetText("");
            }
            else
            {
                btIconText->SetClass("bt-label-connected");
                btIconText->SetText("󰂱");
                std::string btDev;
                std::string tooltip;
                for (auto& dev : info.devices)
                {
                    if (!dev.connected)
                        continue;
                    std::string ico = System::BTTypeToIcon(dev);
                    tooltip += dev.name + " & ";
                    btDev += ico;
                }
                // Delete last delim
                if (tooltip.size())
                    tooltip.erase(tooltip.end() - 3, tooltip.end());
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
                        text.SetText("󰏔 ");
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
                audioIcon->SetText("󰝟");
            }
            else
            {
                audioIcon->SetText("󰕾");
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
                    micIcon->SetText("󰍭");
                }
                else
                {
                    micIcon->SetText("󰍬");
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
            System::PollWorkspaces((uint32_t)monitorID, workspaces.size());
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

    void WidgetSensor(Widget& parent, TimerCallback<Sensor>&& callback, const std::string& sensorClass, const std::string& textClass, Text*& textPtr)
    {
        auto eventBox = Widget::Create<EventBox>();
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({0, false});
            Utils::SetTransform(*box, {-1, true, Alignment::Right});
            box->SetOrientation(Utils::GetOrientation());
            {
                if (!Config::Get().sensorTooltips)
                {
                    auto revealer = Widget::Create<Revealer>();
                    revealer->SetTransition({Utils::GetTransitionType(), 500});
                    // Add event to eventbox for the revealer to open
                    eventBox->SetHoverFn(
                        [textRevealer = revealer.get()](EventBox&, bool hovered)
                        {
                            textRevealer->SetRevealed(hovered);
                        });
                    {
                        auto text = Widget::Create<Text>();
                        text->SetClass(textClass);
                        text->SetAngle(Utils::GetAngle());
                        Utils::SetTransform(*text, {-1, true, Alignment::Fill, 0, 6});
                        textPtr = text.get();
                        revealer->AddChild(std::move(text));
                    }
                    box->AddChild(std::move(revealer));
                }

                auto sensor = Widget::Create<Sensor>();
                sensor->SetClass(sensorClass);
                double angle = -90;
                switch (Config::Get().location)
                {
                case 'T':
                case 'B': angle = -90; break;
                case 'L':
                case 'R': angle = 0; break;
                }
                sensor->SetStyle({angle});
                sensor->AddTimer<Sensor>(std::move(callback), DynCtx::updateTime);
                Utils::SetTransform(*sensor, {24, true, Alignment::Fill});

                box->AddChild(std::move(sensor));
            }
            eventBox->AddChild(std::move(box));
        }

        parent.AddChild(std::move(eventBox));
    }

    // Handles in and out
    void WidgetAudio(Widget& parent)
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

        auto widgetAudioBody = [&widgetAudioVolume](Widget& parent, AudioType type)
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({8, false});
            Utils::SetTransform(*box, {-1, true, Alignment::Right});
            box->SetOrientation(Utils::GetOrientation());
            {
                auto icon = Widget::Create<Text>();
                icon->SetAngle(Utils::GetAngle());
                switch (type)
                {
                case AudioType::Input:
                    icon->SetClass("mic-icon");
                    icon->SetText("󰍬");
                    DynCtx::micIcon = icon.get();
                    break;
                case AudioType::Output:
                    icon->SetClass("audio-icon");
                    icon->SetText("󰕾 ");
                    Utils::SetTransform(*icon, {-1, true, Alignment::Fill, 0, 6});
                    DynCtx::audioIcon = icon.get();
                    break;
                }

                if (Config::Get().audioRevealer)
                {
                    EventBox& eventBox = (EventBox&)parent;
                    auto revealer = Widget::Create<Revealer>();
                    revealer->SetTransition({Utils::GetTransitionType(), 500});
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

    void WidgetPackages(Widget& parent)
    {
        auto text = Widget::Create<Text>();
        text->SetText("");
        text->SetVisible(false);
        text->SetClass("package-empty");
        text->SetAngle(Utils::GetAngle());
        text->AddTimer<Text>(DynCtx::UpdatePackages, 1000 * Config::Get().checkUpdateInterval, TimerDispatchBehaviour::ImmediateDispatch);
        parent.AddChild(std::move(text));
    }

#ifdef WITH_BLUEZ
    void WidgetBluetooth(Widget& parent)
    {
        auto box = Widget::Create<Box>();
        box->SetSpacing({0, false});
        box->SetOrientation(Utils::GetOrientation());
        {
            auto devText = Widget::Create<Text>();
            devText->SetAngle(Utils::GetAngle());
            DynCtx::btDevText = devText.get();
            devText->SetClass("bt-num");

            auto iconText = Widget::Create<Button>();
            iconText->OnClick(DynCtx::OnBTClick);
            iconText->SetAngle(Utils::GetAngle());
            Utils::SetTransform(*iconText, {-1, true, Alignment::Fill, 0, 6});
            DynCtx::btIconText = iconText.get();

            box->AddChild(std::move(devText));
            box->AddChild(std::move(iconText));
        }
        box->AddTimer<Box>(DynCtx::UpdateBluetooth, DynCtx::updateTime);

        parent.AddChild(std::move(box));
    }
#endif

    void WidgetNetwork(Widget& parent)
    {
        auto eventBox = Widget::Create<EventBox>();
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({0, false});
            Utils::SetTransform(*box, {-1, true, Alignment::Right});
            box->SetOrientation(Utils::GetOrientation());
            {
                if (!Config::Get().sensorTooltips)
                {
                    auto revealer = Widget::Create<Revealer>();
                    revealer->SetTransition({Utils::GetTransitionType(), 500});
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
                        Utils::SetTransform(*text, {-1, true, Alignment::Fill, 0, 6});
                        DynCtx::networkText = text.get();
                        revealer->AddChild(std::move(text));
                    }
                    box->AddChild(std::move(revealer));
                }

                auto sensor = Widget::Create<NetworkSensor>();
                sensor->SetLimitUp({(double)Config::Get().minUploadBytes, (double)Config::Get().maxUploadBytes});
                sensor->SetLimitDown({(double)Config::Get().minDownloadBytes, (double)Config::Get().maxDownloadBytes});
                sensor->SetAngle(Utils::GetAngle());
                sensor->AddTimer<NetworkSensor>(DynCtx::UpdateNetwork, DynCtx::updateTime);
                Utils::SetTransform(*sensor, {24, true, Alignment::Fill});

                box->AddChild(std::move(sensor));
            }
            eventBox->AddChild(std::move(box));
        }

        parent.AddChild(std::move(eventBox));
    }

    void WidgetSensors(Widget& parent)
    {
        WidgetSensor(parent, DynCtx::UpdateDisk, "disk-util-progress", "disk-data-text", DynCtx::diskText);
#if defined WITH_NVIDIA || defined WITH_AMD
        if (RuntimeConfig::Get().hasNvidia || RuntimeConfig::Get().hasAMD)
        {
            WidgetSensor(parent, DynCtx::UpdateVRAM, "vram-util-progress", "vram-data-text", DynCtx::vramText);
            WidgetSensor(parent, DynCtx::UpdateGPU, "gpu-util-progress", "gpu-data-text", DynCtx::gpuText);
        }
#endif
        WidgetSensor(parent, DynCtx::UpdateRAM, "ram-util-progress", "ram-data-text", DynCtx::ramText);
        WidgetSensor(parent, DynCtx::UpdateCPU, "cpu-util-progress", "cpu-data-text", DynCtx::cpuText);
        // Only show battery percentage if battery folder is set and exists
        if (System::GetBatteryPercentage() >= 0)
        {
            WidgetSensor(parent, DynCtx::UpdateBattery, "battery-util-progress", "battery-data-text", DynCtx::batteryText);
        }
    }

    void WidgetPower(Widget& parent)
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
        {
            auto powerBox = Widget::Create<Box>();
            powerBox->SetClass("power-box");
            Utils::SetTransform(*powerBox, {-1, false, Alignment::Right});
            powerBox->SetSpacing({0, false});
            powerBox->SetOrientation(Utils::GetOrientation());
            {
                auto revealer = Widget::Create<Revealer>();
                DynCtx::powerBoxRevealer = revealer.get();
                revealer->SetTransition({Utils::GetTransitionType(), 500});
                {
                    auto powerBoxExpand = Widget::Create<Box>();
                    powerBoxExpand->SetClass("power-box-expand");
                    powerBoxExpand->SetSpacing({8, true});
                    powerBoxExpand->SetOrientation(Utils::GetOrientation());
                    Utils::SetTransform(*powerBoxExpand, {-1, true, Alignment::Fill, 0, 6});
                    {
                        auto exitButton = Widget::Create<Button>();
                        exitButton->SetClass("exit-button");
                        exitButton->SetText("󰗼");
                        exitButton->SetAngle(Utils::GetAngle());
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
                        lockButton->SetClass("sleep-button");
                        lockButton->SetText("");
                        lockButton->SetAngle(Utils::GetAngle());
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
                        sleepButton->SetText("󰏤");
                        sleepButton->SetAngle(Utils::GetAngle());
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
                        rebootButton->SetText("󰑐");
                        rebootButton->SetAngle(Utils::GetAngle());
                        Utils::SetTransform(*rebootButton, {-1, true, Alignment::Fill, 0, 6});
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
                powerButton->SetText(" ");
                powerButton->SetAngle(Utils::GetAngle());
                Utils::SetTransform(*powerButton, {24, true, Alignment::Fill});
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

                powerBox->AddChild(std::move(revealer));
                powerBox->AddChild(std::move(powerButton));
            }
            eventBox->AddChild(std::move(powerBox));
        }

        parent.AddChild(std::move(eventBox));
    }

#ifdef WITH_WORKSPACES
    void WidgetWorkspaces(Widget& parent)
    {
        auto eventBox = Widget::Create<EventBox>();
        eventBox->SetScrollFn(DynCtx::ScrollWorkspaces);
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({8, true});
            box->SetOrientation(Utils::GetOrientation());
            Utils::SetTransform(*box, {-1, true, Alignment::Left, 12, 0});
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

    void Create(Window& window, int32_t monitor)
    {
        monitorID = monitor;

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
            left->SetSpacing({0, false});
            left->SetOrientation(Utils::GetOrientation());
            // For centerTime the width of the left widget handles the centering.
            // For not centerTime we want to set it as much right as possible. So let this expand as much as possible.
            Utils::SetTransform(*left, {endLeftWidgets, !Config::Get().centerTime, Alignment::Left});
#ifdef WITH_WORKSPACES
            if (RuntimeConfig::Get().hasWorkspaces)
            {
                WidgetWorkspaces(*left);
            }
#endif

            auto center = Widget::Create<Box>();
            center->SetOrientation(Utils::GetOrientation());
            Utils::SetTransform(*center, {(int)Config::Get().timeSpace, false, Alignment::Left});
            {
                auto time = Widget::Create<Text>();
                Utils::SetTransform(*time, {-1, true, Alignment::Center});
                time->SetAngle(Utils::GetAngle());
                time->SetClass("time-text");
                time->SetText("Uninitialized");
                time->AddTimer<Text>(DynCtx::UpdateTime, 1000);
                center->AddChild(std::move(time));
            }

            auto right = Widget::Create<Box>();
            right->SetClass("right");
            right->SetSpacing({8, false});
            right->SetOrientation(Utils::GetOrientation());
            Utils::SetTransform(*right, {-1, true, Alignment::Right, 0, 10});
            {
#ifdef WITH_SNI
                SNI::WidgetSNI(*right);
#endif

                WidgetPackages(*right);

                WidgetAudio(*right);

#ifdef WITH_BLUEZ
                if (RuntimeConfig::Get().hasBlueZ)
                    WidgetBluetooth(*right);
#endif
                if (Config::Get().networkWidget && RuntimeConfig::Get().hasNet)
                    WidgetNetwork(*right);

                WidgetSensors(*right);

                WidgetPower(*right);
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

#include "Bar.h"

#include "System.h"
#include "Common.h"
#include "Config.h"

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

            cpuText->SetText("CPU: " + Utils::ToStringPrecision(usage * 100, "%0.1f") + "% " + Utils::ToStringPrecision(temp, "%0.1f") + "°C");
            sensor.SetValue(usage);
            return TimerResult::Ok;
        }

        static Text* batteryText;
        static TimerResult UpdateBattery(Sensor& sensor)
        {
            double percentage = System::GetBatteryPercentage();

            batteryText->SetText("Battery: " + Utils::ToStringPrecision(percentage * 100, "%0.1f") + "%");
            sensor.SetValue(percentage);
            return TimerResult::Ok;
        }

        static Text* ramText;
        static TimerResult UpdateRAM(Sensor& sensor)
        {
            System::RAMInfo info = System::GetRAMInfo();
            double used = info.totalGiB - info.freeGiB;
            double usedPercent = used / info.totalGiB;

            ramText->SetText("RAM: " + Utils::ToStringPrecision(used, "%0.2f") + "GiB/" + Utils::ToStringPrecision(info.totalGiB, "%0.2f") + "GiB");
            sensor.SetValue(usedPercent);
            return TimerResult::Ok;
        }

#if defined WITH_NVIDIA || defined WITH_AMD
        static Text* gpuText;
        static TimerResult UpdateGPU(Sensor& sensor)
        {
            System::GPUInfo info = System::GetGPUInfo();

            gpuText->SetText("GPU: " + Utils::ToStringPrecision(info.utilisation, "%0.1f") + "% " + Utils::ToStringPrecision(info.coreTemp, "%0.1f") +
                             "°C");
            sensor.SetValue(info.utilisation / 100);
            return TimerResult::Ok;
        }

        static Text* vramText;
        static TimerResult UpdateVRAM(Sensor& sensor)
        {
            System::VRAMInfo info = System::GetVRAMInfo();

            vramText->SetText("VRAM: " + Utils::ToStringPrecision(info.usedGiB, "%0.2f") + "GiB/" + Utils::ToStringPrecision(info.totalGiB, "%0.2f") +
                              "GiB");
            sensor.SetValue(info.usedGiB / info.totalGiB);
            return TimerResult::Ok;
        }
#endif

        static Text* diskText;
        static TimerResult UpdateDisk(Sensor& sensor)
        {
            System::DiskInfo info = System::GetDiskInfo();

            diskText->SetText("Disk: " + Utils::ToStringPrecision(info.usedGiB, "%0.2f") + "GiB/" + Utils::ToStringPrecision(info.totalGiB, "%0.2f") +
                              "GiB");
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
                btIconText->SetText("");
                btDevText->SetText("");
            }
            else if (info.devices.empty())
            {
                btIconText->SetClass("bt-label-on");
                btIconText->SetText("");
                btDevText->SetText("");
            }
            else
            {
                btIconText->SetClass("bt-label-connected");
                btIconText->SetText("");
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

        void OnChangeVolume(Slider&, double value)
        {
            System::SetVolume(value);
        }

        Text* audioIcon;
        TimerResult UpdateAudio(Slider& slider)
        {
            System::AudioInfo info = System::GetAudioInfo();
            slider.SetValue(info.volume);
            if (info.muted)
            {
                audioIcon->SetText("ﱝ");
            }
            else
            {
                audioIcon->SetText("墳");
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

            networkText->SetText(Config::Get().networkAdapter + ": " + upload + " Up/" + download + " Down");

            sensor.SetUp(bpsUp);
            sensor.SetDown(bpsDown);

            return TimerResult::Ok;
        }

        TimerResult UpdateTime(Text& text)
        {
            text.SetText(System::GetTime());
            return TimerResult::Ok;
        }

#ifdef WITH_HYPRLAND
        static std::array<Button*, 9> workspaces;
        TimerResult UpdateWorkspaces(Box&)
        {
            for (size_t i = 0; i < workspaces.size(); i++)
            {
                switch (System::GetWorkspaceStatus((uint32_t)monitorID, i + 1))
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
            box->SetHorizontalTransform({-1, true, Alignment::Right});
            {
                auto revealer = Widget::Create<Revealer>();
                revealer->SetTransition({TransitionType::SlideLeft, 500});
                // Add event to eventbox for the revealer to open
                eventBox->SetHoverFn(
                    [textRevealer = revealer.get()](EventBox&, bool hovered)
                    {
                        textRevealer->SetRevealed(hovered);
                    });
                {
                    auto text = Widget::Create<Text>();
                    text->SetClass(textClass);
                    textPtr = text.get();
                    revealer->AddChild(std::move(text));
                }

                auto sensor = Widget::Create<Sensor>();
                sensor->SetClass(sensorClass);
                sensor->AddTimer<Sensor>(std::move(callback), DynCtx::updateTime);
                sensor->SetHorizontalTransform({24, true, Alignment::Fill});

                box->AddChild(std::move(revealer));
                box->AddChild(std::move(sensor));
            }
            eventBox->AddChild(std::move(box));
        }

        parent.AddChild(std::move(eventBox));
    }

    void WidgetAudio(Widget& parent)
    {
        auto widgetAudioSlider = [](Widget& parent)
        {
            auto slider = Widget::Create<Slider>();
            slider->SetOrientation(Orientation::Horizontal);
            slider->SetHorizontalTransform({100, true, Alignment::Fill});
            slider->SetInverted(true);
            slider->SetClass("audio-volume");
            slider->AddTimer<Slider>(DynCtx::UpdateAudio, DynCtx::updateTimeFast);
            slider->OnValueChange(DynCtx::OnChangeVolume);
            slider->SetRange({0, 1, 0.01});
            slider->SetScrollSpeed((double)Config::Get().audioScrollSpeed / 100.);

            parent.AddChild(std::move(slider));
        };

        auto widgetAudioBody = [&widgetAudioSlider](Widget& parent)
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({8, false});
            box->SetHorizontalTransform({-1, true, Alignment::Right});
            {
                auto icon = Widget::Create<Text>();
                icon->SetClass("audio-icon");
                icon->SetText("墳");
                DynCtx::audioIcon = icon.get();

                if (Config::Get().audioRevealer)
                {
                    EventBox& eventBox = (EventBox&)parent;
                    auto revealer = Widget::Create<Revealer>();
                    revealer->SetTransition({TransitionType::SlideLeft, 500});
                    // Add event to eventbox for the revealer to open
                    eventBox.SetHoverFn(
                        [slideRevealer = revealer.get()](EventBox&, bool hovered)
                        {
                            slideRevealer->SetRevealed(hovered);
                        });
                    {
                        widgetAudioSlider(*revealer);
                    }

                    box->AddChild(std::move(revealer));
                }
                else
                {
                    // Straight forward
                    widgetAudioSlider(*box);
                }

                box->AddChild(std::move(icon));
            }
            parent.AddChild(std::move(box));
        };

        if (Config::Get().audioRevealer)
        {
            // Need an EventBox
            auto eventBox = Widget::Create<EventBox>();
            widgetAudioBody(*eventBox);
            parent.AddChild(std::move(eventBox));
        }
        else
        {
            // Just invoke it.
            widgetAudioBody(parent);
        }
    }

#ifdef WITH_BLUEZ
    void WidgetBluetooth(Widget& parent)
    {
        auto box = Widget::Create<Box>();
        box->SetSpacing({0, false});
        {
            auto devText = Widget::Create<Text>();
            DynCtx::btDevText = devText.get();
            devText->SetClass("bt-num");

            auto iconText = Widget::Create<Button>();
            iconText->OnClick(DynCtx::OnBTClick);
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
            box->SetHorizontalTransform({-1, true, Alignment::Right});
            {
                auto revealer = Widget::Create<Revealer>();
                revealer->SetTransition({TransitionType::SlideLeft, 500});
                // Add event to eventbox for the revealer to open
                eventBox->SetHoverFn(
                    [textRevealer = revealer.get()](EventBox&, bool hovered)
                    {
                        textRevealer->SetRevealed(hovered);
                    });
                {
                    auto text = Widget::Create<Text>();
                    text->SetClass("network-data-text");
                    DynCtx::networkText = text.get();
                    revealer->AddChild(std::move(text));
                }

                auto sensor = Widget::Create<NetworkSensor>();
                sensor->SetLimitUp({(double)Config::Get().minUploadBytes, (double)Config::Get().maxUploadBytes});
                sensor->SetLimitDown({(double)Config::Get().minDownloadBytes, (double)Config::Get().maxDownloadBytes});
                sensor->AddTimer<NetworkSensor>(DynCtx::UpdateNetwork, DynCtx::updateTime);
                sensor->SetHorizontalTransform({24, true, Alignment::Fill});

                box->AddChild(std::move(revealer));
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
        auto eventBox = Widget::Create<EventBox>();
        eventBox->SetHoverFn(DynCtx::PowerBoxEvent);
        {
            auto powerBox = Widget::Create<Box>();
            powerBox->SetClass("power-box");
            powerBox->SetHorizontalTransform({-1, false, Alignment::Right});
            powerBox->SetSpacing({0, false});
            {
                auto revealer = Widget::Create<Revealer>();
                DynCtx::powerBoxRevealer = revealer.get();
                revealer->SetTransition({TransitionType::SlideLeft, 500});
                {
                    auto powerBoxExpand = Widget::Create<Box>();
                    powerBoxExpand->SetClass("power-box-expand");
                    powerBoxExpand->SetSpacing({8, true});
                    {
                        auto exitButton = Widget::Create<Button>();
                        exitButton->SetClass("exit-button");
                        exitButton->SetText("");
                        exitButton->OnClick(
                            [](Button&)
                            {
                                System::ExitWM();
                            });

                        auto lockButton = Widget::Create<Button>();
                        lockButton->SetClass("sleep-button");
                        lockButton->SetText("");
                        lockButton->OnClick(
                            [](Button&)
                            {
                                System::Lock();
                            });

                        auto sleepButton = Widget::Create<Button>();
                        sleepButton->SetClass("sleep-button");
                        sleepButton->SetText("");
                        sleepButton->OnClick(
                            [](Button&)
                            {
                                System::Suspend();
                            });

                        auto rebootButton = Widget::Create<Button>();
                        rebootButton->SetClass("reboot-button");
                        rebootButton->SetText("累");
                        rebootButton->OnClick(
                            [](Button&)
                            {
                                System::Reboot();
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
                powerButton->SetHorizontalTransform({24, true, Alignment::Fill});
                powerButton->OnClick(
                    [](Button&)
                    {
                        System::Shutdown();
                    });

                powerBox->AddChild(std::move(revealer));
                powerBox->AddChild(std::move(powerButton));
            }
            eventBox->AddChild(std::move(powerBox));
        }

        parent.AddChild(std::move(eventBox));
    }

#ifdef WITH_HYPRLAND
    void WidgetWorkspaces(Widget& parent)
    {
        auto margin = Widget::Create<Box>();
        margin->SetHorizontalTransform({12, true, Alignment::Left});
        parent.AddChild(std::move(margin));
        auto eventBox = Widget::Create<EventBox>();
        eventBox->SetScrollFn(DynCtx::ScrollWorkspaces);
        {
            auto box = Widget::Create<Box>();
            box->SetSpacing({8, true});
            box->SetHorizontalTransform({-1, true, Alignment::Left});
            {
                for (size_t i = 0; i < DynCtx::workspaces.size(); i++)
                {
                    auto workspace = Widget::Create<Button>();
                    workspace->SetHorizontalTransform({8, false, Alignment::Fill});
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
        mainWidget->SetSpacing({0, Config::Get().centerTime});
        mainWidget->SetClass("bar");
        {
            auto left = Widget::Create<Box>();
            left->SetSpacing({0, false});
            left->SetHorizontalTransform({-1, true, Alignment::Left});
#ifdef WITH_HYPRLAND
            if (RuntimeConfig::Get().hasHyprland)
            {
                WidgetWorkspaces(*left);
            }
#endif

            auto time = Widget::Create<Text>();
            time->SetHorizontalTransform({-1, true, Alignment::Center});
            time->SetClass("time-text");
            time->SetText("Uninitialized");
            time->AddTimer<Text>(DynCtx::UpdateTime, 1000);

            auto right = Widget::Create<Box>();
            right->SetClass("right");
            right->SetSpacing({8, false});
            right->SetHorizontalTransform({-1, true, Alignment::Right});
            {
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
            mainWidget->AddChild(std::move(time));
            mainWidget->AddChild(std::move(right));
        }
        window = Window(std::move(mainWidget), monitor);
        window.SetAnchor(Anchor::Left | Anchor::Right | Anchor::Top);
    }
}

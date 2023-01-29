#include "Bar.h"

#include "System.h"
#include "Common.h"

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
        static TimerResult UpdateCPU(CairoSensor& sensor)
        {
            double usage = System::GetCPUUsage();
            double temp = System::GetCPUTemp();

            cpuText->SetText("CPU: " + Utils::ToStringPrecision(usage * 100, "%0.1f") + "% " + Utils::ToStringPrecision(temp, "%0.1f") + "°C");
            sensor.SetValue(usage);
            return TimerResult::Ok;
        }

        static Text* batteryText;
        static TimerResult UpdateBattery(CairoSensor& sensor)
        {
            double percentage = System::GetBatteryPercentage();

            batteryText->SetText("Battery: " + Utils::ToStringPrecision(percentage * 100, "%0.1f") + "%");
            sensor.SetValue(percentage);
            return TimerResult::Ok;
        }

        static Text* ramText;
        static TimerResult UpdateRAM(CairoSensor& sensor)
        {
            System::RAMInfo info = System::GetRAMInfo();
            double used = info.totalGiB - info.freeGiB;
            double usedPercent = used / info.totalGiB;

            ramText->SetText("RAM: " + Utils::ToStringPrecision(used, "%0.2f") + "GiB/" + Utils::ToStringPrecision(info.totalGiB, "%0.2f") + "GiB");
            sensor.SetValue(usedPercent);
            return TimerResult::Ok;
        }

#ifdef HAS_NVIDIA
        static Text* gpuText;
        static TimerResult UpdateGPU(CairoSensor& sensor)
        {
            System::GPUInfo info = System::GetGPUInfo();

            gpuText->SetText("GPU: " + Utils::ToStringPrecision(info.utilisation, "%0.1f") + "% " + Utils::ToStringPrecision(info.coreTemp, "%0.1f") +
                             "°C");
            sensor.SetValue(info.utilisation / 100);
            return TimerResult::Ok;
        }

        static Text* vramText;
        static TimerResult UpdateVRAM(CairoSensor& sensor)
        {
            System::VRAMInfo info = System::GetVRAMInfo();

            vramText->SetText("VRAM: " + Utils::ToStringPrecision(info.usedGiB, "%0.2f") + "GiB/" + Utils::ToStringPrecision(info.totalGiB, "%0.2f") +
                              "GiB");
            sensor.SetValue(info.usedGiB / info.totalGiB);
            return TimerResult::Ok;
        }
#endif

        static Text* diskText;
        static TimerResult UpdateDisk(CairoSensor& sensor)
        {
            System::DiskInfo info = System::GetDiskInfo();

            diskText->SetText("Disk: " + Utils::ToStringPrecision(info.usedGiB, "%0.2f") + "GiB/" + Utils::ToStringPrecision(info.totalGiB, "%0.2f") +
                              "GiB");
            sensor.SetValue(info.usedGiB / info.totalGiB);
            return TimerResult::Ok;
        }

#ifdef HAS_BLUEZ
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

        TimerResult UpdateTime(Text& text)
        {
            text.SetText(System::GetTime());
            return TimerResult::Ok;
        }

#ifdef HAS_HYPRLAND
        static std::array<Button*, 9> workspaces;
        TimerResult UpdateWorkspaces(Box&)
        {
            for (size_t i = 0; i < workspaces.size(); i++)
            {
                switch (System::GetWorkspaceStatus((uint32_t)monitorID, i + 1))
                {
                case System::WorkspaceStatus::Dead:
                    workspaces[i]->SetClass("ws-dead");
                    break;
                case System::WorkspaceStatus::Inactive:
                    workspaces[i]->SetClass("ws-inactive");
                    break;
                case System::WorkspaceStatus::Visible:
                    workspaces[i]->SetClass("ws-visible");
                    break;
                case System::WorkspaceStatus::Current:
                    workspaces[i]->SetClass("ws-current");
                    break;
                case System::WorkspaceStatus::Active:
                    workspaces[i]->SetClass("ws-active");
                    break;
                }
                workspaces[i]->SetText(System::GetWorkspaceSymbol(i));
            }
            return TimerResult::Ok;
        }
#endif
    }

    void Sensor(Widget& parent, TimerCallback<CairoSensor>&& callback, const std::string& sensorClass, const std::string& textClass, Text*& textPtr)
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
                eventBox->SetEventFn(
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

                auto cairoSensor = Widget::Create<CairoSensor>();
                cairoSensor->SetClass(sensorClass);
                cairoSensor->AddTimer<CairoSensor>(std::move(callback), DynCtx::updateTime);
                cairoSensor->SetHorizontalTransform({24, true, Alignment::Fill});

                box->AddChild(std::move(revealer));
                box->AddChild(std::move(cairoSensor));
            }
            eventBox->AddChild(std::move(box));
        }

        parent.AddChild(std::move(eventBox));
    }

    void WidgetAudio(Widget& parent)
    {
        auto box = Widget::Create<Box>();
        box->SetSpacing({8, false});
        box->SetHorizontalTransform({-1, false, Alignment::Right});
        {
            auto icon = Widget::Create<Text>();
            icon->SetClass("audio-icon");
            icon->SetText("墳");
            DynCtx::audioIcon = icon.get();

            auto slider = Widget::Create<Slider>();
            slider->SetOrientation(Orientation::Horizontal);
            slider->SetHorizontalTransform({100, true, Alignment::Fill});
            slider->SetInverted(true);
            slider->SetClass("audio-volume");
            slider->AddTimer<Slider>(DynCtx::UpdateAudio, DynCtx::updateTimeFast);
            slider->OnValueChange(DynCtx::OnChangeVolume);
            slider->SetRange({0, 1, 0.01});

            box->AddChild(std::move(slider));
            box->AddChild(std::move(icon));
        }

        parent.AddChild(std::move(box));
    }

#ifdef HAS_BLUEZ
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

    void WidgetSensors(Widget& parent)
    {
        Sensor(parent, DynCtx::UpdateDisk, "disk-util-progress", "disk-data-text", DynCtx::diskText);
#ifdef HAS_NVIDIA
        Sensor(parent, DynCtx::UpdateVRAM, "vram-util-progress", "vram-data-text", DynCtx::vramText);
        Sensor(parent, DynCtx::UpdateGPU, "gpu-util-progress", "gpu-data-text", DynCtx::gpuText);
#endif
        Sensor(parent, DynCtx::UpdateRAM, "ram-util-progress", "ram-data-text", DynCtx::ramText);
        Sensor(parent, DynCtx::UpdateCPU, "cpu-util-progress", "cpu-data-text", DynCtx::cpuText);
        // Only show battery percentage if battery folder is set and exists
        if (System::GetBatteryPercentage() >= 0) {
            Sensor(parent, DynCtx::UpdateBattery, "battery-util-progress", "battery-data-text", DynCtx::batteryText);
        }
    }

    void WidgetPower(Widget& parent)
    {
        auto eventBox = Widget::Create<EventBox>();
        eventBox->SetEventFn(DynCtx::PowerBoxEvent);
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

#ifdef HAS_HYPRLAND
    void WidgetWorkspaces(Widget& parent)
    {
        auto margin = Widget::Create<Box>();
        margin->SetHorizontalTransform({12, true, Alignment::Left});
        parent.AddChild(std::move(margin));

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
        parent.AddChild(std::move(box));
    }
#endif

    void Create(Window& window, int32_t monitor)
    {
        monitorID = monitor;

        auto mainWidget = Widget::Create<Box>();
        mainWidget->SetSpacing({0, true});
        mainWidget->SetClass("bar");
        {
#ifdef HAS_HYPRLAND
            auto left = Widget::Create<Box>();
            left->SetSpacing({0, false});
            left->SetHorizontalTransform({-1, true, Alignment::Left});
            WidgetWorkspaces(*left);
#else
            auto left = Widget::Create<Box>();
            left->SetSpacing({0, false});
            left->SetHorizontalTransform({-1, true, Alignment::Left});
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

#ifdef HAS_BLUEZ
                WidgetBluetooth(*right);
#endif

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

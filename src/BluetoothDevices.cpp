#include "BluetoothDevices.h"
#include "System.h"
#include <mutex>
#include <unordered_map>
#include <string>
#include <algorithm>

#ifdef WITH_BLUEZ
namespace BluetoothDevices
{
    namespace DynCtx
    {
        enum class DeviceState
        {
            Invalid = BIT(0),
            Failed = BIT(1),            // Request failed
            RequestConnect = BIT(2),    // If device is requested to connect
            RequestDisconnect = BIT(3), // If device is requested to disconnect
            Connected = BIT(4),         // BlueZ says to us, that it is connected
            Disconnected = BIT(5),      // BlueZ says to us, that it is disconnected
        };
        DEFINE_ENUM_FLAGS(DeviceState);

        struct BTDeviceWithState
        {
            System::BluetoothDevice device{};
            DeviceState state{};
        };

        std::mutex deviceMutex;
        std::vector<BTDeviceWithState> devices;
        Box* deviceListBox;
        Window* win;
        bool scanning = false;

        void OnClick(Button& button, BTDeviceWithState& device)
        {
            DeviceState& state = device.state;

            // Clear failed bit
            state &= ~DeviceState::Failed;
            button.RemoveClass("failed");

            // Only try to connect, if we know we're already disconnected and we haven't requested before(Same for disconnect)
            if (FLAG_CHECK(state, DeviceState::Disconnected) && !FLAG_CHECK(state, DeviceState::RequestConnect))
            {
                button.AddClass("active");
                button.RemoveClass("inactive");
                state |= DeviceState::RequestConnect;

                System::ConnectBTDevice(device.device,
                                        [&dev = device, &but = button](bool success, System::BluetoothDevice&)
                                        {
                                            deviceMutex.lock();
                                            if (!success)
                                            {
                                                dev.state &= ~DeviceState::RequestConnect;
                                                dev.state |= DeviceState::Failed;
                                                but.AddClass("failed");
                                            }
                                            deviceMutex.unlock();
                                        });
            }
            else if (FLAG_CHECK(state, DeviceState::Connected) && !FLAG_CHECK(state, DeviceState::RequestDisconnect))
            {
                button.AddClass("inactive");
                button.RemoveClass("active");
                state |= DeviceState::RequestDisconnect;

                System::DisconnectBTDevice(device.device,
                                           [&dev = device, &but = button](bool success, System::BluetoothDevice&)
                                           {
                                               deviceMutex.lock();
                                               if (!success)
                                               {
                                                   dev.state &= ~DeviceState::RequestDisconnect;
                                                   dev.state |= DeviceState::Failed;
                                                   but.AddClass("failed");
                                               }
                                               deviceMutex.unlock();
                                           });
            }
        }

        void UpdateDeviceUIElem(Button& button, BTDeviceWithState& device)
        {
            if (device.device.name.size())
            {
                button.SetText(System::BTTypeToIcon(device.device) + device.device.name);
            }
            else
            {
                button.SetText(device.device.mac);
            }
            bool requestConnect = FLAG_CHECK(device.state, DeviceState::RequestConnect);
            bool requestDisconnect = FLAG_CHECK(device.state, DeviceState::RequestDisconnect);
            if (requestConnect || (!requestDisconnect && FLAG_CHECK(device.state, DeviceState::Connected)))
            {
                button.AddClass("active");
                button.RemoveClass("inactive");
            }
            else if (requestDisconnect || (!requestConnect && FLAG_CHECK(device.state, DeviceState::Disconnected)))
            {
                button.AddClass("inactive");
                button.RemoveClass("active");
            }
            if (FLAG_CHECK(device.state, DeviceState::Failed))
            {
                button.AddClass("failed");
            }
            else
            {
                button.RemoveClass("failed");
            }

            button.OnClick(
                [&dev = device](Button& button)
                {
                    OnClick(button, dev);
                });
        }

        void InvalidateDeviceUI()
        {
            // Shrink
            if (deviceListBox->GetChilds().size() > devices.size())
            {
                for (size_t i = deviceListBox->GetChilds().size() - 1; i >= devices.size(); i--)
                {
                    deviceListBox->RemoveChild(i);
                }
            }

            size_t idx = 0;
            for (auto& device : devices)
            {
                if (idx >= deviceListBox->GetChilds().size())
                {
                    // Create new
                    auto button = Widget::Create<Button>();
                    button->SetClass("bt-button");
                    deviceListBox->AddChild(std::move(button));
                }

                // Initialise
                UpdateDeviceUIElem((Button&)*deviceListBox->GetChilds()[idx], device);

                idx++;
            }
        }

        TimerResult OnUpdate(Widget&)
        {
            // Invalidate each current device
            for (auto& device : devices)
            {
                device.state |= DeviceState::Invalid;
            }

            for (auto& device : System::GetBluetoothInfo().devices)
            {
                auto stateDevIt = std::find_if(devices.begin(), devices.end(),
                                               [&](auto& x)
                                               {
                                                   return device.mac == x.device.mac;
                                               });
                BTDeviceWithState* stateDev = nullptr;
                if (stateDevIt != devices.end())
                {
                    stateDev = &*stateDevIt;
                }
                else
                {
                    devices.push_back({});
                    stateDev = &devices.back();
                }
                // This device exists
                stateDev->state &= ~DeviceState::Invalid;
                if (device.connected)
                {
                    // Clear any requests, it is now connected
                    stateDev->state &= ~DeviceState::RequestConnect;

                    stateDev->state &= ~DeviceState::Disconnected;
                    stateDev->state |= DeviceState::Connected;
                }
                else
                {
                    stateDev->state &= ~DeviceState::RequestDisconnect;

                    stateDev->state &= ~DeviceState::Connected;
                    stateDev->state |= DeviceState::Disconnected;
                }
                stateDev->device = device;
            }
            // Erase all invalid
            for (auto it = devices.begin(); it != devices.end();)
            {
                if (FLAG_CHECK(it->state, DeviceState::Invalid))
                {
                    LOG("Removing " << it->device.name);
                    it = devices.erase(it);
                }
                else
                {
                    it++;
                }
            }

            // Sort devices by, whether they're connected or not
            std::sort(devices.begin(), devices.end(),
                      [](auto& a, auto& b)
                      {
                          auto& deviceA = a.device;
                          auto& deviceB = b.device;
                          if (deviceA.connected && !deviceB.connected)
                          {
                              return true;
                          }
                          else if (deviceB.connected && !deviceA.connected)
                          {
                              return false;
                          }
                          return deviceA.name < deviceB.name;
                      });

            InvalidateDeviceUI();

            return TimerResult::Ok;
        }

        void Close(Button&)
        {
            win->Close();
        }

        void Scan(Button& button)
        {
            scanning = !scanning;
            if (scanning)
            {
                button.AddClass("active");
                button.RemoveClass("inactive");
                System::StartBTScan();
            }
            else
            {
                button.AddClass("inactive");
                button.RemoveClass("active");
                System::StopBTScan();
            }
        }
    }

    void WidgetHeader(Widget& parentWidget)
    {
        auto headerBox = Widget::Create<Box>();
        headerBox->SetClass("bt-header-box");
        {
            auto headerText = Widget::Create<Text>();
            headerText->SetText("󰂯 Bluetooth");
            headerBox->AddChild(std::move(headerText));

            auto headerRefresh = Widget::Create<Button>();
            headerRefresh->SetText("");
            headerRefresh->SetClass("bt-scan");
            headerRefresh->OnClick(DynCtx::Scan);
            headerBox->AddChild(std::move(headerRefresh));

            auto headerClose = Widget::Create<Button>();
            headerClose->SetText("");
            headerClose->SetClass("bt-close");
            headerClose->OnClick(DynCtx::Close);
            headerBox->AddChild(std::move(headerClose));
        }
        parentWidget.AddChild(std::move(headerBox));
    }

    void WidgetBody(Widget& parentWidget)
    {
        auto bodyBox = Widget::Create<Box>();
        DynCtx::deviceListBox = bodyBox.get();
        bodyBox->SetOrientation(Orientation::Vertical);
        bodyBox->SetClass("bt-body-box");
        bodyBox->AddTimer<Widget>(DynCtx::OnUpdate, 100);
        parentWidget.AddChild(std::move(bodyBox));
    }

    void Create(Window& window, int32_t monitor)
    {
        DynCtx::win = &window;
        auto mainWidget = Widget::Create<Box>();
        mainWidget->SetSpacing({8, false});
        mainWidget->SetOrientation(Orientation::Vertical);
        mainWidget->SetVerticalTransform({32, true, Alignment::Fill});
        mainWidget->SetClass("bt-bg");

        WidgetHeader(*mainWidget);
        WidgetBody(*mainWidget);

        window = Window(std::move(mainWidget), monitor);
        window.SetExclusive(false);
        window.SetMargin(Anchor::Top, 8);
        window.SetAnchor(Anchor::Right | Anchor::Top);
    }
}
#endif

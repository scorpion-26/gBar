#include "SNI.h"
#include "Log.h"
#include "Widget.h"
#include "Config.h"

#ifdef WITH_SNI

#include <sni-watcher.h>
#include <sni-item.h>
#include <gio/gio.h>
#include <libdbusmenu-gtk/menu.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fstream>

namespace SNI
{
    sniWatcher* watcherSkeleton;
    guint watcherID;
    GDBusConnection* dbusConnection = nullptr;

    guint hostID;

    struct Item
    {
        std::string name;
        std::string object;
        size_t w;
        size_t h;
        uint8_t* iconData = nullptr;

        std::string tooltip;

        std::string menuObjectPath;

        EventBox* gtkEvent;
    };
    std::vector<Item> items;

    std::unordered_map<std::string, Item> clientsToQuery;

    // Gtk stuff, TODO: Allow more than one instance
    // Simply removing the gtk_drawing_areas doesn't trigger proper redrawing
    //   HACK: Make an outer permanent and an inner box, which will be deleted and readded
    Widget* parentBox;
    Widget* iconBox;

    static Item CreateItem(std::string&& name, std::string&& object)
    {
        Item item{};
        item.name = name;
        item.object = object;
        auto getProperty = [&](const char* prop) -> GVariant*
        {
            GError* err = nullptr;
            GVariant* params[2];
            params[0] = g_variant_new_string("org.kde.StatusNotifierItem");
            params[1] = g_variant_new_string(prop);
            GVariant* param = g_variant_new_tuple(params, 2);
            GVariant* res = g_dbus_connection_call_sync(dbusConnection, name.c_str(), object.c_str(), "org.freedesktop.DBus.Properties", "Get", param,
                                                        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &err);
            if (err)
            {
                g_error_free(err);
                return nullptr;
            }
            // There's probably a better method than to use 3 variants
            // g_variant_unref(params[0]);
            // g_variant_unref(params[1]);
            // g_variant_unref(param);
            return res;
        };
        GVariant* iconPixmap = getProperty("IconPixmap");
        if (iconPixmap)
        {
            // Only get first item
            GVariant* arr = nullptr;
            g_variant_get(iconPixmap, "(v)", &arr);

            GVariantIter* arrIter = nullptr;
            g_variant_get(arr, "a(iiay)", &arrIter);

            int width;
            int height;
            GVariantIter* data = nullptr;
            g_variant_iter_next(arrIter, "(iiay)", &width, &height, &data);

            LOG("SNI: Width: " << width);
            LOG("SNI: Height: " << height);
            item.w = width;
            item.h = height;
            item.iconData = new uint8_t[width * height * 4];

            uint8_t px = 0;
            int i = 0;
            while (g_variant_iter_next(data, "y", &px))
            {
                item.iconData[i] = px;
                i++;
            }
            for (int i = 0; i < width * height; i++)
            {
                struct Px
                {
                    // This should be bgra...
                    // Since source is ARGB32 in network order(=big-endian)
                    // and x86 Linux is little-endian, we *should* swap b and r...
                    uint8_t a, r, g, b;
                };
                Px& pixel = ((Px*)item.iconData)[i];
                // Swap to create rgba
                pixel = {pixel.r, pixel.g, pixel.b, pixel.a};
            }

            g_variant_iter_free(data);
            g_variant_iter_free(arrIter);
            g_variant_unref(arr);
            g_variant_unref(iconPixmap);
        }
        else
        {
            // Get icon theme path
            GVariant* themePathVariant = getProperty("IconThemePath"); // Not defined by freedesktop, I think ayatana does this...
            GVariant* iconNameVariant = getProperty("IconName");

            std::string iconPath;
            if (themePathVariant && iconNameVariant)
            {
                // Why GLib?
                GVariant* themePathStr = nullptr;
                g_variant_get(themePathVariant, "(v)", &themePathStr);
                GVariant* iconNameStr = nullptr;
                g_variant_get(iconNameVariant, "(v)", &iconNameStr);

                const char* themePath = g_variant_get_string(themePathStr, nullptr);
                const char* iconName = g_variant_get_string(iconNameStr, nullptr);
                iconPath = std::string(themePath) + "/" + iconName + ".png"; // TODO: Find out if this is always png

                g_variant_unref(themePathVariant);
                g_variant_unref(themePathStr);
                g_variant_unref(iconNameVariant);
                g_variant_unref(iconNameStr);
            }
            else if (iconNameVariant)
            {
                GVariant* iconNameStr = nullptr;
                g_variant_get(iconNameVariant, "(v)", &iconNameStr);

                const char* iconName = g_variant_get_string(iconNameStr, nullptr);
                iconPath = std::string(iconName);

                g_variant_unref(iconNameVariant);
                g_variant_unref(iconNameStr);
            }
            else
            {
                LOG("SNI: Unknown path!");
                return item;
            }

            int width, height, channels;
            stbi_uc* pixels = stbi_load(iconPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (!pixels)
            {
                LOG("SNI: Cannot open " << iconPath);
                return item;
            }
            item.w = width;
            item.h = height;
            item.iconData = new uint8_t[width * height * 4];
            // Already rgba32
            memcpy(item.iconData, pixels, width * height * 4);
            stbi_image_free(pixels);
        }

        // Query tooltip(Steam e.g. doesn't have one)
        GVariant* tooltip = getProperty("ToolTip");
        if (tooltip)
        {
            GVariant* tooltipVar;
            g_variant_get_child(tooltip, 0, "v", &tooltipVar);
            const gchar* title;
            // Both telegram and discord only set the "title" component
            g_variant_get_child(tooltipVar, 2, "s", &title);
            item.tooltip = title;
            LOG("SNI: Title: " << item.tooltip);
            g_variant_unref(tooltip);
            g_variant_unref(tooltipVar);
        }

        // Query menu
        GVariant* menuPath = getProperty("Menu");
        if (menuPath)
        {
            GVariant* menuVariant;
            g_variant_get_child(menuPath, 0, "v", &menuVariant);
            const char* objectPath;
            g_variant_get(menuVariant, "o", &objectPath);
            LOG("SNI: Menu object path: " << objectPath);

            item.menuObjectPath = objectPath;

            g_variant_unref(menuVariant);
            g_variant_unref(menuPath);
        }

        return item;
    }

    static void InvalidateWidget();

    static void DBusNameVanished(GDBusConnection*, const char* name, void*)
    {
        auto it = std::find_if(items.begin(), items.end(),
                               [&](const Item& item)
                               {
                                   return item.name == name;
                               });
        if (it != items.end())
        {
            LOG("SNI: " << name << " vanished!");
            items.erase(it);
            InvalidateWidget();
        }
        else
        {
            LOG("SNI: Cannot remove unregistered bus name " << name);
        }
    }

    static void ItemPropertyChanged(GDBusConnection*, const char*, const char* object, const char*, const char*, GVariant*, void* name)
    {
        std::string nameStr = (const char*)name;
        LOG("SNI: Reloading " << (const char*)name << " " << object);
        // We don't care about *what* changed, just remove and reload
        auto it = std::find_if(items.begin(), items.end(),
                               [&](const Item& item)
                               {
                                   return item.name == nameStr;
                               });
        if (it != items.end())
        {
            items.erase(it);
        }
        else
        {
            LOG("SNI: Coudn't remove item " << nameStr << " when reloading");
        }
        clientsToQuery[nameStr] = {nameStr, std::string(object)};
    }

    static TimerResult UpdateWidgets(Box&)
    {
        if (RuntimeConfig::Get().hasSNI == false || Config::Get().enableSNI == false)
        {
            // Don't bother
            return TimerResult::Delete;
        }
        for (auto& [name, client] : clientsToQuery)
        {
            LOG("SNI: Creating Item " << client.name << " " << client.object);
            Item item = CreateItem(std::move(client.name), std::move(client.object));
            // Add handler for removing
            g_bus_watch_name_on_connection(dbusConnection, item.name.c_str(), G_BUS_NAME_WATCHER_FLAGS_NONE, nullptr, DBusNameVanished, nullptr,
                                           nullptr);

            // Add handler for icon change
            char* staticBuf = new char[item.name.size() + 1]{0x0};
            memcpy(staticBuf, item.name.c_str(), item.name.size());
            g_dbus_connection_signal_subscribe(
                dbusConnection, item.name.c_str(), "org.kde.StatusNotifierItem", nullptr, nullptr, nullptr, G_DBUS_SIGNAL_FLAGS_NONE,
                ItemPropertyChanged, staticBuf,
                +[](void* ptr)
                {
                    LOG("SNI: Delete static name buffer for " << (char*)ptr);
                    delete[] (char*)ptr;
                });

            items.push_back(std::move(item));
        }
        if (clientsToQuery.size() > 0)
        {
            InvalidateWidget();
        }
        clientsToQuery.clear();
        return TimerResult::Ok;
    }

    // SNI implements the GTK-Thingies itself internally
    static void InvalidateWidget()
    {
        LOG("SNI: Clearing old children");
        parentBox->RemoveChild(iconBox);

        auto container = Widget::Create<Box>();
        container->SetSpacing({4, false});
        iconBox = container.get();
        for (auto& item : items)
        {
            if (item.iconData)
            {
                auto eventBox = Widget::Create<EventBox>();
                item.gtkEvent = eventBox.get();

                eventBox->SetOnCreate(
                    [&](Widget& w)
                    {
                        auto clickFn = [](GtkWidget*, GdkEventButton* event, void* data) -> gboolean
                        {
                            if (event->button == 1)
                            {
                                Item* item = (Item*)data;

                                GtkMenu* menu = (GtkMenu*)dbusmenu_gtkmenu_new(item->name.data(), item->menuObjectPath.data());
                                LOG(menu);
                                gtk_menu_attach_to_widget(menu, item->gtkEvent->Get(), nullptr);
                                gtk_menu_popup_at_pointer(menu, (GdkEvent*)event);
                                LOG(item->menuObjectPath << " click");
                            }
                            return GDK_EVENT_STOP;
                        };
                        g_signal_connect(w.Get(), "button-release-event", G_CALLBACK(+clickFn), &item);
                    });

                LOG("SNI: Add " << item.name << " to widget");
                auto texture = Widget::Create<Texture>();
                bool wasExplicitOverride = false;
                for (auto& [filter, size] : Config::Get().sniIconSizes)
                {
                    if (item.tooltip.find(filter) != std::string::npos)
                    {
                        wasExplicitOverride = true;
                        texture->ForceHeight(size);
                    }
                    else if (filter == "*" && !wasExplicitOverride)
                    {
                        texture->ForceHeight(size);
                    }
                }
                wasExplicitOverride = false;
                for (auto& [filter, padding] : Config::Get().sniPaddingTop)
                {
                    if (item.tooltip.find(filter) != std::string::npos)
                    {
                        LOG("Padding " << padding);
                        wasExplicitOverride = true;
                        texture->AddPaddingTop(padding);
                    }
                    else if (filter == "*" && !wasExplicitOverride)
                    {
                        texture->AddPaddingTop(padding);
                    }
                }
                texture->SetHorizontalTransform({0, true, Alignment::Fill});
                texture->SetBuf(item.w, item.h, item.iconData);
                texture->SetTooltip(item.tooltip);

                eventBox->AddChild(std::move(texture));
                iconBox->AddChild(std::move(eventBox));
            }
        }
        parentBox->AddChild(std::move(container));
    }

    void WidgetSNI(Widget& parent)
    {
        if (RuntimeConfig::Get().hasSNI == false || Config::Get().enableSNI == false)
        {
            return;
        }
        // Add parent box
        auto box = Widget::Create<Box>();
        auto container = Widget::Create<Box>();
        container->AddTimer<Box>(UpdateWidgets, 1000, TimerDispatchBehaviour::LateDispatch);
        iconBox = container.get();
        parentBox = box.get();
        InvalidateWidget();
        box->AddChild(std::move(container));
        parent.AddChild(std::move(box));
    }

    // Methods
    static bool RegisterItem(sniWatcher* watcher, GDBusMethodInvocation* invocation, const char* service)
    {
        std::string name;
        std::string object;
        if (strncmp(service, "/", 1) == 0)
        {
            // service is object (used by e.g. ayatana -> steam, discord)
            object = service;
            name = g_dbus_method_invocation_get_sender(invocation);
        }
        else
        {
            // service is bus name (used by e.g. Telegram)
            name = service;
            object = "/StatusNotifierItem";
        }
        auto it = std::find_if(items.begin(), items.end(),
                               [&](const Item& item)
                               {
                                   return item.name == name && item.object == object;
                               });
        if (it != items.end())
        {
            LOG("Rejecting " << name << " " << object);
            return false;
        }
        sni_watcher_emit_status_notifier_item_registered(watcher, service);
        sni_watcher_complete_register_status_notifier_item(watcher, invocation);
        LOG("SNI: Registered Item " << name << " " << object);
        clientsToQuery[name] = {name, std::move(object)};
        return true;
    }

    static void RegisterHost(sniWatcher*, GDBusMethodInvocation*, const char*)
    {
        LOG("TODO: Implement RegisterHost!");
    }

    // Signals
    static void ItemRegistered(sniWatcher*, const char*, void*)
    {
        // Don't care, since watcher and host will always be from gBar (at least for now)
    }
    static void ItemUnregistered(sniWatcher*, const char*, void*)
    {
        // Don't care, since watcher and host will always be from gBar (at least for now)
    }

    void Init()
    {
        if (RuntimeConfig::Get().hasSNI == false || Config::Get().enableSNI == false)
        {
            return;
        }

        auto busAcquired = [](GDBusConnection* connection, const char*, void*)
        {
            GError* err = nullptr;
            g_dbus_interface_skeleton_export((GDBusInterfaceSkeleton*)watcherSkeleton, connection, "/StatusNotifierWatcher", &err);
            if (err)
            {
                LOG("SNI: Failed to connect to dbus! Disabling SNI. Error: " << err->message);
                RuntimeConfig::Get().hasSNI = false;
                g_error_free(err);
                return;
            }
            dbusConnection = connection;

            // Connect methods and signals
            g_signal_connect(watcherSkeleton, "handle-register-status-notifier-item", G_CALLBACK(RegisterItem), nullptr);
            g_signal_connect(watcherSkeleton, "handle-register-status-notifier-host", G_CALLBACK(RegisterHost), nullptr);

            g_signal_connect(watcherSkeleton, "status-notifier-item-registered", G_CALLBACK(ItemRegistered), nullptr);
            g_signal_connect(watcherSkeleton, "status-notifier-item-unregistered", G_CALLBACK(ItemUnregistered), nullptr);

            // Host is always available
            sni_watcher_set_is_status_notifier_host_registered(watcherSkeleton, true);
        };
        auto emptyCallback = [](GDBusConnection*, const char*, void*) {};
        auto lostName = [](GDBusConnection*, const char* msg, void*)
        {
            LOG("SNI: Lost Name! Disabling SNI!");
            RuntimeConfig::Get().hasSNI = false;
        };
        auto flags = G_BUS_NAME_OWNER_FLAGS_REPLACE | G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
        g_bus_own_name(G_BUS_TYPE_SESSION, "org.kde.StatusNotifierWatcher", (GBusNameOwnerFlags)flags, +busAcquired, +emptyCallback, +lostName,
                       nullptr, nullptr);
        watcherSkeleton = sni_watcher_skeleton_new();

        std::string hostName = "org.kde.StatusNotifierHost-" + std::to_string(getpid());
        g_bus_own_name(G_BUS_TYPE_SESSION, hostName.c_str(), (GBusNameOwnerFlags)flags, +emptyCallback, +emptyCallback, +emptyCallback, nullptr,
                       nullptr);

        // Host is always available
        sni_watcher_set_is_status_notifier_host_registered(watcherSkeleton, true);
        sni_watcher_emit_status_notifier_host_registered(watcherSkeleton);
    }

    void Shutdown() {}
}
#endif

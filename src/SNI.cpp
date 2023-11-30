#include "SNI.h"
#include "Log.h"
#include "Widget.h"
#include "Config.h"
#include "Common.h"

#ifdef WITH_SNI

#include <sni-watcher.h>
#include <sni-item.h>
#include <gio/gio.h>
#include <libdbusmenu-gtk/menu.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <fstream>
#include <cstdio>
#include <unordered_set>

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
        size_t w = 0;
        size_t h = 0;
        GdkPixbuf* pixbuf = nullptr;

        std::string tooltip = "";

        std::string menuObjectPath = "";

        EventBox* gtkEvent = nullptr;

        int watcherID = -1;
        int propertyChangeWatcherID = -1;
    };
    std::vector<Item> items;

    std::unordered_map<std::string, Item> clientsToQuery;
    std::unordered_set<std::string> reloadedNames;

    // Gtk stuff, TODO: Allow more than one instance
    // Simply removing the gtk_drawing_areas doesn't trigger proper redrawing
    //   HACK: Make an outer permanent and an inner box, which will be deleted and readded
    Widget* parentBox;
    Widget* iconBox;

    // Swap channels for the render format and create a pixbuf out of it
    static GdkPixbuf* ToPixbuf(uint8_t* sniData, int32_t width, int32_t height)
    {
        for (int i = 0; i < width * height; i++)
        {
            struct Px
            {
                // This should be bgra...
                // Since source is ARGB32 in network order(=big-endian)
                // and x86 Linux is little-endian, we *should* swap b and r...
                uint8_t a, r, g, b;
            };
            Px& pixel = ((Px*)sniData)[i];
            // Swap to create rgba
            pixel = {pixel.r, pixel.g, pixel.b, pixel.a};
        }
        return gdk_pixbuf_new_from_data(
            sniData, GDK_COLORSPACE_RGB, true, 8, width, height, width * 4,
            +[](uint8_t* data, void*)
            {
                delete[] data;
            },
            nullptr);
    }

    // Allocates a pixbuf that contains a bitmap of the icon
    static void ToPixbuf(const std::string& location, GdkPixbuf*& outPixbuf, size_t& outWidth, size_t& outHeight)
    {
        std::string ext = location.substr(location.find_last_of('.') + 1);
        if (ext == "png" || ext == "jpg")
        {
            // png, load via svg
            int width, height, channels;
            stbi_uc* pixels = stbi_load(location.c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (!pixels)
            {
                LOG("SNI: Cannot open " << location);
                return;
            }
            outWidth = width;
            outHeight = height;
            uint8_t* iconData = new uint8_t[width * height * 4];
            // Already rgba32
            memcpy(iconData, pixels, width * height * 4);
            stbi_image_free(pixels);

            outPixbuf = gdk_pixbuf_new_from_data(
                iconData, GDK_COLORSPACE_RGB, true, 8, width, height, width * 4,
                +[](uint8_t* data, void*)
                {
                    delete[] data;
                },
                nullptr);
        }
        else if (ext == "svg")
        {
            // Just a random size, this should be plenty enough wiggle room
            outWidth = 64;
            outHeight = 64;

            // Use glib functions
            GError* err = nullptr;
            outPixbuf = gdk_pixbuf_new_from_file_at_scale(location.c_str(), outWidth, outHeight, true, &err);
            if (err)
            {
                LOG("SNI: Error loading svg " << location << ": " << err->message);
                return;
            }
        }
    }

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
            // g_dbus_connection_call_sync consumes the parameter and its children, so no need to unref
            return res;
        };

        bool hasPixmap = false;
        GVariant* iconPixmap = getProperty("IconPixmap");
        if (iconPixmap)
        {
            // Only get first item
            GVariant* arr = nullptr;
            g_variant_get(iconPixmap, "(v)", &arr);

            GVariantIter* arrIter = nullptr;
            g_variant_get(arr, "a(iiay)", &arrIter);

            if (g_variant_iter_n_children(arrIter) != 0)
            {
                int width;
                int height;
                GVariantIter* data = nullptr;
                g_variant_iter_next(arrIter, "(iiay)", &width, &height, &data);

                LOG("SNI: Width: " << width);
                LOG("SNI: Height: " << height);
                item.w = width;
                item.h = height;
                uint8_t* iconData = new uint8_t[width * height * 4];

                uint8_t px = 0;
                int i = 0;
                while (g_variant_iter_next(data, "y", &px))
                {
                    iconData[i] = px;
                    i++;
                }
                item.pixbuf = ToPixbuf(iconData, width, height);

                g_variant_iter_free(data);

                hasPixmap = true;
            }
            g_variant_iter_free(arrIter);
            g_variant_unref(arr);
            g_variant_unref(iconPixmap);
        }

        // Pixmap querying has failed, try IconName
        if (!hasPixmap)
        {
            auto findIconWithoutPath = [](const char* iconName) -> std::string
            {
                std::string iconPath;
                const char* dataDirs = getenv("XDG_DATA_DIRS");
                // Nothing defined, look in $XDG_DATA_DIRS/icons
                // network-manager-applet does this e.g.
                if (dataDirs)
                {
                    for (auto& dataDir : Utils::Split(dataDirs, ':'))
                    {
                        LOG("SNI: Searching icon " << iconName << " in " << dataDir << "/icons");
                        std::string path = Utils::FindFileWithName(dataDir + "/icons", iconName);
                        if (path != "")
                        {
                            iconPath = path;
                            break;
                        }
                    }
                }
                if (iconPath == "")
                {
                    // Fallback to /usr/share/icons
                    LOG("SNI: Searching icon " << iconName << " in "
                                               << "/usr/share/icons");
                    iconPath = Utils::FindFileWithName("/usr/share/icons", iconName);
                }
                return iconPath;
            };

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
                if (strlen(themePath) == 0)
                {
                    iconPath = findIconWithoutPath(iconName);
                }
                else
                {
                    LOG("SNI: Searching icon " << iconName << " in " << themePath);
                    iconPath = Utils::FindFileWithName(themePath, iconName);
                }

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
                iconPath = findIconWithoutPath(iconName);
                if (iconPath == "")
                {
                    // Try our luck with just using iconName, maybe its just an absolute path
                    iconPath = iconName;
                }

                g_variant_unref(iconNameVariant);
                g_variant_unref(iconNameStr);
            }
            else
            {
                LOG("SNI: Unknown path!");
                return item;
            }

            if (iconPath == "")
            {
                LOG("SNI: Cannot find icon path for " << name);
                return item;
            }
            LOG("SNI: Creating icon from \"" << iconPath << "\"");
            ToPixbuf(iconPath, item.pixbuf, item.w, item.h);
        }

        // Query tooltip(Steam e.g. doesn't have one)
        GVariant* tooltip = getProperty("ToolTip");
        if (tooltip)
        {
            GVariant* tooltipVar;
            g_variant_get_child(tooltip, 0, "v", &tooltipVar);
            const gchar* title = nullptr;
            if (g_variant_is_container(tooltipVar) && g_variant_n_children(tooltipVar) >= 4)
            {
                // According to spec, ToolTip is of type (sa(iiab)ss) => 4 children
                // Most icons only set the "title" component (e.g. Discord, KeePassXC, ...)
                g_variant_get_child(tooltipVar, 2, "s", &title);
            }
            else
            {
                // TeamViewer only exposes a string, which is not according to spec!
                title = g_variant_get_string(tooltipVar, nullptr);
            }

            if (title != nullptr)
            {
                item.tooltip = title;
            }
            else
            {
                LOG("SNI: Error querying tooltip");
            }
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
            g_bus_unwatch_name(it->watcherID);
            g_dbus_connection_signal_unsubscribe(dbusConnection, it->propertyChangeWatcherID);
            g_free(it->pixbuf);
            items.erase(it);
            InvalidateWidget();
            return;
        }

        auto toRegisterIt = clientsToQuery.find(name);
        if (toRegisterIt != clientsToQuery.end())
        {
            clientsToQuery.erase(toRegisterIt);
        }

        LOG("SNI: Cannot remove unregistered bus name " << name);
        return;
    }

    static void ItemPropertyChanged(GDBusConnection*, const char* senderName, const char* object, const char*, const char*, GVariant*, void* name)
    {
        // I think the multiple calling of this callback is a symptom of not unsubscribing the ItemPropertyChanged callback. Since this is now done, I
        // think the reloadedNames and object path checking is no longer needed. Since it doesn't do any harm (except being a little bit pointless)
        // I'll leave the checks here for now.
        // TODO: Investigate whether it is now actually fixed

        if (reloadedNames.insert(senderName).second == false)
        {
            // senderName has already requested a change, ignore
            LOG("SNI: " << senderName << " already signaled property change");
            return;
        }

        std::string nameStr = (const char*)name;
        LOG("SNI: Reloading " << (const char*)name << " " << object << " (Sender: " << senderName << ")");

        // We can't trust the object path given to us, since ItemPropertyChanged is called multiple times with the same name, but with different
        // object paths.
        auto it = std::find_if(items.begin(), items.end(),
                               [&](const Item& item)
                               {
                                   return item.name == nameStr;
                               });
        std::string itemObjectPath;
        if (it != items.end())
        {
            itemObjectPath = it->object;
        }
        else
        {
            // Item not already registered, fallback to provided name and hope the object path provided is accurate
            LOG("SNI: Couldn't find name for actual object path!");
            itemObjectPath = object;
        }

        // We don't care about *what* changed, just remove and reload
        LOG("SNI: Actual object path: " << itemObjectPath)
        if (it != items.end())
        {
            g_bus_unwatch_name(it->watcherID);
            g_dbus_connection_signal_unsubscribe(dbusConnection, it->propertyChangeWatcherID);
            g_free(it->pixbuf);
            items.erase(it);
        }
        else
        {
            LOG("SNI: Coudn't remove item " << nameStr << " when reloading");
        }
        clientsToQuery[nameStr] = {nameStr, itemObjectPath};
    }

    static TimerResult UpdateWidgets(Box&)
    {
        // Flush connection, so we hopefully don't deadlock with any client
        GError* err = nullptr;
        g_dbus_connection_flush_sync(dbusConnection, nullptr, &err);
        if (err)
        {
            LOG("SNI: g_dbus_connection_call_sync failed: " << err->message);
            g_error_free(err);
        }

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
            item.watcherID = g_bus_watch_name_on_connection(dbusConnection, item.name.c_str(), G_BUS_NAME_WATCHER_FLAGS_NONE, nullptr,
                                                            DBusNameVanished, nullptr, nullptr);

            // Add handler for icon change
            char* staticBuf = new char[item.name.size() + 1]{0x0};
            memcpy(staticBuf, item.name.c_str(), item.name.size());
            LOG("SNI: Allocating static name buffer for " << item.name);
            item.propertyChangeWatcherID = g_dbus_connection_signal_subscribe(
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

        // Allow further updates from the icon
        reloadedNames.clear();

        auto container = Widget::Create<Box>();
        container->SetSpacing({4, false});
        container->SetOrientation(Utils::GetOrientation());
        Utils::SetTransform(*container, {-1, true, Alignment::Fill, 0, 8});
        iconBox = container.get();

        // Sort items, so they don't jump around randomly
        std::sort(items.begin(), items.end(),
                  [](const Item& a, const Item& b)
                  {
                      return a.name < b.name;
                  });

        bool rotatedIcons = (Config::Get().location == 'L' || Config::Get().location == 'R') && Config::Get().iconsAlwaysUp;
        for (auto& item : items)
        {
            if (item.pixbuf)
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
                int size = 24;
                for (auto& [filter, iconSize] : Config::Get().sniIconSizes)
                {
                    if (item.tooltip.find(filter) != std::string::npos)
                    {
                        wasExplicitOverride = true;
                        size = iconSize;
                    }
                    else if (filter == "*" && !wasExplicitOverride)
                    {
                        size = iconSize;
                    }
                }
                wasExplicitOverride = false;
                for (auto& [filter, padding] : Config::Get().sniPaddingTop)
                {
                    if (item.tooltip.find(filter) != std::string::npos)
                    {
                        wasExplicitOverride = true;
                        texture->AddPaddingTop(padding);
                    }
                    else if (filter == "*" && !wasExplicitOverride)
                    {
                        texture->AddPaddingTop(padding);
                    }
                }
                Utils::SetTransform(*texture, {size, true, Alignment::Fill}, {size, true, Alignment::Fill, 0, rotatedIcons ? 6 : 0});
                texture->SetBuf(item.pixbuf, item.w, item.h);
                texture->SetTooltip(item.tooltip);
                texture->SetAngle(Utils::GetAngle() == 270 ? 90 : 0);

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
        box->AddClass("widget");
        box->AddClass("sni");
        Utils::SetTransform(*box, {-1, false, Alignment::Fill});
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
        auto lostName = [](GDBusConnection*, const char*, void*)
        {
            LOG("SNI: Lost Name! Disabling SNI!");
            RuntimeConfig::Get().hasSNI = false;
        };
        auto flags = G_BUS_NAME_OWNER_FLAGS_REPLACE;
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

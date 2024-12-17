#pragma once
#include <atomic>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <thread>
#include <unistd.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <filesystem>
#include <map>
#include <mutex>

#include "Log.h"

#define UNUSED [[maybe_unused]]

// Flag helper macros
#define BIT(x) (1 << (x))

template<typename Enum, typename std::enable_if_t<std::is_enum_v<Enum>, bool> = true>
using EnumType = std::underlying_type_t<Enum>;

// Based on https://stackoverflow.com/questions/1448396/how-to-use-enums-as-flags-in-c
// (Answer https://stackoverflow.com/a/23152590): Licensed under CC BY-SA 3.0
#define DEFINE_ENUM_FLAGS(T)                            \
    inline T operator~(T a)                             \
    {                                                   \
        return (T) ~(EnumType<T>)a;                     \
    }                                                   \
    inline T operator|(T a, T b)                        \
    {                                                   \
        return (T)((EnumType<T>)a | (EnumType<T>)b);    \
    }                                                   \
    inline T operator&(T a, T b)                        \
    {                                                   \
        return (T)((EnumType<T>)a & (EnumType<T>)b);    \
    }                                                   \
    inline T operator^(T a, T b)                        \
    {                                                   \
        return (T)((EnumType<T>)a ^ (EnumType<T>)b);    \
    }                                                   \
    inline T& operator|=(T& a, T b)                     \
    {                                                   \
        return (T&)((EnumType<T>&)a |= (EnumType<T>)b); \
    }                                                   \
    inline T& operator&=(T& a, T b)                     \
    {                                                   \
        return (T&)((EnumType<T>&)a &= (EnumType<T>)b); \
    }                                                   \
    inline T& operator^=(T& a, T b)                     \
    {                                                   \
        return (T&)((EnumType<T>&)a ^= (EnumType<T>)b); \
    }

#define FLAG_CHECK(val, check) ((int)(val & (check)) == (int)check)

namespace Utils
{
    template<typename T>
    constexpr bool IsMapLike = false;

    template<typename Key, typename Value>
    constexpr bool IsMapLike<std::map<Key, Value>> = true;
    template<typename Key, typename Value>
    constexpr bool IsMapLike<std::unordered_map<Key, Value>> = true;

    inline std::string ToStringPrecision(double x, const char* fmt)
    {
        char buf[128];
        snprintf(buf, sizeof(buf), fmt, x);
        return buf;
    }

    // Format must be something like %0.1f %s
    inline std::string StorageUnitDynamic(double bytes, const char* fmt)
    {
        constexpr double KiB = 1024;
        constexpr double MiB = 1024 * KiB;
        constexpr double GiB = 1024 * MiB;
        char buf[128];
        if (bytes >= GiB)
        {
            snprintf(buf, sizeof(buf), fmt, bytes * (1 / GiB), "GiB");
            return buf;
        }
        if (bytes >= MiB)
        {
            snprintf(buf, sizeof(buf), fmt, bytes * (1 / MiB), "MiB");
            return buf;
        }
        if (bytes >= KiB)
        {
            snprintf(buf, sizeof(buf), fmt, bytes * (1 / KiB), "KiB");
            return buf;
        }

        snprintf(buf, sizeof(buf), fmt, bytes, "B");
        return buf;
    }

    template<typename Func>
    size_t RetrySocketOp(Func func, size_t retries, const char* socketOp)
    {
        ssize_t ret;
        size_t tries = 0;
        do
        {
            ret = func();
            if (ret < 0)
            {
                // Error
                LOG("RetrySocketOp: " << socketOp << " failed with " << ret);
            }
            else
            {
                return ret;
            }
            tries++;
        } while (tries < retries);
        LOG("RetrySocketOp: Failed after " << retries << "tries");
        return ret;
    }

    inline std::vector<std::string> Split(const std::string& str, char delim)
    {
        std::stringstream strstr(str);
        std::string curElem;
        std::vector<std::string> result;
        while (std::getline(strstr, curElem, delim))
        {
            if (!curElem.empty())
            {
                result.emplace_back(curElem);
            }
        }
        return result;
    }

    inline std::string FindFileWithName(const std::string& directory, const std::string& name)
    {
        if (!std::filesystem::exists(directory))
        {
            return "";
        }
        for (auto& path : std::filesystem::recursive_directory_iterator(directory))
        {
            if (path.is_directory())
                continue;
            if (path.path().filename().string().find(name) != std::string::npos)
            {
                return path.path().string();
            }
        }
        return "";
    }

    inline void Replace(std::string& str, const std::string& string, const std::string& replacement)
    {
        size_t curPos = 0;
        curPos = str.find(string);
        while (curPos != std::string::npos)
        {
            str.replace(curPos, string.length(), replacement);
            curPos += string.length();
            curPos = str.find(string, curPos);
        }
    }
}

struct Process
{
    pid_t pid;
};

inline Process OpenProcess(const std::string& command)
{
    pid_t child = fork();
    ASSERT(child != -1, "fork error");

    if (child == 0)
    {
        // Child
        system(command.c_str());
        exit(0);
    }
    else
    {
        return {child};
    }
}

template<typename Data>
struct AsyncAtomicContext
{
    std::atomic<bool> running = false;
    std::mutex queueLock;
    std::optional<Data> queue;
};

// Executes the callback function asynchronously, but only one at a time.
// Multiple requests at once are stored in a FIFO of size 1.
// The context should point to a static reference.
template<typename Data, typename Callback>
inline void ExecuteAsyncAtomically(AsyncAtomicContext<Data>& context, const Callback& callback, const Data& data)
{
    // Update the queue
    context.queueLock.lock();
    context.queue = data;
    context.queueLock.unlock();

    if (!context.running)
    {
        // Launch the thread
        context.running = true;
        std::thread(
            [&, callback]()
            {
                while (true)
                {
                    context.queueLock.lock();
                    if (!context.queue.has_value())
                    {
                        context.queueLock.unlock();
                        break;
                    }
                    Data data = std::move(*context.queue);
                    context.queue = {};
                    context.queueLock.unlock();

                    // Execute
                    callback(std::move(data));
                }
                context.running = false;
            })
            .detach();
    }
}

// Plugins
#include "Window.h"
#define DL_VERSION 1

#define DEFINE_PLUGIN(fun)                                              \
    extern "C" int32_t Plugin_GetVersion()                              \
    {                                                                   \
        return DL_VERSION;                                              \
    };                                                                  \
    extern "C" void Plugin_InvokeCreateFun(void* window, void* monitor) \
    {                                                                   \
        fun(*(Window*)window, *(const std::string&*)monitor);           \
    }

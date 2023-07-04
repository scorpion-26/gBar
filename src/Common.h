#pragma once
#include <iostream>
#include <unistd.h>
#include <string>
#include <filesystem>

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

    inline std::string FindFileWithName(const std::string& directory, const std::string& name, const std::string& extension)
    {
        for (auto& path : std::filesystem::recursive_directory_iterator(directory))
        {
            if (path.is_directory())
                continue;
            if (path.path().filename().string().find(name) != std::string::npos && path.path().extension().string() == extension)
            {
                return path.path().string();
            }
        }
        return "";
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

// Plugins
#include "Window.h"
#define DL_VERSION 1

#define DEFINE_PLUGIN(fun)                                                \
    extern "C" int32_t Plugin_GetVersion()                                \
    {                                                                     \
        return DL_VERSION;                                                \
    };                                                                    \
    extern "C" void Plugin_InvokeCreateFun(void* window, int32_t monitor) \
    {                                                                     \
        fun(*(Window*)window, monitor);                                   \
    }

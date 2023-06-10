#include "Config.h"
#include "Common.h"

#include <fstream>

static Config config;

const Config& Config::Get()
{
    return config;
}

template<typename T>
void ApplyProperty(T& propertyToSet, const std::string_view& value);

template<>
void ApplyProperty<std::string>(std::string& propertyToSet, const std::string_view& value)
{
    propertyToSet = value;
}

template<>
void ApplyProperty<uint32_t>(uint32_t& propertyToSet, const std::string_view& value)
{
    // Why, C++?
    std::string valStr = std::string(value);
    propertyToSet = atoi(valStr.c_str());
}

template<>
void ApplyProperty<int32_t>(int32_t& propertyToSet, const std::string_view& value)
{
    // Why, C++?
    std::string valStr = std::string(value);
    propertyToSet = atoi(valStr.c_str());
}

template<>
void ApplyProperty<double>(double& propertyToSet, const std::string_view& value)
{
    // Why, C++?
    std::string valStr = std::string(value);
    propertyToSet = std::stod(valStr.c_str());
}

template<>
void ApplyProperty<bool>(bool& propertyToSet, const std::string_view& value)
{
    // Why, C++?
    if (value == "true")
    {
        propertyToSet = true;
    }
    else if (value == "false")
    {
        propertyToSet = false;
    }
    else
    {
        LOG("Invalid value for bool property: " << value);
    }
}

template<>
void ApplyProperty<std::pair<std::string, uint32_t>>(std::pair<std::string, uint32_t>& propertyToSet, const std::string_view& value)
{
    // TODO: Ignore escaped delimiter (e.g. \, is the same as ,)
    const char delim = ',';
    const char* whitespace = " \t";
    size_t delimPos = value.find(delim);
    if (delimPos == std::string::npos)
    {
        propertyToSet = {std::string(value), 0};
        return;
    }
    std::string_view before = value.substr(0, delimPos);
    std::string_view after = value.substr(delimPos + 1);

    // Strip whitespaces for before
    size_t beginBefore = before.find_first_not_of(whitespace);
    size_t endBefore = before.find_last_not_of(whitespace);
    before = before.substr(beginBefore, endBefore - beginBefore + 1);

    ApplyProperty(propertyToSet.first, before);
    ApplyProperty(propertyToSet.second, after);
}

template<typename T>
void AddConfigVar(const std::string& propertyName, T& propertyToSet, std::string_view line, bool& setConfig)
{
    const char* whitespace = " \t";
    if (setConfig)
    {
        // Don't bother, already found something else
        return;
    }

    // Strip empty space at the beginning
    size_t firstChar = line.find_last_not_of(whitespace);
    if (firstChar == std::string::npos)
    {
        // Line is empty, don't need to check anymore
        setConfig = true;
        return;
    }
    line = line.substr(line.find_first_not_of(whitespace));

    // Check if line starts with [propertyName]:
    if (line.find(propertyName + ":") != 0)
    {
        return;
    }
    size_t colon = line.find_first_of(":");

    // Now get the value part
    std::string_view value = line.substr(colon + 1);
    size_t beginValue = value.find_first_not_of(whitespace);
    size_t endValue = value.find_last_not_of(whitespace);
    value = value.substr(beginValue, endValue - beginValue + 1);

    // Set value
    ApplyProperty<T>(propertyToSet, value);
    LOG("Set value for " << propertyName << ": " << value);
    setConfig = true;
}

void Config::Load()
{
    const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
    std::ifstream file;
    if (xdgConfigHome)
    {
        file = std::ifstream(std::string(xdgConfigHome) + "/gBar/config");
    }
    else
    {
        std::string home = getenv("HOME");
        file = std::ifstream(home + "/.config/gBar/config");
    }
    if (!file.is_open())
    {
        LOG("Failed opening config!");
        return;
    }
    std::string line;
    while (std::getline(file, line))
    {
        std::string_view lineView = {line};
        // Strip comments
        size_t comment = line.find_first_of('#');
        if (comment == 0)
        {
            continue;
        }
        if (comment != std::string_view::npos)
        {
            lineView = lineView.substr(0, comment - 1);
        }

        bool foundProperty = false;
        AddConfigVar("CPUThermalZone", config.cpuThermalZone, lineView, foundProperty);
        AddConfigVar("NetworkAdapter", config.networkAdapter, lineView, foundProperty);
        AddConfigVar("SuspendCommand", config.suspendCommand, lineView, foundProperty);
        AddConfigVar("LockCommand", config.lockCommand, lineView, foundProperty);
        AddConfigVar("ExitCommand", config.exitCommand, lineView, foundProperty);
        AddConfigVar("BatteryFolder", config.batteryFolder, lineView, foundProperty);
        AddConfigVar("DefaultWorkspaceSymbol", config.defaultWorkspaceSymbol, lineView, foundProperty);
        AddConfigVar("DateTimeStyle", config.dateTimeStyle, lineView, foundProperty);
        AddConfigVar("CheckPackagesCommand", config.checkPackagesCommand, lineView, foundProperty);
        for (int i = 1; i < 10; i++)
        {
            // Subtract 1 to index from 1 to 9 rather than 0 to 8
            AddConfigVar("WorkspaceSymbol-" + std::to_string(i), config.workspaceSymbols[i - 1], lineView, foundProperty);
        }

        AddConfigVar("CenterTime", config.centerTime, lineView, foundProperty);
        AddConfigVar("AudioInput", config.audioInput, lineView, foundProperty);
        AddConfigVar("AudioRevealer", config.audioRevealer, lineView, foundProperty);
        AddConfigVar("NetworkWidget", config.networkWidget, lineView, foundProperty);
        AddConfigVar("WorkspaceScrollOnMonitor", config.workspaceScrollOnMonitor, lineView, foundProperty);
        AddConfigVar("WorkspaceScrollInvert", config.workspaceScrollInvert, lineView, foundProperty);
        AddConfigVar("UseHyprlandIPC", config.useHyprlandIPC, lineView, foundProperty);
        AddConfigVar("EnableSNI", config.enableSNI, lineView, foundProperty);

        AddConfigVar("MinUploadBytes", config.minUploadBytes, lineView, foundProperty);
        AddConfigVar("MaxUploadBytes", config.maxUploadBytes, lineView, foundProperty);
        AddConfigVar("MinDownloadBytes", config.minDownloadBytes, lineView, foundProperty);
        AddConfigVar("MaxDownloadBytes", config.maxDownloadBytes, lineView, foundProperty);

        AddConfigVar("CheckUpdateInterval", config.checkUpdateInterval, lineView, foundProperty);

        AddConfigVar("TimeSpace", config.timeSpace, lineView, foundProperty);

        AddConfigVar("AudioScrollSpeed", config.audioScrollSpeed, lineView, foundProperty);

        AddConfigVar("AudioMinVolume", config.audioMinVolume, lineView, foundProperty);
        AddConfigVar("AudioMaxVolume", config.audioMaxVolume, lineView, foundProperty);

        std::pair<std::string, uint32_t> buf;
        bool hasntFoundProperty = !foundProperty;
        AddConfigVar("SNIIconSize", buf, lineView, foundProperty);
        if (foundProperty && hasntFoundProperty)
        {
            // This was found
            config.sniIconSizes[buf.first] = buf.second;
        }
        hasntFoundProperty = !foundProperty;
        AddConfigVar("SNIPaddingTop", buf, lineView, foundProperty);
        if (foundProperty && hasntFoundProperty)
        {
            config.sniPaddingTop[buf.first] = buf.second;
        }

        if (foundProperty == false)
        {
            LOG("Warning: unknown config var: " << lineView);
            continue;
        }
    }
}

RuntimeConfig& RuntimeConfig::Get()
{
    static RuntimeConfig config;
    return config;
}

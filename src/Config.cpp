#include "Config.h"
#include "Common.h"

#include <fstream>

static Config config;

const Config& Config::Get()
{
    return config;
}

void PrintLegacyVariable(const std::string_view& oldName, const std::string_view& newName)
{
    LOG("Warning: Legacy variable \"" << oldName << "\" used.");
    LOG("         Please consider switching to its new alias \"" << newName << "\" instead!");
}

void PrintLegacyNotation(const std::string_view& variableName, const std::string_view& newNotation)
{
    LOG("Warning: Legacy notation for \"" << variableName << "\" used.");
    LOG("         Please consider switching to its new alias \"" << newNotation << "\" instead!");
}

template<typename T>
void ApplyProperty(T& propertyToSet, const std::string_view& value);

template<>
void ApplyProperty<std::string>(std::string& propertyToSet, const std::string_view& value)
{
    propertyToSet = value;

    // Escape characters. This is a very hacky way to this, I know.
    // TODO: Do something more efficient
    Utils::Replace(propertyToSet, "\\n", "\n");
    Utils::Replace(propertyToSet, "\\\\", "\\");
    Utils::Replace(propertyToSet, "\\t", "\t");
    Utils::Replace(propertyToSet, "\\s", " ");
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
void ApplyProperty<char>(char& propertyToSet, const std::string_view& value)
{
    if (value.size() > 1)
    {
        LOG("Invalid size for char property: " << value);
        return;
    }
    propertyToSet = value[0];
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

// Why does C++ not have partial template specialization?
template<typename First, typename Second>
void ApplyProperty(std::pair<First, Second>& propertyToSet, const std::string_view& value)
{
    // TODO: Ignore escaped delimiter (e.g. \, is the same as ,)
    const char delim = ',';
    const char* whitespace = " \t";
    size_t delimPos = value.find(delim);
    if (delimPos == std::string::npos)
    {
        return;
    }
    std::string_view before = value.substr(0, delimPos);
    std::string_view after = value.substr(delimPos + 1);

    // Strip whitespaces for before
    size_t beginBefore = before.find_first_not_of(whitespace);
    size_t endBefore = before.find_last_not_of(whitespace);
    before = before.substr(beginBefore, endBefore - beginBefore + 1);

    // Strip whitespace for after
    size_t beginAfter = after.find_first_not_of(whitespace);
    after = after.substr(beginAfter);

    ApplyProperty(propertyToSet.first, before);
    ApplyProperty(propertyToSet.second, after);
}

template<typename T>
void ApplyProperty(std::vector<T>& propertyToSet, const std::string_view& value)
{
    propertyToSet.clear();
    // Delete []
    size_t beginBracket = value.find('[');
    size_t endBracket = value.find(']');
    if (beginBracket == std::string::npos || endBracket == std::string::npos)
    {
        LOG("Error: Missing [ or ] for vector property!");
        return;
    }
    std::string_view elems = value.substr(beginBracket, endBracket - beginBracket + 1);
    size_t beginString = 0;
    while (true)
    {
        // Find first not whitespace
        beginString = elems.find_first_not_of("[ \t", beginString);
        size_t endString = elems.find_first_of(",]", beginString);
        if (beginString == std::string::npos || endString == std::string::npos)
            break;

        // Append to property
        std::string_view elem = elems.substr(beginString, endString - beginString);
        propertyToSet.push_back(std::string(elem));
        beginString = ++endString;
    }
}

template<typename T, std::enable_if_t<!Utils::IsMapLike<T>, bool> = true>
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
    if (beginValue == std::string::npos || endValue == std::string::npos)
    {
        value = "";
    }
    else
    {
        value = value.substr(beginValue, endValue - beginValue + 1);
    }

    // Set value
    ApplyProperty(propertyToSet, value);
    LOG("Set value for " << propertyName << ": " << value);

    setConfig = true;
}

// Specialization for std::[unordered_]map
template<typename MapLike, std::enable_if_t<Utils::IsMapLike<MapLike>, bool> = true>
void AddConfigVar(const std::string& propertyName, MapLike& propertyToSet, std::string_view line, bool& setConfig)
{
    bool hasntSetProperty = !setConfig;
    std::pair<typename MapLike::key_type, typename MapLike::mapped_type> buf;
    AddConfigVar(propertyName, buf, line, setConfig);
    if (setConfig && hasntSetProperty)
    {
        // This was found
        propertyToSet[buf.first] = buf.second;
    }
}

void Config::Load(const std::string& overrideConfigLocation)
{
    const char* xdgConfigHome = getenv("XDG_CONFIG_HOME");
    std::ifstream file;
    if (overrideConfigLocation != "")
    {
        file = std::ifstream(overrideConfigLocation + "/config");
    }
    else if (xdgConfigHome)
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
        AddConfigVar("WidgetsLeft", config.widgetsLeft, lineView, foundProperty);
        AddConfigVar("WidgetsCenter", config.widgetsCenter, lineView, foundProperty);
        AddConfigVar("WidgetsRight", config.widgetsRight, lineView, foundProperty);

        AddConfigVar("CPUThermalZone", config.cpuThermalZone, lineView, foundProperty);
        AddConfigVar("AmdGPUThermalZone", config.amdGpuThermalZone, lineView, foundProperty);
        AddConfigVar("NetworkAdapter", config.networkAdapter, lineView, foundProperty);
        AddConfigVar("DrmAmdCard", config.drmAmdCard, lineView, foundProperty);
        AddConfigVar("SuspendCommand", config.suspendCommand, lineView, foundProperty);
        AddConfigVar("LockCommand", config.lockCommand, lineView, foundProperty);
        AddConfigVar("ExitCommand", config.exitCommand, lineView, foundProperty);
        AddConfigVar("BatteryFolder", config.batteryFolder, lineView, foundProperty);
        AddConfigVar("DefaultWorkspaceSymbol", config.defaultWorkspaceSymbol, lineView, foundProperty);
        AddConfigVar("DateTimeStyle", config.dateTimeStyle, lineView, foundProperty);
        AddConfigVar("DateTimeLocale", config.dateTimeLocale, lineView, foundProperty);
        AddConfigVar("CheckPackagesCommand", config.checkPackagesCommand, lineView, foundProperty);
        AddConfigVar("DiskPartition", config.diskPartition, lineView, foundProperty);

        AddConfigVar("ShutdownIcon", config.shutdownIcon, lineView, foundProperty);
        AddConfigVar("RebootIcon", config.rebootIcon, lineView, foundProperty);
        AddConfigVar("SleepIcon", config.sleepIcon, lineView, foundProperty);
        AddConfigVar("LockIcon", config.lockIcon, lineView, foundProperty);
        AddConfigVar("ExitIcon", config.exitIcon, lineView, foundProperty);
        AddConfigVar("BTOffIcon", config.btOffIcon, lineView, foundProperty);
        AddConfigVar("BTOnIcon", config.btOnIcon, lineView, foundProperty);
        AddConfigVar("BTConnectedIcon", config.btConnectedIcon, lineView, foundProperty);
        AddConfigVar("DevKeyboardIcon", config.devKeyboardIcon, lineView, foundProperty);
        AddConfigVar("DevMouseIcon", config.devMouseIcon, lineView, foundProperty);
        AddConfigVar("DevHeadsetIcon", config.devHeadsetIcon, lineView, foundProperty);
        AddConfigVar("DevControllerIcon", config.devControllerIcon, lineView, foundProperty);
        AddConfigVar("DevUnknownIcon", config.devUnknownIcon, lineView, foundProperty);
        AddConfigVar("SpeakerMutedIcon", config.speakerMutedIcon, lineView, foundProperty);
        AddConfigVar("SpeakerHighIcon", config.speakerHighIcon, lineView, foundProperty);
        AddConfigVar("MicMutedIcon", config.micMutedIcon, lineView, foundProperty);
        AddConfigVar("MicHighIcon", config.micHighIcon, lineView, foundProperty);
        AddConfigVar("PackageOutOfDateIcon", config.packageOutOfDateIcon, lineView, foundProperty);

        AddConfigVar("ForceCSS", config.forceCSS, lineView, foundProperty);
        AddConfigVar("CenterWidgets", config.centerWidgets, lineView, foundProperty);
        AddConfigVar("AudioInput", config.audioInput, lineView, foundProperty);
        AddConfigVar("AudioRevealer", config.audioRevealer, lineView, foundProperty);
        AddConfigVar("AudioNumbers", config.audioNumbers, lineView, foundProperty);
        AddConfigVar("ManualFlyinAnimation", config.manualFlyinAnimation, lineView, foundProperty);
        AddConfigVar("NetworkWidget", config.networkWidget, lineView, foundProperty);
        AddConfigVar("WorkspaceScrollOnMonitor", config.workspaceScrollOnMonitor, lineView, foundProperty);
        AddConfigVar("WorkspaceScrollInvert", config.workspaceScrollInvert, lineView, foundProperty);
        AddConfigVar("WorkspaceHideUnused", config.workspaceHideUnused, lineView, foundProperty);
        AddConfigVar("UseHyprlandIPC", config.useHyprlandIPC, lineView, foundProperty);
        AddConfigVar("EnableSNI", config.enableSNI, lineView, foundProperty);
        AddConfigVar("SensorTooltips", config.sensorTooltips, lineView, foundProperty);
        AddConfigVar("IconsAlwaysUp", config.iconsAlwaysUp, lineView, foundProperty);

        AddConfigVar("MinUploadBytes", config.minUploadBytes, lineView, foundProperty);
        AddConfigVar("MaxUploadBytes", config.maxUploadBytes, lineView, foundProperty);
        AddConfigVar("MinDownloadBytes", config.minDownloadBytes, lineView, foundProperty);
        AddConfigVar("MaxDownloadBytes", config.maxDownloadBytes, lineView, foundProperty);

        AddConfigVar("CheckUpdateInterval", config.checkUpdateInterval, lineView, foundProperty);
        AddConfigVar("CenterSpace", config.centerSpace, lineView, foundProperty);
        AddConfigVar("MaxTitleLength", config.maxTitleLength, lineView, foundProperty);
        AddConfigVar("NumWorkspaces", config.numWorkspaces, lineView, foundProperty);
        AddConfigVar("AudioScrollSpeed", config.audioScrollSpeed, lineView, foundProperty);
        AddConfigVar("SensorSize", config.sensorSize, lineView, foundProperty);
        AddConfigVar("NetworkIconSize", config.networkIconSize, lineView, foundProperty);
        AddConfigVar("BatteryWarnThreshold", config.batteryWarnThreshold, lineView, foundProperty);

        AddConfigVar("AudioMinVolume", config.audioMinVolume, lineView, foundProperty);
        AddConfigVar("AudioMaxVolume", config.audioMaxVolume, lineView, foundProperty);

        AddConfigVar("Location", config.location, lineView, foundProperty);

        AddConfigVar("SNIIconSize", config.sniIconSizes, lineView, foundProperty);
        AddConfigVar("SNIPaddingTop", config.sniPaddingTop, lineView, foundProperty);
        AddConfigVar("SNIIconName", config.sniIconNames, lineView, foundProperty);
        AddConfigVar("SNIDisabled", config.sniDisabled, lineView, foundProperty);
        // Modern map syntax
        AddConfigVar("WorkspaceSymbol", config.workspaceSymbols, lineView, foundProperty);

        // Legacy WorkspaceSymbol
        bool hasFoundProperty = false;
        for (int i = 1; i < 10; i++)
        {
            // Subtract 1 to index from 1 to 9 rather than 0 to 8
            std::string symbol;
            hasFoundProperty = foundProperty;
            AddConfigVar("WorkspaceSymbol-" + std::to_string(i), symbol, lineView, foundProperty);
            if (foundProperty && !hasFoundProperty)
            {
                config.workspaceSymbols[i] = symbol;
                PrintLegacyNotation("WorkspaceSymbol", "WorkspaceSymbol: [number], [symbol]");
            }
        }

        // Legacy center configuration (CenterTime, TimeSpace)
        hasFoundProperty = foundProperty;
        AddConfigVar("CenterTime", config.centerWidgets, lineView, foundProperty);
        if (foundProperty && !hasFoundProperty)
        {
            PrintLegacyVariable("CenterTime", "CenterWidgets");
        }
        hasFoundProperty = foundProperty;
        AddConfigVar("TimeSpace", config.centerSpace, lineView, foundProperty);
        if (foundProperty && !hasFoundProperty)
        {
            PrintLegacyVariable("TimeSpace", "CenterSpace");
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

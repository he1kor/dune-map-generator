#pragma once

enum class Resource{
    DUNES,
    SAND,
    BASIC_SPICE,
    THICK_SPICE
};

inline std::optional<Resource> resourceFromString(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    static std::unordered_map<std::string, Resource> map = {
        {"dunes", Resource::DUNES},
        {"sand", Resource::SAND},
        {"spice", Resource::BASIC_SPICE},
        {"thick_spice", Resource::THICK_SPICE}
    };
    auto mapIt = map.find(lowerStr);
    if (mapIt == map.end()){
        return std::nullopt;
    }
    return mapIt->second;
}
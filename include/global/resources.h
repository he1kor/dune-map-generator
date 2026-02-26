#pragma once

enum class Resource{
    ROCK,
    DUNES,
    SAND,
    BASIC_SPICE,
    THICK_SPICE
};

inline std::string resourceToString(Resource resource) {
    switch(resource) {
        case Resource::ROCK: return "ROCK";
        case Resource::DUNES: return "DUNES";
        case Resource::SAND: return "SAND";
        case Resource::BASIC_SPICE: return "BASIC_SPICE";
        case Resource::THICK_SPICE: return "THICK_SPICE";
        default: return "UNKNOWN";
    }
}

inline std::optional<Resource> resourceFromString(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), ::tolower);
    static std::unordered_map<std::string, Resource> map = {
        {"rock", Resource::ROCK},
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
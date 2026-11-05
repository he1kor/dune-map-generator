#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

enum class Resource{
    ROCK,
    DUNES,
    SAND,
    BASIC_SPICE,
    THICK_SPICE,
    CLIFF_ROCK,
    NONE
};

inline std::string resourceToString(Resource resource) {
    switch(resource) {
        case Resource::ROCK: return "ROCK";
        case Resource::DUNES: return "DUNES";
        case Resource::SAND: return "SAND";
        case Resource::BASIC_SPICE: return "BASIC_SPICE";
        case Resource::THICK_SPICE: return "THICK_SPICE";
        case Resource::CLIFF_ROCK: return "CLIFF_ROCK";
        case Resource::NONE: return "NONE";
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
        {"thick_spice", Resource::THICK_SPICE},
        {"cliff_rock", Resource::CLIFF_ROCK}
    };
    auto mapIt = map.find(lowerStr);
    if (mapIt == map.end()){
        return std::nullopt;
    }
    return mapIt->second;
}

inline constexpr uint32_t resourceColor(Resource resource) {
    switch (resource) {
        case Resource::ROCK: return 0xff254d74;
        case Resource::CLIFF_ROCK: return 0xff14283f;
        case Resource::SAND: return 0xffb2d7e3;
        case Resource::DUNES: return 0xff7ab1c9;
        case Resource::BASIC_SPICE: return 0xff0499ff;
        case Resource::THICK_SPICE: return 0xff0766bb;
        case Resource::NONE: return 0xff202028;
        default: return 0xff000000;
    }
}

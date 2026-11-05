#pragma once

#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

struct MapDimensions {
    int width = 128;
    int height = 128;
    bool operator==(const MapDimensions&) const = default;
};

enum class DimensionPriority { BOTH, WIDTH, HEIGHT };

struct MapDimensionLimits {
    static constexpr int minimumSide = 32;
    static constexpr int maximumSide = 128;
    int minWidth = minimumSide;
    int maxWidth = maximumSide;
    int minHeight = minimumSide;
    int maxHeight = maximumSide;
    double minAspectRatio = 0.25;
    double maxAspectRatio = 4.0;

    bool operator==(const MapDimensionLimits&) const = default;

    bool allows(MapDimensions size) const {
        if (size.width < minimumSide || size.width > maximumSide || size.height < minimumSide || size.height > maximumSide
            || size.width % 2 != 0 || size.height % 2 != 0
            || size.width < minWidth || size.width > maxWidth || size.height < minHeight || size.height > maxHeight)
            return false;
        const double ratio = static_cast<double>(size.width) / size.height;
        return ratio + 1e-9 >= minAspectRatio && ratio <= maxAspectRatio + 1e-9;
    }

    bool hasAllowedSize() const {
        for (int width = minimumSide; width <= maximumSide; width += 2)
            for (int height = minimumSide; height <= maximumSide; height += 2)
                if (allows({width, height})) return true;
        return false;
    }

    bool squareOnly() const { return minAspectRatio == 1.0 && maxAspectRatio == 1.0; }

    MapDimensions closest(MapDimensions requested, DimensionPriority priority = DimensionPriority::BOTH) const {
        std::pair<long long, long long> bestScore{std::numeric_limits<long long>::max(), 0};
        MapDimensions best;
        bool found = false;
        for (int width = minimumSide; width <= maximumSide; width += 2) {
            for (int height = minimumSide; height <= maximumSide; height += 2) {
                if (!allows({width, height})) continue;
                const auto dx = std::llabs(static_cast<long long>(width) - requested.width);
                const auto dy = std::llabs(static_cast<long long>(height) - requested.height);
                const auto score = priority == DimensionPriority::WIDTH ? std::pair{dx, dy}
                    : priority == DimensionPriority::HEIGHT ? std::pair{dy, dx} : std::pair{dx + dy, dx};
                if (!found || score < bestScore) {
                    best = {width, height};
                    bestScore = score;
                    found = true;
                }
            }
        }
        if (!found) throw std::invalid_argument("Template dimensions permit no even map size from 32 to 128");
        return best;
    }

    std::vector<int> allowedSides(bool widthAxis) const {
        std::vector<int> result;
        for (int side = minimumSide; side <= maximumSide; side += 2) {
            for (int other = minimumSide; other <= maximumSide; other += 2) {
                if (allows(widthAxis ? MapDimensions{side, other} : MapDimensions{other, side})) {
                    result.push_back(side);
                    break;
                }
            }
        }
        return result;
    }
};

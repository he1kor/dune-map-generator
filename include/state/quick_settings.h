#pragma once
#include "limited_setting.h"
#include "limited_int.h"
#include "map_dimensions.h"

class QuickSettings{
public:
    QuickSettings();
    LimitedSetting amountOfSpice;
    LimitedSetting amountOfDunes;
    LimitedSetting amountOfInfantryCliffs;
    LimitedInt spotCount;
    LimitedInt width;
    LimitedInt height;
    MapDimensionLimits dimensionLimits;
    bool sizeAdjustedToTemplate = false;
    MapDimensions dimensions() const;
    void applyDimensionLimits(const MapDimensionLimits& limits);
    void setWidth(int value);
    void setHeight(int value);
private:
    void setDimensions(MapDimensions size, DimensionPriority priority);
};

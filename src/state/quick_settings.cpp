#include "quick_settings.h"

QuickSettings::QuickSettings(){
    amountOfSpice = LimitedSetting(0.0f, 100.0f, 30.0f);
    amountOfDunes = LimitedSetting(0.0f, 100.0f, 10.0f);
    amountOfInfantryCliffs = LimitedSetting(0.0f, 100.0f, 5.0f);
    spotCount = LimitedInt(1, 8, 1);
    width = LimitedInt(MapDimensionLimits::minimumSide, MapDimensionLimits::maximumSide, 64);
    height = LimitedInt(MapDimensionLimits::minimumSide, MapDimensionLimits::maximumSide, 64);
}

MapDimensions QuickSettings::dimensions() const {
    return {width.getValue(), height.getValue()};
}

void QuickSettings::applyDimensionLimits(const MapDimensionLimits& limits) {
    const auto previous = dimensions();
    const auto adjusted = limits.closest(previous);
    if (dimensionLimits != limits || adjusted != previous)
        sizeAdjustedToTemplate = adjusted != previous;
    dimensionLimits = limits;
    width.setValue(adjusted.width);
    height.setValue(adjusted.height);
}

void QuickSettings::setDimensions(MapDimensions size, DimensionPriority priority) {
    const auto adjusted = dimensionLimits.closest(size, priority);
    width.setValue(adjusted.width);
    height.setValue(adjusted.height);
    sizeAdjustedToTemplate = false;
}

void QuickSettings::setWidth(int value) {
    setDimensions({value, height.getValue()}, DimensionPriority::WIDTH);
}

void QuickSettings::setHeight(int value) {
    setDimensions({width.getValue(), value}, DimensionPriority::HEIGHT);
}

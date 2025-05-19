#include "quick_settings.h"

QuickSettings::QuickSettings(){
    amountOfSpice = LimitedSetting(0.0f, 100.0f, 30.0f);
    amountOfDunes = LimitedSetting(0.0f, 100.0f, 10.0f);
    amountOfInfantryCliffs = LimitedSetting(0.0f, 100.0f, 5.0f);
    spotCount = LimitedInt(1, 8, 1);
    width = LimitedInt(32, 128, 64);
    height = LimitedInt(32, 128, 64);
}
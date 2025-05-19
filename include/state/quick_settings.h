#pragma once
#include "limited_setting.h"
#include "limited_int.h"

class QuickSettings{
public:
    QuickSettings();
    LimitedSetting amountOfSpice;
    LimitedSetting amountOfDunes;
    LimitedSetting amountOfInfantryCliffs;
    LimitedInt spotCount;
    LimitedInt width;
    LimitedInt height;
private:
};
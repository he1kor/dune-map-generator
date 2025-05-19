#pragma once
#include "quick_settings.h"

class QuickSettingsUI{
    public:
        QuickSettingsUI(QuickSettings* quickSettingsPtr);
        void build();
    private:
        QuickSettings* quickSettingsPtr;
};
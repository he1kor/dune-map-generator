#pragma once

class LimitedSetting{
    public:
        LimitedSetting();
        bool setValue(float value);
        LimitedSetting(float min_value, float max_value, float default_value);
        float getMaxLimit();
        float getMinLimit();
        float getValue();
    private:
        float value;
        float max_value;
        float min_value;
};
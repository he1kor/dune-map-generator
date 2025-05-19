#include "limited_setting.h"

LimitedSetting::LimitedSetting(){
    min_value = 0;
    max_value = 0;
    value = 0;
}

LimitedSetting::LimitedSetting(float min_value, float max_value, float default_value){
    this->min_value = min_value;
    this->max_value = max_value;
    this->value = default_value;
}

bool LimitedSetting::setValue(float value){
    if (value > max_value || value < min_value){
        return false;
    }
    this->value = value;
    return true;
}

float LimitedSetting::getMaxLimit(){
    return max_value;
}

float LimitedSetting::getMinLimit(){
    return min_value;
}

float LimitedSetting::getValue(){
    return value;
}

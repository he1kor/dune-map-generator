#include "limited_int.h"
#include <algorithm>

LimitedInt::LimitedInt(){
    min_value = 0;
    max_value = 0;
    value = 0;
}

LimitedInt::LimitedInt(int min_value, int max_value, int default_value){
    this->min_value = min_value;
    this->max_value = max_value;
    this->value = default_value;
}

bool LimitedInt::setValue(int value){
    if (value > max_value || value < min_value){
        this->value = std::clamp(value, min_value, max_value);
        return false;
    }
    this->value = value;
    return true;
}

int LimitedInt::getMaxLimit(){
    return max_value;
}

int LimitedInt::getMinLimit(){
    return min_value;
}

int LimitedInt::getValue(){
    return value;
}

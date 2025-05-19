#include "slider.h"
#include "imgui.h"

void buildSlider(LimitedSetting &limitedSetting, const char *value_name, const char *id){
    ImGui::Text(value_name);
    float x = limitedSetting.getValue();
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat(id, &x, limitedSetting.getMinLimit(), limitedSetting.getMaxLimit(), "%.2f");
    limitedSetting.setValue(x);
}
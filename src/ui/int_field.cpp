#include "int_field.h"
#include "imgui.h"
#include <iostream>


void buildIntField(LimitedInt &limitedInt, const char* value_name, const char* id){
    ImGui::Text(value_name);
    int x = limitedInt.getValue();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputInt(id, &x);
    limitedInt.setValue(x);
}
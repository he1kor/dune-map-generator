#include "file_pick_button.h"
#include "imgui.h"

void buildFilePickButton(std::string name, std::string default_value, std::string id){
    ImGui::Text(name.c_str());
    ImGui::Button((default_value + "##" + id).c_str(), ImVec2(-1, 30));
}
#include "imgui.h"
#include "template_picker_ui.h"
#include <iostream>
#include "file_pick_button.h"

TemplatePickerUI::TemplatePickerUI(TemplatePicker *templatePickerPtr){
    this->templatePickerPtr = templatePickerPtr;
}

void TemplatePickerUI::build(){
    ImGui::Text("Files");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 5));


    buildFilePickButton("Template:", "Select file...", "template");
    ImGui::Dummy(ImVec2(0, 10));
    buildFilePickButton("Tileset config:", "Select file...", "config");


    float available_space = ImGui::GetContentRegionAvail().y - 40.0f - ImGui::GetStyle().ItemSpacing.y;
    ImGui::Dummy(ImVec2(0, available_space));

    if (ImGui::Button("Generate", ImVec2(-1, 40))) {
        // Generate action
    }
}

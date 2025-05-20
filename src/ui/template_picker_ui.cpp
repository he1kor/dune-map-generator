#include "imgui.h"
#include "template_picker_ui.h"
#include <iostream>
#include "file_pick_button.h"
#include "colors.h"

TemplatePickerUI::TemplatePickerUI(TemplatePicker *templatePickerPtr){
    this->templatePickerPtr = templatePickerPtr;
}

void TemplatePickerUI::build(){
    ImGui::Text("Files");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 5));



    buildFilePickButton("Template:", "Select file...", "template");


    ImGui::Dummy(ImVec2(0, 10));

    ImGui::PushStyleColor(ImGuiCol_Button, VARIANT_BASIC);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, VARIANT_EXTRA);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, VARIANT_ULTRA);
    buildFilePickButton("Tileset config:", "Select file...", "config");

    ImGui::PopStyleColor(3);

    float available_space = ImGui::GetContentRegionAvail().y - 40.0f - ImGui::GetStyle().ItemSpacing.y;
    ImGui::Dummy(ImVec2(0, available_space));

    ImGui::PushStyleColor(ImGuiCol_Button, VARIANT2_BASIC);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, VARIANT2_EXTRA);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, VARIANT2_ULTRA);
    if (ImGui::Button("Generate", ImVec2(-1, 40))) {
        // Generate action
    }
    ImGui::PopStyleColor(3);

}

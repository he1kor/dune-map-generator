#include "imgui.h"
#include "template_picker_ui.h"
#include <iostream>
#include "file_pick_button.h"
#include "colors.h"
#include <string>


TemplatePickerUI::TemplatePickerUI(TemplatePicker *templatePickerPtr, Generation *generationPtr){
    this->templatePickerPtr = templatePickerPtr;
    this->generationPtr = generationPtr;
}

void TemplatePickerUI::buildGenerateButton(){
    if (generationPtr->generationProgress == 1.0f || generationPtr->generationProgress == 0.0f){
        if (ImGui::Button("Generate", ImVec2(-1, 30))){
        //TODO: Generation
        }
    }
    else ImGui::Button("Generation...", ImVec2(-1, 30));
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

    float available_space = ImGui::GetContentRegionAvail().y - 30.0f - ImGui::GetStyle().ItemSpacing.y;
    ImGui::Dummy(ImVec2(0, available_space));

    ImGui::PushStyleColor(ImGuiCol_Button, VARIANT2_BASIC);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, VARIANT2_EXTRA);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, VARIANT2_ULTRA);
    
    buildGenerateButton();

    ImGui::PopStyleColor(3);

}

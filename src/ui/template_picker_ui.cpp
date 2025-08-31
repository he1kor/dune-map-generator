#include "imgui.h"
#include "template_picker_ui.h"
#include "colors.h"
#include <string>


TemplatePickerUI::TemplatePickerUI(TemplatePicker *templatePickerPtr, Generation *generationPtr){
    this->templatePickerPtr = templatePickerPtr;
    this->generationPtr = generationPtr;
}

void TemplatePickerUI::buildGenerateButton(){
    if (generationPtr->generationProgress == 1.0f || generationPtr->generationProgress == 0.0f){
        if (ImGui::Button("Generate", ImVec2(-1, 30))){
            generationPtr->generate();
        }
    }
    else ImGui::Button("Generation...", ImVec2(-1, 30));
}

void TemplatePickerUI::buildTemplatePickButton(){
    if (buildFilePickButton("Template:", "Select file...", "template")){

    }
}

bool TemplatePickerUI::buildFilePickButton(std::string name, std::string default_value, std::string id){
    ImGui::Text(name.c_str());
    return ImGui::Button((default_value + "##" + id).c_str(), ImVec2(-1, 30));
}

void TemplatePickerUI::build(){
    ImGui::Text("Files");
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 5));



    buildTemplatePickButton();


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

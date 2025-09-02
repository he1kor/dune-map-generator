#include "imgui.h"
#include "template_picker_ui.h"
#include "colors.h"
#include <string>

#include "filedialog.h"
#include "systemsound.h"


TemplatePickerUI::TemplatePickerUI(TemplatePicker *templatePickerPtr, Generation *generationPtr, MapRenderer& mapRenderer) : mapRenderer{mapRenderer}{
    this->templatePickerPtr = templatePickerPtr;
    this->generationPtr = generationPtr;
}

void TemplatePickerUI::buildError(){
    
    if (ImGui::BeginPopup("Error")) {
        ImGui::Text("Error:");
        ImGui::TextWrapped("%s", errorText.c_str());
        ImGui::Separator();
        
        if (ImGui::Button("OK", ImVec2(220, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void TemplatePickerUI::initError(const std::string &text){
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(1000, 0), ImGuiCond_Appearing);
    errorText = text;
    ImGui::OpenPopup("Error");
    syssound::playError();
}

void TemplatePickerUI::buildErrors()
{
    if (templatePickerPtr->syntaxError.ocurred){
        templatePickerPtr->syntaxError.ocurred = false;
        initError("Invalid JSON syntax");
    }
    if (templatePickerPtr->noKeyError.ocurred){
        templatePickerPtr->noKeyError.ocurred = false;
        initError("Missing JSON key \"" + templatePickerPtr->noKeyError.missingKey + "\"");
    }
    if (templatePickerPtr->badValueError.ocurred){
        templatePickerPtr->badValueError.ocurred = false;
        initError("Invalid value by \"" + templatePickerPtr->badValueError.targetKey + "\" (expected " + templatePickerPtr->badValueError.expectedType + ")");
    }
    buildError();
}

void TemplatePickerUI::buildGenerateButton(){
    if (generationPtr->generationProgress == 1.0f || generationPtr->generationProgress == 0.0f){
        bool disabled = templatePickerPtr->mapTemplate == nullptr;
        ImGui::BeginDisabled(disabled);
        if (ImGui::Button("Generate", ImVec2(-1, 30))){
            generationPtr->generate(templatePickerPtr->mapTemplate);
            mapRenderer.setDistinctColors(*templatePickerPtr->mapTemplate);
        }
        if (disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("No template is set!");
        }
        ImGui::EndDisabled();
    }
    else ImGui::Button("Generation...", ImVec2(-1, 30));
}

void TemplatePickerUI::buildTemplatePickButton(){
    if (buildFilePickButton("Template:", templatePickerPtr->templateName, "template")){
        auto expectedPath = fileDialog.tryGetPathFromDialog();
        if (expectedPath){
            templatePickerPtr->tryReadTemplate(*expectedPath);
        }
    }
}

bool TemplatePickerUI::buildFilePickButton(const std::string& name, const std::string& value, const std::string& id){
    ImGui::Text(name.c_str());
    return ImGui::Button((value + "##" + id).c_str(), ImVec2(-1, 30));
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
    
    buildErrors();
}

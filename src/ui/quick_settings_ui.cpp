#include "imgui.h"
#include "quick_settings_ui.h"
#include "slider.h"
#include "int_field.h"
#include <iostream>

QuickSettingsUI::QuickSettingsUI(QuickSettings *quickSettingsPtr){
    this->quickSettingsPtr = quickSettingsPtr;
}

void QuickSettingsUI::build(){
    ImGui::Text("Settings");
    ImGui::Dummy(ImVec2(0, 5));

    ImGui::BeginGroup();
        {
            const float column_width = ImGui::GetContentRegionAvail().x;
            const float spacing = 8.0f;
            const float symbol_width = ImGui::CalcTextSize("×").x;
            const float input_width = (column_width - symbol_width - spacing * 2) / 2.0f;

            // LABELS ROW (left-aligned)
            ImGui::Text("Width");
            ImGui::SameLine(input_width + 2* spacing + symbol_width);
            ImGui::Text("Height");

            // INPUTS ROW (with ×)
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().ItemSpacing.y);
            
            // Width input
            ImGui::SetNextItemWidth(input_width);
            int width = quickSettingsPtr->width.getValue();
            ImGui::InputInt("##width", &width, 0, 0, ImGuiInputTextFlags_CharsDecimal);
            
            // × symbol
            ImGui::SameLine(0, spacing);
            ImGui::Text("×");
            
            // Height input
            ImGui::SameLine(0, spacing);
            ImGui::SetNextItemWidth(input_width);
            int height = quickSettingsPtr->height.getValue();
            ImGui::InputInt("##height", &height, 0, 0, ImGuiInputTextFlags_CharsDecimal);

            quickSettingsPtr->width.setValue(width);
            quickSettingsPtr->height.setValue(height);
        }
    ImGui::EndGroup();
        
    ImGui::Dummy(ImVec2(0, 10));
    buildIntField(quickSettingsPtr->spotCount, "Number of spots", "##spots");
    ImGui::Dummy(ImVec2(0, 10));
    buildSlider(quickSettingsPtr->amountOfSpice, "Amount of spice", "##spice");
    ImGui::Dummy(ImVec2(0, 10));
    buildSlider(quickSettingsPtr->amountOfDunes, "Amount of dunes", "##dunes");
    ImGui::Dummy(ImVec2(0, 10));
    buildSlider(quickSettingsPtr->amountOfInfantryCliffs, "Amount of infantry cliffs", "##inf_cliffs");
    
    
    


    
}

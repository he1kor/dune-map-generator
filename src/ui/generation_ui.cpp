#include "generation_ui.h"
#include "imgui.h"
#include "map_render.h"
#include <iostream>
#include <string>


GenerationUI::GenerationUI(Generation *generationPtr, MapRenderer *mapRendererPtr){
    this->generationPtr = generationPtr;
    this->mapRendererPtr = mapRendererPtr;
}
void GenerationUI::build(){
    ImGui::Text("Generation");
    ImGui::Dummy(ImVec2(0, 5));
    
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float controlsHeight = 5.0f + 2 * spacing + ImGui::GetTextLineHeightWithSpacing()
        + 2 * ImGui::GetFrameHeightWithSpacing() + 30.0f;
    mapRendererPtr->render(std::max(0.0f, ImGui::GetContentRegionAvail().y - controlsHeight));

    ImGui::Dummy(ImVec2(0, 5));
    
    ImGui::Text("Seed");
    ImGui::SetNextItemWidth(-1);
    
    
    if (generationPtr->isSeedRandom) {
        ImGui::BeginDisabled();
        ImGui::InputScalar("##Seed", ImGuiDataType_S64, &(generationPtr->seed), NULL, NULL, NULL, ImGuiInputTextFlags_ReadOnly);   
        ImGui::EndDisabled();

        if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())) {
        ImGui::SetTooltip("Seed will randomize on generation");
    }
    } else {
        ImGui::InputScalar("##Seed", ImGuiDataType_S64, &(generationPtr->seed));
    }
    ImGui::Checkbox("Use random##use random seed", &(generationPtr->isSeedRandom));

    float available_space = std::max(0.0f, ImGui::GetContentRegionAvail().y - 30.0f - ImGui::GetStyle().ItemSpacing.y);
    ImGui::Dummy(ImVec2(0, available_space));
    ImGui::ProgressBar(generationPtr->generationProgress, ImVec2(-1, 30));
}

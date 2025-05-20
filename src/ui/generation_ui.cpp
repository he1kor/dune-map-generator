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
    
    mapRendererPtr->render();

    ImGui::Dummy(ImVec2(0, 5));
    
    ImGui::Text("Seed");
    ImGui::SetNextItemWidth(-1);
    
    
    if (generationPtr->is_seed_random) {
        ImGui::BeginDisabled();
        ImGui::InputScalar("##Seed", ImGuiDataType_S64, &(generationPtr->seed), NULL, NULL, NULL, ImGuiInputTextFlags_ReadOnly);   
        ImGui::EndDisabled();

        if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())) {
        ImGui::SetTooltip("Seed will randomize on generation");
    }
    } else {
        ImGui::InputScalar("##Seed", ImGuiDataType_S64, &(generationPtr->seed));
    }
    ImGui::Checkbox("Use random##use random seed", &(generationPtr->is_seed_random));

    float available_space = ImGui::GetContentRegionAvail().y - 30.0f - ImGui::GetStyle().ItemSpacing.y;
    ImGui::Dummy(ImVec2(0, available_space));
    ImGui::ProgressBar(generationPtr->generationProgress, ImVec2(-1, 30));
}

#include "generation_ui.h"
#include "imgui.h"
#include "map_render.h"
#include <iostream>
#include <string>
#include <cstdio>


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
    if (!generationPtr->isRunning() && mapRendererPtr->areZoneIdsVisible())
        mapRendererPtr->setZoneNodes(generationPtr->plane);
    mapRendererPtr->render(std::max(0.0f, ImGui::GetContentRegionAvail().y - controlsHeight));

    ImGui::Dummy(ImVec2(0, 5));
    
    ImGui::Text("Seed");
    ImGui::SetNextItemWidth(-1);
    ImGui::BeginDisabled(generationPtr->isRunning());
    
    if (generationPtr->isSeedRandom) {
        ImGui::BeginDisabled();
        ImGui::InputScalar("##Seed", ImGuiDataType_S64, &(generationPtr->seed), NULL, NULL, NULL, ImGuiInputTextFlags_ReadOnly);   
        ImGui::EndDisabled();

        if (ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax())) {
        ImGui::SetTooltip("New seed for each map.");
    }
    } else {
        ImGui::InputScalar("##Seed", ImGuiDataType_S64, &(generationPtr->seed));
    }
    ImGui::Checkbox("Random##use random seed", &(generationPtr->isSeedRandom));
    ImGui::EndDisabled();

    float available_space = std::max(0.0f, ImGui::GetContentRegionAvail().y - 30.0f - ImGui::GetStyle().ItemSpacing.y);
    ImGui::Dummy(ImVec2(0, available_space));
    char status[128];
    if (generationPtr->isRunning())
        std::snprintf(status, sizeof(status), "%s (%.0f%%)", generationPtr->statusText(), generationPtr->generationProgress * 100.0f);
    else
        std::snprintf(status, sizeof(status), "%s", generationPtr->statusText());
    const bool fits = ImGui::CalcTextSize(status).x + 2 * ImGui::GetStyle().FramePadding.x <= ImGui::GetContentRegionAvail().x;
    ImGui::ProgressBar(generationPtr->generationProgress, ImVec2(-1, 30), fits ? status : generationPtr->statusText());
    if (ImGui::IsItemHovered()) {
        if (!generationPtr->generationError.empty())
            ImGui::SetTooltip("%s", generationPtr->generationError.c_str());
        else if (generationPtr->isRunning())
            ImGui::SetTooltip("%s\nEstimated progress.", status);
    }
}

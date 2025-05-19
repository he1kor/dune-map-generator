#include "generation_ui.h"
#include "imgui.h"
#include "map_render.h"


GenerationUI::GenerationUI(Generation *generationPtr, MapRenderer *mapRendererPtr){
    this->generationPtr = generationPtr;
    this->mapRendererPtr = mapRendererPtr;
}
void GenerationUI::build()
{
    ImGui::Text("Generation");
    ImGui::Dummy(ImVec2(0, 5));
    
    mapRendererPtr->render();

    ImGui::Dummy(ImVec2(0, 5));
    ImGui::Text("Seed");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputScalar("##Seed", ImGuiDataType_S64, &generationPtr->seed);



    float available_space = ImGui::GetContentRegionAvail().y - 40.0f - ImGui::GetStyle().ItemSpacing.y;
    ImGui::Dummy(ImVec2(0, available_space));
    ImGui::ProgressBar(generationPtr->generationProgress, ImVec2(-1, 40));
}

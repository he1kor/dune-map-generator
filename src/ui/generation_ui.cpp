#include "generation_ui.h"
#include "imgui.h"

GenerationUI::GenerationUI(Generation* generationPtr){
    this->generationPtr = generationPtr;
}



void GenerationUI::showMapPreview() {

    const int map_width = 64;
    const int map_height = 48;

    // Get current column width (this will be our square size)
    const float container_size = ImGui::GetContentRegionAvail().x;
    
    // Create container (matches column width, square aspect ratio)
    ImVec2 container_min = ImGui::GetCursorScreenPos();
    ImVec2 container_max = ImVec2(container_min.x + container_size, container_min.y + container_size);
    
    // Fill container with black background
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(container_min, container_max, IM_COL32(0, 0, 0, 255));
    
    // Calculate centered position for the map
    const float map_scale = 1.0f; // 1:1 pixels (no scaling)
    const ImVec2 map_size(map_width * map_scale, map_height * map_scale);
    const ImVec2 map_offset(
        (container_size - map_size.x) * 0.5f,
        (container_size - map_size.y) * 0.5f
    );
    
    // Draw the map texture (1:1 pixels)
    ImGui::SetCursorScreenPos(ImVec2(
        container_min.x + map_offset.x,
        container_min.y + map_offset.y
    ));
    //ImGui::Image(
    //    (void*)(intptr_t)map_texture,
    //    map_size,
    //    ImVec2(0,0), ImVec2(1,1),
    //    ImVec4(1,1,1,1) // White tint
    //);
    
    // Advance cursor (important for layout)
    ImGui::SetCursorScreenPos(ImVec2(container_min.x, container_min.y + container_size));
}


void GenerationUI::build(){
    ImGui::Text("Generation");
    ImGui::Dummy(ImVec2(0, 5));
    showMapPreview();
    ImGui::Dummy(ImVec2(0, 5));
    ImGui::Text("Seed");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputInt("##Seed", &generationPtr->seed, 0, 0, ImGuiInputTextFlags_CharsDecimal);



    float available_space = ImGui::GetContentRegionAvail().y - 40.0f - ImGui::GetStyle().ItemSpacing.y;
    ImGui::Dummy(ImVec2(0, available_space));
    ImGui::ProgressBar(generationPtr->generationProgress, ImVec2(-1, 40));
}

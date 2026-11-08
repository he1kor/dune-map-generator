#include "setup.h"
#include <filesystem>
#include <iostream>
#include <imgui.h>
#include "colors.h"

void setFont(){
    const char* font_path = "C:\\Windows\\Fonts\\Verdana.ttf";
    if (!std::filesystem::exists(font_path)) {
        std::cerr << "Font file not found!" << std::endl;
    }
    auto io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF(font_path, 18.0f);
}

void setColors(){

    ImVec4* colors = ImGui::GetStyle().Colors;
    
    colors[ImGuiCol_TitleBgActive] = HARD_BROWN;
    colors[ImGuiCol_Border] = HARD_BROWN;
    colors[ImGuiCol_Separator] = MEDIUM_BROWN;
    
    colors[ImGuiCol_PopupBg] = HARD_BROWN;

    colors[ImGuiCol_SliderGrab] = PINK_HARD;
    colors[ImGuiCol_SliderGrabActive] = PINK_SOFT;

    colors[ImGuiCol_Text] = PINK_ULTRA_SOFT;
    colors[ImGuiCol_TextSelectedBg] = VARIANT_BASIC_TRANSPARENT;
    
    colors[ImGuiCol_InputTextCursor] = VARIANT2_ULTRA_TRANSPARENT;

    colors[ImGuiCol_FrameBg] = HARD_BROWN;
    colors[ImGuiCol_FrameBgHovered] = MEDIUM_BROWN; 
    colors[ImGuiCol_FrameBgActive] = VARIANT2_BASIC;
    
    colors[ImGuiCol_PlotHistogram] = RED_EXTRA;
    //colors[ImGuiCol_PlotHistogram] = VARIANT2_ULTRA;

    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_Tab] = MEDIUM_BROWN;
    colors[ImGuiCol_TabHovered] = VARIANT2_EXTRA;
    colors[ImGuiCol_TabSelected] = VARIANT2_BASIC;
    colors[ImGuiCol_TabSelectedOverline] = VARIANT2_ULTRA;
    colors[ImGuiCol_TabDimmed] = MEDIUM_BROWN;
    colors[ImGuiCol_TabDimmedSelected] = VARIANT2_BASIC;
    colors[ImGuiCol_TabDimmedSelectedOverline] = VARIANT2_ULTRA;
    
    colors[ImGuiCol_ScrollbarGrab] = HARD_BROWN;
    colors[ImGuiCol_ScrollbarGrabHovered] = MEDIUM_BROWN;
    colors[ImGuiCol_ScrollbarGrabActive] = SOFT_BROWN;
    colors[ImGuiCol_WindowBg] = SOFT_BROWN;
    



    colors[ImGuiCol_Button] = RED_BASIC;
    colors[ImGuiCol_ButtonHovered] = RED_EXTRA;
    colors[ImGuiCol_ButtonActive] = RED_ULTRA;
    colors[ImGuiCol_Header] = RED_BASIC;
    colors[ImGuiCol_HeaderHovered] = RED_EXTRA;
    colors[ImGuiCol_HeaderActive] = RED_ULTRA;
    
    colors[ImGuiCol_CheckMark] = VARIANT_ULTRA;

}

void setShape(){
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.ChildRounding = 1.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.GrabMinSize = 10.0f;
}

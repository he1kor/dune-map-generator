#include "setup.h"
#include <filesystem>
#include <iostream>
#include <imgui.h>

void setFont(){
    const char* font_path = "C:\\Windows\\Fonts\\Verdana.ttf";
    if (!std::filesystem::exists(font_path)) {
        std::cerr << "Font file not found!" << std::endl;
    }
    auto io = ImGui::GetIO();
    ImFont* font = io.Fonts->AddFontFromFileTTF(font_path, 18.0f);
}
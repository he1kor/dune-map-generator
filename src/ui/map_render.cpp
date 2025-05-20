#include "map_render.h"

MapRenderer::MapRenderer() : textureID(0), mapWidth(0), mapHeight(0) {}

MapRenderer::~MapRenderer(){
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

void MapRenderer::updateMap(const std::vector<uint32_t> &pixels, int width, int height) {
    mapWidth = width;
    mapHeight = height;

    if (textureID == 0) {
        glGenTextures(1, &textureID);
    }

    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Setup texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload pixel data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

int MapRenderer::getOptimalMapSize(int availableWidth){
    return std::min(
        (availableWidth / maxMapSize) * maxMapSize,
        (maxScaleFactor * maxMapSize)
    );
}

void MapRenderer::render(){
    if (textureID == 0 || mapWidth == 0 || mapHeight == 0) return;
    
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    float squareSize = getOptimalMapSize(static_cast<int>(availableSize.x));
    
    float scale = squareSize / maxMapSize;
    
    float renderWidth = mapWidth * scale;
    float renderHeight = mapHeight * scale;
    
    float offsetX = (availableSize.x - squareSize) * 0.5f;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
    
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImVec2 squarePos = cursorPos;
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(
        squarePos, 
        ImVec2(squarePos.x + squareSize, squarePos.y + squareSize), 
        IM_COL32(43, 33, 32, 255)
    );
    
    // Draw the map texture (centered)
    offsetX = (squareSize - renderWidth) * 0.5f;
    float offsetY = (squareSize - renderHeight) * 0.5f;
    ImGui::SetCursorScreenPos(ImVec2(
        cursorPos.x + offsetX,
        cursorPos.y + offsetY
    ));
    
    ImGui::Image(
        (ImTextureID)(intptr_t)textureID,
        ImVec2(renderWidth, renderHeight)
    );
    
    // Advance cursor position (important for column layout)
    ImGui::SetCursorScreenPos(ImVec2(
        cursorPos.x,
        cursorPos.y + squareSize + ImGui::GetStyle().ItemSpacing.y
    ));
}

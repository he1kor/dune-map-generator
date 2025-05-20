#include "map_render.h"

MapRenderer::MapRenderer(Palette *palette) : textureID(0), mapWidth(0), mapHeight(0), palette(palette) {}

MapRenderer::~MapRenderer()
{
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

const uint32_t MapRenderer::mapTile(const Tile tile){
    std::string material = palette->findMaterialName(tile.tileID);
    if (material == "sand"){
        if (tile.entityID == 1)
            return 0xff2e9ef7;
        if (tile.entityID == 2)
            return 0xff0072ce;
        return 0xff8abde2;
    }
    if (material == "rock")
        return 0xff254d74;
    if (material == "dunes")
        return 0xffc5feff;
    return 0xff091f30;
}

const std::vector<uint32_t> MapRenderer::convertMap(SmartMap map){
    std::vector<uint32_t> result(map.height() * map.width());
    for (int y = 0; y < map.height(); y++){
        for (int x = 0; x < map.width(); x++){
            result[y * map.width() + x] = mapTile(map.getTile(x, y));
        }
    }   
    return result;
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

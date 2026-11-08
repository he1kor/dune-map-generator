#include "map_render.h"
#include "generation.h"

#include <cmath>
#include <string>

MapRenderer::MapRenderer(Palette *palette) : textureID(0), mapWidth(0), mapHeight(0), palette(palette) {}

MapRenderer::~MapRenderer(){
    if (textureID != 0) {
        glDeleteTextures(1, &textureID);
    }
}

const uint32_t MapRenderer::mapTile(const Tile tile){
    const Material& material = palette->findMaterial(tile.tileID);
    if (material == palette->operator[]("sand")){
        if (tile.entityID == 1)
            return 0xff2e9ef7;
        if (tile.entityID == 2)
            return 0xff0072ce;
        return 0xff8abde2;
    }
    if (material == palette->operator[]("rock"))
        return 0xff254d74;
    if (material == palette->operator[]("dunes"))
        return 0xffc5feff;
    return 0xff091f30;
}

const uint32_t MapRenderer::mapResource(const Resource spice){
    return resourceColor(spice);
}

std::vector<uint32_t> MapRenderer::generateDistinctColors(int n){
    std::vector<uint32_t> colors;
    if (n <= 0) return colors;
    
    const double golden_ratio_conjugate = 0.618033988749895;
    double hue = 0.0; // Start from 0
    
    for (int i = 0; i < n; ++i) {
        hue += golden_ratio_conjugate;
        hue = fmod(hue, 1.0);
        
        double h = hue * 6.0;
        double r, g, b;
        
        int sector = static_cast<int>(h);
        double frac = h - sector;
        double q = 1.0 - frac;
        
        switch (sector % 6) {
            case 0: r = 1.0; g = frac; b = 0.0; break;
            case 1: r = q; g = 1.0; b = 0.0; break;
            case 2: r = 0.0; g = 1.0; b = frac; break;
            case 3: r = 0.0; g = q; b = 1.0; break;
            case 4: r = frac; g = 0.0; b = 1.0; break;
            case 5: r = 1.0; g = 0.0; b = q; break;
        }
        
        uint8_t red = static_cast<uint8_t>(r * 255);
        uint8_t green = static_cast<uint8_t>(g * 255);
        uint8_t blue = static_cast<uint8_t>(b * 255);
        
        uint32_t color = 0xff000000 | (red << 16) | (green << 8) | blue;
        colors.push_back(color);
    }
    
    return colors;
}

void MapRenderer::drawLine(std::vector<uint32_t>& pixels, int width, int height, 
                          const DoubleVector2& start, const DoubleVector2& end, uint32_t color) {
    int x0 = start.x;
    int y0 = start.y;
    int x1 = end.x;
    int y1 = end.y;

    bool steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1 - x0;
    int dy = abs(y1 - y0);
    int error = dx / 2;
    int ystep = (y0 < y1) ? 1 : -1;
    int y = y0;

    for (int x = x0; x <= x1; x++) {
        if (steep) {
            if (y >= 0 && y < width && x >= 0 && x < height) {
                pixels[x * width + y] = color;
            }
        } else {
            if (x >= 0 && x < width && y >= 0 && y < height) {
                pixels[y * width + x] = color;
            }
        }
        error -= dy;
        if (error < 0) {
            y += ystep;
            error += dx;
        }
    }
}

void MapRenderer::drawCircle(std::vector<uint32_t>& pixels, int width, int height,
                            int centerX, int centerY, int radius, uint32_t color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x*x + y*y <= radius*radius) {
                int px = centerX + x;
                int py = centerY + y;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    pixels[py * width + px] = color;
                }
            }
        }
    }
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

const std::vector<uint32_t> MapRenderer::convertResourceMatrix(const Matrix<Resource>& matrix){
    std::vector<uint32_t> result(matrix.getHeight() * matrix.getWidth());
    
    for (int y = 0; y < matrix.getHeight(); y++){
        for (int x = 0; x < matrix.getWidth(); x++){
            double color = mapResource(matrix.get(x, y));
            result[y * matrix.getWidth() + x] = color;
        }
    }   
    return result;
}

std::vector<uint32_t> MapRenderer::convertPreview(const GenerationPreview& preview) {
    zoneNodes.clear();
    for (size_t i = 0; i < std::min(preview.nodes.size(), preview.nodeIds.size()); ++i)
        zoneNodes.emplace_back(preview.nodeIds[i], preview.nodes[i]);
    std::vector<uint32_t> pixels(preview.width * preview.height, 0xff000000);
    if (!preview.zones.empty()) {
        for (size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = zoneColors.at(Identifiable(preview.zones[i]));
    } else {
        for (const auto& [start, end] : preview.edges)
            drawLine(pixels, preview.width, preview.height, start, end, 0xffaaaaaa);
        for (const auto& point : preview.nodes)
            drawCircle(pixels, preview.width, preview.height, static_cast<int>(point.x), static_cast<int>(point.y), 3, 0xff0000ff);
    }
    return pixels;
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

int MapRenderer::getOptimalMapSize(int availableWidth, int availableHeight){
    if (mapWidth <= 0 || mapHeight <= 0) return 0;
    const int scale = std::max(0, std::min(availableWidth / mapWidth, availableHeight / mapHeight));
    return scale * std::max(mapWidth, mapHeight);
}

void MapRenderer::render(float availableHeight){
    if (textureID == 0 || mapWidth == 0 || mapHeight == 0) return;
    
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    const int longestSide = getOptimalMapSize(static_cast<int>(availableSize.x), static_cast<int>(availableHeight));
    if (longestSide <= 0) return;
    const float scale = longestSide / std::max(mapWidth, mapHeight);
    
    float renderWidth = mapWidth * scale;
    float renderHeight = mapHeight * scale;
    
    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // ImGui's OpenGL sampler overrides the texture's GL_NEAREST parameters.
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    draw_list->AddCallback(platform_io.DrawCallback_SetSamplerNearest);
    // Integer scale and pixel-aligned origin keep every map cell crisp.
    ImVec2 imagePos(std::floor(cursorPos.x + (availableSize.x - renderWidth) * 0.5f), std::floor(cursorPos.y));
    draw_list->AddImage(
        (ImTextureID)(intptr_t)textureID,
        imagePos,
        ImVec2(imagePos.x + renderWidth, imagePos.y + renderHeight)
    );
    draw_list->AddCallback(platform_io.DrawCallback_SetSamplerLinear);

    if (showZoneIds) {
        draw_list->PushClipRect(imagePos, ImVec2(imagePos.x + renderWidth, imagePos.y + renderHeight), true);
        for (const auto& [id, node] : zoneNodes) {
            if (!std::isfinite(node.x) || !std::isfinite(node.y)
                || node.x < 0 || node.y < 0 || node.x >= mapWidth || node.y >= mapHeight)
                continue;
            const std::string label = std::to_string(id);
            const ImVec2 textSize = ImGui::CalcTextSize(label.c_str());
            const ImVec2 textPos(imagePos.x + node.x * scale - textSize.x * 0.5f,
                imagePos.y + node.y * scale - textSize.y * 0.5f);
            draw_list->AddRectFilled(ImVec2(textPos.x - 3, textPos.y - 1),
                ImVec2(textPos.x + textSize.x + 3, textPos.y + textSize.y + 1),
                IM_COL32(0, 0, 0, 200), 3.0f);
            draw_list->AddText(textPos, IM_COL32(255, 255, 255, 255), label.c_str());
        }
        draw_list->PopClipRect();
    }
    ImGui::Dummy(ImVec2(availableSize.x, renderHeight));
}

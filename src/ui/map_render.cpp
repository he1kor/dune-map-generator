#include "map_render.h"

MapRenderer::MapRenderer(Palette *palette) : textureID(0), mapWidth(0), mapHeight(0), palette(palette) {}

MapRenderer::~MapRenderer(){
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

const std::vector<uint32_t> MapRenderer::convertPlane(const Plane& plane, const Graph& graph) {
    int width = plane.getWidth();
    int height = plane.getHeight();
    std::vector<uint32_t> result(width * height, 0xff000000); // Initialize with black background

    // First draw edges (so spots appear on top)
    const auto& spots = plane.getSpots();
    for (int nodeId = 0; nodeId < graph.size(); ++nodeId) {
        const Node& node = graph.getNode(nodeId);
        Spot startSpot = spots[nodeId];
        
        for (int neighborId : node.getNeighbours()) {
            // To avoid drawing edges twice, only draw when neighborId > nodeId
            if (neighborId > nodeId) {
                Spot endSpot = spots[neighborId];
                drawLine(result, width, height, startSpot, endSpot, 0xffaaaaaa); // Light gray for edges
            }
        }
    }

    // Then draw spots on top of edges
    for (auto spot : spots) {
        int x = spot.getX();
        int y = spot.getY();
        if (x >= 0 && x < width && y >= 0 && y < height) {
            result[y * width + x] = 0xff0000ff; // Blue for nodes
            // Draw a small circle around the spot for better visibility
            drawCircle(result, width, height, x, y, 2, 0xff0000ff);
        }
    }

    return result;
}

void MapRenderer::drawLine(std::vector<uint32_t>& pixels, int width, int height, 
                          const Spot& start, const Spot& end, uint32_t color) {
    int x0 = start.getX();
    int y0 = start.getY();
    int x1 = end.getX();
    int y1 = end.getY();

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

// Helper function to draw a simple circle
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

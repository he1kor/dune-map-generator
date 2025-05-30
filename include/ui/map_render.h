#pragma once

#include <algorithm>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <imgui.h>
#include "smart_map.h"
#include "palette.h"
#include "plane.h"
#include "graph.h"
#include "spot.h"

class MapRenderer {
public:
    const int maxMapSize = 128;
    const int maxScaleFactor = 4;

    MapRenderer(Palette* palette);

    ~MapRenderer();

    const uint32_t mapTile(const Tile tile);

    const std::vector<uint32_t> convertPlane(const Plane& plane, const Graph& graph);

    void drawLine(std::vector<uint32_t> &pixels, int width, int height, const Spot &start, const Spot &end, uint32_t color);

    void drawCircle(std::vector<uint32_t> &pixels, int width, int height, int centerX, int centerY, int radius, uint32_t color);

    const std::vector<uint32_t> convertMap(SmartMap map);
    //RGBA format updating
    void updateMap(const std::vector<uint32_t>& pixels, int width, int height);
    int getOptimalMapSize(int availableWidth);
    void render();

private:
    GLuint textureID;
    int mapWidth;
    int mapHeight;
    Palette* palette;
};
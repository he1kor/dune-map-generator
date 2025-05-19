#pragma once

#include <algorithm>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <imgui.h>

class MapRenderer {
public:
    const int maxMapSize = 128;
    const int maxScaleFactor = 4;

    MapRenderer();

    ~MapRenderer();

    //RGBA format updating
    void updateMap(const std::vector<uint32_t>& pixels, int width, int height);
    int getOptimalMapSize(int availableWidth);
    void render();

private:
    GLuint textureID;
    int mapWidth;
    int mapHeight;
};
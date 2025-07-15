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

    template <typename T>
    const std::vector<uint32_t> convertPlane(const Plane<T>& plane, const Graph<T>& graph){
        int width = plane.getWidth();
        int height = plane.getHeight();
        std::vector<uint32_t> result(width * height, 0xff000000);

        for (Identifiable id : plane.getIDs()){
            Spot<T> startSpot = plane.getSpot(id);
            const Node<T>& node = graph.getNode(id);

            for (Identifiable neighbourId : node.getNeighbours()) {
                if (neighbourId > id) {
                    Spot<T> endSpot = plane.getSpot(neighbourId);
                    drawLine(result, width, height, startSpot.getCoords(), endSpot.getCoords(), 0xffaaaaaa);
                }
            }
        }

        for (Identifiable id : plane.getIDs()) {
            int x = plane.getSpot(id).getX();
            int y = plane.getSpot(id).getY();
            if (x >= 0 && x < width && y >= 0 && y < height) {
                result[y * width + x] = 0xff0000ff;
                drawCircle(result, width, height, x, y, 3, 0xff0000ff);
            }
        }

        return result;
    }

    void drawLine(std::vector<uint32_t> &pixels, int width, int height, const DoublePoint2 &start, const DoublePoint2 &end, uint32_t color);

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
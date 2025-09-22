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
#include "grid.h"

#include "border.h"

class MapRenderer {
public:
    const int maxMapSize = 128;
    const int maxScaleFactor = 4;

    MapRenderer(Palette* palette);

    ~MapRenderer();

    const uint32_t mapTile(const Tile tile);

    template <typename T>
    const std::vector<uint32_t> convertGrid(const Grid<T>& grid){
        int width = grid.getWidth();
        int height = grid.getHeight();
        std::vector<uint32_t> result(width * height, 0xff000000);

        for (int y = 0; y < height; y++){
            for (int x = 0; x < width; x++){
                result[y * width + x] = zoneColors.at(grid.getTileID(x, y));
            }
        }
        return result;
    }
    const std::vector<uint32_t> convertBorders(std::unordered_map<std::pair<Identifiable, Identifiable>, std::vector<tiles::Border>, PairIDHash> borders, size_t width, size_t height){

        size_t size = 0;

        for (const auto& [ids, sameIDBorders] : borders){
            for (const auto& border : sameIDBorders){
                size++;
            }
        }

        std::vector<uint32_t> colors = generateDistinctColors(size);

        std::vector<uint32_t> result(width * height, 0xff000000);

        constexpr bool markIntersections = false;

        size_t i = 0;
        for (const auto& [ids, sameIDBorders] : borders){
            for (const auto& border : sameIDBorders){
                for (const auto& segment : border.getSegments()){
                    auto pos1 = segment.getLeftPos();
                    auto pos2 = segment.getRightPos();
                    if (markIntersections && result[pos1.y * width + pos1.x] != 0xff000000){
                        result[pos1.y * width + pos1.x] = 0xff00a0ff;
                    } else {
                        result[pos1.y * width + pos1.x] = colors[i];
                    }


                    if (markIntersections && result[pos2.y * width + pos2.x] != 0xff000000){
                        result[pos2.y * width + pos2.x] = 0xff00a0ff;
                    } else {
                        result[pos2.y * width + pos2.x] = colors[i];
                    }
                }
                i++;
            }
        }
        return result;
    }

    template <typename T>
    void setDistinctColors(const Graph<T>& graph);

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

    void drawLine(std::vector<uint32_t> &pixels, int width, int height, const DoubleVector2 &start, const DoubleVector2 &end, uint32_t color);

    void drawCircle(std::vector<uint32_t> &pixels, int width, int height, int centerX, int centerY, int radius, uint32_t color);

    const std::vector<uint32_t> convertMap(SmartMap map);
    //RGBA format updating
    void updateMap(const std::vector<uint32_t>& pixels, int width, int height);
    int getOptimalMapSize(int availableWidth);
    void render();

    
private:
    std::vector<uint32_t> generateDistinctColors(int n);
    GLuint textureID;
    int mapWidth;
    int mapHeight;
    Palette* palette;
    std::unordered_map<Identifiable, uint32_t, IDHash> zoneColors;
};

template <typename T>
void MapRenderer::setDistinctColors(const Graph<T> &graph){
    std::vector<uint32_t> colors = generateDistinctColors(graph.getSize());
    zoneColors.clear();
    int i = 0;
    for (Identifiable id : graph.getIDs()){
        zoneColors[id] = colors[i];
        i++;
    }
    zoneColors[Identifiable::nullID] = 0xFF000000;
}

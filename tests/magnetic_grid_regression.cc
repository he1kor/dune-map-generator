#include "generation.h"
#include "template_picker.h"
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using Json = nlohmann::json;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static Json generate(const Json& data, unsigned int seed) {
    TemplatePicker picker;
    require(picker.tryReadEdgeGraph(data), "Magnetic grid JSON was rejected");
    const auto& graph = *picker.mapTemplate->zoneGraph;
    require(graph.size() == 9 && graph.getSymEdges().size() == 12, "Grid topology was not loaded");
    require(graph.getNeighbours(4).size() == 4 && graph.getNeighbours(0).size() == 2,
        "Grid adjacency is incorrect");
    for (const auto& node : data["nodes"]) {
        require(graph.getValue(node["id"].get<int>()).getSusceptibility() == node["data"]["susceptibility"].get<double>(),
            "Susceptibility was not loaded from JSON");
    }
    Generation generation;
    generation.isSeedRandom = false;
    generation.seed = seed;
    generation.generate(picker.mapTemplate);
    Json result = {{"seed", seed}, {"initial_positions", Json::array()},
        {"positions", Json::array()}, {"colors", Json::array()}, {"zones", Json::array()}};
    for (auto id : graph.getIDs()) {
        const auto point = generation.plane.getSpot(id).getCoords();
        result["initial_positions"].push_back({id.getID(), point.x, point.y});
    }
    generation.runIteration();
    require(generation.is_done, "Generation did not finish");
    double meanRadius = 0.0;
    for (auto id : graph.getIDs()) {
        const auto point = generation.plane.getSpot(id).getCoords();
        require(std::isfinite(point.x) && std::isfinite(point.y), "Non-finite node position");
        require(point.x >= 0.0 && point.x <= 128.0 && point.y >= 0.0 && point.y <= 128.0,
            "Node escaped the map");
        meanRadius += std::hypot(point.x - 64.0, point.y - 64.0) / graph.size();
        result["positions"].push_back({id.getID(), point.x, point.y});
    }
    std::array<int, 9> zoneCells{};
    int walls = 0;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const auto owner = generation.grid->getTileID(x, y);
            const bool wall = generation.wallMask.get(x, y);
            const auto resource = generation.spiceMap.get(x, y);
            require(wall == (owner == Identifiable::nullID), "Wall mask differs from generated walls");
            if (wall) {
                require(resource == Resource::CLIFF_ROCK, "Wall has an incorrect material");
                ++walls;
            } else {
                const int id = owner.getID();
                require(id >= 0 && id < 9, "Cell has an unexpected zone");
                ++zoneCells[id];
                require(resource == (id == 4 ? Resource::ROCK : Resource::SAND), "Zone has an incorrect material");
            }
            result["colors"].push_back(resourceColor(resource));
            result["zones"].push_back(owner.getID());
        }
    }
    for (int cells : zoneCells) require(cells > 0, "A zone disappeared during generation");
    require(walls > 0, "Grid generated no walls");
    result["mean_radius"] = meanRadius;
    result["zone_cells"] = zoneCells;
    result["wall_cells"] = walls;
    return result;
}

int main(int argc, char** argv) {
    try {
        require(argc >= 2, "Pass the magnetic grid JSON path");
        Json data;
        std::ifstream(argv[1]) >> data;
        for (const auto& node : data["nodes"])
            require(node["data"]["susceptibility"] == 0.02, "Example does not enable magnetic placement");
        Json saved;
        for (const auto seed : {12419u, 2026u, 42u}) {
            const auto attraction = generate(data, seed);
            auto neutralData = data;
            for (auto& node : neutralData["nodes"]) node["data"]["susceptibility"] = 0.0;
            const auto neutral = generate(neutralData, seed);
            require(attraction["initial_positions"] == neutral["initial_positions"], "Comparisons started from different positions");
            require(attraction["mean_radius"] < neutral["mean_radius"], "Magnet did not contract the generated grid");
            if (seed == 12419) {
                // Both polarities also have controlled coverage in GenCore. Exercise
                // repulsion through the full pipeline on this demonstration seed.
                auto repulsionData = data;
                for (auto& node : repulsionData["nodes"]) node["data"]["susceptibility"] = -0.02;
                const auto repulsion = generate(repulsionData, seed);
                require(neutral["initial_positions"] == repulsion["initial_positions"], "Repulsion started from different positions");
                require(neutral["mean_radius"] < repulsion["mean_radius"], "Magnet did not expand the generated grid");
                require(generate(data, seed) == attraction, "Magnetic map is not reproducible");
                saved = {{"attraction", attraction}, {"neutral", neutral}, {"repulsion", repulsion}};
                std::cout << "Repulsion seed " << seed << ": radius=" << repulsion["mean_radius"] << "; all nine zones survived\n";
            }
            std::cout << "Seed " << seed << ": attraction=" << attraction["mean_radius"]
                << ", neutral=" << neutral["mean_radius"]
                << "; all nine zones survived\n";
        }
        if (argc >= 3) {
            std::ofstream output(argv[2]);
            require(bool(output), "Cannot open snapshot output");
            output << saved.dump();
        }
        std::cout << "Current JSON, full generation, polarity, materials and repeatability passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

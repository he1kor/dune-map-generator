#include "generation.h"
#include "template_picker.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

using Json = nlohmann::json;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static Json generate(const Json& data, unsigned int seed) {
    TemplatePicker picker;
    require(picker.tryReadEdgeGraph(data), "Current template was rejected");
    require(picker.mapTemplate->wallResource == Resource::CLIFF_ROCK, "Wall material was not loaded from JSON");
    Generation generation;
    generation.isSeedRandom = false;
    generation.seed = seed;
    generation.generate(picker.mapTemplate);
    generation.runIteration();
    Json result = {{"seed", seed}, {"width", 128}, {"height", 128},
        {"resources", Json::array()}, {"zones", Json::array()}, {"colors", Json::array()},
        {"walls", Json::array()}, {"bonuses", Json::object()}};
    int cliffs = 0, rocks = 0, sandWalls = 0;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const auto owner = generation.grid->getTileID(x, y);
            const auto resource = generation.spiceMap.get(x, y);
            const bool wall = generation.wallMask.get(x, y);
            require(resource != Resource::NONE, "An unassigned cell remains in the finished map");
            require(wall == (owner == Identifiable::nullID), "Wall mask differs from the generated walls");
            require(wall == (resource == Resource::CLIFF_ROCK), "Wall material leaked into a zone or a wall is not a cliff");
            if (owner.getID() >= 1 && owner.getID() <= 9) {
                require(resource == Resource::ROCK, "Explicit rock zone was not filled with plain rock");
                ++rocks;
            }
            cliffs += wall;
            result["resources"].push_back(static_cast<int>(resource));
            result["zones"].push_back(owner.getID());
            result["colors"].push_back(resourceColor(resource));
            result["walls"].push_back(wall);
        }
    }
    for (const auto& [pair, borders] : generation.edgeToborderMap) {
        if (pair.first.getID() <= 9 || pair.second.getID() <= 9) continue;
        for (const auto& border : borders) {
            for (const auto& segment : border.getSegments()) {
                sandWalls += generation.wallMask.get(segment.getLeftPos());
            }
        }
    }
    require(cliffs > 0 && rocks > 0 && sandWalls > 0, "Rock zones or sand-to-sand walls were not exercised");
    for (auto& [id, mask] : generation.zoneMasks.bonusMask) {
        auto& saved = result["bonuses"][std::to_string(id.getID())] = Json::array();
        for (double value : mask) saved.push_back(value);
    }
    result["cliff_cells"] = cliffs;
    result["plain_rock_cells"] = rocks;
    return result;
}

int main(int argc, char** argv) {
    try {
        require(argc >= 2, "Pass the canonical template path");
        Json data;
        std::ifstream(argv[1]) >> data;
        for (const auto& node : data["nodes"]) {
            if (node["id"].get<int>() > 9) continue;
            const auto& content = node["data"]["resources"]["content"];
            require(content.size() == 1 && content[0]["name"] == "rock"
                && content[0]["startThreshold"] == 0.0 && content[0]["endThreshold"] == 1.0,
                "Rock island is not explicitly declared as 100% rock");
        }
        for (const auto seed : {12419u, 2026u, 42u}) {
            const auto baseline = generate(data, seed);
            Json reordered = data;
            std::reverse(reordered["nodes"][9]["data"]["resources"]["content"].begin(),
                reordered["nodes"][9]["data"]["resources"]["content"].end());
            require(generate(reordered, seed) == baseline, "Reordering one zone's resources changed the map");
            for (auto& node : reordered["nodes"]) {
                auto& content = node["data"]["resources"]["content"];
                std::reverse(content.begin(), content.end());
            }
            require(generate(reordered, seed) == baseline, "Reordering every resource list changed the map");
            if (seed == 12419 && argc >= 3) std::ofstream(argv[2]) << baseline.dump();
            std::cout << "Seed " << seed << ": " << baseline["plain_rock_cells"] << " plain rock cells, "
                << baseline["cliff_cells"] << " cliff cells; both resource permutations identical\n";
        }
        TemplatePicker badResource;
        Json invalid = data;
        invalid["nodes"][0]["data"]["resources"]["content"][0]["name"] = "unknown";
        require(!badResource.tryReadEdgeGraph(invalid) && badResource.badValueError.ocurred, "Unknown material did not produce a validation error");
        TemplatePicker uncovered;
        invalid = data;
        invalid["nodes"][0]["data"]["resources"]["content"][0]["endThreshold"] = 0.0;
        require(!uncovered.tryReadEdgeGraph(invalid) && uncovered.badValueError.ocurred, "Implicit rock fallback was still accepted");
        TemplatePicker badWall;
        invalid = data;
        invalid["walls"]["resource"] = "unknown";
        require(!badWall.tryReadEdgeGraph(invalid) && badWall.badValueError.ocurred, "Unknown wall material was accepted");
        const auto plain = resourceColor(Resource::ROCK), cliff = resourceColor(Resource::CLIFF_ROCK);
        for (int shift : {0, 8, 16}) require(((cliff >> shift) & 255) < ((plain >> shift) & 255), "Cliff color is not darker than plain rock");
        std::cout << "Explicit materials, all walls, resource order and JSON validation checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

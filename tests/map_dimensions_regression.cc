#include "generation.h"
#include "quick_settings.h"
#include "template_picker.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

using Json = nlohmann::json;

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static Json load(const char* path) {
    Json data;
    std::ifstream(path) >> data;
    return data;
}

static std::shared_ptr<MapTemplate> parse(const Json& data) {
    TemplatePicker picker;
    require(picker.tryReadEdgeGraph(data), "Valid dimension rules were rejected");
    return picker.mapTemplate;
}

template<typename T>
static void dimensions(const T& value, MapDimensions expected, const std::string& name) {
    require(value.getWidth() == expected.width && value.getHeight() == expected.height, name + " has stale dimensions");
}

static std::vector<int> generate(Generation& generation, const std::shared_ptr<MapTemplate>& mapTemplate, MapDimensions size) {
    generation.seed = 12419;
    generation.isSeedRandom = false;
    generation.generate(mapTemplate, size.width, size.height);
    require(!generation.is_done && !generation.grid && generation.resourceGenerators.empty(), "Previous run was not reset");
    generation.runIteration();
    require(generation.is_done, "Generation did not finish");
    dimensions(generation.plane, size, "Plane");
    dimensions(*generation.grid, size, "Grid");
    dimensions(generation.noiseMap, size, "Noise");
    dimensions(generation.spiceMap, size, "Resources");
    dimensions(generation.wallMask, size, "Walls");
    for (const auto& [id, matrix] : generation.zoneMasks.influenceMask) dimensions(matrix, size, "Influence mask");
    for (const auto& [id, matrix] : generation.zoneMasks.bonusMask) dimensions(matrix, size, "Bonus mask");
    for (const auto& [id, generator] : generation.resourceGenerators) dimensions(generator.getNoise(), size, "Zone noise");
    for (auto id : generation.plane.getIDs()) {
        const auto point = generation.plane.getSpot(id).getCoords();
        require(std::isfinite(point.x) && std::isfinite(point.y) && point.x >= 0 && point.y >= 0
            && point.x <= size.width && point.y <= size.height, "Node escaped the selected map dimensions");
    }
    std::vector<int> result;
    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            const auto resource = generation.spiceMap.get(x, y);
            require(resource != Resource::NONE, "Unassigned resource cell");
            require(generation.wallMask.get(x, y) == (resource == Resource::CLIFF_ROCK), "Wall material is incorrect");
            result.push_back(generation.grid->getTileID(x, y).getID());
            result.push_back(static_cast<int>(resource));
        }
    }
    std::cout << size.width << " x " << size.height << ": complete map, correctly sized buffers\n";
    return result;
}

int main(int argc, char** argv) {
    try {
        require(argc == 3, "Pass the grid and canonical template paths");
        const Json grid = load(argv[1]);
        const Json canonical = load(argv[2]);
        const auto unrestricted = parse(grid);
        require(unrestricted->dimensionLimits == MapDimensionLimits{}, "Templates without limits are restricted");
        QuickSettings settings;
        require(settings.dimensions() == MapDimensions{64, 64}, "Initial UI dimensions changed");
        for (int side = 32; side <= 128; side += 2) {
            settings.setWidth(side);
            require(settings.dimensions().width == side, "An even width cannot be selected");
            settings.setHeight(side);
            require(settings.dimensions().height == side, "An even height cannot be selected");
        }
        require(settings.dimensionLimits.allowedSides(true).size() == 49, "Wrong number of even width choices");
        for (int value : {-1000, 31, 33, 65, 129, 1000}) {
            settings.setWidth(value);
            settings.setHeight(value);
            require(settings.dimensionLimits.allows(settings.dimensions()), "Invalid size escaped UI normalization");
        }

        auto data = grid;
        data["dimensions"] = {{"width", {{"min", 64}, {"max", 128}}},
            {"height", {{"min", 64}, {"max", 128}}}, {"aspectRatio", {{"min", 1.0}, {"max", 1.0}}}};
        const auto square = parse(data);
        settings.setWidth(32);
        settings.setHeight(128);
        settings.applyDimensionLimits(square->dimensionLimits);
        require(settings.dimensions() == MapDimensions{64, 64} && settings.sizeAdjustedToTemplate,
            "Template loading did not adjust and explain the selected size");
        settings.applyDimensionLimits(square->dimensionLimits);
        require(settings.sizeAdjustedToTemplate, "Adjustment notice vanished before the user changed a value");
        settings.setWidth(126);
        require(settings.dimensions() == MapDimensions{126, 126} && !settings.sizeAdjustedToTemplate,
            "Width did not keep a square map square");
        settings.setHeight(96);
        require(settings.dimensions() == MapDimensions{96, 96}, "Height did not keep a square map square");

        auto rectangular = square->dimensionLimits;
        rectangular.minWidth = 32;
        rectangular.minAspectRatio = 0.5;
        rectangular.maxAspectRatio = 2.0;
        settings.applyDimensionLimits(rectangular);
        settings.setWidth(32);
        require(settings.dimensions() == MapDimensions{32, 64}, "Height did not adjust for the minimum aspect ratio");
        settings.setHeight(128);
        require(settings.dimensions() == MapDimensions{64, 128}, "Width did not adjust for the minimum aspect ratio");
        settings.setWidth(128);
        settings.setHeight(64);
        require(settings.dimensions() == MapDimensions{128, 64}, "Valid rectangular selection was changed");
        settings.applyDimensionLimits({});
        require(settings.dimensions() == MapDimensions{128, 64}, "An unrestricted template reset a valid selection");

        int rejected = 0;
        auto reject = [&](const Json& rules, const std::string& path) {
            auto invalid = grid;
            invalid["dimensions"] = rules;
            TemplatePicker picker;
            require(picker.tryReadEdgeGraph(grid), "Grid was rejected");
            const auto previous = picker.mapTemplate;
            require(!picker.tryReadEdgeGraph(invalid) && picker.badValueError.ocurred
                && picker.badValueError.targetKey == path && picker.mapTemplate == previous,
                "Bad dimension rules did not produce an error at " + path);
            ++rejected;
        };
        reject(nullptr, "dimensions");
        reject({{"widht", 64}}, "dimensions/widht");
        for (const std::string axis : {"width", "height"}) {
            reject({{axis, 64}}, "dimensions/" + axis);
            reject({{axis, {{"maximum", 128}}}}, "dimensions/" + axis + "/maximum");
            for (const std::string key : {"min", "max"}) {
                for (const Json value : {Json(30), Json(130), Json(65), Json(64.5), Json(true), Json("64"), Json(nullptr)})
                    reject({{axis, {{key, value}}}}, "dimensions/" + axis + "/" + key);
            }
            reject({{axis, {{"min", 96}, {"max", 64}}}}, "dimensions/" + axis + "/max");
        }
        reject({{"aspectRatio", 1}}, "dimensions/aspectRatio");
        reject({{"aspectRatio", {{"maximum", 1}}}}, "dimensions/aspectRatio/maximum");
        for (const std::string key : {"min", "max"}) {
            for (const Json value : {Json(0), Json(-1), Json(0.1), Json(5), Json(true), Json("1"), Json(nullptr),
                Json(std::numeric_limits<double>::infinity()), Json(std::numeric_limits<double>::quiet_NaN())})
                reject({{"aspectRatio", {{key, value}}}}, "dimensions/aspectRatio/" + key);
        }
        reject({{"aspectRatio", {{"min", 2}, {"max", 1}}}}, "dimensions/aspectRatio/max");
        reject({{"width", {{"min", 64}, {"max", 64}}}, {"height", {{"min", 64}, {"max", 64}}},
            {"aspectRatio", {{"min", 1.01}, {"max", 1.02}}}}, "dimensions");
        require(parse(grid)->dimensionLimits.allows({32, 128}), "Portrait maps are disallowed by default");
        data = grid;
        data["dimensions"] = {{"aspectRatio", {{"max", 2}}}};
        require(parse(data)->dimensionLimits.allows({32, 128}) && !parse(data)->dimensionLimits.allows({128, 32}),
            "Width/height ratio is interpreted incorrectly");

        Generation generation;
        const auto original = generate(generation, unrestricted, {128, 128});
        for (MapDimensions size : {MapDimensions{32, 32}, {32, 128}, {128, 32}, {64, 96}, {96, 64}, {126, 128}, {64, 64}})
            generate(generation, unrestricted, size);
        require(generate(generation, unrestricted, {128, 128}) == original, "Resizing changed subsequent seeded output");
        generate(generation, square, {64, 64});
        const auto previousGrid = generation.grid;
        for (MapDimensions size : {MapDimensions{30, 64}, {64, 130}, {65, 64}, {64, 65}, {32, 32}, {64, 96}}) {
            bool failed = false;
            try { generation.generate(square, size.width, size.height); }
            catch (const std::invalid_argument&) { failed = true; }
            require(failed && generation.grid == previousGrid && generation.is_done,
                "Invalid dimensions started or modified a generation run");
            dimensions(generation.plane, {64, 64}, "Rejected generation");
        }
        // Exercise a resource-rich template at a smaller square and a rectangle.
        generate(generation, parse(canonical), {64, 64});
        generate(generation, parse(canonical), {96, 64});
        std::cout << rejected << " malformed size-rule cases rejected; selection, bounds, ratios and regeneration passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

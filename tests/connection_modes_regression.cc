#include "generation.h"
#include "template_editor.h"
#include "template_picker.h"
#include "template_validation.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using Json = nlohmann::json;

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static Json chain(int count = 2) {
    TemplateEditor editor;
    for (int i = 0; i < count; ++i) editor.addNode({float(i * 160), 0});
    for (int i = 1; i < count; ++i) require(editor.connect(i, i + 1), editor.error());
    return editor.document();
}

static void checkBorders() {
    // Exercise single-segment, straight, bent and enclosed borders, including
    // the terminal segment that the ranged passage algorithm leaves intact.
    for (const auto size : {IntVector2{2, 1}, {8, 8}}) {
        Grid<BasicNode> grid(size.x, size.y, std::vector<BasicNode>{{1, 0}, {2, 0}});
        for (int shape = 0; shape < 3; ++shape) {
            for (int y = 0; y < size.y; ++y) for (int x = 0; x < size.x; ++x) {
                const bool second = shape == 0 ? x >= size.x / 2
                    : shape == 1 ? x >= size.x / 2 + y / 3
                    : x >= size.x / 4 && x < 3 * size.x / 4 && y >= size.y / 4 && y < 3 * size.y / 4;
                grid.setTile(x, y, second ? 2 : 1);
            }
            for (const auto mode : {tiles::PassMode::Open, tiles::PassMode::Closed}) {
                auto borders = tiles::Border::getAllBorders(grid);
                require(!borders.empty() || shape == 2 && size.y == 1, "Missing test border");
                for (auto& [pair, pieces] : borders) for (auto& border : pieces) {
                    // Deliberately contradictory ranges: explicit modes must
                    // bypass all ranged allocation and passage calculations.
                    border.initPassData({20, 1, 10, 0, mode});
                    border.generatePasses();
                    for (const auto& segment : border.getSegments())
                        require(segment.isDisabled() == (mode == tiles::PassMode::Open),
                            "Explicit mode left an incorrect segment, including a border endpoint");
                }
            }
        }
    }
    for (const auto mode : {tiles::PassMode::Ranged, tiles::PassMode::Open, tiles::PassMode::Closed}) {
        tiles::Border empty;
        empty.initPassData({.mode = mode});
        empty.generatePasses();
    }
}

static void checkGeneration(const std::string& mode, bool retainedRanges, int width, int height, int seed) {
    auto document = chain();
    auto& passes = document["symmetrical_edges"][0]["data"]["passes"];
    if (!retainedRanges) passes = Json::object();
    passes["mode"] = mode;
    TemplatePicker picker;
    require(picker.tryReadEdgeGraph(document), templateValidationError(document));
    Generation generation;
    generation.isSeedRandom = false;
    generation.seed = seed;
    generation.generate(picker.mapTemplate, width, height);
    Matrix<bool> expected(width, height, false);
    bool checked = false;
    generation.runIteration([&](const Generation& state) {
        if (state.generationStage == GenerationStage::BORDERS) {
            const auto borders = tiles::Border::getAllBorders(*state.grid);
            require(!borders.empty(), "Generation did not produce a shared border");
            if (mode == "closed") for (const auto& [pair, pieces] : borders)
                for (const auto& border : pieces) for (const auto& segment : border.getSegments()) {
                    const auto left = segment.getLeftPos(), right = segment.getRightPos();
                    expected.set(left.x, left.y, true);
                    expected.set(right.x, right.y, true);
                }
        }
        if (state.generationStage == GenerationStage::NOISE) {
            checked = true;
            for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
                require(state.wallMask.get(x, y) == expected.get(x, y), "Generated wall coverage differs from the explicit mode");
        }
        return true;
    });
    require(checked && generation.is_done, "Generation did not finish or skipped wall verification");
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x)
        require(generation.spiceMap.get(x, y) == (expected.get(x, y) ? Resource::CLIFF_ROCK : Resource::SAND),
            "Final wall material differs from the explicit border mode");
}

static void checkEditorAndValidation(const std::filesystem::path& scratch) {
    TemplateEditor editor;
    const auto original = chain(3);
    require(editor.replace(original), editor.error());
    TemplatePicker picker;
    require(picker.tryReadEdgeGraph(original), "Legacy ranges failed to load");
    require(picker.mapTemplate->zoneGraph->getSymEdge(1, 2).mode == tiles::PassMode::Ranged,
        "Missing mode did not default to ranged");
    for (const char* mode : {"open", "closed"}) {
        auto document = original;
        document["symmetrical_edges"][0]["data"]["passes"] = {{"mode", mode}};
        require(editor.replace(document), editor.error());
        const auto payload = editor.copyConnectionData(0);
        require(editor.pasteConnectionData(payload, 1), editor.error());
        require(editor.copyConnectionData(1) == payload, "Copy/paste lost the border mode");
        const auto applied = editor.document();
        editor.undo();
        require(editor.document() == document, "Undo lost the original border mode");
        editor.redo();
        require(editor.document() == applied, "Redo lost the explicit border mode");
        require(editor.save(scratch), editor.error());
        TemplateEditor reopened;
        require(reopened.load(scratch) && reopened.document() == applied, "Save/load changed the border mode");
        require(picker.tryReadEdgeGraph(reopened.document()), "Saved border mode cannot generate");
    }
    for (const Json bad : {Json("unknown"), Json(""), Json(true), Json(1), Json(nullptr), Json::array()}) {
        auto invalid = original;
        invalid["symmetrical_edges"][0]["data"]["passes"]["mode"] = bad;
        require(!picker.tryReadEdgeGraph(invalid) && picker.badValueError.ocurred
            && picker.badValueError.targetKey == "symmetrical_edges/0/data/passes/mode", "Invalid mode was not rejected at its field");
    }
    auto symmetric = chain(4);
    symmetric["embedding"] = {{"symmetry", "complete"}};
    // Fix a vertical reflection, so a path cannot satisfy the request by
    // placing all four zones on the axis with every edge mapped to itself.
    const double x[] = {.1, .3, .7, .9};
    for (int i = 0; i < 4; ++i) symmetric["nodes"][i]["data"]["magnet"] = {
        {"force", 10}, {"position", {{"x", x[i]}, {"y", i == 0 || i == 3 ? .4 : .6}}}};
    symmetric["symmetrical_edges"][0]["data"]["passes"]["mode"] = "open";
    symmetric["symmetrical_edges"][2]["data"]["passes"] = {{"mode", "open"}};
    require(picker.tryReadEdgeGraph(symmetric), "Inactive ranges prevented matching open connections for symmetry");
    symmetric["symmetrical_edges"][2]["data"]["passes"]["mode"] = "closed";
    require(!picker.tryReadEdgeGraph(symmetric), "Symmetry accepted paired connections with different border modes");
    require(picker.badValueError.targetKey == "embedding/symmetry", "Symmetry mismatch was not reported at embedding/symmetry");
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Pass a temporary JSON path");
        checkBorders();
        checkEditorAndValidation(argv[1]);
        for (const char* mode : {"open", "closed"}) for (const bool retained : {false, true})
            for (const int seed : {3, 42}) {
                checkGeneration(mode, retained, 32, 32, seed);
                checkGeneration(mode, retained, 64, 32, seed);
            }
        std::cout << "Explicit connection modes: border coverage, generation, symmetry, validation and editor round trips passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

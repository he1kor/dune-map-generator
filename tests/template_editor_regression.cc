#include "template_editor.h"
#include "template_picker.h"
#include "template_validation.h"
#include "generation.h"
#include "planar_layout.h"

#include <fstream>
#include <cmath>
#include <iostream>
#include <set>
#include <tuple>
#include <stdexcept>

using Json = nlohmann::json;
static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static std::vector<int> generate(const Json& document) {
    TemplatePicker picker;
    require(picker.tryReadEdgeGraph(document), "Generation rejected editor document");
    Generation generation;
    generation.isSeedRandom = false;
    generation.seed = 12419;
    generation.generate(picker.mapTemplate);
    generation.runIteration();
    require(generation.is_done, "Generation did not finish");
    std::vector<int> result;
    for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x) {
        result.push_back(generation.grid->getTileID(x, y).getID());
        result.push_back(static_cast<int>(generation.spiceMap.get(x, y)));
    }
    return result;
}

static void checkMagnets(const std::filesystem::path& scratch) {
    TemplateEditor editor;
    const int a = editor.addNode({-160, -80}), b = editor.addNode({160, 80});
    editor.connect(a, b);
    const auto plain = editor.document();
    editor.setMagnet(a, true);
    const auto enabled = editor.document();
    require(editor.isMagnet(a) && !editor.isMagnet(b) && editor.magnetInsideFrame(a), "Magnet toggle or frame membership failed");
    require(editor.magnetForce(a) == TemplateEditor::defaultMagnetForce, "New magnet has no usable default force");
    require(editor.magnetTarget(a).x < 0.5f && editor.magnetTarget(a).y < 0.5f, "Magnet target does not follow its map quadrant");
    editor.beginAction("Magnet gesture");
    for (double force : {2.0, 4.0, 8.0}) editor.setMagnetForce(a, force);
    editor.endAction();
    require(editor.undo().find("8.0 -> 1.0") != std::string::npos && editor.document() == enabled,
        "Magnet gesture is not one undo step with force details");
    editor.redo();
    require(editor.magnetForce(a) == 8, "Magnet force redo failed");
    editor.setMagnetForce(a, -10);
    require(editor.isMagnet(a) && editor.magnetForce(a) == TemplateEditor::minMagnetForce, "Minimum force disables a magnet");
    editor.setMagnetForce(a, 1e8);
    require(editor.magnetForce(a) == TemplateEditor::maxMagnetForce, "Unbounded magnet force accepted");
    const auto beforeResize = editor.document();
    const auto beforeTarget = editor.magnetTarget(a);
    const auto beforeFrame = editor.magnetFrame();
    editor.setMagnetFrameScale(0.2);
    require(editor.document() == beforeResize, "Frame shrinks below the fit of all zones");
    editor.setMagnetFrameScale(2);
    require(editor.magnetFrame().center == beforeFrame.center && editor.magnetFrame().halfExtent >= beforeFrame.halfExtent * 2,
        "Resizing the map frame moved its center or did not resize it");
    require(editor.position(a) == TemplatePoint{-160,-80} && editor.position(b) == TemplatePoint{160,80},
        "Map frame resize changed zone positions");
    require(editor.magnetTarget(a).x > beforeTarget.x && editor.magnetTarget(a).x < 0.5f,
        "Larger frame did not bring normalized targets toward its center");
    editor.setPosition(b, {2000, 800});
    const auto frame = editor.magnetFrame();
    for (int id : {a,b}) {
        const auto p = editor.position(id);
        require(std::abs(p.x-frame.center.x)+TemplateEditor::halfSize(*editor.node(id)) <= frame.halfExtent
            && std::abs(p.y-frame.center.y)+TemplateEditor::halfSize(*editor.node(id)) <= frame.halfExtent,
            "Frame does not fit a moved non-magnet zone square");
    }
    require(editor.pasteNodeData(editor.copyNodeData(a), {b}), editor.error());
    require(editor.isMagnet(b) && editor.magnetForce(b) == editor.magnetForce(a)
        && editor.node(a)->at("data").at("magnet").at("position") != editor.node(b)->at("data").at("magnet").at("position"),
        "Pasting a magnet reused the source target instead of the destination position");
    const auto fixedOrientation = editor.document();
    auto rotating = fixedOrientation;
    rotating["magnets"] = {{"randomRotation", true}};
    require(editor.replace(rotating), editor.error());
    editor.undo(); require(editor.document() == fixedOrientation, "Magnet rotation setting is not undoable");
    editor.redo(); require(editor.document() == rotating, "Magnet rotation redo lost the setting or moved targets");
    require(editor.save(scratch), editor.error());
    TemplateEditor reopened;
    require(reopened.load(scratch) && reopened.document() == editor.document(), "Magnet force, frame, targets or rotation lost during save/reload");
    TemplatePicker rotatedPicker;
    require(rotatedPicker.tryReadEdgeGraph(reopened.document()) && rotatedPicker.mapTemplate->rotatePositionMagnets,
        "Saved rotation setting was not passed to generation");
    const auto good = editor.document();
    for (const Json force : {Json(0), Json(-1), Json(101), Json("bad"), Json(nullptr)}) {
        auto bad = good; bad["nodes"][0]["data"]["magnet"]["force"] = force;
        require(!editor.replace(bad) && editor.document() == good, "Invalid magnet force changed the document");
    }
    auto bad = good; bad["visualizer"]["magnetFrame"]["scale"] = 0.5;
    require(!editor.replace(bad) && editor.document() == good, "Invalid frame scale changed the document");
    for (const Json position : {Json("bad"), Json::array(), Json{{"x", 0}}, Json{{"x", 0}, {"y", nullptr}}}) {
        bad = good; bad["nodes"][0]["data"]["magnet"]["position"] = position;
        require(!editor.replace(bad) && editor.document() == good, "Malformed magnet target changed the document");
    }
    editor.setMagnet(a, false);
    require(!editor.isMagnet(a), "Single-zone magnet disable failed");
    editor.undo(); require(editor.document() == good, "Undo did not restore a disabled magnet");
    auto sameTargets = editor.document();
    for (auto& node : sameTargets["nodes"]) node["visualizer"] = {{"x", 12345}, {"y", -6789}};
    require(generate(sameTargets) == generate(editor.document()), "Generation read visualizer coordinates instead of saved magnet targets");
    std::filesystem::remove(scratch);
}

static void checkHistory(const std::filesystem::path& scratch) {
    TemplateEditor editor;
    const auto blank = editor.document();
    require(!editor.canUndo() && !editor.dirty(), "New editor starts with phantom history");
    const int first = editor.addNode({10, 20});
    const auto one = editor.document();
    require(editor.canUndo() && editor.undo().find("-> absent") != std::string::npos, "Undo creation lacks from/to details");
    require(editor.document() == blank && !editor.dirty() && editor.canRedo(), "Undo creation did not restore clean draft");
    editor.redo();
    require(editor.document() == one, "Redo creation lost values");
    const int second = editor.addNode({50, 80});
    editor.connect(first, second);
    const auto beforeMove = editor.document();
    editor.beginAction("Move 2 zones");
    for (int step = 1; step <= 8; ++step) {
        editor.setPosition(first, {10.0f + step, 20.0f + step});
        editor.setPosition(second, {50.0f + step, 80.0f + step});
    }
    require(!editor.canUndo(), "Partially finished drag is undoable");
    editor.endAction();
    const auto afterMove = editor.document();
    require(editor.save(scratch) && !editor.dirty(), "Cannot save history checkpoint");
    const auto moveMessage = editor.undo();
    require(editor.document() == beforeMove && editor.dirty(), "Group move was not one undo step");
    require(moveMessage.find("Zone 1/visualizer/x: 18.0 -> 10.0") != std::string::npos
        && moveMessage.find("Zone 2/visualizer/y: 88.0 -> 80.0") != std::string::npos,
        "Undo notification omits changed positions or values");
    require(editor.path() == scratch, "Undo after Save as switched the file path");
    editor.redo();
    require(editor.document() == afterMove && !editor.dirty(), "Redo did not return to saved checkpoint");
    editor.undo();
    require(!editor.connect(first, first) && editor.canRedo(), "No-op action destroyed redo");
    auto invalid = editor.document(); invalid["name"] = "";
    require(!editor.replace(invalid) && editor.canRedo(), "Invalid edit destroyed redo");
    editor.setPosition(first, {999, 999});
    require(!editor.canRedo(), "New branch retained obsolete redo actions");
    editor.undo();
    auto settings = editor.document();
    settings["nodes"][0]["data"]["radius"] = 3;
    settings["nodes"][0]["data"]["susceptibility"] = -0.125;
    settings["asymmetrical_edges"] = Json::array({{{"ID_from", first}, {"ID_to", second},
        {"data", {{"intakeDistance", 0}, {"areaGuaranteed", 110}, {"bonusValue", 0.65}}}}});
    require(editor.replace(settings, "Edit node and edge values"), editor.error());
    const auto settingsMessage = editor.undo();
    require(settingsMessage.find("radius: 3 -> 1.0") != std::string::npos
        && settingsMessage.find("susceptibility: -0.125 -> 0.0") != std::string::npos, "Settings undo has no exact values");
    editor.redo();
    require(editor.document() == settings, "Redo did not restore all settings and directed rules");
    editor.beginAction("Delete group"); editor.removeNode(first); editor.removeNode(second); editor.endAction();
    require(editor.document()["nodes"].empty() && editor.document()["asymmetrical_edges"].empty(), "Group deletion left data");
    editor.undo();
    require(editor.document() == settings, "Undo group deletion failed to restore edges or rules");
    const auto previousPath = editor.path();
    editor.createNew(); editor.undo();
    require(editor.document() == settings && editor.path() == previousPath, "New template is not undoable");
    require(editor.load(scratch), editor.error());
    editor.undo(); require(editor.document() == settings, "Load is not undoable");
    editor.redo(); require(editor.document() == afterMove, "Redo load failed");
    std::filesystem::remove(scratch);
}

static void checkConnectionInversion() {
    TemplateEditor editor;
    const int a = editor.addNode({0, 0}), b = editor.addNode({160, 0}), c = editor.addNode({320, 0});
    editor.connect(a, b); editor.connect(b, c);
    const auto plain = editor.document();
    require(!editor.invertConnection(0) && !editor.invertConnection(99) && editor.document() == plain,
        "Inverting a connection without rules or an invalid index changed the document");
    auto oneWay = plain;
    oneWay["asymmetrical_edges"] = Json::array({
        {{"ID_from", a}, {"ID_to", b}, {"data", {{"intakeDistance", 2}, {"areaGuaranteed", 100}, {"bonusValue", 0.6}}},
            {"extension", "keep"}},
        {{"ID_from", b}, {"ID_to", c}, {"data", {{"intakeDistance", 0}, {"areaGuaranteed", 0}, {"bonusValue", 0.0}}}}
    });
    require(editor.replace(oneWay), editor.error());
    require(editor.invertConnection(0), "Single directed rule was not inverted");
    const auto reversed = editor.document();
    require(reversed["asymmetrical_edges"][0]["ID_from"] == b
        && reversed["asymmetrical_edges"][0]["ID_to"] == a
        && reversed["asymmetrical_edges"][0]["data"] == oneWay["asymmetrical_edges"][0]["data"]
        && reversed["asymmetrical_edges"][0]["extension"] == "keep"
        && reversed["asymmetrical_edges"][1] == oneWay["asymmetrical_edges"][1]
        && reversed["symmetrical_edges"] == oneWay["symmetrical_edges"] && reversed["nodes"] == oneWay["nodes"],
        "Inversion changed rule parameters, metadata, symmetric settings, positions or another connection");
    require(templateValidationError(reversed, false).empty(), "Inverted single rule is invalid");
    editor.undo(); require(editor.document() == oneWay, "Inversion was not one undo step");
    require(!editor.invertConnection(1 + 10) && editor.canRedo(), "No-op inversion cleared redo");
    editor.redo(); require(editor.document() == reversed, "Inversion redo lost its direction");
    require(editor.invertConnection(0) && editor.document() == oneWay, "Double inversion did not restore the connection");
    auto twoWay = oneWay;
    twoWay["asymmetrical_edges"].push_back({{"ID_from", b}, {"ID_to", a},
        {"data", {{"intakeDistance", 3}, {"areaGuaranteed", 20}, {"bonusValue", 0.15}}}});
    require(editor.replace(twoWay), editor.error());
    require(editor.invertConnection(0), "Two directed rules were not inverted");
    const auto payload = editor.copyConnectionData(0);
    require(payload["forward"] == twoWay["asymmetrical_edges"][2]["data"]
        && payload["reverse"] == twoWay["asymmetrical_edges"][0]["data"], "Opposing rule parameters were not exchanged");
    require(templateValidationError(editor.document(), false).empty(), "Inverted opposing rules are invalid");
    editor.undo(); require(editor.document() == twoWay, "Undo did not restore both directions together");
    auto draft = twoWay;
    require(TemplateEditor::invertConnection(draft, 0) && draft != twoWay && editor.document() == twoWay,
        "Draft inversion committed unapplied settings");
    require(TemplateEditor::invertConnection(draft, 0) && draft == twoWay, "Draft inversion is not reversible");
}

static void checkDataClipboard() {
    TemplateEditor editor;
    const int source = editor.addNode({3.125785f, -8.889999f});
    const int a = editor.addNode({80, 32}), b = editor.addNode({160, 64}), c = editor.addNode({240, 96});
    require(editor.node(source)->at("visualizer").at("x").dump() == "3.13"
        && editor.node(source)->at("visualizer").at("y").dump() == "-8.89", "Coordinates were not rounded before serialization");
    editor.connect(source, a); editor.connect(c, b);
    auto customized = editor.document();
    auto& data = customized["nodes"][0]["data"];
    data["radius"] = 2.5; data["susceptibility"] = -0.3;
    data["resources"]["octave1_weight"] = 0.8;
    data["resources"]["content"] = Json::array({
        {{"name", "sand"}, {"startThreshold", 0}, {"endThreshold", 0.3}},
        {{"name", "spice"}, {"startThreshold", 0.3}, {"endThreshold", 1}}});
    data["extension"] = {{"notes", "copy all data"}};
    customized["symmetrical_edges"][0]["data"]["geometry"] = {{"preferredCenterDistance", 19}, {"distanceWeight", 0.4}};
    customized["asymmetrical_edges"] = Json::array({
        {{"ID_from", source}, {"ID_to", a}, {"data", {{"intakeDistance", 2}, {"areaGuaranteed", 110}, {"bonusValue", 0.65}}}},
        {{"ID_from", a}, {"ID_to", source}, {"data", {{"intakeDistance", 0}, {"areaGuaranteed", 20}, {"bonusValue", 0.15}}}}
    });
    require(editor.replace(customized), editor.error());
    const auto nodePayload = editor.copyNodeData(source), edgePayload = editor.copyConnectionData(0);
    require(!nodePayload.contains("id") && !nodePayload.contains("visualizer")
        && !edgePayload.contains("ID_first"), "Clipboard leaked node identities or layout");
    const auto before = editor.document();
    require(editor.pasteNodeData(nodePayload, {a, b, c}), editor.error());
    const auto after = editor.document();
    for (size_t i = 1; i < 4; ++i) {
        require(after["nodes"][i]["data"] == data, "Batch paste lost some node settings");
        require(after["nodes"][i]["id"] == before["nodes"][i]["id"]
            && after["nodes"][i]["visualizer"] == before["nodes"][i]["visualizer"], "Paste changed target identity or position");
    }
    require(after["symmetrical_edges"] == before["symmetrical_edges"]
        && after["asymmetrical_edges"] == before["asymmetrical_edges"], "Node paste changed connections");
    editor.undo(); require(editor.document() == before, "Batch paste is not one undo step");
    editor.redo(); require(editor.document() == after, "Redo lost pasted node settings");
    const auto beforeEdge = editor.document();
    require(editor.pasteConnectionData(edgePayload, 1), editor.error());
    require(editor.copyConnectionData(1) == edgePayload, "Edge data or relative directions did not copy");
    require(editor.document()["symmetrical_edges"][1]["ID_first"] == c
        && editor.document()["symmetrical_edges"][1]["ID_second"] == b, "Edge paste changed endpoint identity/order");
    require(editor.document()["nodes"] == beforeEdge["nodes"], "Edge paste changed nodes");
    const auto afterEdge = editor.document();
    editor.undo(); require(editor.document() == beforeEdge, "Edge paste did not undo directed data atomically");
    editor.redo(); require(editor.document() == afterEdge, "Edge paste redo lost directed data");
    auto oneDirection = edgePayload; oneDirection["reverse"] = nullptr;
    require(editor.pasteConnectionData(oneDirection, 1) && editor.copyConnectionData(1) == oneDirection,
        "Absent direction did not clear destination's previous rule");
    editor.undo();
    const auto unchanged = editor.document();
    auto invalid = nodePayload; invalid["data"]["radius"] = -1;
    for (const auto& payload : {Json::parse("bad", nullptr, false), Json::array(), edgePayload, invalid})
        require(!editor.pasteNodeData(payload, {a, b}) && editor.document() == unchanged && editor.canRedo(),
            "Invalid clipboard mutated targets or history");
    require(!editor.pasteNodeData(nodePayload, {a, 999}) && editor.document() == unchanged, "Missing target caused partial paste");
    auto badEdge = edgePayload; badEdge["forward"]["intakeDistance"] = -1;
    require(!editor.pasteConnectionData(badEdge, 1) && editor.document() == unchanged, "Invalid directional data partially pasted");
    require(!editor.pasteConnectionData(nodePayload, 1) && editor.document() == unchanged, "Node clipboard accepted for edge");
    auto oldCoordinates = editor.document();
    oldCoordinates["nodes"][0]["visualizer"] = {{"x", 3.125785187258712}, {"y", -0.0001}, {"extra", true}};
    require(editor.replace(oldCoordinates) && editor.node(source)->at("visualizer").at("x").dump() == "3.13"
        && editor.node(source)->at("visualizer").at("y").dump() == "0.0"
        && editor.node(source)->at("visualizer").at("extra") == true, "Imported coordinates were not rounded or lost metadata");
}

static void requirePlacedLayout(const TemplateEditor& editor) {
    std::vector<DoubleVector2> points;
    std::map<int, size_t> indices;
    std::vector<double> halfSizes;
    for (const auto& node : editor.document()["nodes"]) {
        const auto p = editor.position(node["id"]);
        const auto remainder = [](float value) { return std::fmod(std::fmod(value, 64) + 64, 64); };
        require((remainder(p.x) == 0 && remainder(p.y) == 0) || (remainder(p.x) == 32 && remainder(p.y) == 32),
            "Auto placement did not align to a grid cross or cell center");
        indices[node["id"].get<int>()] = points.size();
        points.emplace_back(p.x, p.y);
        halfSizes.push_back(TemplateEditor::halfSize(node));
    }
    std::vector<planar::Edge> edges;
    for (const auto& edge : editor.document()["symmetrical_edges"]) {
        const auto a = indices.at(edge["ID_first"].get<int>()), b = indices.at(edge["ID_second"].get<int>());
        edges.emplace_back(a, b);
        const double dx = points[a].x - points[b].x, dy = points[a].y - points[b].y;
        const double visibleLength = std::hypot(dx, dy) * (1 - (halfSizes[a] + halfSizes[b]) / std::max(std::abs(dx), std::abs(dy)));
        require(visibleLength + 1e-6 >= TemplateEditor::patternLength(edge),
            "Placed connection cannot show one full dotted pattern beyond node squares");
        for (size_t i = 0; i < points.size(); ++i) {
            if (i == a || i == b) continue;
            const double vx = points[b].x - points[a].x, vy = points[b].y - points[a].y;
            // Independently clip the segment against the node's padded square.
            double lo = 0, hi = 1;
            for (const auto [origin, delta, center] : {std::tuple{points[a].x, vx, points[i].x},
                    std::tuple{points[a].y, vy, points[i].y}}) {
                const double extent = halfSizes[i] + 7;
                if (delta == 0) { if (std::abs(origin - center) > extent) hi = -1; continue; }
                double first = (center - extent - origin) / delta, last = (center + extent - origin) / delta;
                if (first > last) std::swap(first, last);
                lo = std::max(lo, first); hi = std::min(hi, last);
            }
            require(lo > hi, "Placed edge passes through an unrelated node square");
        }
    }
    require(planar::validDrawing(points, edges), "Grid snapping introduced edge crossings or collapsed nodes");
    for (size_t i = 0; i < points.size(); ++i) for (size_t j = 0; j < i; ++j)
        require(std::abs(points[i].x - points[j].x) >= halfSizes[i] + halfSizes[j] + 16
            || std::abs(points[i].y - points[j].y) >= halfSizes[i] + halfSizes[j] + 16,
            "Auto placement left overlapping node squares");
}

static void requireCompactDiagram(const TemplateEditor& editor, size_t side, double maxSpan) {
    std::set<float> columns, rows;
    for (const auto& node : editor.document()["nodes"]) {
        const auto p = editor.position(node["id"]);
        columns.insert(p.x); rows.insert(p.y);
    }
    require(columns.size() == side && rows.size() == side, "Grid-like topology did not produce symmetric rows and columns");
    const float width = *columns.rbegin() - *columns.begin(), height = *rows.rbegin() - *rows.begin();
    require(width <= maxSpan && height <= maxSpan && width == height, "Diagram remains oversized or asymmetric");
    const float pitch = *std::next(columns.begin()) - *columns.begin();
    float previous = *columns.begin();
    for (auto it = std::next(columns.begin()); it != columns.end(); ++it) {
        require(*it - previous == pitch, "Diagram column spacing is not uniform"); previous = *it;
    }
    previous = *rows.begin();
    for (auto it = std::next(rows.begin()); it != rows.end(); ++it) {
        require(*it - previous == pitch, "Diagram row spacing differs from its columns"); previous = *it;
    }
}

static Json withoutPositions(Json document) {
    for (auto& node : document["nodes"]) {
        node["visualizer"].erase("x"); node["visualizer"].erase("y");
    }
    return document;
}

static void checkAutoPlacement() {
    TemplateEditor empty;
    require(empty.topologyWarning().empty() && empty.autoPlace() && !empty.canUndo(), "Empty graph has a topology warning or spurious placement history");
    std::vector<std::vector<planar::Edge>> examples{{}, {}, {{0, 1}}, {{0, 1}, {2, 3}},
        {{0, 1}, {1, 2}, {2, 3}, {3, 0}},
        {{0, 1}, {1, 2}, {2, 0}, {2, 3}, {3, 4}, {4, 5}, {5, 3}, {3, 6}}};
    const std::vector<int> counts{1, 2, 2, 5, 4, 7};
    std::vector<planar::Edge> grid, nested{{0, 1}, {1, 2}, {2, 0}};
    for (size_t y = 0; y < 3; ++y) for (size_t x = 0; x < 3; ++x) {
        if (x < 2) grid.emplace_back(y * 3 + x, y * 3 + x + 1);
        if (y < 2) grid.emplace_back(y * 3 + x, (y + 1) * 3 + x);
    }
    for (size_t v = 3; v < 16; ++v) {
        nested.emplace_back(0, v); nested.emplace_back(1, v); nested.emplace_back(v - 1, v);
    }
    examples.push_back(grid); examples.push_back(nested);
    for (size_t example = 0; example < examples.size(); ++example) {
        TemplateEditor editor;
        const int count = example < counts.size() ? counts[example] : example == counts.size() ? 9 : 16;
        std::vector<int> ids;
        // Start collapsed to establish that current drawing crossings/overlaps
        // never produce a topology warning or prevent auto placement.
        for (int i = 0; i < count; ++i) ids.push_back(editor.addNode({0, 0}));
        for (auto [a, b] : examples[example]) editor.connect(ids[a], ids[b]);
        auto document = editor.document();
        for (int i = 0; i < count; ++i) {
            document["nodes"][i]["data"]["radius"] = i % 3 == 0 ? 5.0 : i % 3 == 1 ? 0.1 : 2.0;
            document["nodes"][i]["visualizer"]["extension"] = "preserve";
        }
        if (count > 2 && !examples[example].empty()) {
            document["symmetrical_edges"][0]["data"]["passes"]["maxPassWidth"] = 300;
            const auto& edge = document["symmetrical_edges"][0];
            document["asymmetrical_edges"].push_back({{"ID_from", edge["ID_first"]}, {"ID_to", edge["ID_second"]},
                {"data", {{"intakeDistance", 2}, {"areaGuaranteed", 110}, {"bonusValue", 0.65}}}});
        }
        require(editor.replace(document), editor.error());
        require(editor.topologyWarning().empty(), "Planar graph with a collapsed manual drawing has a topology warning");
        require(editor.autoPlace(), "Auto place failed for example " + std::to_string(example) + ": " + editor.error());
        requirePlacedLayout(editor);
        const auto placed = editor.document();
        require(withoutPositions(placed) == withoutPositions(document), "Auto placement changed template data, IDs, connections or metadata");
        if (placed != document) {
            editor.undo(); require(editor.document() == document, "Auto placement is not a single undo step");
            editor.redo(); require(editor.document() == placed, "Auto placement redo lost positions");
        }
        require(editor.autoPlace() && editor.document() == placed, "Auto placement changes on repeated calls");
    }
    TemplateEditor lattice;
    for (int i = 0; i < 9; ++i) lattice.addNode({float(i * 17), float(i * -23)});
    for (auto [a, b] : grid) lattice.connect(static_cast<int>(a) + 1, static_cast<int>(b) + 1);
    require(lattice.autoPlace(), lattice.error());
    requirePlacedLayout(lattice);
    requireCompactDiagram(lattice, 3, 384);
    for (bool bipartite : {false, true}) {
        TemplateEditor editor;
        const int n = bipartite ? 6 : 5;
        for (int i = 0; i < n + 1; ++i) editor.addNode({0, 0});
        for (int a = 1; a <= n; ++a) for (int b = a + 1; b <= n; ++b)
            if (!bipartite || (a <= 3 && b > 3)) editor.connect(a, b);
        require(!editor.topologyWarning().empty(), "K5 or K3,3 did not raise a topology warning");
        auto snapshot = editor.document();
        const auto label = editor.undoLabel();
        require(!editor.autoPlace() && editor.document() == snapshot && editor.undoLabel() == label,
            "Non-planar auto placement changed document or history");
        const auto last = snapshot["symmetrical_edges"].back();
        editor.removeConnection(snapshot["symmetrical_edges"].size() - 1);
        require(editor.topologyWarning().empty(), "Deleting the obstruction edge did not clear the warning");
        editor.undo(); require(!editor.topologyWarning().empty(), "Undo did not restore the topology warning");
        editor.redo(); require(editor.topologyWarning().empty(), "Redo did not clear the topology warning");
        // Subdivision is still non-planar, despite having fewer edges per node.
        editor.connect(last["ID_first"], n + 1); editor.connect(n + 1, last["ID_second"]);
        require(!editor.topologyWarning().empty() && !editor.autoPlace(), "Subdivided non-planar obstruction was accepted");
    }
}

static void checkSpiceScout(const std::filesystem::path& path) {
    TemplateEditor editor;
    require(editor.load(path), editor.error());
    const auto original = editor.document();
    require(editor.autoPlace(), editor.error());
    requirePlacedLayout(editor);
    require(withoutPositions(editor.document()) == withoutPositions(original), "Spice Scout placement changed generation settings");
    double minX = 1e10, minY = 1e10, maxX = -1e10, maxY = -1e10;
    for (const auto& node : editor.document()["nodes"]) {
        const auto p = editor.position(node["id"]);
        minX = std::min(minX, double(p.x)); maxX = std::max(maxX, double(p.x));
        minY = std::min(minY, double(p.y)); maxY = std::max(maxY, double(p.y));
    }
    require(maxX - minX <= 1800 && maxY - minY <= 1800, "Spice Scout still has an oversized diagram");
    const auto placed = editor.document();
    require(editor.autoPlace() && editor.document() == placed, "Spice Scout placement is not repeatable");
}

int main(int argc, char** argv) {
    try {
        require(argc == 4 || argc == 5, "Pass the canonical template, geometry template, scratch path and optional Spice Scout fixture");
        if (argc == 5) checkSpiceScout(argv[4]);
        TemplateEditor editor;
        const std::filesystem::path scratch(argv[3]);
        checkHistory(scratch);
        checkMagnets(scratch);
        checkDataClipboard();
        checkConnectionInversion();
        checkAutoPlacement();
        require(templateValidationError(editor.document(), false).empty(), "New draft is malformed");
        require(!templateValidationError(editor.document()).empty(), "Empty draft accepted for generation");
        require(editor.save(scratch) && editor.load(scratch), "Empty draft cannot round trip");
        const int a = editor.addNode({-100.25f, 19.5f}), b = editor.addNode({250, -50});
        require(a != b && editor.dirty(), "Node creation did not produce unique IDs");
        require(!templateValidationError(editor.document()).empty(), "Isolated zones accepted for generation");
        require(editor.save(scratch) && editor.load(scratch), "Isolated draft cannot round trip");
        require(editor.connect(a, b) && !editor.connect(b, a) && !editor.connect(a, a)
            && !editor.connect(a, 999), "Connection uniqueness or endpoint validation failed");
        require(templateValidationError(editor.document()).empty(), "New connected graph cannot generate");
        auto data = editor.document();
        data["asymmetrical_edges"] = Json::array({
            {{"ID_from", a}, {"ID_to", b}, {"data", {{"intakeDistance", 2}, {"areaGuaranteed", 110}, {"bonusValue", 0.65}}}},
            {{"ID_from", b}, {"ID_to", a}, {"data", {{"intakeDistance", 0}, {"areaGuaranteed", 20}, {"bonusValue", 0.15}}}}
        });
        require(editor.renameNode(a, 42, data) && !editor.renameNode(b, 42, data), "ID collision accepted");
        require(editor.replace(data), editor.error());
        require(editor.document()["symmetrical_edges"][0]["ID_first"] == 42
            && editor.document()["asymmetrical_edges"][0]["ID_from"] == 42
            && editor.document()["asymmetrical_edges"][1]["ID_to"] == 42, "Renaming did not update every reference");
        editor.setPosition(42, {-740.125f, 50.25f});
        data = editor.document();
        data["author_extension"] = {{"notes", "Keep this"}};
        data["nodes"][0]["visualizer"]["extra"] = true;
        require(editor.replace(data), editor.error());
        require(editor.save(scratch) && !editor.dirty(), editor.error());
        TemplateEditor reopened;
        require(reopened.load(scratch) && reopened.document() == editor.document(), "JSON/position/direction round trip lost data");
        require(reopened.position(42) == TemplatePoint{-740.13f, 50.25f}, "Rounded position did not survive save/load");
        reopened.removeConnection(0);
        require(reopened.document()["asymmetrical_edges"].empty(), "Deleting connection left directed rules");
        require(reopened.connect(42, b), "Cannot recreate deleted connection");
        reopened.removeNode(42);
        require(!reopened.node(42) && reopened.document()["symmetrical_edges"].empty(), "Deleting node left dangling edges");
        require(!reopened.save(scratch / "missing" / "file.json") && reopened.dirty(), "Failed save cleared dirty state");
        require(reopened.load(scratch) && reopened.document() == data, "Failed save damaged existing template");
        const auto before = reopened.document();
        for (const Json point : {Json(nullptr), Json::array(), Json{{"x", "bad"}, {"y", 0}}, Json{{"x", 0}}}) {
            auto bad = before;
            bad["nodes"][0]["visualizer"] = point;
            require(!reopened.replace(bad) && reopened.document() == before, "Invalid coordinate changed document");
        }
        auto bad = before;
        bad["nodes"][0]["data"]["resources"]["content"][0]["endThreshold"] = 0.5;
        require(!reopened.replace(bad) && reopened.document() == before, "Invalid resource coverage accepted");
        {
            std::ofstream output(scratch);
            output << "{";
        }
        require(!reopened.load(scratch) && reopened.document() == before, "Malformed load discarded work");

        require(editor.load(argv[1]), editor.error());
        Json original;
        std::ifstream(argv[1]) >> original;
        auto legacy = original;
        for (auto& node : legacy["nodes"]) node.erase("visualizer");
        require(editor.replace(legacy), editor.error());
        require(editor.document()["nodes"].size() == original["nodes"].size(), "Legacy nodes lost");
        for (const auto& node : editor.document()["nodes"]) require(node.contains("visualizer"), "Legacy node has no position");
        const auto generated = generate(original);
        for (const auto& node : editor.document()["nodes"]) {
            const int id = node["id"];
            editor.setPosition(id, {float(id * -139), float(id * 61)});
        }
        require(generate(editor.document()) == generated, "Visualizer coordinates changed generation");
        require(editor.autoPlace(), editor.error());
        requirePlacedLayout(editor);
        requireCompactDiagram(editor, 5, 1024);
        require(generate(editor.document()) == generated, "Auto placement changed generated map output");
        require(editor.save(scratch) && reopened.load(scratch) && reopened.document() == editor.document(),
            "Canonical template failed save/reload");
        require(editor.load(argv[2]), editor.error());
        require(editor.save(scratch) && reopened.load(scratch) && reopened.document() == editor.document(),
            "Optional geometry did not survive round trip");
        std::filesystem::remove(scratch);
        std::cout << "Drafts, graph edits, directed rules, transactional load/save, legacy layouts and metadata-independent generation passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

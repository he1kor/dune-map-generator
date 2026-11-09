#include "template_editor.h"
#include "template_validation.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using Json = nlohmann::json;

namespace {
double canvasCoordinate(double value) {
    const double rounded = std::round(value * 100.0) / 100.0;
    return rounded == 0 ? 0.0 : rounded;
}
}

TemplateEditor::TemplateEditor() { createNew(); undo_.clear(); }

void TemplateEditor::createNew() {
    Action action(*this, "New template");
    document_ = {{"name", "New template"}, {"walls", {{"resource", "cliff_rock"}}},
        {"octaves", {{"octave1", {{"minRadius", 2.0}, {"maxRadius", 3.0}}}}},
        {"nodes", Json::array()}, {"symmetrical_edges", Json::array()}, {"asymmetrical_edges", Json::array()}};
    path_.clear();
    session_ = ++nextSession_;
    error_.clear();
    saved_[path_] = document_;
}

std::string TemplateEditor::checkPositions(const Json& candidate) {
    if (candidate.contains("visualizer") && !candidate["visualizer"].is_object())
        return "visualizer: expected an object";
    if (candidate.contains("visualizer") && candidate["visualizer"].is_object()
            && candidate["visualizer"].contains("magnetFrame")) {
        const auto& frame = candidate["visualizer"]["magnetFrame"];
        if (!frame.is_object() || !frame.contains("scale") || !frame["scale"].is_number()
                || !std::isfinite(frame["scale"].get<double>())
                || frame["scale"].get<double>() < 1 || frame["scale"].get<double>() > 100)
            return "visualizer/magnetFrame/scale: expected a finite scale between 1 and 100";
    }
    for (const auto& node : candidate.at("nodes")) {
        if (node["data"].contains("magnet")) {
            const auto& magnet = node["data"]["magnet"];
            if (!magnet.is_object() || !magnet.contains("force") || !magnet["force"].is_number()
                    || !std::isfinite(magnet["force"].get<double>())
                    || magnet["force"].get<double>() < minMagnetForce || magnet["force"].get<double>() > maxMagnetForce)
                return "nodes/" + node["id"].dump() + "/data/magnet/force: expected a finite force between 0.1 and 100";
            if (magnet.contains("position")) for (const char* axis : {"x", "y"}) {
                const auto& target = magnet["position"];
                if (!target.is_object() || !target.contains(axis) || !target[axis].is_number()
                        || !std::isfinite(target[axis].get<double>()))
                    return "nodes/" + node["id"].dump() + "/data/magnet/position/" + axis + ": expected a finite coordinate";
            }
        }
        if (!node.contains("visualizer")) continue;
        const auto& point = node["visualizer"];
        for (const char* axis : {"x", "y"}) {
            if (!point.is_object() || !point.contains(axis) || !point[axis].is_number()
                || !std::isfinite(point[axis].get<double>()) || std::abs(point[axis].get<double>()) > 1e7)
                return "nodes/" + std::to_string(node["id"].get<int>()) + "/visualizer/" + axis
                    + ": expected a finite canvas coordinate between -10000000 and 10000000";
        }
    }
    return {};
}

TemplateMagnetFrame TemplateEditor::magnetFrame(const Json& document) {
    TemplateMagnetFrame frame;
    if (document.contains("visualizer") && document["visualizer"].is_object()
            && document["visualizer"].contains("magnetFrame"))
        frame.scale = document["visualizer"]["magnetFrame"].value("scale", 1.0);
    double minX = 1e30, minY = 1e30, maxX = -1e30, maxY = -1e30;
    for (const auto& node : document["nodes"]) {
        const double x = node["visualizer"]["x"], y = node["visualizer"]["y"], half = halfSize(node);
        minX = std::min(minX, x - half); maxX = std::max(maxX, x + half);
        minY = std::min(minY, y - half); maxY = std::max(maxY, y + half);
    }
    if (!document["nodes"].empty()) {
        frame.center = {static_cast<float>(canvasCoordinate((minX + maxX) / 2)),
            static_cast<float>(canvasCoordinate((minY + maxY) / 2))};
        frame.halfExtent = std::max({64.0, maxX - frame.center.x, frame.center.x - minX,
            maxY - frame.center.y, frame.center.y - minY}) + 16;
    }
    // Round outward so serialization never shrinks the frame onto a node.
    frame.halfExtent = std::ceil(frame.halfExtent * frame.scale * 100) / 100;
    return frame;
}

void TemplateEditor::updateMagnets(Json& document) {
    bool hasMagnets = false;
    for (const auto& node : document["nodes"]) hasMagnets |= node["data"].contains("magnet");
    const bool hasFrame = document.contains("visualizer") && document["visualizer"].is_object()
        && document["visualizer"].contains("magnetFrame");
    if (!hasMagnets && !hasFrame) return;
    const auto frame = magnetFrame(document);
    auto& savedFrame = document["visualizer"]["magnetFrame"];
    savedFrame["scale"] = frame.scale;
    savedFrame["center"] = {{"x", canvasCoordinate(frame.center.x)}, {"y", canvasCoordinate(frame.center.y)}};
    savedFrame["halfExtent"] = frame.halfExtent;
    for (auto& node : document["nodes"]) if (node["data"].contains("magnet")) {
        auto& magnet = node["data"]["magnet"];
        magnet["force"] = canvasCoordinate(magnet["force"].get<double>());
        for (const char* axis : {"x", "y"}) {
            const double center = std::string(axis) == "x" ? frame.center.x : frame.center.y;
            const double target = 0.5 + (node["visualizer"][axis].get<double>() - center) / (2 * frame.halfExtent);
            magnet["position"][axis] = std::round(target * 1000000) / 1000000;
        }
    }
}

bool TemplateEditor::isMagnet(int id) const {
    const auto* entry = node(id);
    return entry && entry->at("data").contains("magnet");
}

double TemplateEditor::magnetForce(int id) const {
    return isMagnet(id) ? node(id)->at("data").at("magnet").at("force").get<double>() : defaultMagnetForce;
}

TemplatePoint TemplateEditor::magnetTarget(int id) const {
    const auto frame = magnetFrame();
    const auto point = position(id);
    return {static_cast<float>(0.5 + (point.x - frame.center.x) / (2 * frame.halfExtent)),
        static_cast<float>(0.5 + (point.y - frame.center.y) / (2 * frame.halfExtent))};
}

bool TemplateEditor::magnetInsideFrame(int id) const {
    if (!isMagnet(id)) return false;
    const auto target = magnetTarget(id);
    return target.x >= 0 && target.x <= 1 && target.y >= 0 && target.y <= 1;
}

void TemplateEditor::setMagnet(int id, bool enabled) {
    Action action(*this, std::string(enabled ? "Enable magnet on zone " : "Disable magnet on zone ") + std::to_string(id));
    for (auto& node : document_["nodes"]) if (node["id"] == id) {
        if (enabled && !node["data"].contains("magnet")) node["data"]["magnet"] = {{"force", defaultMagnetForce}};
        else if (!enabled) node["data"].erase("magnet");
        return;
    }
}

void TemplateEditor::setMagnetForce(int id, double force) {
    if (!std::isfinite(force)) return;
    Action action(*this, "Adjust magnet force on zone " + std::to_string(id));
    for (auto& node : document_["nodes"]) if (node["id"] == id) {
        node["data"]["magnet"]["force"] = canvasCoordinate(std::clamp(force, minMagnetForce, maxMagnetForce));
        return;
    }
}

void TemplateEditor::setMagnetFrameScale(double scale) {
    if (!std::isfinite(scale)) return;
    scale = std::clamp(canvasCoordinate(scale), 1.0, 100.0);
    if (scale == magnetFrame().scale) return;
    Action action(*this, "Resize magnet frame");
    document_["visualizer"]["magnetFrame"]["scale"] = scale;
}

float TemplateEditor::halfSize(const Json& node) {
    return static_cast<float>(std::clamp(24.0 * node.at("data").at("radius").get<double>(), 1.0, 100000.0));
}

void TemplateEditor::addMissingPositions(Json& candidate) {
    float spacing = 160;
    for (const auto& node : candidate["nodes"]) spacing = std::max(spacing, 2 * halfSize(node) + 64);
    const int columns = std::max(1, static_cast<int>(std::ceil(std::sqrt(candidate["nodes"].size()))));
    int i = 0;
    for (auto& node : candidate["nodes"]) {
        if (!node.contains("visualizer"))
            node["visualizer"] = {{"x", (i % columns) * spacing}, {"y", (i / columns) * spacing}};
        for (const char* axis : {"x", "y"})
            node["visualizer"][axis] = canvasCoordinate(node["visualizer"][axis].get<double>());
        ++i;
    }
}

bool TemplateEditor::replace(Json candidate, const std::string& label) {
    error_ = templateValidationError(candidate, false);
    if (!error_.empty()) return false;
    error_ = checkPositions(candidate);
    if (!error_.empty()) return false;
    addMissingPositions(candidate);
    updateMagnets(candidate);
    Action action(*this, label);
    if (document_ != candidate) {
        document_ = std::move(candidate);
    }
    return true;
}

bool TemplateEditor::load(const std::filesystem::path& path) {
    try {
        std::ifstream input(path);
        if (!input) { error_ = "Could not open the template file."; return false; }
        auto candidate = Json::parse(input);
        Action action(*this, "Load template");
        if (!replace(std::move(candidate))) return false;
        path_ = path;
        session_ = ++nextSession_;
        saved_[path_] = document_;
        return true;
    } catch (const std::exception& error) {
        error_ = error.what();
        return false;
    }
}

bool TemplateEditor::save(const std::filesystem::path& path) {
    // Write beside the destination first; a failed write leaves the old file
    // and the editor's dirty state intact. Windows rename cannot replace files.
    std::filesystem::path temporary;
    try {
        error_ = templateValidationError(document_, false);
        if (!error_.empty()) return false;
        temporary = path;
        temporary += ".saving";
        for (int i = 0; std::filesystem::exists(temporary); ++i) {
            temporary = path;
            temporary += ".saving" + std::to_string(i);
        }
        std::ofstream output(temporary, std::ios::binary);
        if (!output) { error_ = "Could not write the template file."; return false; }
        output << document_.dump(4) << '\n';
        output.close();
        if (!output) throw std::runtime_error("Writing the template failed.");
#ifdef _WIN32
        // MoveFileExW provides atomic replacement, including Unicode paths.
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace the template file (it may be read-only or in use).");
#else
        std::filesystem::rename(temporary, path);
#endif
        // Save as changes the file associated with this editing session, but
        // does not turn undoing an earlier edit into switching back to a file.
        if (path_ != path) {
            auto retarget = [&](Snapshot& snapshot) { if (snapshot.session == session_) snapshot.path = path; };
            for (auto& change : undo_) { retarget(change.before); retarget(change.after); }
            for (auto& change : redo_) { retarget(change.before); retarget(change.after); }
        }
        path_ = path;
        saved_[path_] = document_;
        error_.clear();
        return true;
    } catch (const std::exception& error) {
        error_ = error.what();
        if (!temporary.empty()) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        return false;
    }
}

const Json* TemplateEditor::node(int id) const {
    for (const auto& node : document_["nodes"]) if (node["id"] == id) return &node;
    return nullptr;
}

BoundaryPlacement TemplateEditor::boundaryPlacement(int id) const {
    const auto* entry=node(id);
    return entry?boundaryPlacementFromName(entry->at("data").value("boundary",std::string("any"))):BoundaryPlacement::Any;
}

void TemplateEditor::setBoundaryPlacement(const std::vector<int>& ids,BoundaryPlacement placement) {
    if(ids.empty())return;
    const auto name=boundaryPlacementName(placement);
    const std::set<int> selected(ids.begin(),ids.end());
    Action action(*this,"Set boundary placement to "+std::string(name));
    for(auto& entry:document_["nodes"])if(selected.contains(entry["id"].get<int>())) {
        if(placement==BoundaryPlacement::Any)entry["data"].erase("boundary");
        else entry["data"]["boundary"]=name;
    }
    error_.clear();
}

void TemplateEditor::cycleBoundaryPlacement(const std::vector<int>& ids) {
    if(ids.empty())return;
    const auto current=boundaryPlacement(ids.front());
    bool uniform=true;
    for(int id:ids)uniform&=boundaryPlacement(id)==current;
    const auto next=!uniform||current==BoundaryPlacement::Any?BoundaryPlacement::Outer
        :current==BoundaryPlacement::Outer?BoundaryPlacement::Inner:BoundaryPlacement::Any;
    setBoundaryPlacement(ids,next);
}

TemplatePoint TemplateEditor::position(int id) const {
    if (const auto* entry = node(id)) {
        const auto& point = entry->at("visualizer");
        return {point["x"].get<float>(), point["y"].get<float>()};
    }
    return {};
}

void TemplateEditor::setPosition(int id, TemplatePoint position) {
    Action action(*this, "Move zone " + std::to_string(id));
    if (!std::isfinite(position.x) || !std::isfinite(position.y)) return;
    position.x = std::clamp(position.x, -1e7f, 1e7f);
    position.y = std::clamp(position.y, -1e7f, 1e7f);
    for (auto& node : document_["nodes"]) if (node["id"] == id) {
        node["visualizer"]["x"] = canvasCoordinate(position.x);
        node["visualizer"]["y"] = canvasCoordinate(position.y);
        return;
    }
}

int TemplateEditor::addNode(TemplatePoint point) {
    Action action(*this, "Add zone");
    std::set<int> used;
    for (const auto& node : document_["nodes"]) used.insert(node["id"].get<int>());
    int id = 1;
    while (used.contains(id)) {
        if (id == std::numeric_limits<int>::max()) { error_ = "No free zone ID."; return -1; }
        ++id;
    }
    document_["nodes"].push_back({{"id", id}, {"visualizer", {{"x", 0.0}, {"y", 0.0}}},
        {"data", {{"radius", 1.0}, {"susceptibility", 0.0}, {"resources", {{"content", Json::array({
            {{"name", "sand"}, {"startThreshold", 0.0}, {"endThreshold", 1.0}}})}}}}}});
    setPosition(id, point);
    return id;
}

bool TemplateEditor::connect(int first, int second) {
    Action action(*this, "Connect " + std::to_string(first) + " - " + std::to_string(second));
    if (first == second || !node(first) || !node(second)) return false;
    for (const auto& edge : document_["symmetrical_edges"])
        if ((edge["ID_first"] == first && edge["ID_second"] == second)
            || (edge["ID_first"] == second && edge["ID_second"] == first)) return false;
    document_["symmetrical_edges"].push_back({{"ID_first", first}, {"ID_second", second},
        {"data", {{"passes", {{"minPassWidth", 13}, {"maxPassWidth", 25},
            {"minWallLength", 3}, {"maxWallLength", 5}}}, {"resourceBlend", {{"distance", 0}}}}}});
    return true;
}

bool TemplateEditor::invertConnection(Json& document, size_t index) {
    const auto& edges = document["symmetrical_edges"];
    if (index >= edges.size()) return false;
    const int first = edges[index]["ID_first"], second = edges[index]["ID_second"];
    bool changed = false;
    for (auto& rule : document["asymmetrical_edges"]) {
        if ((rule["ID_from"] == first && rule["ID_to"] == second)
            || (rule["ID_from"] == second && rule["ID_to"] == first)) {
            std::swap(rule["ID_from"], rule["ID_to"]);
            changed = true;
        }
    }
    return changed;
}

bool TemplateEditor::invertConnection(size_t index) {
    Action action(*this, "Reverse connection");
    return invertConnection(document_, index);
}

bool TemplateEditor::isDataClipboard(const Json& payload, const char* kind) {
    return payload.is_object() && payload.contains("map_generator_clipboard")
        && payload["map_generator_clipboard"] == 1 && payload.contains("kind") && payload["kind"] == kind
        && payload.contains("data") && payload["data"].is_object()
        && (std::string(kind) != "edge" || (payload.contains("forward") && payload.contains("reverse")
            && (payload["forward"].is_null() || payload["forward"].is_object())
            && (payload["reverse"].is_null() || payload["reverse"].is_object())));
}

Json TemplateEditor::copyNodeData(int id) const {
    const auto* source = node(id);
    return source ? Json{{"map_generator_clipboard", 1}, {"kind", "node"}, {"data", source->at("data")}} : Json();
}

Json TemplateEditor::copyConnectionData(size_t index) const {
    const auto& edges = document_["symmetrical_edges"];
    if (index >= edges.size()) return {};
    const auto& source = edges[index];
    Json result{{"map_generator_clipboard", 1}, {"kind", "edge"}, {"data", source["data"]},
        {"forward", nullptr}, {"reverse", nullptr}};
    for (const auto& rule : document_["asymmetrical_edges"]) {
        if (rule["ID_from"] == source["ID_first"] && rule["ID_to"] == source["ID_second"])
            result["forward"] = rule["data"];
        if (rule["ID_from"] == source["ID_second"] && rule["ID_to"] == source["ID_first"])
            result["reverse"] = rule["data"];
    }
    return result;
}

bool TemplateEditor::pasteNodeData(const Json& payload, const std::vector<int>& ids) {
    if (!isDataClipboard(payload, "node") || ids.empty()) {
        error_ = "Paste requires zone data and a zone selection."; return false;
    }
    auto candidate = document_;
    for (int id : ids) {
        auto found = std::find_if(candidate["nodes"].begin(), candidate["nodes"].end(),
            [id](const Json& entry) { return entry["id"] == id; });
        if (found == candidate["nodes"].end()) { error_ = "A destination zone no longer exists."; return false; }
        (*found)["data"] = payload["data"];
    }
    return replace(std::move(candidate), "Paste data to " + std::to_string(ids.size()) + " zones");
}

bool TemplateEditor::pasteConnectionData(const Json& payload, size_t index) {
    if (!isDataClipboard(payload, "edge") || index >= document_["symmetrical_edges"].size()) {
        error_ = "Paste requires connection data and a selected connection."; return false;
    }
    auto candidate = document_;
    auto& target = candidate["symmetrical_edges"][index];
    const int first = target["ID_first"], second = target["ID_second"];
    target["data"] = payload["data"];
    auto& rules = candidate["asymmetrical_edges"].get_ref<Json::array_t&>();
    std::erase_if(rules, [&](const Json& rule) {
        return (rule["ID_from"] == first && rule["ID_to"] == second)
            || (rule["ID_from"] == second && rule["ID_to"] == first);
    });
    if (!payload["forward"].is_null())
        rules.push_back({{"ID_from", first}, {"ID_to", second}, {"data", payload["forward"]}});
    if (!payload["reverse"].is_null())
        rules.push_back({{"ID_from", second}, {"ID_to", first}, {"data", payload["reverse"]}});
    return replace(std::move(candidate), "Paste connection data");
}

void TemplateEditor::removeConnection(size_t index) {
    Action action(*this, "Delete connection");
    auto& edges = document_["symmetrical_edges"];
    if (index >= edges.size()) return;
    const int first = edges[index]["ID_first"], second = edges[index]["ID_second"];
    auto& directed = document_["asymmetrical_edges"];
    std::erase_if(directed.get_ref<Json::array_t&>(), [&](const Json& edge) {
        return (edge["ID_from"] == first && edge["ID_to"] == second)
            || (edge["ID_from"] == second && edge["ID_to"] == first);
    });
    edges.erase(edges.begin() + index);
}

void TemplateEditor::removeNode(int id) {
    Action action(*this, "Delete zone " + std::to_string(id));
    for (size_t i = document_["symmetrical_edges"].size(); i-- > 0;)
        if (document_["symmetrical_edges"][i]["ID_first"] == id || document_["symmetrical_edges"][i]["ID_second"] == id)
            removeConnection(i);
    auto& nodes = document_["nodes"].get_ref<Json::array_t&>();
    std::erase_if(nodes, [&](const Json& node) { return node["id"] == id; });
}

bool TemplateEditor::renameNode(int oldId, int newId, Json& candidate) const {
    if (oldId == newId) return true;
    if (newId < 0) return false;
    for (const auto& node : candidate["nodes"]) if (node["id"] == newId) return false;
    for (auto& node : candidate["nodes"]) if (node["id"] == oldId) node["id"] = newId;
    for (auto& edge : candidate["symmetrical_edges"])
        for (const char* key : {"ID_first", "ID_second"}) if (edge[key] == oldId) edge[key] = newId;
    for (auto& edge : candidate["asymmetrical_edges"])
        for (const char* key : {"ID_from", "ID_to"}) if (edge[key] == oldId) edge[key] = newId;
    return true;
}

bool TemplateEditor::dirty() const {
    const auto saved = saved_.find(path_);
    return saved == saved_.end() || saved->second != document_;
}

void TemplateEditor::beginAction(const std::string& label) {
    if (actionDepth_++ == 0) { actionBefore_ = {document_, path_, session_}; actionLabel_ = label; }
}

void TemplateEditor::endAction() {
    if (actionDepth_ == 0 || --actionDepth_ != 0) return;
    updateMagnets(document_);
    if (actionBefore_.document == document_ && actionBefore_.path == path_ && actionBefore_.session == session_) return;
    undo_.push_back({std::move(actionBefore_), {document_, path_, session_}, std::move(actionLabel_)});
    redo_.clear();
}

namespace {
Json historyView(Json document) {
    if (!document.is_object()) return document;
    for (const auto* section : {"nodes", "symmetrical_edges", "asymmetrical_edges"}) {
        if (!document.contains(section)) continue;
        Json items = Json::object();
        for (auto item : document[section]) {
            std::string key;
            if (std::string(section) == "nodes") {
                key = "Zone " + item["id"].dump();
                item.erase("id");
            } else if (std::string(section) == "symmetrical_edges") {
                key = "Connection " + item["ID_first"].dump() + " - " + item["ID_second"].dump();
                item.erase("ID_first"); item.erase("ID_second");
            } else {
                key = item["ID_from"].dump() + " -> " + item["ID_to"].dump();
                item.erase("ID_from"); item.erase("ID_to");
            }
            if (item.contains("data")) {
                auto data = item["data"];
                item.erase("data");
                item.update(data);
            }
            items[key] = std::move(item);
        }
        document.erase(section);
        document.update(items);
    }
    return document;
}

void leafValues(const Json& value, const std::string& path, std::map<std::string, std::string>& result) {
    if (value.is_primitive() || value.empty()) { result[path] = value.dump(); return; }
    for (const auto& [key, child] : value.items())
        leafValues(child, path.empty() ? key : path + "/" + key, result);
}

std::string describe(const Json& from, const Json& to) {
    std::map<std::string, std::string> before, after;
    leafValues(historyView(from), "", before);
    leafValues(historyView(to), "", after);
    std::set<std::string> paths;
    for (const auto& [path, value] : before) paths.insert(path);
    for (const auto& [path, value] : after) paths.insert(path);
    std::string result;
    if (from.is_object() && to.is_object() && from.contains("nodes") && to.contains("nodes")
        && from["nodes"].size() == to["nodes"].size()) {
        for (size_t i = 0; i < from["nodes"].size(); ++i)
            if (from["nodes"][i]["id"] != to["nodes"][i]["id"])
                result += "\nZone ID: " + from["nodes"][i]["id"].dump() + " -> " + to["nodes"][i]["id"].dump();
    }
    for (const auto& path : paths) {
        const std::string a = before.contains(path) ? before[path] : "absent";
        const std::string b = after.contains(path) ? after[path] : "absent";
        if (a != b) result += "\n" + path + ": " + a + " -> " + b;
    }
    return result;
}
}

std::string TemplateEditor::undo() {
    if (!canUndo()) return {};
    auto change = std::move(undo_.back()); undo_.pop_back();
    auto message = "Undo: " + change.label + describe(change.after.document, change.before.document);
    document_ = change.before.document; path_ = change.before.path; session_ = change.before.session;
    redo_.push_back(std::move(change));
    error_.clear();
    return message;
}

std::string TemplateEditor::describeChanges(const Json& from, const Json& to) {
    return describe(from, to);
}

std::string TemplateEditor::redo() {
    if (!canRedo()) return {};
    auto change = std::move(redo_.back()); redo_.pop_back();
    auto message = "Redo: " + change.label + describe(change.before.document, change.after.document);
    document_ = change.after.document; path_ = change.after.path; session_ = change.after.session;
    undo_.push_back(std::move(change));
    error_.clear();
    return message;
}

const std::string& TemplateEditor::undoLabel() const {
    static const std::string empty;
    return undo_.empty() ? empty : undo_.back().label;
}
const std::string& TemplateEditor::redoLabel() const {
    static const std::string empty;
    return redo_.empty() ? empty : redo_.back().label;
}

#include "template_picker.h"
#include "template_validation.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <nlohmann/json.hpp>
#include "systemsound.h"
#include "planar_layout.h"

namespace {
using Json = nlohmann::json;
using SymEdges = std::unordered_map<std::pair<Identifiable, Identifiable>, BasicSymConnection, PairIDHash>;
using AsymEdges = std::unordered_map<std::pair<Identifiable, Identifiable>, BasicAsymConnection, AsymPairIDHash>;

struct ValidationError {
    bool missingKey;
    std::string path;
    std::string expected;
};

std::string child(const std::string& path, const std::string& key) {
    return path.empty() ? key : path + "/" + key;
}

[[noreturn]] void invalid(const std::string& path, const std::string& expected) {
    throw ValidationError{false, path.empty() ? "/" : path, expected};
}

void object(const Json& value, const std::string& path) {
    if (!value.is_object()) invalid(path, "an object");
}

void array(const Json& value, const std::string& path) {
    if (!value.is_array()) invalid(path, "an array");
}

const Json& member(const Json& value, const std::string& key, const std::string& path) {
    object(value, path);
    const auto it = value.find(key);
    if (it == value.end()) throw ValidationError{true, child(path, key), {}};
    return *it;
}

std::string text(const Json& value, const std::string& path) {
    if (!value.is_string()) invalid(path, "a string");
    return value.get<std::string>();
}

double number(const Json& value, const std::string& path) {
    if (!value.is_number()) invalid(path, "a finite number");
    const double result = value.get<double>();
    if (!std::isfinite(result)) invalid(path, "a finite number");
    return result;
}

double nonnegative(const Json& value, const std::string& path) {
    const double result = number(value, path);
    if (result < 0.0) invalid(path, "a non-negative number");
    return result;
}

double positive(const Json& value, const std::string& path) {
    const double result = number(value, path);
    if (result <= 0.0) invalid(path, "a positive number");
    return result;
}

int integer(const Json& value, const std::string& path) {
    const double result = number(value, path);
    if (result < 0.0 || result > std::numeric_limits<int>::max() || std::trunc(result) != result)
        invalid(path, "a non-negative whole number within the int range");
    return static_cast<int>(result);
}

Resource material(const Json& value, const std::string& path) {
    const auto result = resourceFromString(text(value, path));
    if (!result) invalid(path, "a known resource name (rock, dunes, sand, spice, thick_spice, cliff_rock)");
    return *result;
}

void report(TemplatePicker& picker, const ValidationError& error) {
    if (error.missingKey) picker.noKeyError = {true, error.path};
    else picker.badValueError = {true, error.path, error.expected};
}

std::string name(const Json& json) {
    const auto result = text(member(json, "name", ""), "name");
    if (result.empty()) invalid("name", "a non-empty string");
    return result;
}

std::vector<NoiseOctaveParam> readOctaves(const Json& json) {
    const auto& data = member(json, "octaves", "");
    object(data, "octaves");
    for (const auto& [key, value] : data.items()) {
        if (key != "octave1" && key != "octave2" && key != "octave3")
            invalid(child("octaves", key), "octave1, octave2 or octave3");
    }
    if (!data.contains("octave1")) throw ValidationError{true, "octaves/octave1", {}};
    std::vector<NoiseOctaveParam> result;
    for (int i = 1; i <= 3; ++i) {
        const auto key = "octave" + std::to_string(i);
        const auto path = child("octaves", key);
        const auto it = data.find(key);
        if (it == data.end()) continue;
        if (result.size() != static_cast<size_t>(i - 1))
            invalid(path, "consecutive octaves starting at octave1");
        const double minimum = positive(member(*it, "minRadius", path), child(path, "minRadius"));
        const double maximum = positive(member(*it, "maxRadius", path), child(path, "maxRadius"));
        if (maximum < minimum) invalid(child(path, "maxRadius"), "a value >= minRadius");
        result.push_back({1.0, minimum, maximum});
    }
    return result;
}

MapDimensionLimits readDimensionLimits(const Json& json) {
    MapDimensionLimits result;
    const auto it = json.find("dimensions");
    if (it == json.end()) return result;
    const auto& data = *it;
    object(data, "dimensions");
    for (const auto& [key, value] : data.items()) {
        if (key != "width" && key != "height" && key != "aspectRatio")
            invalid(child("dimensions", key), "width, height or aspectRatio");
    }
    auto range = [&](const std::string& key) -> const Json* {
        const auto entry = data.find(key);
        if (entry == data.end()) return nullptr;
        const auto path = child("dimensions", key);
        object(*entry, path);
        for (const auto& [bound, value] : entry->items()) {
            if (bound != "min" && bound != "max") invalid(child(path, bound), "min or max");
        }
        return &*entry;
    };
    auto side = [&](const std::string& key, int& minimum, int& maximum) {
        const auto entry = range(key);
        if (!entry) return;
        const auto path = child("dimensions", key);
        auto bound = [&](const std::string& key, int fallback) {
            const auto value = entry->find(key);
            if (value == entry->end()) return fallback;
            const int side = integer(*value, child(path, key));
            if (side < MapDimensionLimits::minimumSide || side > MapDimensionLimits::maximumSide || side % 2 != 0)
                invalid(child(path, key), "an even number from 32 to 128");
            return side;
        };
        minimum = bound("min", minimum);
        maximum = bound("max", maximum);
        if (minimum > maximum) invalid(child(path, "max"), "a value >= min");
    };
    side("width", result.minWidth, result.maxWidth);
    side("height", result.minHeight, result.maxHeight);
    if (const auto entry = range("aspectRatio")) {
        const auto path = "dimensions/aspectRatio";
        auto bound = [&](const std::string& key, double fallback) {
            const auto value = entry->find(key);
            if (value == entry->end()) return fallback;
            const double ratio = positive(*value, child(path, key));
            if (ratio < 0.25 || ratio > 4.0) invalid(child(path, key), "a width/height ratio from 0.25 to 4");
            return ratio;
        };
        result.minAspectRatio = bound("min", result.minAspectRatio);
        result.maxAspectRatio = bound("max", result.maxAspectRatio);
        if (result.minAspectRatio > result.maxAspectRatio)
            invalid(child(path, "max"), "a value >= min");
    }
    if (!result.hasAllowedSize())
        invalid("dimensions", "limits that allow at least one even width/height combination");
    return result;
}

ResourceRadialNode<Resource> readNode(const Json& node, const std::string& path, int id, size_t octaveCount) {
    const auto dataPath = child(path, "data");
    const auto& data = member(node, "data", path);
    const double radius = positive(member(data, "radius", dataPath), child(dataPath, "radius"));
    const double susceptibility = number(member(data, "susceptibility", dataPath), child(dataPath, "susceptibility"));
    const auto resourcesPath = child(dataPath, "resources");
    const auto& resources = member(data, "resources", dataPath);
    object(resources, resourcesPath);
    std::vector<double> weights(octaveCount, 0.0);
    for (const auto& [key, value] : resources.items()) {
        if (key.starts_with("octave") && key != "octave1_weight" && key != "octave2_weight" && key != "octave3_weight")
            invalid(child(resourcesPath, key), "octave1_weight, octave2_weight or octave3_weight");
    }
    for (int i = 1; i <= 3; ++i) {
        const auto key = "octave" + std::to_string(i) + "_weight";
        const auto it = resources.find(key);
        if (it == resources.end()) continue;
        const auto weightPath = child(resourcesPath, key);
        if (i > octaveCount) invalid(weightPath, "a weight for a declared octave");
        weights[i - 1] = nonnegative(*it, weightPath);
    }
    const auto contentPath = child(resourcesPath, "content");
    const auto& content = member(resources, "content", resourcesPath);
    array(content, contentPath);
    std::vector<ResourceMapping<Resource>> mappings;
    for (size_t i = 0; i < content.size(); ++i) {
        const auto entryPath = child(contentPath, std::to_string(i));
        const auto& entry = content[i];
        const auto resource = material(member(entry, "name", entryPath), child(entryPath, "name"));
        const double start = number(member(entry, "startThreshold", entryPath), child(entryPath, "startThreshold"));
        const double end = number(member(entry, "endThreshold", entryPath), child(entryPath, "endThreshold"));
        if (start < 0.0 || start > 1.0) invalid(child(entryPath, "startThreshold"), "a number in [0, 1]");
        if (end < start || end > 1.0) invalid(child(entryPath, "endThreshold"), "a number in [startThreshold, 1]");
        mappings.push_back({start, end, resource});
    }
    try {
        validateResourceMappings(mappings);
    } catch (const std::invalid_argument& error) {
        invalid(contentPath, error.what());
    }
    return ResourceRadialNode<Resource>(id, radius, susceptibility, weights, mappings);
}

int endpoint(const Json& edge, const std::string& key, const std::string& path, const std::set<int>& ids) {
    const int id = integer(member(edge, key, path), child(path, key));
    if (!ids.contains(id)) invalid(child(path, key), "an existing node ID");
    return id;
}

SymEdges readSymEdges(const Json& json, const std::set<int>& ids) {
    const std::string path = "symmetrical_edges";
    const auto& edges = member(json, path, "");
    array(edges, path);
    SymEdges result;
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto edgePath = child(path, std::to_string(i));
        const auto& edge = edges[i];
        const int first = endpoint(edge, "ID_first", edgePath, ids);
        const int second = endpoint(edge, "ID_second", edgePath, ids);
        if (first == second) invalid(child(edgePath, "ID_second"), "a different node ID");
        if (result.contains({first, second}) || result.contains({second, first}))
            invalid(edgePath, "a unique undirected connection");
        const auto dataPath = child(edgePath, "data");
        const auto& data = member(edge, "data", edgePath);
        const auto passesPath = child(dataPath, "passes");
        const auto& passes = member(data, "passes", dataPath);
        const int minPass = integer(member(passes, "minPassWidth", passesPath), child(passesPath, "minPassWidth"));
        const int maxPass = integer(member(passes, "maxPassWidth", passesPath), child(passesPath, "maxPassWidth"));
        const int minWall = integer(member(passes, "minWallLength", passesPath), child(passesPath, "minWallLength"));
        const int maxWall = integer(member(passes, "maxWallLength", passesPath), child(passesPath, "maxWallLength"));
        if (maxPass < minPass) invalid(child(passesPath, "maxPassWidth"), "a value >= minPassWidth");
        if (maxWall < minWall) invalid(child(passesPath, "maxWallLength"), "a value >= minWallLength");
        int blendDistance = 0;
        if (const auto it = data.find("resourceBlend"); it != data.end()) {
            const auto blendPath = child(dataPath, "resourceBlend");
            blendDistance = integer(member(*it, "distance", blendPath), child(blendPath, "distance"));
        }
        BasicSymConnection connection(blendDistance, tiles::PassParams{
            static_cast<size_t>(minPass), static_cast<size_t>(maxPass),
            static_cast<size_t>(minWall), static_cast<size_t>(maxWall)
        });
        if (const auto it = data.find("geometry"); it != data.end()) {
            const auto geometryPath = child(dataPath, "geometry");
            object(*it, geometryPath);
            if (const auto distance = it->find("preferredCenterDistance"); distance != it->end())
                connection.geometry.preferredCenterDistance = positive(*distance, child(geometryPath, "preferredCenterDistance"));
            if (const auto weight = it->find("distanceWeight"); weight != it->end())
                connection.geometry.distanceWeight = nonnegative(*weight, child(geometryPath, "distanceWeight"));
        }
        result.emplace(std::make_pair(first, second), connection);
    }
    return result;
}

AsymEdges readAsymEdges(const Json& json, const std::set<int>& ids, const SymEdges& symEdges) {
    const std::string path = "asymmetrical_edges";
    const auto& edges = member(json, path, "");
    array(edges, path);
    AsymEdges result;
    for (size_t i = 0; i < edges.size(); ++i) {
        const auto edgePath = child(path, std::to_string(i));
        const auto& edge = edges[i];
        const int from = endpoint(edge, "ID_from", edgePath, ids);
        const int to = endpoint(edge, "ID_to", edgePath, ids);
        if (from == to) invalid(child(edgePath, "ID_to"), "a different node ID");
        if (!symEdges.contains({from, to}) && !symEdges.contains({to, from}))
            invalid(edgePath, "a directed rule on an existing symmetrical connection");
        if (result.contains({from, to})) invalid(edgePath, "a unique directed connection");
        const auto dataPath = child(edgePath, "data");
        const auto& data = member(edge, "data", edgePath);
        const int intake = integer(member(data, "intakeDistance", dataPath), child(dataPath, "intakeDistance"));
        const int area = integer(member(data, "areaGuaranteed", dataPath), child(dataPath, "areaGuaranteed"));
        const double bonus = nonnegative(member(data, "bonusValue", dataPath), child(dataPath, "bonusValue"));
        result.emplace(std::make_pair(from, to), BasicAsymConnection(intake, area, bonus));
    }
    return result;
}

std::shared_ptr<MapTemplate> readGraph(const Json& json, bool requireGeneratable = true) {
    object(json, "");
    if (json.contains("name")) name(json);
    auto result = std::make_shared<MapTemplate>();
    result->dimensionLimits = readDimensionLimits(json);
    result->octaves = readOctaves(json);
    if (const auto it = json.find("magnets"); it != json.end()) {
        object(*it, "magnets");
        if (const auto rotation = it->find("randomRotation"); rotation != it->end()) {
            if (!rotation->is_boolean()) invalid("magnets/randomRotation", "a boolean");
            result->rotatePositionMagnets = rotation->get<bool>();
        }
    }
    if (const auto it = json.find("walls"); it != json.end())
        result->wallResource = material(member(*it, "resource", "walls"), "walls/resource");

    const auto& nodes = member(json, "nodes", "");
    array(nodes, "nodes");
    if (requireGeneratable && nodes.empty()) invalid("nodes", "a non-empty array of zones");
    std::set<int> ids;
    std::vector<std::pair<ResourceRadialNode<Resource>, std::vector<Identifiable>>> rawGraph;
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto path = child("nodes", std::to_string(i));
        const int id = integer(member(nodes[i], "id", path), child(path, "id"));
        if (!ids.insert(id).second) invalid(child(path, "id"), "a unique node ID");
        rawGraph.push_back({readNode(nodes[i], path, id, result->octaves.size()), {}});
        const auto& data = nodes[i]["data"];
        if (const auto it = data.find("magnet"); it != data.end()) {
            const auto magnetPath = child(child(path, "data"), "magnet");
            const double force = number(member(*it, "force", magnetPath), child(magnetPath, "force"));
            if (force < 0.1 || force > 100) invalid(child(magnetPath, "force"), "a force between 0.1 and 100");
            // The editor derives a newly enabled target after draft validation.
            if (!requireGeneratable && !it->contains("position")) continue;
            const auto positionPath = child(magnetPath, "position");
            const auto& position = member(*it, "position", magnetPath);
            const double x = number(member(position, "x", positionPath), child(positionPath, "x"));
            const double y = number(member(position, "y", positionPath), child(positionPath, "y"));
            if (x >= 0 && x <= 1 && y >= 0 && y <= 1)
                result->positionMagnets.emplace(Identifiable(id), PositionMagnet{{x,y},force});
        }
    }
    const auto symEdges = readSymEdges(json, ids);
    const auto asymEdges = readAsymEdges(json, ids, symEdges);
    std::unordered_map<Identifiable, std::vector<Identifiable>, IDHash> neighbours;
    for (const auto& [pair, edge] : symEdges) {
        neighbours[pair.first].push_back(pair.second);
        neighbours[pair.second].push_back(pair.first);
    }
    for (size_t i = 0; i < rawGraph.size(); ++i) {
        auto& [node, adjacent] = rawGraph[i];
        adjacent = neighbours[node];
        // The current bloater creates starting cells from connections.
        if (requireGeneratable && adjacent.empty()) invalid(child(child("nodes", std::to_string(i)), "id"),
            "a zone with at least one symmetrical connection");
    }
    result->zoneGraph = std::make_shared<EdgeGraph<ResourceRadialNode<Resource>, BasicSymConnection, BasicAsymConnection>>(
        rawGraph, symEdges, asymEdges);
    if (requireGeneratable) {
        try {
            result->embeddingTopology = planar::makeTopology(*result->zoneGraph);
        } catch (const planar::NonPlanarGraph& error) {
            invalid("symmetrical_edges", error.what());
        }
    }
    return result;
}
} // namespace

std::string templateValidationError(const nlohmann::json& document, bool requireGeneratable) {
    try {
        name(document);
        readGraph(document, requireGeneratable);
        return {};
    } catch (const ValidationError& error) {
        return error.missingKey ? "Missing field: " + error.path
            : error.path + ": expected " + error.expected;
    } catch (const std::exception& error) {
        return error.what();
    }
}

void TemplatePicker::resetErrors() {
    syntaxError = {};
    noKeyError = {};
    badValueError = {};
    fileOpenErrorOccurred = false;
}

bool TemplatePicker::tryReadEdgeGraph(const nlohmann::json& json) {
    resetErrors();
    try {
        auto next = readGraph(json);
        mapTemplate = std::move(next);
    } catch (const ValidationError& error) {
        report(*this, error);
        return false;
    }
    syssound::playInfo();
    return true;
}

void TemplatePicker::tryReadTemplate(const std::filesystem::path& path) {
    resetErrors();
    std::ifstream file(path);
    if (!file) {
        fileOpenErrorOccurred = true;
        return;
    }
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(file);
    } catch (const nlohmann::json::exception&) {
        syntaxError = {true};
        return;
    }
    try {
        const auto nextName = name(json);
        if (tryReadEdgeGraph(json)) templateName = nextName;
    } catch (const ValidationError& error) {
        report(*this, error);
    }
}

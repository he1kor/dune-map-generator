#include "generation.h"
#include "template_picker.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using Json = nlohmann::json;

static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static Json load(const char* path) {
    Json result;
    std::ifstream(path) >> result;
    return result;
}

static void integers(Json& value) {
    if (value.is_number_float()) {
        const double number = value.get<double>();
        if (std::trunc(number) == number) value = static_cast<long long>(number);
    } else if (value.is_structured()) {
        for (auto& item : value) integers(item);
    }
}

static bool noErrors(const TemplatePicker& picker) {
    return !picker.syntaxError.ocurred && !picker.noKeyError.ocurred
        && !picker.badValueError.ocurred && !picker.fileOpenErrorOccurred;
}

static std::vector<int> generate(const Json& data, size_t octaves) {
    TemplatePicker picker;
    require(picker.tryReadEdgeGraph(data), "Valid template was rejected before generation");
    require(picker.mapTemplate->octaves.size() == octaves, "Wrong octave count");
    for (auto id : picker.mapTemplate->zoneGraph->getIDs())
        require(picker.mapTemplate->zoneGraph->getValue(id).octaveWeights.size() == octaves,
            "Octave weights do not match the declared octaves");
    Generation generation;
    generation.isSeedRandom = false;
    generation.seed = 12419;
    generation.generate(picker.mapTemplate);
    generation.runIteration();
    require(generation.is_done, "Generation did not finish");
    std::vector<int> result;
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            const auto resource = generation.spiceMap.get(x, y);
            require(resource != Resource::NONE, "Generation left an unassigned cell");
            result.push_back(generation.grid->getTileID(x, y).getID());
            result.push_back(static_cast<int>(resource));
        }
    }
    return result;
}

int main(int argc, char** argv) {
    try {
        require(argc == 4, "Pass the canonical template, magnetic grid and temporary JSON paths");
        const Json baseline = load(argv[1]);
        const Json grid = load(argv[2]);
        TemplatePicker picker;
        require(picker.tryReadEdgeGraph(baseline) && noErrors(picker), "Canonical template was rejected");
        int rejected = 0;
        auto reject = [&](const Json& data, const std::string& path, bool missing = false) {
            const auto previous = picker.mapTemplate;
            require(!picker.tryReadEdgeGraph(data), "Invalid template was accepted: " + path);
            require(picker.mapTemplate == previous, "Invalid template replaced the last valid template");
            require(!picker.syntaxError.ocurred && !picker.fileOpenErrorOccurred, "Wrong error category: " + path);
            require(picker.noKeyError.ocurred == missing && picker.badValueError.ocurred == !missing,
                "Expected one validation error: " + path);
            const auto actual = missing ? picker.noKeyError.missingKey : picker.badValueError.targetKey;
            require(actual == path, "Expected error at " + path + ", got " + actual);
            if (!missing) require(!picker.badValueError.expectedType.empty(), "Error has no explanation");
            ++rejected;
        };
        auto invalidAt = [&](const std::string& path, const Json& value) {
            auto data = baseline;
            data[Json::json_pointer("/" + path)] = value;
            reject(data, path);
        };
        auto missingAt = [&](const std::string& parent, const std::string& key) {
            auto data = baseline;
            data[Json::json_pointer(parent.empty() ? "" : "/" + parent)].erase(key);
            reject(data, parent.empty() ? key : parent + "/" + key, true);
        };

        // Shape checks run before indexing or conversion, including at the root.
        for (const Json value : {Json(nullptr), Json(true), Json(3), Json("bad"), Json::array()}) reject(value, "/");
        for (const std::string key : {"octaves", "nodes", "symmetrical_edges", "asymmetrical_edges"}) {
            missingAt("", key);
            invalidAt(key, nullptr);
        }
        for (const std::string path : {"nodes/0", "nodes/0/data", "nodes/0/data/resources",
            "nodes/0/data/resources/content", "nodes/0/data/resources/content/0", "octaves/octave1",
            "symmetrical_edges/0", "symmetrical_edges/0/data", "symmetrical_edges/0/data/passes",
            "asymmetrical_edges/0", "asymmetrical_edges/0/data", "walls"}) invalidAt(path, nullptr);
        for (const auto& [parent, key] : std::vector<std::pair<std::string, std::string>>{
            {"nodes/0", "id"}, {"nodes/0", "data"}, {"nodes/0/data", "radius"},
            {"nodes/0/data", "susceptibility"}, {"nodes/0/data", "resources"},
            {"nodes/0/data/resources", "content"}, {"nodes/0/data/resources/content/0", "name"},
            {"nodes/0/data/resources/content/0", "startThreshold"}, {"nodes/0/data/resources/content/0", "endThreshold"},
            {"octaves/octave1", "minRadius"}, {"octaves/octave1", "maxRadius"},
            {"symmetrical_edges/0", "ID_first"}, {"symmetrical_edges/0", "ID_second"},
            {"symmetrical_edges/0", "data"}, {"symmetrical_edges/0/data", "passes"},
            {"symmetrical_edges/0/data/passes", "minPassWidth"}, {"symmetrical_edges/0/data/passes", "maxPassWidth"},
            {"symmetrical_edges/0/data/passes", "minWallLength"}, {"symmetrical_edges/0/data/passes", "maxWallLength"},
            {"asymmetrical_edges/0", "ID_from"}, {"asymmetrical_edges/0", "ID_to"},
            {"asymmetrical_edges/0", "data"}, {"asymmetrical_edges/0/data", "intakeDistance"},
            {"asymmetrical_edges/0/data", "areaGuaranteed"}, {"asymmetrical_edges/0/data", "bonusValue"}, {"walls", "resource"}
        }) missingAt(parent, key);

        // Missing octaves must not leave a mismatched three-weight vector.
        auto data = baseline;
        data["octaves"].erase("octave3");
        reject(data, "nodes/9/data/resources/octave3_weight");
        data = baseline;
        data["octaves"].erase("octave2");
        reject(data, "octaves/octave3");
        missingAt("octaves", "octave1");
        invalidAt("octaves/octave4", baseline["octaves"]["octave1"]);
        invalidAt("nodes/0/data/resources/octave4_weight", 1);

        // Accept integer and decimal syntax for numbers; reject non-numbers and
        // non-finite values rather than coercing strings, booleans or null.
        for (const std::string path : {"nodes/0/data/radius", "nodes/0/data/susceptibility",
            "nodes/0/data/resources/octave1_weight", "nodes/0/data/resources/octave2_weight",
            "nodes/0/data/resources/octave3_weight", "nodes/0/data/resources/content/0/startThreshold",
            "nodes/0/data/resources/content/0/endThreshold", "octaves/octave1/minRadius",
            "octaves/octave1/maxRadius", "asymmetrical_edges/0/data/bonusValue"}) {
            for (const Json value : {Json("3"), Json(true), Json(nullptr),
                Json(std::numeric_limits<double>::infinity()), Json(std::numeric_limits<double>::quiet_NaN())})
                invalidAt(path, value);
        }
        for (const std::string path : {"nodes/0/id", "symmetrical_edges/0/ID_first", "symmetrical_edges/0/ID_second",
            "symmetrical_edges/0/data/passes/minPassWidth", "symmetrical_edges/0/data/passes/maxPassWidth",
            "symmetrical_edges/0/data/passes/minWallLength", "symmetrical_edges/0/data/passes/maxWallLength",
            "asymmetrical_edges/0/ID_from", "asymmetrical_edges/0/ID_to",
            "asymmetrical_edges/0/data/intakeDistance", "asymmetrical_edges/0/data/areaGuaranteed"}) {
            for (const Json value : {Json(-1), Json(0.5), Json("3"), Json(true), Json(nullptr),
                Json(std::numeric_limits<unsigned long long>::max())}) invalidAt(path, value);
        }
        invalidAt("nodes/0/data/radius", 0);
        invalidAt("nodes/0/data/radius", -1);
        invalidAt("nodes/0/data/resources/octave1_weight", -1);
        invalidAt("asymmetrical_edges/0/data/bonusValue", -1);
        invalidAt("octaves/octave1/minRadius", 0);
        invalidAt("octaves/octave1/maxRadius", 1);
        invalidAt("symmetrical_edges/0/data/passes/maxPassWidth", 1);
        invalidAt("symmetrical_edges/0/data/passes/maxWallLength", 1);
        invalidAt("symmetrical_edges/0/data/resourceBlend", nullptr);
        invalidAt("symmetrical_edges/0/data/resourceBlend/distance", -1);
        data = baseline;
        data["symmetrical_edges"][0]["data"]["resourceBlend"] = Json::object();
        reject(data, "symmetrical_edges/0/data/resourceBlend/distance", true);

        // Resource identities and coverage are local to each zone; list lengths
        // and resource ordering intentionally need not match between zones.
        invalidAt("nodes/0/data/resources/content/0/name", "unknown");
        invalidAt("nodes/0/data/resources/content/0/name", 3);
        invalidAt("walls/resource", "unknown");
        invalidAt("nodes/0/data/resources/content", Json::array());
        invalidAt("nodes/0/data/resources/content/0/startThreshold", -0.1);
        invalidAt("nodes/0/data/resources/content/0/endThreshold", 1.1);
        invalidAt("nodes/0/data/resources/content/0/endThreshold", -0.1);
        for (const double start : {0.4, 0.6}) {
            data = baseline;
            data["nodes"][0]["data"]["resources"]["content"] = Json::array({
                {{"name", "rock"}, {"startThreshold", 0}, {"endThreshold", 0.5}},
                {{"name", "sand"}, {"startThreshold", start}, {"endThreshold", 1}}
            });
            reject(data, "nodes/0/data/resources/content");
        }
        data = baseline;
        data["nodes"][0]["data"]["resources"]["content"][0]["endThreshold"] = 0;
        reject(data, "nodes/0/data/resources/content");

        data = baseline;
        data["nodes"] = Json::array();
        reject(data, "nodes");
        invalidAt("nodes/1/id", 1);
        invalidAt("symmetrical_edges/0/ID_first", 99999);
        invalidAt("symmetrical_edges/0/ID_second", 1);
        data = baseline;
        data["symmetrical_edges"].push_back(data["symmetrical_edges"][0]);
        const auto duplicatePath = "symmetrical_edges/" + std::to_string(data["symmetrical_edges"].size() - 1);
        reject(data, duplicatePath);
        std::swap(data["symmetrical_edges"].back()["ID_first"], data["symmetrical_edges"].back()["ID_second"]);
        reject(data, duplicatePath);
        data = baseline;
        data["nodes"].push_back(data["nodes"][0]);
        data["nodes"].back()["id"] = 99999;
        reject(data, "nodes/" + std::to_string(data["nodes"].size() - 1) + "/id");
        invalidAt("asymmetrical_edges/0/ID_from", 99999);
        invalidAt("asymmetrical_edges/0/ID_to", 1);
        data = baseline;
        data["asymmetrical_edges"].push_back(data["asymmetrical_edges"][0]);
        reject(data, "asymmetrical_edges/" + std::to_string(data["asymmetrical_edges"].size() - 1));
        data = grid;
        data["asymmetrical_edges"].push_back({{"ID_from", 0}, {"ID_to", 8},
            {"data", {{"intakeDistance", 0}, {"areaGuaranteed", 0}, {"bonusValue", 0}}}});
        reject(data, "asymmetrical_edges/0");

        // Geometry is optional and soft; malformed numbers still identify their field.
        invalidAt("symmetrical_edges/0/data/geometry", nullptr);
        const std::string geometryPath = "symmetrical_edges/0/data/geometry/";
        for (const Json value : {Json(0), Json(-1), Json("15"), Json(true), Json(nullptr),
                Json(std::numeric_limits<double>::infinity())})
            invalidAt(geometryPath + "preferredCenterDistance", value);
        for (const Json value : {Json(-1), Json("2"), Json(true), Json(nullptr),
                Json(std::numeric_limits<double>::quiet_NaN())})
            invalidAt(geometryPath + "distanceWeight", value);
        data = baseline;
        data["symmetrical_edges"][0]["data"]["geometry"] =
            {{"preferredCenterDistance", 15.0}, {"distanceWeight", 2.0}};
        require(picker.tryReadEdgeGraph(data), "Valid edge geometry was rejected");
        const auto& firstEdge = data["symmetrical_edges"][0];
        const auto geometry = picker.mapTemplate->zoneGraph->getSymEdge(
            Identifiable(firstEdge["ID_first"]), Identifiable(firstEdge["ID_second"])).geometry;
        require(geometry.preferredCenterDistance == 15.0 && geometry.distanceWeight == 2.0,
            "Edge geometry was not preserved");
        require(bool(picker.mapTemplate->embeddingTopology), "Planar topology was not cached");

        // K3,3 passes the elementary edge-count bound, so require a real planarity test.
        for (bool bipartite : {true, false}) {
            data = grid;
            data["nodes"].erase(data["nodes"].begin() + (bipartite ? 6 : 5), data["nodes"].end());
            data["symmetrical_edges"] = Json::array();
            data["asymmetrical_edges"] = Json::array();
            for (int a = 0; a < (bipartite ? 6 : 5); ++a) {
                for (int b = a + 1; b < (bipartite ? 6 : 5); ++b) {
                    if (bipartite && !(a < 3 && b >= 3)) continue;
                    auto edge = grid["symmetrical_edges"][0];
                    edge["ID_first"] = a; edge["ID_second"] = b;
                    data["symmetrical_edges"].push_back(edge);
                }
            }
            reject(data, "symmetrical_edges");
            require(picker.badValueError.expectedType.find("obstruction") != std::string::npos,
                "Nonplanarity error omitted its obstruction");
        }

        auto integerData = baseline;
        integers(integerData);
        require(generate(integerData, 3) == generate(baseline, 3), "Numeric syntax changed the generated map");
        data = baseline;
        for (auto& node : data["nodes"]) {
            auto& content = node["data"]["resources"]["content"];
            std::reverse(content.begin(), content.end());
        }
        require(picker.tryReadEdgeGraph(data) && noErrors(picker), "Independent resource ordering was rejected");
        data.erase("walls");
        require(picker.tryReadEdgeGraph(data) && picker.mapTemplate->wallResource == Resource::CLIFF_ROCK,
            "Optional wall material default changed");
        for (int count : {1, 2}) {
            data = grid;
            for (int i = count + 1; i <= 3; ++i) data["octaves"].erase("octave" + std::to_string(i));
            data["nodes"][0]["data"]["resources"]["octave1_weight"] = 1;
            generate(data, count);
        }

        // File loading is transactional, and failures do not leak error flags
        // into the next attempt or replace the last successful template.
        const std::filesystem::path scratch(argv[3]);
        auto write = [&](const std::string& contents) {
            std::ofstream output(scratch);
            require(bool(output), "Cannot write temporary template");
            output << contents;
        };
        write(baseline.dump());
        picker.tryReadTemplate(scratch);
        require(noErrors(picker) && picker.templateName == baseline["name"].get<std::string>(), "Valid file was rejected");
        const auto previous = picker.mapTemplate;
        const auto previousName = picker.templateName;
        auto unchanged = [&] {
            require(picker.mapTemplate == previous && picker.templateName == previousName,
                "Failed file load changed the active template");
        };
        for (const std::string contents : {"{", "{\"value\":1e999}"}) {
            write(contents);
            picker.tryReadTemplate(scratch);
            require(picker.syntaxError.ocurred && !picker.badValueError.ocurred && !picker.noKeyError.ocurred,
                "Malformed JSON did not produce a syntax error");
            unchanged();
        }
        data = baseline;
        data.erase("name");
        write(data.dump());
        picker.tryReadTemplate(scratch);
        require(picker.noKeyError.ocurred && picker.noKeyError.missingKey == "name" && !picker.syntaxError.ocurred,
            "Missing file name did not produce a fresh validation error");
        unchanged();
        data["name"] = 7;
        write(data.dump());
        picker.tryReadTemplate(scratch);
        require(picker.badValueError.ocurred && picker.badValueError.targetKey == "name" && !picker.noKeyError.ocurred,
            "Invalid name did not produce a fresh validation error");
        unchanged();
        std::filesystem::remove(scratch);
        picker.tryReadTemplate(scratch);
        require(picker.fileOpenErrorOccurred && !picker.badValueError.ocurred && !picker.syntaxError.ocurred,
            "Unreadable file did not produce a fresh file error");
        unchanged();
        picker.tryReadTemplate(argv[2]);
        require(noErrors(picker) && picker.templateName == grid["name"].get<std::string>() && picker.mapTemplate != previous,
            "Recovery after invalid files failed");

        std::cout << rejected << " malformed-template cases rejected with exact paths; no exceptions escaped\n"
            << "Integer/decimal equivalence, 1-3 octaves, independent resource lists, generation and reload recovery passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

#include "template_picker.h"

#include <fstream>
#include <nlohmann/json.hpp>


std::optional<RadialNode> TemplatePicker::tryReadNodeData(const nlohmann::json& node, int id){
    auto dataIt = node.find("data");
    if (dataIt == node.end()){
        noKeyError = {true, "graph/[]/data"};
        return std::nullopt;
    }

    auto radiusIt = dataIt->find("radius");
    if (radiusIt == dataIt->end()){
        noKeyError = {true, "graph/[]/data/radius"};
        return std::nullopt;
    }
    if (!radiusIt->is_number_float()){
        badValueError = {true, "graph/[]/data/radius", "float"};
        return std::nullopt;
    }

    auto susceptibilityIt = dataIt->find("susceptibility");
    if (susceptibilityIt == dataIt->end()){
        noKeyError = {true, "graph/[]/data/susceptibility"};
        return std::nullopt;
    }
    if (!susceptibilityIt->is_number_float()){
        badValueError = {true, "graph/[]/data/susceptibility", "float"};
        return std::nullopt;
    }

    return RadialNode(id, radiusIt->get<double>(), susceptibilityIt->get<double>());
}

bool TemplatePicker::tryReadEdgeGraph(const nlohmann::json& json){
    auto graphIt = json.find("graph");
    if (graphIt == json.end()){
        noKeyError = {true, "graph"};
        return false;
    }
    if (!graphIt->is_array()){
        badValueError = {true, "graph/[]", "array"};
        return false;
    }

    std::vector<std::pair<RadialNode, std::vector<Identifiable>>> graph;
    for (const auto& node : *graphIt){
        auto idIt = node.find("id");
        if (idIt == node.end()){
            noKeyError = {true, "graph/[]/id"};
            return false;
        }
        if (!idIt->is_number_integer()){
            badValueError = {true, "graph/[]/id", "int"};
            return false;
        }

        auto neighbourIDsIt = node.find("neighbors");
        if (neighbourIDsIt == node.end()){
            noKeyError = {true, "graph/[]/neighbors"};
            return false;
        }
        std::vector<Identifiable> ids;
        try{
            auto rawIds = neighbourIDsIt->get<std::vector<int>>();
            ids = std::vector<Identifiable>(rawIds.begin(), rawIds.end());
        } catch (const nlohmann::json::type_error&){
            badValueError = {true, "graph/[]/neighbors", "int array"};
            return false;
        }

        auto optionalRadialNode = tryReadNodeData(node, idIt->get<int>());
        if (!optionalRadialNode){
            return false;
        }
        
        graph.push_back({*optionalRadialNode, ids});
    }
    mapTemplate = std::make_shared<EdgeGraph<RadialNode, Identifiable>>(graph);
    return true;
}

void TemplatePicker::tryReadTemplate(const std::filesystem::path &path){
    std::ifstream file(path);
    if (!file){
        fileOpenErrorOccurred = true;
        return;
    }
    nlohmann::json json = nlohmann::json::parse(file, nullptr, false);

    if (json.is_discarded()){
        syntaxError = {true};
        return;
    }

    auto templateNameIt = json.find("name");
    if (templateNameIt == json.end()){
        noKeyError = {true, "name"};
        return;
    }
    if (!templateNameIt->is_string()){
        badValueError = {true, "name", "string"};
        return;
    }
    if (!tryReadEdgeGraph(json)){
        return;
    }
    templateName = *templateNameIt;
}

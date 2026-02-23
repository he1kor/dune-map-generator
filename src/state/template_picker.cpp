#include "template_picker.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include "systemsound.h"
#include <connection.h>


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

    std::vector<std::pair<RadialNode, std::vector<Identifiable>>> rawGraph;
    std::unordered_map<std::pair<Identifiable, Identifiable>, BasicConnection, AsymPairIDHash> asymEdges;
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
        int id = idIt->get<int>();

        auto neighboursIt = node.find("neighbors");
        if (neighboursIt == node.end()){
            noKeyError = {true, "graph/[]/neighbors"};
            return false;
        }
        std::vector<Identifiable> ids;
        for (const auto& neighbour : *neighboursIt){
            auto neighbourIdIt = neighbour.find("id");
            if (neighbourIdIt == neighbour.end()){
                noKeyError = {true, "graph/[]/neighbors/id"};
                return false;
            }
            if (!neighbourIdIt->is_number_integer()){
                badValueError = {true, "graph/[]/neighbors/id", "int"};
                return false;
            }
            int neighbourId = neighbourIdIt->get<int>();
            
            auto resourceBlendDistanceIt = neighbour.find("resourceBlendDistance");
            if (resourceBlendDistanceIt == neighbour.end()){
                noKeyError = {true, "graph/[]/neighbors/resourceBlendDistance"};
                return false;
            }
            int resourceBlendDistance = resourceBlendDistanceIt->get<int>();

            auto intakeDistanceIt = neighbour.find("intakeDistance");
            if (intakeDistanceIt == neighbour.end()){
                noKeyError = {true, "graph/[]/neighbors/intakeDistance"};
                return false;
            }
            int intakeDistance = intakeDistanceIt->get<int>();

            auto areaGuaranteedIt = neighbour.find("areaGuaranteed");
            if (areaGuaranteedIt == neighbour.end()){
                noKeyError = {true, "graph/[]/neighbors/areaGuaranteed"};
                return false;
            }
            int areaGuaranteed = areaGuaranteedIt->get<int>();

            auto bonusValueIt = neighbour.find("bonusValue");
            if (bonusValueIt == neighbour.end()){
                noKeyError = {true, "graph/[]/neighbors/bonusValue"};
                return false;
            }
            int bonusValue = bonusValueIt->get<double>();
            
            ids.push_back(neighbourId);
            asymEdges[{id, neighbourId}] = BasicConnection(resourceBlendDistance, intakeDistance, areaGuaranteed, bonusValue);
        }

        auto optionalRadialNode = tryReadNodeData(node, id);
        if (!optionalRadialNode){
            return false;
        }
        
        rawGraph.push_back({*optionalRadialNode, ids});
    }
    mapTemplate = std::make_shared<EdgeGraph<RadialNode, int, BasicConnection>>(rawGraph);
    mapTemplate->initializeAsymEdges(asymEdges);
    syssound::playInfo();
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

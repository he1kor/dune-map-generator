#include "template_picker.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include "systemsound.h"
#include <connection.h>


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

    std::vector<std::pair<ResourceRadialNode<Resource>, std::vector<Identifiable>>> rawGraph;
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
    mapTemplate = std::make_shared<EdgeGraph<ResourceRadialNode<Resource>, int, BasicConnection>>(rawGraph);
    mapTemplate->initializeAsymEdges(asymEdges);
    syssound::playInfo();
    return true;
}


std::optional<ResourceRadialNode<Resource>> TemplatePicker::tryReadNodeData(const nlohmann::json& node, int id){
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

    auto resources = tryReadResourceData(*dataIt);
    if (resources == std::nullopt){
        return std::nullopt;
    }

    return ResourceRadialNode<Resource>(
        id,
        radiusIt->get<double>(),
        susceptibilityIt->get<double>(),
        resources.value().first,
        resources.value().second
    );
}

std::optional<
    std::pair<
        std::vector<NoiseOctaveParam>,
        std::vector<ResourceMapping<Resource>>
    >
> TemplatePicker::tryReadResourceData(const nlohmann::json& nodeJSON){
    auto resourcesIt = nodeJSON.find("resources");
    if (resourcesIt == nodeJSON.end()){
        noKeyError = {true, "graph/[]/data/resources"};
        return std::nullopt;
    }
    auto octave1It = resourcesIt->find("octave1");
    std::optional<NoiseOctaveParam> octave1 = std::nullopt;
    std::optional<NoiseOctaveParam> octave2 = std::nullopt;
    std::optional<NoiseOctaveParam> octave3 = std::nullopt;
    std::vector<NoiseOctaveParam> octaves;
    if (octave1It != resourcesIt->end()){
        octave1 = tryReadOctave(*octave1It);
        if (octave1 == std::nullopt){
            return std::nullopt;
        }
        octaves.push_back(octave1.value());
        auto octave2It = resourcesIt->find("octave2");
        if (octave2It != resourcesIt->end()){
            octave2 = tryReadOctave(*octave2It);
            if (octave2 == std::nullopt){
                return std::nullopt;
            }   
            octaves.push_back(octave2.value());
            auto octave3It = resourcesIt->find("octave3");
            if (octave3It != resourcesIt->end()){
                octave3 = tryReadOctave(*octave3It);
                if (octave3 == std::nullopt){
                    return std::nullopt;
                }
                octaves.push_back(octave3.value());
            }
        }
    }

    auto contentIt = resourcesIt->find("content");
    if (contentIt == resourcesIt->end()){
        noKeyError = {true, "graph/[]/data/resources/content"};
        return std::nullopt;
    }

    if (!contentIt->is_array()){
        badValueError = {true, "graph/[]/data/resources/content/[]", "array"};
        return std::nullopt;
    }

    std::vector<ResourceMapping<Resource>> contents;
    for (const auto& node : *contentIt){
        std::optional<ResourceMapping<Resource>> content = tryReadContentData(node);
        if (content == std::nullopt){
            return std::nullopt;
        }
    }
    
    return std::make_pair(octaves, contents);
}

std::optional<NoiseOctaveParam> TemplatePicker::tryReadOctave(const nlohmann::json& octaveJSON){
    double weight;
    double minRadius;
    double maxRadius;

    auto weightIt = octaveJSON.find("weight");
    if (weightIt == octaveJSON.end()){
        noKeyError = {true, "graph/[]/data/resources/octave{*}/weight"};
        return std::nullopt;
    }
    if (!weightIt->is_number_float()){
        badValueError = {true, "graph/[]/data/resources/octave{*}/weight", "float"};
        return std::nullopt;
    }

    auto minRadiusIt = octaveJSON.find("minRadius");
    if (minRadiusIt == octaveJSON.end()){
        noKeyError = {true, "graph/[]/data/resources/octave{*}/minRadius"};
        return std::nullopt;
    }
    if (!minRadiusIt->is_number_float()){
        badValueError = {true, "graph/[]/data/resources/octave{*}/minRadius", "float"};
        return std::nullopt;
    }


    auto maxRadiusIt = octaveJSON.find("maxRadius");
    if (maxRadiusIt == octaveJSON.end()){
        noKeyError = {true, "graph/[]/data/resources/octave{*}/maxRadius"};
        return std::nullopt;
    }
    if (!maxRadiusIt->is_number_float()){
        badValueError = {true, "graph/[]/data/resources/octave{*}/maxRadius", "float"};
        return std::nullopt;
    }

    return NoiseOctaveParam{
        .weight = weightIt->get<double>(),
        .minimalBlob = minRadiusIt->get<double>(),
        .maximalBlob = maxRadiusIt->get<double>()
    };
}

std::optional<ResourceMapping<Resource>> TemplatePicker::tryReadContentData(const nlohmann::json &contentJSON)
{
    std::optional<Resource> resource;
    double startThreshold;
    double endThreshold;

    auto resourceNameIt = contentJSON.find("name");
    if (resourceNameIt == contentJSON.end()){
        noKeyError = {true, "graph/[]/data/resources/content/[]/name"};
        return std::nullopt;
    }
    if (!resourceNameIt->is_string()){
        badValueError = {true, "graph/[]/data/resources/content/[]/name", "string"};
        return std::nullopt;
    }
    resource = resourceFromString(resourceNameIt->get<std::string>());
    if (resource == std::nullopt){
        badValueError = {true, "graph/[]/data/resources/content/[]/name", std::format("Unknown resource: {}", resourceNameIt->get<std::string>())};
    }


    auto startThresholdIt = contentJSON.find("startThreshold");
    if (startThresholdIt == contentJSON.end()){
        noKeyError = {true, "graph/[]/data/resources/octave{*}/startThreshold"};
        return std::nullopt;
    }
    if (!startThresholdIt->is_number_float()){
        badValueError = {true, "graph/[]/data/resources/octave{*}/startThreshold", "float"};
        return std::nullopt;
    }


    auto endThresholdIt = contentJSON.find("endThreshold");
    if (endThresholdIt == contentJSON.end()){
        noKeyError = {true, "graph/[]/data/resources/octave{*}/endThreshold"};
        return std::nullopt;
    }
    if (!endThresholdIt->is_number_float()){
        badValueError = {true, "graph/[]/data/resources/octave{*}/endThreshold", "float"};
        return std::nullopt;
    }

    return ResourceMapping<Resource>{
        .minimalThreshold = startThresholdIt->get<double>(),
        .maximalThreshold = endThresholdIt->get<double>(),
        .resource = resource.value(),
    };
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

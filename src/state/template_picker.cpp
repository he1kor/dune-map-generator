#include "template_picker.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include "systemsound.h"
#include <connection.h>


bool TemplatePicker::tryReadEdgeGraph(const nlohmann::json& json){
    auto nodesIt = json.find("nodes");
    if (nodesIt == json.end()){
        noKeyError = {true, "nodes"};
        return false;
    }
    if (!nodesIt->is_array()){
        badValueError = {true, "nodes/[]", "array"};
        return false;
    }

    auto octavesIt = json.find("octaves");
    if (octavesIt == json.end()){
        noKeyError = {true, "octaves"};
        return false;
    }
    auto octaves = tryReadOctaves(*octavesIt);
    if (!octaves.has_value()){
        return false;
    }

    auto symmetricalEdgesIt = json.find("symmetrical_edges");
    if (symmetricalEdgesIt == json.end()){
        noKeyError = {true, "symmetrical_edges"};
        return false;
    }
    if (!symmetricalEdgesIt->is_array()){
        badValueError = {true, "symmetrical_edges/[]", "array"};
        return false;
    }

    auto asymmetricalEdgesIt = json.find("asymmetrical_edges");
    if (asymmetricalEdgesIt == json.end()){
        noKeyError = {true, "asymmetrical_edges"};
        return false;
    }
    if (!asymmetricalEdgesIt->is_array()){
        badValueError = {true, "asymmetrical_edges/[]", "array"};
        return false;
    }

    std::optional<std::unordered_map<std::pair<Identifiable, Identifiable>, BasicConnection, AsymPairIDHash>> asymEdges = tryReadAsymEdges(*asymmetricalEdgesIt);
    
    if (!asymEdges){
        return false;
    }
    
    std::optional<std::unordered_map<std::pair<Identifiable, Identifiable>, SymConnection, PairIDHash>> symEdges = tryReadSymEdges(*symmetricalEdgesIt);
    
    if (!symEdges){
        return false;
    }
    
    std::unordered_map<Identifiable, std::vector<Identifiable>, IDHash> nodeNeighbours = getNodeNeighbours(symEdges.value());
    std::vector<std::pair<ResourceRadialNode<Resource>, std::vector<Identifiable>>> rawGraph;
    
    
    auto asymEdgesIt = json.find("asymmetrical_edges");
    if (asymEdgesIt == json.end()){
        noKeyError = {true, "asymmetrical_edges"};
        return false;
    }
    if (!asymEdgesIt->is_array()){
        badValueError = {true, "asymmetrical_edges/[]", "array"};
        return false;
    }

    
    
    for (const auto& node : *nodesIt){
        auto idIt = node.find("id");
        if (idIt == node.end()){
            noKeyError = {true, "nodes/[]/id"};
            return false;
        }
        if (!idIt->is_number_integer()){
            badValueError = {true, "nodes/[]/id", "int"};
            return false;
        }
        int id = idIt->get<int>();

        auto optionalRadialNode = tryReadNodeData(node, id);
        if (!optionalRadialNode){
            return false;
        }
        
        rawGraph.push_back({*optionalRadialNode, nodeNeighbours[id]});
    }



    mapTemplate = std::make_shared<MapTemplate>();
    mapTemplate->zoneGraph = std::make_shared<EdgeGraph<ResourceRadialNode<Resource>, SymConnection, BasicConnection>>(
        rawGraph,
        symEdges.value(),
        asymEdges.value()
    );
    mapTemplate->octaves = octaves.value();
    syssound::playInfo();
    return true;
}

std::optional<std::vector<NoiseOctaveParam>> TemplatePicker::tryReadOctaves(const nlohmann::json& octavesJson){

    auto octave1It = octavesJson.find("octave1");
    std::optional<NoiseOctaveParam> octave1 = std::nullopt;
    std::optional<NoiseOctaveParam> octave2 = std::nullopt;
    std::optional<NoiseOctaveParam> octave3 = std::nullopt;
    std::vector<NoiseOctaveParam> octaves;
    if (octave1It != octavesJson.end()){
        octave1 = tryReadOctave(*octave1It);
        if (octave1 == std::nullopt){
            return std::nullopt;
        }
        octaves.push_back(octave1.value());
        auto octave2It = octavesJson.find("octave2");
        if (octave2It != octavesJson.end()){
            octave2 = tryReadOctave(*octave2It);
            if (octave2 == std::nullopt){
                return std::nullopt;
            }   
            octaves.push_back(octave2.value());
            auto octave3It = octavesJson.find("octave3");
            if (octave3It != octavesJson.end()){
                octave3 = tryReadOctave(*octave3It);
                if (octave3 == std::nullopt){
                    return std::nullopt;
                }
                octaves.push_back(octave3.value());
            }
        }
    }
    return octaves;
}

std::optional<std::unordered_map<std::pair<Identifiable, Identifiable>, BasicConnection, AsymPairIDHash>> TemplatePicker::tryReadAsymEdges(const nlohmann::json& asymEdges){
    std::unordered_map<std::pair<Identifiable, Identifiable>, BasicConnection, AsymPairIDHash> result;
    for (const auto& asymEdgePair : asymEdges){
        auto ID_fromIt = asymEdgePair.find("ID_from");
        if (ID_fromIt == asymEdgePair.end()){
            noKeyError = {true, "asymmetrical_edges/[]/ID_from"};
            return std::nullopt;
        }
        if (!ID_fromIt->is_number_integer()){
            badValueError = {true, "asymmetrical_edges/[]/ID_from", "int"};
            return std::nullopt;
        }
        int ID_from = ID_fromIt->get<int>();

        auto ID_toIt = asymEdgePair.find("ID_to");
        if (ID_toIt == asymEdgePair.end()){
            noKeyError = {true, "asymmetrical_edges/[]/ID_to"};
            return std::nullopt;
        }
        if (!ID_toIt->is_number_integer()){
            badValueError = {true, "asymmetrical_edges/[]/ID_to", "int"};
            return std::nullopt;
        }
        int ID_to = ID_toIt->get<int>();



        auto dataIt = asymEdgePair.find("data");
        if (dataIt == asymEdgePair.end()){
            noKeyError = {true, "asymmetrical_edges/[]/data"};
            return std::nullopt;
        }
        
            auto resourceBlendDistanceIt = dataIt->find("resourceBlendDistance");
            if (resourceBlendDistanceIt == dataIt->end()){
                noKeyError = {true, "asymmetrical_edges/resourceBlendDistance"};
                return std::nullopt;
            }
            int resourceBlendDistance = resourceBlendDistanceIt->get<int>();

            auto intakeDistanceIt = dataIt->find("intakeDistance");
            if (intakeDistanceIt == dataIt->end()){
                noKeyError = {true, "asymmetrical_edges/intakeDistance"};
                return std::nullopt;
            }
            int intakeDistance = intakeDistanceIt->get<int>();

            auto areaGuaranteedIt = dataIt->find("areaGuaranteed");
            if (areaGuaranteedIt == dataIt->end()){
                noKeyError = {true, "asymmetrical_edges/areaGuaranteed"};
                return std::nullopt;
            }
            int areaGuaranteed = areaGuaranteedIt->get<int>();

            auto bonusValueIt = dataIt->find("bonusValue");
            if (bonusValueIt == dataIt->end()){
                noKeyError = {true, "asymmetrical_edges/bonusValue"};
                return std::nullopt;
            }
            double bonusValue = bonusValueIt->get<double>();
        result[std::make_pair(ID_from, ID_to)] = BasicConnection(resourceBlendDistance, intakeDistance, areaGuaranteed, bonusValue);
    }
    return result;
}


std::optional<std::unordered_map<std::pair<Identifiable, Identifiable>, SymConnection, PairIDHash>> TemplatePicker::tryReadSymEdges(const nlohmann::json& symEdges){
    std::unordered_map<std::pair<Identifiable, Identifiable>, SymConnection, PairIDHash> result;
    for (const auto& symEdgePair : symEdges){
        auto ID_firstIt = symEdgePair.find("ID_first");
        if (ID_firstIt == symEdgePair.end()){
            noKeyError = {true, "symmetrical_edges/[]/ID_first"};
            return std::nullopt;
        }
        if (!ID_firstIt->is_number_integer()){
            badValueError = {true, "symmetrical_edges/[]/ID_first", "int"};
            return std::nullopt;
        }
        int ID_first = ID_firstIt->get<int>();

        auto ID_secondIt = symEdgePair.find("ID_second");
        if (ID_secondIt == symEdgePair.end()){
            noKeyError = {true, "symmetrical_edges/[]/ID_second"};
            return std::nullopt;
        }
        if (!ID_secondIt->is_number_integer()){
            badValueError = {true, "symmetrical_edges/[]/ID_second", "int"};
            return std::nullopt;
        }
        int ID_second = ID_secondIt->get<int>();



        auto dataIt = symEdgePair.find("data");
        if (dataIt == symEdgePair.end()){
            noKeyError = {true, "symmetrical_edges/[]/data"};
            return std::nullopt;
        }

            auto passesIt = dataIt->find("passes");
            if (passesIt == dataIt->end()){
                noKeyError = {true, "symmetrical_edges/[]/data/passes"};
                return std::nullopt;
            }

                auto minPassWidthIt = passesIt->find("minPassWidth");
                if (minPassWidthIt == passesIt->end()){
                    noKeyError = {true, "symmetrical_edges/[]/data/passes/minPassWidth"};
                    return std::nullopt;
                }
                if (!minPassWidthIt->is_number_integer()){
                    badValueError = {true, "symmetrical_edges/[]/data/passes/minPassWidth", "int"};
                    return std::nullopt;
                }

                auto maxPassWidthIt = passesIt->find("maxPassWidth");
                if (maxPassWidthIt == passesIt->end()){
                    noKeyError = {true, "symmetrical_edges/[]/data/passes/maxPassWidth"};
                    return std::nullopt;
                }
                if (!maxPassWidthIt->is_number_integer()){
                    badValueError = {true, "symmetrical_edges/[]/data/passes/maxPassWidth", "int"};
                    return std::nullopt;
                }

                auto minWallLengthIt = passesIt->find("minWallLength");
                if (minWallLengthIt == passesIt->end()){
                    noKeyError = {true, "symmetrical_edges/[]/data/passes/minWallLength"};
                    return std::nullopt;
                }
                if (!minWallLengthIt->is_number_integer()){
                    badValueError = {true, "symmetrical_edges/[]/data/passes/minWallLength", "int"};
                    return std::nullopt;
                }
                
                auto maxWallLengthIt = passesIt->find("maxWallLength");
                if (maxWallLengthIt == passesIt->end()){
                    noKeyError = {true, "symmetrical_edges/[]/data/passes/maxWallLength"};
                    return std::nullopt;
                }
                if (!maxWallLengthIt->is_number_integer()){
                    badValueError = {true, "symmetrical_edges/[]/data/passes/maxWallLength", "int"};
                    return std::nullopt;
                }
        
        result[std::make_pair(ID_first, ID_second)] = SymConnection(tiles::PassParams{
            .minPassWidth = minPassWidthIt->get<size_t>(),
            .maxPassWidth = maxPassWidthIt->get<size_t>(),
            .minWallLength = minWallLengthIt->get<size_t>(),
            .maxWallLength = maxWallLengthIt->get<size_t>()
        });
    }
    
    return result;
}

std::unordered_map<Identifiable, std::vector<Identifiable>, IDHash> TemplatePicker::getNodeNeighbours(std::unordered_map<std::pair<Identifiable, Identifiable>, SymConnection, PairIDHash>& symEdges){
    std::unordered_map<Identifiable, std::vector<Identifiable>, IDHash> result;
    for (auto& [ids, _] : symEdges){
        result[ids.first].push_back(ids.second);
        result[ids.second].push_back(ids.first);
    }
    return result;
}


std::optional<ResourceRadialNode<Resource>> TemplatePicker::tryReadNodeData(const nlohmann::json& node, int id){
    auto dataIt = node.find("data");
    if (dataIt == node.end()){
        noKeyError = {true, "nodes/[]/data"};
        return std::nullopt;
    }

    auto radiusIt = dataIt->find("radius");
    if (radiusIt == dataIt->end()){
        noKeyError = {true, "nodes/[]/data/radius"};
        return std::nullopt;
    }
    if (!radiusIt->is_number_float()){
        badValueError = {true, "nodes/[]/data/radius", "float"};
        return std::nullopt;
    }

    auto susceptibilityIt = dataIt->find("susceptibility");
    if (susceptibilityIt == dataIt->end()){
        noKeyError = {true, "nodes/[]/data/susceptibility"};
        return std::nullopt;
    }
    if (!susceptibilityIt->is_number_float()){
        badValueError = {true, "nodes/[]/data/susceptibility", "float"};
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
        std::vector<double>,
        std::vector<ResourceMapping<Resource>>
    >
> TemplatePicker::tryReadResourceData(const nlohmann::json& nodeJSON){
    auto resourcesIt = nodeJSON.find("resources");
    if (resourcesIt == nodeJSON.end()){
        noKeyError = {true, "nodes/[]/data/resources"};
        return std::nullopt;
    }

    double octave1_weight = 0.0;
    double octave2_weight = 0.0;
    double octave3_weight = 0.0;

    auto weight1It = resourcesIt->find("octave1_weight");
    if (weight1It != resourcesIt->end()){
        if (!weight1It->is_number_float()){
            badValueError = {true, "nodes/[]/data/resources/octave1_weight", "float"};
            return std::nullopt;
        }
        octave1_weight = weight1It->get<double>();
    }

    auto weight2It = resourcesIt->find("octave2_weight");
    if (weight2It != resourcesIt->end()){
        if (!weight2It->is_number_float()){
            badValueError = {true, "nodes/[]/data/resources/octave2_weight", "float"};
            return std::nullopt;
        }
        octave2_weight = weight2It->get<double>();
    }

    auto weight3It = resourcesIt->find("octave3_weight");
    if (weight3It != resourcesIt->end()){
        if (!weight3It->is_number_float()){
            badValueError = {true, "nodes/[]/data/resources/octave3_weight", "float"};
            return std::nullopt;
        }
        octave3_weight = weight3It->get<double>();
    }

    std::vector<double> octaveWeights;
    octaveWeights = {octave1_weight, octave2_weight, octave3_weight};

    auto contentIt = resourcesIt->find("content");
    if (contentIt == resourcesIt->end()){
        noKeyError = {true, "nodes/[]/data/resources/content"};
        return std::nullopt;
    }

    if (!contentIt->is_array()){
        badValueError = {true, "nodes/[]/data/resources/content/[]", "array"};
        return std::nullopt;
    }

    std::vector<ResourceMapping<Resource>> contents;
    for (const auto& node : *contentIt){
        std::optional<ResourceMapping<Resource>> content = tryReadContentData(node);
        if (content == std::nullopt){
            return std::nullopt;
        }
        contents.push_back(content.value());
    }
    
    return std::make_pair(octaveWeights, contents);
}

std::optional<NoiseOctaveParam> TemplatePicker::tryReadOctave(const nlohmann::json& octaveJSON){
    double minRadius;
    double maxRadius;

    auto minRadiusIt = octaveJSON.find("minRadius");
    if (minRadiusIt == octaveJSON.end()){
        noKeyError = {true, "octaves/octave{*}/minRadius"};
        return std::nullopt;
    }
    if (!minRadiusIt->is_number_float()){
        badValueError = {true, "octaves/octave{*}/minRadius", "float"};
        return std::nullopt;
    }


    auto maxRadiusIt = octaveJSON.find("maxRadius");
    if (maxRadiusIt == octaveJSON.end()){
        noKeyError = {true, "octaves/octave{*}/maxRadius"};
        return std::nullopt;
    }
    if (!maxRadiusIt->is_number_float()){
        badValueError = {true, "octaves/octave{*}/maxRadius", "float"};
        return std::nullopt;
    }

    return NoiseOctaveParam{
        .weight = 1.0,
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
        noKeyError = {true, "nodes/[]/data/resources/content/[]/name"};
        return std::nullopt;
    }
    if (!resourceNameIt->is_string()){
        badValueError = {true, "nodes/[]/data/resources/content/[]/name", "string"};
        return std::nullopt;
    }
    resource = resourceFromString(resourceNameIt->get<std::string>());
    if (resource == std::nullopt){
        badValueError = {true, "nodes/[]/data/resources/content/[]/name", std::format("Unknown resource: {}", resourceNameIt->get<std::string>())};
    }


    auto startThresholdIt = contentJSON.find("startThreshold");
    if (startThresholdIt == contentJSON.end()){
        noKeyError = {true, "nodes/[]/data/resources/octave{*}/startThreshold"};
        return std::nullopt;
    }
    if (!startThresholdIt->is_number_float()){
        badValueError = {true, "nodes/[]/data/resources/octave{*}/startThreshold", "float"};
        return std::nullopt;
    }


    auto endThresholdIt = contentJSON.find("endThreshold");
    if (endThresholdIt == contentJSON.end()){
        noKeyError = {true, "nodes/[]/data/resources/octave{*}/endThreshold"};
        return std::nullopt;
    }
    if (!endThresholdIt->is_number_float()){
        badValueError = {true, "nodes/[]/data/resources/octave{*}/endThreshold", "float"};
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

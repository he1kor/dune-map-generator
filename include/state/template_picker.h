#pragma once

#include <string>
#include <memory>

#include <nodes.h>
#include <edge_graph.h>
#include <connection.h>

#include <filesystem>

#include <nlohmann/json.hpp>
#include <resources.h>


struct JSON_SyntaxError{
    bool ocurred = false;
};
struct JSON_NoKeyError{
    bool ocurred = false;
    std::string missingKey;
};
struct JSON_BadValueError{
    bool ocurred = false;
    std::string targetKey;
    std::string expectedType;
};

class TemplatePicker{
public:
    TemplatePicker(){};

    JSON_SyntaxError syntaxError;
    JSON_NoKeyError noKeyError;
    JSON_BadValueError badValueError;
    bool fileOpenErrorOccurred = false;

    std::string templateName = "Select...";
    std::string configFileName = "test";
    std::optional<ResourceRadialNode<Resource>> tryReadNodeData(const nlohmann::json &node, int id);
    std::optional<std::pair<std::vector<NoiseOctaveParam>, std::vector<ResourceMapping<Resource>>>> tryReadResourceData(const nlohmann::json &nodeJSON);
    std::optional<NoiseOctaveParam> tryReadOctave(const nlohmann::json &octaveJSON);
    std::optional<ResourceMapping<Resource>> tryReadContentData(const nlohmann::json &contentJSON);
    bool tryReadEdgeGraph(const nlohmann::json &json);
    void tryReadTemplate(const std::filesystem::path& path);
    std::shared_ptr<EdgeGraph<ResourceRadialNode<Resource>, int, BasicConnection>> mapTemplate = nullptr;
private:
};
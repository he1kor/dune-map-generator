#pragma once

#include <string>
#include <memory>

#include <nodes.h>
#include <edge_graph.h>

#include <filesystem>

#include <nlohmann/json.hpp>

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
    std::optional<RadialNode> tryReadNodeData(const nlohmann::json &node, int id);
    bool tryReadEdgeGraph(const nlohmann::json &json);
    void tryReadTemplate(const std::filesystem::path& path);
    std::shared_ptr<EdgeGraph<RadialNode, Identifiable>> mapTemplate = nullptr;
private:
};
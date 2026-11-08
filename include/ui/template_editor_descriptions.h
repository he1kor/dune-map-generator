#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

// The bundled JSON supplies defaults. A JSON file beside the executable can
// override any subset of the descriptions without rebuilding the application.
class TemplateEditorDescriptions {
public:
    TemplateEditorDescriptions();
    bool load(const std::filesystem::path& path);
    const std::string& text(const std::string& key) const;
    const std::string& parameter(const std::string& label) const;
    std::string materialName(const std::string& key) const;
    std::string format(const std::string& key, const std::map<std::string, std::string>& values) const;
    const nlohmann::json& help() const { return data_.at("help"); }
    const std::string& error() const { return error_; }

private:
    nlohmann::json data_;
    std::string error_;
};

TemplateEditorDescriptions& uiDescriptions();

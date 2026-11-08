#include "template_editor_descriptions.h"
#include "template_editor_descriptions_data.h"

#include <fstream>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

using Json = nlohmann::json;

TemplateEditorDescriptions::TemplateEditorDescriptions() : data_(Json::parse(bundledEditorDescriptions)) {}

bool TemplateEditorDescriptions::load(const std::filesystem::path& path) {
    try {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Could not open UI descriptions: " + path.string());
        const auto overrides = Json::parse(input);
        if (!overrides.is_object()) throw std::runtime_error("UI descriptions must be a JSON object.");
        auto candidate = data_;
        candidate.merge_patch(overrides);
        for (const char* group : {"parameters", "text", "materials"}) {
            if (!candidate.at(group).is_object()) throw std::runtime_error("Description dictionaries must be objects.");
            // Required defaults may be overridden, but not removed.
            for (const auto& [key, value] : data_.at(group).items())
                if (!candidate.at(group).contains(key)) throw std::runtime_error("Missing description: " + key);
            for (const auto& [key, value] : candidate.at(group).items())
                if (!value.is_string()) throw std::runtime_error("Description must be a string: " + key);
        }
        if (!candidate.at("help").is_array()) throw std::runtime_error("Help must be an array of sections.");
        for (const auto& section : candidate.at("help")) {
            if (!section.at("title").is_string() || !section.at("paragraphs").is_array())
                throw std::runtime_error("Each help section needs a title and paragraphs.");
            for (const auto& paragraph : section.at("paragraphs"))
                if (!paragraph.is_string()) throw std::runtime_error("Help paragraphs must be strings.");
        }
        data_ = std::move(candidate);
        error_.clear();
        return true;
    } catch (const std::exception& error) {
        error_ = error.what();
        return false;
    }
}

const std::string& TemplateEditorDescriptions::text(const std::string& key) const {
    return data_.at("text").at(key).get_ref<const std::string&>();
}

const std::string& TemplateEditorDescriptions::parameter(const std::string& label) const {
    const auto& parameters = data_.at("parameters");
    return parameters.at(parameters.contains(label) ? label : "default").get_ref<const std::string&>();
}

std::string TemplateEditorDescriptions::materialName(const std::string& key) const {
    const auto& materials = data_.at("materials");
    return materials.contains(key) ? materials.at(key).get<std::string>() : key;
}

std::string TemplateEditorDescriptions::format(const std::string& key, const std::map<std::string, std::string>& values) const {
    // Replace named tokens in the template only. User text is never a printf
    // format string and tokens inside substituted values stay literal.
    const auto& source = text(key);
    std::string result;
    for (size_t at = 0; at < source.size();) {
        if (source[at] == '{') {
            const auto end = source.find('}', at + 1);
            if (end != std::string::npos) {
                const auto found = values.find(source.substr(at + 1, end - at - 1));
                if (found != values.end()) { result += found->second; at = end + 1; continue; }
            }
        }
        result += source[at++];
    }
    return result;
}

TemplateEditorDescriptions& uiDescriptions() {
    static TemplateEditorDescriptions descriptions = [] {
        TemplateEditorDescriptions result;
#ifdef _WIN32
        std::vector<wchar_t> executable(32768);
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        const auto path = length && length < executable.size()
            ? std::filesystem::path(std::wstring(executable.data(), length)).parent_path() / "template_editor_descriptions.json"
            : std::filesystem::path("template_editor_descriptions.json");
#else
        const auto path = std::filesystem::path("template_editor_descriptions.json");
#endif
        std::error_code ignored;
        if (std::filesystem::exists(path, ignored)) result.load(path);
        return result;
    }();
    return descriptions;
}

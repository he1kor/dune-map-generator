#pragma once

#include <nlohmann/json.hpp>
#include <string>

// Drafts may have isolated nodes, an empty graph, or a nonplanar graph. All
// field types, resource intervals, IDs and references are still checked.
std::string templateValidationError(const nlohmann::json& document, bool requireGeneratable = true);

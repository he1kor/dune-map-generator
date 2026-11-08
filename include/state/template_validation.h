#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <memory>
namespace planar { struct Topology; }

// Drafts may have isolated nodes, an empty graph, or a nonplanar graph. All
// field types, resource intervals, IDs and references are still checked.
std::string templateValidationError(const nlohmann::json& document, bool requireGeneratable = true);
std::string templateSymmetryWarning(const nlohmann::json& document, std::shared_ptr<const planar::Topology> topology);

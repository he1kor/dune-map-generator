#pragma once

#include <nlohmann/json.hpp>
#include <stdexcept>

enum class SymmetryMode { None, Faces, Embedding, Complete };

inline const char* symmetryKey(SymmetryMode mode) {
    switch (mode) {
        case SymmetryMode::Faces: return "faces";
        case SymmetryMode::Embedding: return "embedding";
        case SymmetryMode::Complete: return "complete";
        default: return "none";
    }
}

inline SymmetryMode templateSymmetryMode(const nlohmann::json& document) {
    const auto it=document.find("embedding");
    if(it==document.end())return SymmetryMode::None;
    if(!it->is_object())throw std::invalid_argument("embedding: expected an object");
    if(it->contains("symmetrical")&&!(*it)["symmetrical"].is_boolean())
        throw std::invalid_argument("embedding/symmetrical: expected a boolean");
    if(const auto mode=it->find("symmetry");mode!=it->end()) {
        if(mode->is_string())for(auto value:{SymmetryMode::None,SymmetryMode::Faces,SymmetryMode::Embedding,SymmetryMode::Complete})
            if(*mode==symmetryKey(value))return value;
        throw std::invalid_argument("embedding/symmetry: expected none, faces, embedding or complete");
    }
    // Read templates saved by the earlier checkbox without changing their mode.
    return it->value("symmetrical",false)?SymmetryMode::Embedding:SymmetryMode::None;
}

#pragma once
#include "embedding.h"
#include <memory>

#include "grid.h"
#include <zone_bloater.h>
#include "templates.h"

enum class GenerationStage{
    NONE,
    EMBED,
    ZONE_BLOAT
};

class Generation{
    public:
        Generation();
        void generate();
        void runIteration();
        float generationProgress = 0.0f;
        long long seed = 12419;
        bool is_seed_random = true;
        
        // EmbeddablePlane<Identifiable> plane;
        EmbeddablePlane<MagneticNode> plane;
        
        // std::shared_ptr<const EdgeGraph<Identifiable, Identifiable>> defaultGraph = habbanyaErgSymmetry;
        std::shared_ptr<const EdgeGraph<MagneticNode, Identifiable>> defaultGraph = habbanyaErgSymmetry;
            
        // std::shared_ptr<Grid<Identifiable>> grid = nullptr;
        std::shared_ptr<Grid<MagneticNode>> grid = nullptr;
        
        // ZoneBloater<Identifiable, Identifiable> zoneBloater;
        ZoneBloater<MagneticNode, Identifiable> zoneBloater;
        
        GenerationStage generationStage = GenerationStage::NONE;
};
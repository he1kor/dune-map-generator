#pragma once
#include "embedding.h"
#include <memory>

#include "grid.h"
#include <zone_bloater.h>
#include "templates.h"

enum class GenerationStage{
    NONE,
    EMBED,
    ZONE_BLOAT,
    FINISH
};

class Generation{
    public:
        std::shared_ptr<Grid<BasicNode>> generateMap();
        Generation();
        void generate();
        void runIteration();
        float generationProgress = 0.0f;
        long long seed = 12419;
        bool is_seed_random = true;
        bool is_done = false;
        
        // EmbeddablePlane<Identifiable> plane;
        EmbeddablePlane<BasicNode> plane;
        
        // std::shared_ptr<const EdgeGraph<Identifiable, Identifiable>> defaultGraph = habbanyaErgSymmetry;
        std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> defaultGraph = templates::grid3x3;
            
        // std::shared_ptr<Grid<Identifiable>> grid = nullptr;
        std::shared_ptr<Grid<BasicNode>> grid = nullptr;
        GenerationStage generationStage = GenerationStage::NONE;
    private:
        // ZoneBloater<Identifiable, Identifiable> zoneBloater;
        ZoneBloater<BasicNode, Identifiable> zoneBloater;
        
};
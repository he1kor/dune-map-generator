#pragma once
#include "embedding.h"
#include <memory>

#include "grid.h"
#include <zone_bloater.h>
#include "templates.h"

#include "border.h"
#include <matrix.h>
#include <connection.h>
#include <resources.h>

enum class GenerationStage{
    NONE,
    EMBED,
    ZONE_BLOAT,
    POST_PROCESS,
    FINISH
};

class Generation{
    public:
        std::shared_ptr<Grid<ResourceRadialNode<Resource>>> generateMap();
        Generation();
        void generate(std::shared_ptr<const EdgeGraph<ResourceRadialNode<Resource>, SymConnection, BasicConnection>> mapTemplate);
        void runIteration();
        float generationProgress = 0.0f;
        long long seed = 12419;
        bool isSeedRandom = true;
        bool is_done = false;
        
        // EmbeddablePlane<Identifiable> plane;
        EmbeddablePlane<ResourceRadialNode<Resource>> plane;

        Matrix<double> noiseMap;
        Matrix<Resource> spiceMap;
        
        // std::shared_ptr<const EdgeGraph<Identifiable, Identifiable>> defaultGraph = habbanyaErgSymmetry;
        std::shared_ptr<const EdgeGraph<ResourceRadialNode<Resource>, SymConnection, BasicConnection>> mapTemplate;
            
        // std::shared_ptr<Grid<Identifiable>> grid = nullptr;
        std::shared_ptr<Grid<ResourceRadialNode<Resource>>> grid = nullptr;
        std::unordered_map<std::pair<Identifiable, Identifiable>, std::vector<tiles::Border>, PairIDHash> edgeToborderMap;
        GenerationStage generationStage = GenerationStage::NONE;
        std::unordered_map<Identifiable, Matrix<double>, IDHash> zoneMasks;
    private:
        // ZoneBloater<Identifiable, Identifiable> zoneBloater;
        ZoneBloater<ResourceRadialNode<Resource>, SymConnection, BasicConnection> zoneBloater;

        void deduceSeed();

        void embedTemplate();
        void bloatZones();
        void postProcess();
        
};
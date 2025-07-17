#pragma once
#include "embedding.h"
#include <memory>

#include "grid.h"
#include <zone_bloater.h>

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
        EmbeddablePlane<Identifiable> plane;
        bool iterateNext = false;
        //const Graph defaultGraph = Graph(
        //    {
        //        {1, 2, 3},  // 0
        //        {0, 2, 4},   // 1
        //        {0, 1, 3},   // 2
        //        {0, 2, 4},   // 3
        //        {1, 3}       // 4
        //    });
        std::shared_ptr<const EdgeGraph<Identifiable, Identifiable>> defaultGraph = std::make_shared<const EdgeGraph<Identifiable, Identifiable>>(
            EdgeGraph<Identifiable, Identifiable>(std::vector<std::pair<Identifiable, std::vector<Identifiable>>>
            {
                {0, {1, 2, 3, 4}},       // 0
                {1, {0, 2, 5, 6}},       // 1
                {2, {0, 1, 3, 6, 7}},    // 2
                {3, {0, 2, 4, 7, 8}},    // 3
                {4, {0, 3, 8, 9}},       // 4
                {5, {1, 6, 10}},        // 5
                {6, {1, 2, 5, 7, 10}},  // 6
                {7, {2, 3, 6, 8, 10}},  // 7
                {8, {3, 4, 7, 9, 10}},  // 8
                {9, {4, 8, 10, 11}},    // 9
                {10, {5, 6, 7, 8, 9, 11, 12, 13}}, // 10
                {11, {9, 10, 13, 14}},   // 11
                {12, {10, 13, 15}},      // 12
                {13, {10, 11, 12, 14, 15}}, // 13
                {14, {11, 13, 15}},      // 14
                {15, {12, 13, 14}}       // 15
            }
        ));
        std::shared_ptr<Grid<Identifiable>> grid = nullptr;
        ZoneBloater<Identifiable, Identifiable> zoneBloater;
        GenerationStage generationStage = GenerationStage::NONE;
};
#pragma once
#include "embedding.h"

class Generation{
    public:
        Generation();
        void generate();
        void runIteration();
        float generationProgress = 0.0f;
        long long seed = 12419;
        bool is_seed_random = true;
        EmbeddablePlane plane;
        bool iterateNext = false;
        const Graph defaultGraph = Graph(
            {
                {1, 2, 3},  // 0
                {0, 2, 4},   // 1
                {0, 1, 3},   // 2
                {0, 2, 4},   // 3
                {1, 3}       // 4
            });
};
#include "generation.h"

#include <rasterization.h>
#include <templates.h>
#include <embedding.h>
#include <grid.h>
#include <zone_bloater.h>

#include <morphology.h>

std::shared_ptr<Grid<RadialNode>> Generation::generateMap() {
    // --- Phase 1: Graph Embedding ---
    EmbeddablePlane<RadialNode> embedding(128, 128);
    embedding.initEmbed(templates::radial::grid3x3);  // Predefined 3x3 grid

    while (embedding.stepForceDirected()) {  // Runs until layout stabilizes
        #if 0  // Debug: Uncomment to log positions
        for (const auto& id : embedding.getIDs()) { ... }
        #endif
    }

    // --- Phase 2: Rasterization ---
    auto map = std::make_shared<Grid<RadialNode>>(safeRasterizePlane(embedding));

    // --- Phase 3: Zone Expansion ---
    ZoneBloater<RadialNode> zoneBloater;
    zoneBloater.initVoronoi(templates::radial::grid3x3, map);
    zoneBloater.start();
    while (zoneBloater.step()) {  // Progressively expands zones
        #if 0  // Debug: Uncomment to inspect tiles
        std::cout << "Progress: " << zoneBloater.getProgress() << "%\n";
        #endif
    }

    return map;  // Final generated map
}

Generation::Generation() : plane(128.0, 128.0) {};
void Generation::generate(){
    //this->grid = generateMap();
    //return;
    plane.clear();
    plane.applyMagnetGrid(cornerMagnets);
    plane.initEmbed(defaultGraph);
    generationStage = GenerationStage::EMBED;
    zoneBloater.finishAndReset();
}

void Generation::runIteration(){
    switch (generationStage){
        case GenerationStage::NONE:
            break;
        case GenerationStage::EMBED:
            if (!plane.stepForceDirected()){
                grid = std::make_shared<Grid<RadialNode>>(safeRasterizePlane(plane));
                generationStage = GenerationStage::ZONE_BLOAT;
            }
            break;

        case GenerationStage::ZONE_BLOAT:
            if (zoneBloater.isInitialized()){
                zoneBloater.initVoronoi(defaultGraph, grid);
                zoneBloater.setStartFromEdges(true);
                zoneBloater.setBloatMode(bloatStrategies::DiagonalRandomBloatStrategy());
                zoneBloater.start();
            }
            else if (zoneBloater.isRunning()){
                zoneBloater.step();
            }
            else {
                generationStage = GenerationStage::FINISH;
                zoneBloater.reset();
                
                morphology::closeForAll(*grid, defaultGraph->getIDs(),
                    {
                        {true, true, true},
                        {true, true, true},
                        {true, true, true}
                    }
                );
                morphology::openForAll(*grid, defaultGraph->getIDs(),
                    {
                        {true, true, true},
                        {true, true, true},
                        {true, true, true}
                    }
                );
            }
            break;
        case GenerationStage::FINISH:
            generationStage = GenerationStage::NONE;
            is_done = true;
            break;
        default:
            break;
    }
}

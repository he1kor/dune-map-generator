#include "generation.h"
#include <iostream>
#include "rasterization.h"

Generation::Generation() : plane(128.0, 128.0){};
void Generation::generate(){
    plane.clear();
    std::cout << "init\n";
    plane.initEmbed(defaultGraph);
    iterateNext = true;
    generationStage = GenerationStage::EMBED;
}

void Generation::runIteration(){
    switch (generationStage){
        case GenerationStage::EMBED:
            if (!plane.stepForceDirected()){
                grid = std::make_shared<Grid<Identifiable>>(safeRasterizePlane(plane));
                generationStage = GenerationStage::ZONE_BLOAT;
            }
            break;

        case GenerationStage::ZONE_BLOAT:
            if (zoneBloater.isInitialized()){
                zoneBloater.initVoronoi(defaultGraph, grid);
                zoneBloater.start();
            }
            else if (zoneBloater.isRunning()){
                zoneBloater.step();
            }
            else {
                generationStage = GenerationStage::NONE;
                zoneBloater.reset();
            }
            break;
        default:
            break;
    }
}

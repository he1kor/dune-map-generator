#include "generation.h"

#include <rasterization.h>
#include <templates.h>
#include <embedding.h>
#include <grid.h>
#include <zone_bloater.h>

std::shared_ptr<Grid<BasicNode>> Generation::generateMap(){
    EmbeddablePlane<BasicNode> embedding(128, 128); //embedding with 128x128 map size

    embedding.initEmbed(templates::grid3x3); //an example template from templates.h header. Define custom ones with graph.h

    while (embedding.stepForceDirected()){
        // for (Identifiable id : embedding.getIDs()){
        //     const auto& spot = embedding.getSpot(id);
        //     std::cout << spot.getX() << "\t" << spot.getY() << "\n";
        // }
        //debug the embedding process
    }
    //...embedding is done...

    auto map = std::make_shared<Grid<BasicNode>>(safeRasterizePlane(embedding)); //get Grid map from embedding
    
    ZoneBloater<BasicNode> zoneBloater;
    zoneBloater.initVoronoi(templates::grid3x3, map);
    zoneBloater.start();
    while (zoneBloater.isRunning()){
        zoneBloater.step();
        //debug the zone bloat process
        //grid->getTile(x, y);
    }
    //...generation is done...
    return map;
}

Generation::Generation() : plane(128.0, 128.0) {};
void Generation::generate(){
    this->grid = generateMap();
    is_done = true;
    return;
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
                //grid = std::make_shared<Grid<MagneticNode>>(safeRasterizePlane(plane));
                // grid = std::make_shared<Grid<Identifiable>>(safeRasterizePlane(plane));
                generationStage = GenerationStage::ZONE_BLOAT;
            }
            break;

        case GenerationStage::ZONE_BLOAT:
            if (zoneBloater.isInitialized()){
                //zoneBloater.initVoronoi(defaultGraph, grid);
                zoneBloater.setMode(BloatMode::RANDOM_DIAGONAL);
                zoneBloater.start();
            }
            else if (zoneBloater.isRunning()){
                zoneBloater.step();
            }
            else {
                generationStage = GenerationStage::FINISH;
                zoneBloater.reset();
            }
            break;
        case GenerationStage::FINISH:
            generationStage = GenerationStage::NONE;
            break;
        default:
            break;
    }
}

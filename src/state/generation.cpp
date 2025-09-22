#include "generation.h"

#include <rasterization.h>
#include <templates.h>
#include <embedding.h>
#include <grid.h>
#include <zone_bloater.h>

#include <morphology.h>
#include <border.h>

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
    zoneBloater.initVoronoi(map);
    zoneBloater.start();
    while (zoneBloater.step()) {  // Progressively expands zones
        #if 0  // Debug: Uncomment to inspect tiles
        std::cout << "Progress: " << zoneBloater.getProgress() << "%\n";
        #endif
    }

    return map;  // Final generated map
}

Generation::Generation() : plane(128.0, 128.0) {};
void Generation::generate(std::shared_ptr<const EdgeGraph<RadialNode>> mapTemplate){
    //this->grid = generateMap();
    //return;
    //RandomGenerator::instance().reset();
    this->mapTemplate = mapTemplate;

    deduceSeed();
    plane.clear();
    plane.applyMagnetGrid(cornerMagnets);
    plane.initEmbed(mapTemplate);
    generationStage = GenerationStage::EMBED;
    zoneBloater.finishAndReset();
}

void Generation::runIteration(){
    while (generationStage != GenerationStage::NONE)
    switch (generationStage){
        case GenerationStage::NONE:
            break;
        case GenerationStage::EMBED:
            embedTemplate();
            break;
        case GenerationStage::ZONE_BLOAT:
            bloatZones();
            break;
        case GenerationStage::POST_PROCESS:
            postProcess();
            break;
        case GenerationStage::FINISH:
            is_done = true;
            generationStage = GenerationStage::NONE;
            break;
        default:
            break;
    }
}

void Generation::deduceSeed(){
    if (isSeedRandom){
        seed = RandomGenerator::instance().generateSeed();
    } else{
        RandomGenerator::instance().setSeed(seed);
    }
}

void Generation::embedTemplate(){
    if (!plane.stepForceDirected()){
        grid = std::make_shared<Grid<RadialNode>>(safeRasterizePlane(plane));
        generationStage = GenerationStage::ZONE_BLOAT;
    }
}

void Generation::bloatZones(){
    if (zoneBloater.isInitialized()){
        zoneBloater.initEdgeVoronoi(*mapTemplate, grid);
        zoneBloater.setBloatMode(bloatStrategies::DiagonalRandomBloat());
        zoneBloater.start();
    }
    else if (zoneBloater.isRunning()){
        zoneBloater.step();
    }
    else {
        zoneBloater.reset();
        generationStage = GenerationStage::POST_PROCESS;
    }
}

void Generation::postProcess(){
    if (zoneBloater.isInitialized()){
        morphology::closeForAll(*grid, mapTemplate->getIDs(),
            {
                {1, 1, 1},
                {1, 1, 1},
                {1, 1, 1}
            }
        );
        morphology::openForAll(*grid, mapTemplate->getIDs(),
            {
                {1, 1, 1},
                {1, 1, 1},
                {1, 1, 1}
            }
        );
        zoneBloater.initAdjacentCornerFill(grid);
        zoneBloater.start();
    }
    else if (zoneBloater.isRunning()){
        zoneBloater.step();
    }
    else {
        zoneBloater.reset();
        edgeToborderMap = tiles::Border::getAllBorders<RadialNode>(*grid);
        
        for (auto& [pair, border] : edgeToborderMap){
            for (auto sameBorders : border){
                // sameBorders.initPassData(tiles::PassParams{.minPassWidth = 4, .maxPassWidth = 8, .minWallLength = 2, .maxWallLength = 10});
                sameBorders.initPassData(tiles::PassParams{.minPassWidth = 12, .maxPassWidth = 18, .minWallLength = 4, .maxWallLength = 10});
                sameBorders.generatePasses();
                for (const tiles::Border::Segment& segment : sameBorders.getSegments()){
                    if (segment.isDisabled()) continue;
                    grid->setTile(segment.getLeftPos(), Identifiable::nullID);
                    grid->setTile(segment.getRightPos(), Identifiable::nullID);
                }
            }
        }
        generationStage = GenerationStage::FINISH;
    }
}

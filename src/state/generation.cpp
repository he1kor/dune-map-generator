#include "generation.h"

#include <rasterization.h>
#include <templates.h>
#include <embedding.h>
#include <grid.h>
#include <zone_bloater.h>

#include <noise.h>

#include <morphology.h>
#include <border.h>
#include <blender.h>
#include <resource_generator.h>
#include <multizone_resource_generator.h>
/*
std::shared_ptr<Grid<ResourceRadialNode<Resource>>> Generation::generateMap() {
    // --- Phase 1: Graph Embedding ---
    EmbeddablePlane<ResourceRadialNode<Resource>> embedding(128, 128);
    embedding.initEmbed(templates::radial::grid3x3);  // Predefined 3x3 grid

    while (embedding.stepForceDirected()) {  // Runs until layout stabilizes
        #if 0  // Debug: Uncomment to log positions
        for (const auto& id : embedding.getIDs()) { ... }
        #endif
    }

    // --- Phase 2: Rasterization ---
    auto map = std::make_shared<Grid<ResourceRadialNode<Resource>>>(safeRasterizePlane(embedding));

    // --- Phase 3: Zone Expansion ---
    ZoneBloater<ResourceRadialNode<Resource>> zoneBloater;
    zoneBloater.initVoronoi(map);
    zoneBloater.start();
    while (zoneBloater.step()) {  // Progressively expands zones
        #if 0  // Debug: Uncomment to inspect tiles
        std::cout << "Progress: " << zoneBloater.getProgress() << "%\n";
        #endif
    }

    return map;  // Final generated map
}
    */

Generation::Generation() : plane(128.0, 128.0), noiseMap(128, 128), spiceMap(128, 128) {};
void Generation::generate(std::shared_ptr<const EdgeGraph<ResourceRadialNode<Resource>, SymConnection, BasicConnection>> mapTemplate){
    //this->grid = generateMap();
    //return;
    //RandomGenerator::instance().reset();
    this->mapTemplate = mapTemplate;
    // auto noise1 = EllipticalBlobNoise(128, 128, 2, 3); 
    // auto noise2 = EllipticalBlobNoise(128, 128, 4.5, 5.5);

    // Matrix<double> noiseMap1 = noise1.generate();
    // Matrix<double> noiseMap2 = noise2.generate();

    // std::vector<Matrix<double>> maps = {noiseMap1, noiseMap2};
    // noiseMap = Matrix<double>::normalizedAverage(maps, std::vector<double>{4, 3});
    // spiceMap = Matrix<double>::mapToBinary(noiseMap, [](double h) { return h >= 0.66;});

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
        grid = std::make_shared<Grid<ResourceRadialNode<Resource>>>(safeRasterizePlane(plane));
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
        edgeToborderMap = tiles::Border::getAllBorders<ResourceRadialNode<Resource>>(*grid);
        
        for (auto& [pair, border] : edgeToborderMap){
            auto passParams = mapTemplate->tryGetSymEdge(pair.first, pair.second);
            if (!passParams.has_value()){
                passParams = tiles::PassParams{
                    .minPassWidth = 0,
                    .maxPassWidth = 0,
                    .minWallLength = 0,
                    .maxWallLength = 200
                }; // No pass if no connection
            }
            for (auto sameBorders : border){
                //seed 801119385
                sameBorders.initPassData(passParams.value());
                sameBorders.generatePasses();
                for (const tiles::Border::Segment& segment : sameBorders.getSegments()){
                    if (segment.isDisabled()) continue;
                    grid->setTile(segment.getLeftPos(), Identifiable::nullID);
                    grid->setTile(segment.getRightPos(), Identifiable::nullID);
                }
            }
        }
        zoneMasks = blendConnections(*grid, *mapTemplate);

        std::unordered_map<Identifiable, ResourceGenerator<Resource>, IDHash> resourceGenerators;
        resourceGenerators.reserve(mapTemplate->size());
        for (Identifiable zoneID : mapTemplate->getIDs()){
            ResourceGenerator<Resource> resourceGenerator;
            resourceGenerator.setup(
                mapTemplate->getValue(zoneID).octaves,
                mapTemplate->getValue(zoneID).resources,
                noiseMap.getDimension(),
                zoneMasks.at(zoneID)
            );
            resourceGenerators[zoneID] = resourceGenerator;
        }
        MultizoneResourceGenerator<Resource> multizoneGenerator;
        multizoneGenerator.setup(resourceGenerators, zoneMasks);
        spiceMap = multizoneGenerator.generateResourcesMap();
        generationStage = GenerationStage::FINISH;
    }
}

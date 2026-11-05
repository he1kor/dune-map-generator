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
void Generation::generate(std::shared_ptr<MapTemplate> mapTemplate, int width, int height){
    if (!mapTemplate || !mapTemplate->zoneGraph)
        throw std::invalid_argument("Select a template before generating a map");
    if (!MapDimensionLimits{}.allows({width, height}))
        throw std::invalid_argument("Map width and height must be even numbers from 32 to 128");
    if (!mapTemplate->dimensionLimits.allows({width, height}))
        throw std::invalid_argument("The selected map size does not satisfy the template limits");

    zoneBloater.finishAndReset();
    this->mapTemplate = mapTemplate;
    plane.setSize(width, height);
    noiseMap = Matrix<double>(width, height);
    spiceMap = Matrix<Resource>(width, height, Resource::NONE);
    wallMask = Matrix<bool>(width, height, false);
    grid.reset();
    edgeToborderMap.clear();
    zoneMasks = {};
    resourceGenerators.clear();
    is_done = false;
    deduceSeed();
    plane.clear();
    plane.applyMagnetGrid(cornerMagnets);
    plane.initEmbed(mapTemplate->zoneGraph);
    generationStage = GenerationStage::EMBED;
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
        zoneBloater.initEdgeVoronoi(*(mapTemplate->zoneGraph), grid);
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
        morphology::closeForAll(*grid, mapTemplate->zoneGraph->getIDs(), morphology::fullKernel3x3);
        morphology::openForAll(*grid, mapTemplate->zoneGraph->getIDs(), morphology::fullKernel3x3);
        zoneBloater.initAdjacentCornerFill(grid);
        zoneBloater.start();
    }
    else if (zoneBloater.isRunning()){
        zoneBloater.step();
    }
    else {
        zoneBloater.reset();
        edgeToborderMap = tiles::Border::getAllBorders<ResourceRadialNode<Resource>>(*grid);
        wallMask = Matrix<bool>(grid->getWidth(), grid->getHeight(), false);
        
        for (auto& [pair, border] : edgeToborderMap){
            auto passParams = mapTemplate->zoneGraph->tryGetSymEdge(pair.first, pair.second);
            if (!passParams.has_value()){
                passParams = BasicSymConnection(
                    0,
                    tiles::PassParams{
                        .minPassWidth = 0,
                        .maxPassWidth = 0,
                        .minWallLength = 0,
                        .maxWallLength = 200
                    }
                );
            }; // No pass if no connection
            for (auto sameBorders : border){
                sameBorders.initPassData(passParams.value());
                sameBorders.generatePasses();
                for (const tiles::Border::Segment& segment : sameBorders.getSegments()){
                    if (segment.isDisabled()) continue;
                    grid->setTile(segment.getLeftPos(), Identifiable::nullID);
                    grid->setTile(segment.getRightPos(), Identifiable::nullID);
                    wallMask.set(segment.getLeftPos().x, segment.getLeftPos().y, true);
                    wallMask.set(segment.getRightPos().x, segment.getRightPos().y, true);
                }
            }
        }
        std::vector<Matrix<double>> resourceOctaves;
        resourceOctaves.reserve(mapTemplate->octaves.size());
        for (auto& octave : mapTemplate->octaves){
            resourceOctaves.push_back(EllipticalBlobNoise(noiseMap.getDimension().x, noiseMap.getDimension().y, octave.minimalBlob, octave.maximalBlob).generate());
        }
        zoneMasks = blendConnections(*grid, *(mapTemplate->zoneGraph));
        resourceGenerators = {};
        resourceGenerators.reserve(mapTemplate->zoneGraph->size());

        for (Identifiable zoneID : mapTemplate->zoneGraph->getIDs()){
            ResourceGenerator<Resource> resourceGenerator;
            resourceGenerator.setup(
                resourceOctaves,
                mapTemplate->zoneGraph->getValue(zoneID).octaveWeights,
                mapTemplate->zoneGraph->getValue(zoneID).resources,
                noiseMap.getDimension(),
                zoneMasks.influenceMask.at(zoneID),
                zoneMasks.bonusMask.at(zoneID)
            );
            resourceGenerators[zoneID] = resourceGenerator;
        }
        MultizoneResourceGenerator<Resource> multizoneGenerator;
        multizoneGenerator.setup(resourceGenerators, zoneMasks.influenceMask);
        spiceMap = multizoneGenerator.generateResourcesMap(Resource::NONE);
        for (int y = 0; y < spiceMap.getHeight(); ++y) {
            for (int x = 0; x < spiceMap.getWidth(); ++x) {
                if (wallMask.get(x, y)) spiceMap.set(x, y, mapTemplate->wallResource);
            }
        }

        auto spiceMask = spiceMap.mapToBinary([](Resource resource) { 
            return resource == Resource::BASIC_SPICE || resource == Resource::THICK_SPICE;
        });
        morphology::erode(spiceMask, morphology::fullKernel3x3);
        spiceMap.applyByMask(spiceMask, [](Resource& resource, bool isMasked){
            if (!isMasked && resource == Resource::THICK_SPICE){
                resource = Resource::BASIC_SPICE;
            }
        });
        generationStage = GenerationStage::FINISH;
    }
}

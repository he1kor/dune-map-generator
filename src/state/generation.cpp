#include "generation.h"

#include <rasterization.h>
#include <templates.h>
#include <embedding.h>
#include <planar_symmetry.h>
#include <grid.h>
#include <zone_bloater.h>

#include <noise.h>

#include <morphology.h>
#include <border.h>
#include <blender.h>
#include <resource_generator.h>
#include <multizone_resource_generator.h>
#include <chrono>
#include <cmath>
#include <map>

Generation::Generation() : plane(128.0, 128.0), noiseMap(128, 128), spiceMap(128, 128) {};

Generation::~Generation() {
    stopping = true;
    animationWake.notify_all();
    if (worker.valid()) worker.wait();
}

static void validateRequest(const std::shared_ptr<MapTemplate>& mapTemplate, int width, int height) {
    if (!mapTemplate || !mapTemplate->zoneGraph)
        throw std::invalid_argument("Select a template before generating a map");
    if (!MapDimensionLimits{}.allows({width, height}))
        throw std::invalid_argument("Map width and height must be even numbers from 32 to 128");
    if (!mapTemplate->dimensionLimits.allows({width, height}))
        throw std::invalid_argument("The selected map size does not satisfy the template limits");
}

void Generation::generate(std::shared_ptr<MapTemplate> mapTemplate, int width, int height){
    if (isRunning()) throw std::logic_error("Generation is already running");
    validateRequest(mapTemplate, width, height);

    zoneBloater.finishAndReset();
    zoneBloater.setPlacementConstraint({});
    this->mapTemplate = mapTemplate;
    plane.setSize(width, height);
    noiseMap = Matrix<double>(width, height);
    spiceMap = Matrix<Resource>(width, height, Resource::NONE);
    wallMask = Matrix<bool>(width, height, false);
    grid.reset();
    edgeToborderMap.clear();
    zoneMasks = {};
    resourceGenerators.clear();
    resourceOctaves.clear();
    mirroredZoneIDs.clear();
    canonicalZoneSide.clear();
    is_done = false;
    generationProgress = 0.0f;
    generationError.clear();
    deduceSeed();
    plane.clear();
    plane.applyMagnetGrid(cornerMagnets);
    EdgeGeometryMap preferences;
    for (const auto& [edge, connection] : mapTemplate->zoneGraph->getSymEdges())
        preferences.emplace(edge, connection.geometry);
    const auto positionMagnets = mapTemplate->magnetsForGeneration(static_cast<std::uint64_t>(seed), width, height);
    auto topology = mapTemplate->embeddingTopology;
    if(!topology&&!mapTemplate->nodeBoundary.empty())topology=planar::makeTopology(*mapTemplate->zoneGraph,mapTemplate->nodeBoundary);
    std::shared_ptr<const planar::Reflection> reflection;
    if (mapTemplate->symmetryMode!=SymmetryMode::None) {
        if (!topology) topology = planar::makeTopology(*mapTemplate->zoneGraph);
        reflection = mapTemplate->reflectionForGeneration(positionMagnets,width,height);
    }
    plane.initEmbed(mapTemplate->zoneGraph, preferences, static_cast<std::uint64_t>(seed), topology, positionMagnets, reflection,
        mapTemplate->symmetryMode!=SymmetryMode::Faces);
    if(mapTemplate->symmetryMode==SymmetryMode::Complete) {
        mirrorNormal=plane.symmetryAxisNormal().value();
        mirroredZoneIDs.emplace(Identifiable::nullID,Identifiable::nullID);
        for(size_t i=0;i<topology->ids.size();++i)mirroredZoneIDs.emplace(topology->ids[i],topology->ids[reflection->partner[i]]);
    }
    generationStage = GenerationStage::EMBED;
}

void Generation::runIteration(const std::function<bool(const Generation&)>& onProgress){
    if (isRunning()) throw std::logic_error("Generation is already running");
    while (generationStage != GenerationStage::NONE) {
        if (onProgress && !onProgress(*this)) return;
        switch (generationStage){
            case GenerationStage::NONE:
                break;
            case GenerationStage::EMBED:
                // Estimates within iterative phases approach their next milestone.
                generationProgress += (0.20f - generationProgress) * 0.025f;
                embedTemplate();
                break;
            case GenerationStage::ZONE_BLOAT:
                generationProgress += (0.45f - generationProgress) * 0.12f;
                bloatZones();
                break;
            case GenerationStage::POST_PROCESS:
                generationProgress += (0.60f - generationProgress) * 0.12f;
                postProcess();
                break;
            case GenerationStage::BORDERS:
                buildBorders();
                break;
            case GenerationStage::NOISE:
                generateNoise();
                break;
            case GenerationStage::BLEND:
                blendZones();
                break;
            case GenerationStage::RESOURCES:
                generateResources();
                break;
            case GenerationStage::FINISH:
                is_done = true;
                generationProgress = 1.0f;
                generationStage = GenerationStage::NONE;
                break;
            default:
                break;
        }
    }
}

bool Generation::startAsync(std::shared_ptr<MapTemplate> mapTemplate, int width, int height) {
    if (isRunning()) return false;
    validateRequest(mapTemplate, width, height);
    generationError.clear();
    is_done = false;
    stopping = false;
    generationStage = postedStage = GenerationStage::EMBED;
    generationProgress = postedProgress = 0.0f;
    postedSeed = seed;
    preview.reset();
    pendingPreview.reset();
    const auto requestedSeed = seed;
    const auto randomSeed = isSeedRandom;
    try {
        worker = std::async(std::launch::async, [this, mapTemplate, width, height, requestedSeed, randomSeed] {
            auto result = std::make_unique<Generation>();
            result->seed = requestedSeed;
            result->isSeedRandom = randomSeed;
            result->generate(mapTemplate, width, height);

            using Clock = std::chrono::steady_clock;
            constexpr auto interval = std::chrono::milliseconds(33);
            auto nextPost = Clock::now();
            auto nextFrame = Clock::now();
            GenerationStage lastStage = GenerationStage::NONE;
            unsigned int embedSteps = 0;
            result->runIteration([&](const Generation& state) {
                if (stopping) return false;
                const bool stageChanged = state.generationStage != lastStage;
                // Four layout steps per frame; one expansion/cleanup wave per frame.
                const bool drawFrame = state.generationStage == GenerationStage::EMBED
                    ? (++embedSteps % 4 == 1 || stageChanged)
                    : state.generationStage == GenerationStage::ZONE_BLOAT
                        || state.generationStage == GenerationStage::POST_PROCESS || stageChanged;
                std::unique_ptr<GenerationPreview> frame;
                if (animate && drawFrame) {
                    std::unique_lock lock(updateMutex);
                    animationWake.wait_until(lock, nextFrame, [this] { return stopping || !animate; });
                    lock.unlock();
                    if (stopping) return false;
                    if (animate) {
                        frame = state.makePreview();
                        nextFrame = Clock::now() + interval;
                    }
                }
                const auto now = Clock::now();
                if (stageChanged || frame || now >= nextPost) {
                    std::lock_guard lock(updateMutex);
                    postedStage = state.generationStage;
                    postedProgress = state.generationProgress;
                    postedSeed = state.seed;
                    // A single pending frame bounds memory even if rendering falls behind.
                    if (frame && animate) pendingPreview = std::move(frame);
                    nextPost = now + interval;
                }
                lastStage = state.generationStage;
                return true;
            });
            return result;
        });
    } catch (const std::exception& error) {
        generationStage = GenerationStage::NONE;
        generationError = error.what();
        return false;
    }
    return true;
}

void Generation::setAnimate(bool enabled) {
    animate = enabled;
    animationWake.notify_all();
    if (!enabled) {
        std::lock_guard lock(updateMutex);
        pendingPreview.reset();
        preview.reset();
    }
}

void Generation::poll() {
    if (!isRunning()) return;
    {
        std::lock_guard lock(updateMutex);
        generationStage = postedStage;
        generationProgress = postedProgress;
        seed = postedSeed;
        preview = std::move(pendingPreview);
    }
    if (worker.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto result = worker.get();
        if (result->is_done) acceptResult(*result);
    } catch (const std::exception& error) {
        generationError = error.what();
        generationStage = GenerationStage::NONE;
    } catch (...) {
        generationError = "Unknown generation error";
        generationStage = GenerationStage::NONE;
    }
    preview.reset();
    pendingPreview.reset();
}

void Generation::acceptResult(Generation& result) {
    // Transfer completed buffers only. The simulator's self pointers stay with its worker.
    // Keep the embedding graph with its spots so validation also works after an async run.
    plane = std::move(result.plane);
    noiseMap = std::move(result.noiseMap);
    spiceMap = std::move(result.spiceMap);
    wallMask = std::move(result.wallMask);
    mapTemplate = std::move(result.mapTemplate);
    grid = std::move(result.grid);
    edgeToborderMap = std::move(result.edgeToborderMap);
    zoneMasks = std::move(result.zoneMasks);
    resourceGenerators = std::move(result.resourceGenerators);
    mirrorNormal=result.mirrorNormal;
    mirroredZoneIDs=std::move(result.mirroredZoneIDs);
    canonicalZoneSide=std::move(result.canonicalZoneSide);
    seed = result.seed;
    generationStage = GenerationStage::NONE;
    generationProgress = 1.0f;
    is_done = true;
}

std::unique_ptr<GenerationPreview> Generation::makePreview() const {
    auto frame = std::make_unique<GenerationPreview>();
    frame->width = static_cast<int>(plane.getWidth());
    frame->height = static_cast<int>(plane.getHeight());
    for (auto id : plane.getIDs()) {
        frame->nodes.push_back(plane.getSpot(id).getCoords());
        frame->nodeIds.push_back(id.getID());
    }
    if (grid) {
        frame->zones.reserve(frame->width * frame->height);
        for (auto id : *grid) frame->zones.push_back(id.getID());
    } else {
        for (auto id : plane.getIDs()) {
            const auto start = plane.getSpot(id).getCoords();
            for (auto neighbor : mapTemplate->zoneGraph->getNode(id).getNeighbours()) {
                if (neighbor > id) frame->edges.emplace_back(start, plane.getSpot(neighbor).getCoords());
            }
        }
    }
    return frame;
}

const char* Generation::statusText() const {
    if (!generationError.empty()) return "Failed";
    switch (generationStage) {
        case GenerationStage::EMBED: return "Placement";
        case GenerationStage::ZONE_BLOAT: return "Zone growth";
        case GenerationStage::POST_PROCESS: return "Smoothing";
        case GenerationStage::BORDERS: return "Borders";
        case GenerationStage::NOISE: return "Noise";
        case GenerationStage::BLEND: return "Blending";
        case GenerationStage::RESOURCES: return "Resources";
        case GenerationStage::FINISH: return "Finishing";
        default: return generationProgress == 1.0f ? "Complete" : "Ready";
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
    if (!plane.stepPlanar()){
        grid = std::make_shared<Grid<ResourceRadialNode<Resource>>>(safeRasterizePlane(plane));
        mirrorCompleteTiles(true);
        if (mapTemplate->symmetryMode == SymmetryMode::Complete) {
            const auto axis = planar::tileAxisForNormal(mirrorNormal, grid->getWidth(), grid->getHeight()).value();
            for (auto id : plane.getIDs()) {
                if (mirroredZoneIDs.at(id) == id) continue;
                const auto p = plane.getSpot(id).getCoords();
                const IntVector2 tile{static_cast<int>(std::floor(p.x)), static_cast<int>(std::floor(p.y))};
                canonicalZoneSide.emplace(id, planar::canonicalTile(tile,
                    planar::reflectedTile(tile, axis, grid->getWidth(), grid->getHeight())));
            }
            zoneBloater.setPlacementConstraint([this](const ZoneTile& tile) {
                return canPlaceCompleteZone(tile.zoneID, tile);
            });
        }
        generationStage = GenerationStage::ZONE_BLOAT;
        generationProgress = 0.20f;
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
        mirrorCompleteTiles();
        generationStage = GenerationStage::POST_PROCESS;
        generationProgress = 0.45f;
    }
}

void Generation::postProcess(){
    if (zoneBloater.isInitialized()){
        // Morphological kernels may straddle the axis. Restore valid ownership
        // where a kernel tried to move a paired zone onto the opposite side.
        std::optional<Grid<ResourceRadialNode<Resource>>> beforeSmoothing;
        if (mapTemplate->symmetryMode == SymmetryMode::Complete) beforeSmoothing = *grid;
        morphology::closeForAll(*grid, mapTemplate->zoneGraph->getIDs(), morphology::fullKernel3x3);
        morphology::openForAll(*grid, mapTemplate->zoneGraph->getIDs(), morphology::fullKernel3x3);
        if (beforeSmoothing) {
            for (int y = 0; y < grid->getHeight(); ++y)
                for (int x = 0; x < grid->getWidth(); ++x)
                    if (!canPlaceCompleteZone(grid->getTileID(x, y), {x, y}))
                        grid->setTile(x, y, beforeSmoothing->getTileID(x, y));
        }
        zoneBloater.initAdjacentCornerFill(grid);
        zoneBloater.start();
    }
    else if (zoneBloater.isRunning()){
        zoneBloater.step();
    }
    else {
        zoneBloater.reset();
        mirrorCompleteTiles();
        generationStage = GenerationStage::BORDERS;
        generationProgress = 0.60f;
    }
}

void Generation::buildBorders() {
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
    mirrorCompleteTiles();
    generationStage = GenerationStage::NOISE;
    generationProgress = 0.68f;
}

void Generation::generateNoise() {
    resourceOctaves.reserve(mapTemplate->octaves.size());
    for (auto& octave : mapTemplate->octaves){
        resourceOctaves.push_back(EllipticalBlobNoise(noiseMap.getDimension().x, noiseMap.getDimension().y, octave.minimalBlob, octave.maximalBlob).generate());
    }
    mirrorCompleteTiles();
    generationStage = GenerationStage::BLEND;
    generationProgress = 0.78f;
}

void Generation::blendZones() {
    zoneMasks = blendConnections(*grid, *(mapTemplate->zoneGraph));
    mirrorCompleteTiles();
    generationStage = GenerationStage::RESOURCES;
    generationProgress = 0.84f;
}

void Generation::generateResources() {
    resourceGenerators = {};
    resourceGenerators.reserve(mapTemplate->zoneGraph->size());

    // A reflected pair has identical resource settings. Normalize it as one
    // domain, including diagonal cells with a single ownership label. Splitting
    // those cells into half-weight masks biases percentile ranks toward zero.
    auto resourceMasks = zoneMasks.influenceMask;
    auto resourceBonuses = zoneMasks.bonusMask;
    if (mapTemplate->symmetryMode == SymmetryMode::Complete) {
        for (const auto& [id, partner] : mirroredZoneIDs) {
            if (id >= partner) continue;
            auto& mask = resourceMasks.at(id);
            auto& bonus = resourceBonuses.at(id);
            const auto& otherMask = resourceMasks.at(partner);
            const auto& otherBonus = resourceBonuses.at(partner);
            for (int y = 0; y < mask.getHeight(); ++y) for (int x = 0; x < mask.getWidth(); ++x) {
                const double a = mask.get(x, y), b = otherMask.get(x, y), total = a + b;
                bonus.set(x, y, total > 0 ? (a * bonus.get(x, y) + b * otherBonus.get(x, y)) / total : 0);
                mask.set(x, y, total);
            }
            resourceMasks.erase(partner);
            resourceBonuses.erase(partner);
        }
    }
    for (Identifiable zoneID : mapTemplate->zoneGraph->getIDs()){
        if (!resourceMasks.contains(zoneID)) continue;
        ResourceGenerator<Resource> resourceGenerator;
        resourceGenerator.setup(
            resourceOctaves,
            mapTemplate->zoneGraph->getValue(zoneID).octaveWeights,
            mapTemplate->zoneGraph->getValue(zoneID).resources,
            noiseMap.getDimension(),
            resourceMasks.at(zoneID),
            resourceBonuses.at(zoneID)
        );
        resourceGenerators[zoneID] = resourceGenerator;
    }
    MultizoneResourceGenerator<Resource> multizoneGenerator;
    multizoneGenerator.setup(resourceGenerators, resourceMasks);
    spiceMap = multizoneGenerator.generateResourcesMap(Resource::NONE);
    if (mapTemplate->symmetryMode == SymmetryMode::Complete)
        for (const auto& [id, partner] : mirroredZoneIDs)
            if (id < partner) resourceGenerators[partner] = resourceGenerators.at(id);
    for (int y = 0; y < spiceMap.getHeight(); ++y) {
        for (int x = 0; x < spiceMap.getWidth(); ++x) {
            if (wallMask.get(x, y)) spiceMap.set(x, y, mapTemplate->wallResource);
        }
    }

    mirrorCompleteTiles();
    auto spiceMask = spiceMap.mapToBinary([](Resource resource) {
        return resource == Resource::BASIC_SPICE || resource == Resource::THICK_SPICE;
    });
    morphology::erode(spiceMask, morphology::fullKernel3x3);
    spiceMap.applyByMask(spiceMask, [](Resource& resource, bool isMasked){
        if (!isMasked && resource == Resource::THICK_SPICE){
            resource = Resource::BASIC_SPICE;
        }
    });
    resourceOctaves.clear();
    mirrorCompleteTiles();
    generationStage = GenerationStage::FINISH;
    generationProgress = 0.99f;
}

bool Generation::canPlaceCompleteZone(Identifiable id, IntVector2 point) const {
    const auto side = canonicalZoneSide.find(id);
    if (side == canonicalZoneSide.end()) return true; // Empty cells and fixed axis zones.
    const auto axis = planar::tileAxisForNormal(mirrorNormal, grid->getWidth(), grid->getHeight()).value();
    return planar::canonicalTile(point, planar::reflectedTile(point, axis, grid->getWidth(), grid->getHeight())) == side->second;
}

void Generation::mirrorCompleteTiles(bool seedNodes) {
    if(mapTemplate->symmetryMode!=SymmetryMode::Complete||!grid)return;
    const int width=grid->getWidth(),height=grid->getHeight();
    const auto axis=planar::tileAxisForNormal(mirrorNormal,width,height).value();
    auto mirrored=[&](IntVector2 p) { return planar::reflectedTile(p,axis,width,height); };
    auto canonical=[&](IntVector2 p) { return planar::canonicalTile(p,mirrored(p)); };
    if(seedNodes) {
        // Axis nodes land on one side of the central cell boundary when
        // rasterized. Preserve every node seed in the authoritative half before
        // copying it, including those fixed by the graph reflection.
        std::map<std::pair<int,int>,Identifiable> seeds;
        for(auto id:plane.getIDs()) {
            const auto position=plane.getSpot(id).getCoords();
            IntVector2 p{static_cast<int>(std::floor(position.x)),static_cast<int>(std::floor(position.y))};
            const auto q=mirrored(p);
            if(q.x==p.x&&q.y==p.y&&mirroredZoneIDs.at(id)!=id)
                throw std::runtime_error("Map resolution is too small to separate mirrored zone seeds on the diagonal. Choose a larger map or adjust the magnets.");
            if(!canonical(p)) { p=mirrored(p);id=mirroredZoneIDs.at(id); }
            auto [it,added]=seeds.emplace(std::make_pair(p.x,p.y),id);
            if(!added&&it->second!=id)throw std::runtime_error("Map resolution is too small for completely symmetric zone seeds. Choose a larger map or adjust the magnets.");
        }
        for(const auto& [p,id]:seeds)grid->setTile(p.first,p.second,id);
    }
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        const IntVector2 source{x,y};if(!canonical(source))continue;
        const auto destination=mirrored(source);
        if(destination.x==x&&destination.y==y) {
            // A diagonal tile reflects onto itself. Its single zone ID is an
            // ownership tie, so keep its real owner and configured blending.
            // Resource normalization combines paired domains later; forcing
            // 50/50 masks here would create an artificial low-resource seam.
            continue;
        }
        grid->setTile(destination,mirroredZoneIDs.at(grid->getTileID(source)));
        wallMask.set(destination.x,destination.y,wallMask.get(x,y));
        spiceMap.set(destination.x,destination.y,spiceMap.get(x,y));
        noiseMap.set(destination.x,destination.y,noiseMap.get(x,y));
        for(auto& octave:resourceOctaves)octave.set(destination.x,destination.y,octave.get(x,y));
        for(auto* masks:{&zoneMasks.influenceMask,&zoneMasks.bonusMask})
            for(const auto& [id,mask]:*masks)masks->at(mirroredZoneIDs.at(id)).set(destination.x,destination.y,mask.get(x,y));
    }
}

#pragma once
#include "embedding.h"
#include <memory>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <string>

#include "grid.h"
#include <zone_bloater.h>
#include "templates.h"

#include <border.h>
#include <matrix.h>
#include <blender.h>
#include <connection.h>
#include <resources.h>
#include "map_template.h"

enum class GenerationStage{
    NONE,
    EMBED,
    ZONE_BLOAT,
    POST_PROCESS,
    BORDERS,
    NOISE,
    BLEND,
    RESOURCES,
    FINISH
};

// Only the data needed to draw a preview crosses the worker/UI boundary.
struct GenerationPreview {
    int width = 0;
    int height = 0;
    std::vector<DoubleVector2> nodes;
    std::vector<int> nodeIds; // Same order as nodes, also present in zone previews.
    std::vector<std::pair<DoubleVector2, DoubleVector2>> edges;
    std::vector<int> zones;
};

class Generation{
    public:
        Generation();
        ~Generation();
        void generate(std::shared_ptr<MapTemplate> mapTemplate, int width = 128, int height = 128);
        // Synchronous entry point, also used by the background worker.
        void runIteration(const std::function<bool(const Generation&)>& onProgress = {});
        bool startAsync(std::shared_ptr<MapTemplate> mapTemplate, int width = 128, int height = 128);
        void poll();
        bool isRunning() const { return worker.valid(); }
        void setAnimate(bool enabled);
        bool isAnimationEnabled() const { return animate.load(); }
        std::unique_ptr<GenerationPreview> takePreview() { return std::move(preview); }
        const char* statusText() const;
        std::string generationError;
        float generationProgress = 0.0f;
        long long seed = 12419;
        bool isSeedRandom = true;
        bool is_done = false;
        
        // EmbeddablePlane<Identifiable> plane;
        EmbeddablePlane<ResourceRadialNode<Resource>> plane;

        Matrix<double> noiseMap;
        Matrix<Resource> spiceMap;
        Matrix<bool> wallMask;
        
        // std::shared_ptr<const EdgeGraph<Identifiable, Identifiable>> defaultGraph = habbanyaErgSymmetry;
        std::shared_ptr<MapTemplate> mapTemplate;
            
        // std::shared_ptr<Grid<Identifiable>> grid = nullptr;
        std::shared_ptr<Grid<ResourceRadialNode<Resource>>> grid = nullptr;
        std::unordered_map<std::pair<Identifiable, Identifiable>, std::vector<tiles::Border>, PairIDHash> edgeToborderMap;
        GenerationStage generationStage = GenerationStage::NONE;
        ZoneMasks zoneMasks;
        std::unordered_map<Identifiable, ResourceGenerator<Resource>, IDHash> resourceGenerators;
    private:
        // ZoneBloater<Identifiable, Identifiable> zoneBloater;
        ZoneBloater<ResourceRadialNode<Resource>, BasicSymConnection, BasicAsymConnection> zoneBloater;
        std::vector<Matrix<double>> resourceOctaves;
        DoubleVector2 mirrorNormal{1,0};
        std::unordered_map<Identifiable, Identifiable, IDHash> mirroredZoneIDs;
        std::unordered_map<Identifiable, bool, IDHash> canonicalZoneSide;
        bool canPlaceCompleteZone(Identifiable id, IntVector2 point) const;
        void mirrorCompleteTiles(bool seedNodes = false);

        std::future<std::unique_ptr<Generation>> worker;
        std::atomic<bool> animate{false};
        std::atomic<bool> stopping{false};
        std::mutex updateMutex;
        std::condition_variable animationWake;
        GenerationStage postedStage = GenerationStage::NONE;
        float postedProgress = 0.0f;
        long long postedSeed = 12419;
        std::unique_ptr<GenerationPreview> pendingPreview;
        std::unique_ptr<GenerationPreview> preview;

        std::unique_ptr<GenerationPreview> makePreview() const;
        void acceptResult(Generation& result);

        void deduceSeed();

        void embedTemplate();
        void bloatZones();
        void postProcess();
        void buildBorders();
        void generateNoise();
        void blendZones();
        void generateResources();
        
};

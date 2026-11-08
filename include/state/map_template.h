#pragma once
#include <edge_graph.h>
#include <resources.h>
#include <connection.h>
#include <nodes.h>
#include "map_dimensions.h"
#include <cstdint>

namespace planar { struct Topology; }

class MapTemplate{
public:
    MapTemplate(){};
    std::vector<NoiseOctaveParam> octaves;
    Resource wallResource = Resource::CLIFF_ROCK;
    MapDimensionLimits dimensionLimits;
    std::shared_ptr<EdgeGraph<ResourceRadialNode<Resource>, BasicSymConnection, BasicAsymConnection>> zoneGraph;
    std::shared_ptr<const planar::Topology> embeddingTopology;
    PositionMagnetMap positionMagnets;
    bool rotatePositionMagnets = false;
    PositionMagnetMap magnetsForGeneration(std::uint64_t seed, int width, int height) const;
};

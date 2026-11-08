#pragma once
#include <edge_graph.h>
#include <resources.h>
#include <connection.h>
#include <nodes.h>
#include "map_dimensions.h"
#include "template_symmetry.h"
#include <cstdint>

namespace planar { struct Topology; struct Reflection; }

class MapTemplate{
public:
    MapTemplate(){};
    std::vector<NoiseOctaveParam> octaves;
    Resource wallResource = Resource::CLIFF_ROCK;
    MapDimensionLimits dimensionLimits;
    std::shared_ptr<EdgeGraph<ResourceRadialNode<Resource>, BasicSymConnection, BasicAsymConnection>> zoneGraph;
    std::shared_ptr<const planar::Topology> embeddingTopology;
    PositionMagnetMap positionMagnets;
    NodeBoundaryMap nodeBoundary;
    bool rotatePositionMagnets = false;
    SymmetryMode symmetryMode = SymmetryMode::None;
    std::shared_ptr<const planar::Reflection> embeddingReflection;
    PositionMagnetMap magnetsForGeneration(std::uint64_t seed, int width, int height) const;
    std::shared_ptr<const planar::Reflection> reflectionForGeneration(const PositionMagnetMap& magnets,
        double width = 1, double height = 1) const;
};

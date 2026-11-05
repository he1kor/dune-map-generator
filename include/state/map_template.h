#pragma once
#include <edge_graph.h>
#include <resources.h>
#include <connection.h>
#include <nodes.h>
#include "map_dimensions.h"

class MapTemplate{
public:
    MapTemplate(){};
    std::vector<NoiseOctaveParam> octaves;
    Resource wallResource = Resource::CLIFF_ROCK;
    MapDimensionLimits dimensionLimits;
    std::shared_ptr<EdgeGraph<ResourceRadialNode<Resource>, BasicSymConnection, BasicAsymConnection>> zoneGraph;
};

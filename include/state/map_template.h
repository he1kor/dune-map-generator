#pragma once
#include <edge_graph.h>
#include <resources.h>
#include <connection.h>
#include <nodes.h>

class MapTemplate{
public:
    MapTemplate(){};
    std::vector<NoiseOctaveParam> octaves;
    std::shared_ptr<EdgeGraph<ResourceRadialNode<Resource>, BasicSymConnection, BasicAsymConnection>> zoneGraph;
};
#pragma once

#include <memory>

#include "edge_graph.h"
#include "identifiable.h"
#include "nodes.h"

namespace templates{

    static std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> grid3x3 = std::make_shared<const EdgeGraph<BasicNode, Identifiable>>(
    EdgeGraph<BasicNode, Identifiable>(std::vector<std::pair<BasicNode, std::vector<Identifiable>>>
    {
        {{0, 0}, {1, 3}},       // Top-left (connects right and down)
        {{1, 0}, {0, 2, 4}},    // Top-middle (connects left, right, down)
        {{2, 0}, {1, 5}},       // Top-right (connects left and down)
        {{3, 0}, {0, 4, 6}},    // Middle-left (connects up, right, down)
        {{4, 0}, {1, 3, 5, 7}}, // Center (connects all 4 directions)
        {{5, 0}, {2, 4, 8}},    // Middle-right (connects left, up, down)
        {{6, 0}, {3, 7}},       // Bottom-left (connects up and right)
        {{7, 0}, {4, 6, 8}},    // Bottom-middle (connects left, up, right)
        {{8, 0}, {5, 7}}        // Bottom-right (connects left and up)
    }
    ));

    static std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> habbanyaErg = std::make_shared<const EdgeGraph<BasicNode, Identifiable>>(
    EdgeGraph<BasicNode, Identifiable>(std::vector<std::pair<BasicNode, std::vector<Identifiable>>>
    {
        {{1, 0}, {2, 4, 6}},
        {{2, 0}, {1, 3, 4, 5, 7}},
        {{3, 0}, {2, 5, 8}},
        {{4, 0}, {1, 2, 6, 7, 9}},
        {{5, 0}, {2, 3, 7, 8, 10}},
        {{6, 0}, {1, 4, 9, 11}},
        {{7, 0}, {2, 4, 5, 9, 10, 12}},
        {{8, 0}, {3, 5, 10, 13}},
        {{9, 0}, {4, 6, 7, 11, 12}},
        {{10, 0}, {5, 7, 8, 12, 13}},
        {{11, 0}, {6, 9, 12}},
        {{12, 0}, {7, 9, 10, 11, 13}},
        {{13, 0}, {8, 10, 12}}
    }
    ));

    static std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> habbanyaErgSymmetry = std::make_shared<const EdgeGraph<BasicNode, Identifiable>>(
    EdgeGraph<BasicNode, Identifiable>(std::vector<std::pair<BasicNode, std::vector<Identifiable>>>
    {
        {{1, 0}, {2, 4, 6}},
        {{2, 0}, {1, 3, 4, 5}},
        {{3, 0}, {2, 5, 8}},
        {{4, 0}, {1, 2, 5, 6, 7, 9}},
        {{5, 0}, {2, 3, 4, 7, 8, 10}},
        {{6, 0}, {1, 4, 9, 11}},
        {{7, 0}, {4, 5, 9, 10}},
        {{8, 0}, {3, 5, 10, 13}},
        {{9, 0}, {4, 6, 7, 10, 11, 12}},
        {{10, 0}, {5, 7, 8, 9, 12, 13}},
        {{11, 0}, {6, 9, 12}},
        {{12, 0}, {9, 10, 11, 13}},
        {{13, 0}, {8, 10, 12}}
    }
    ));

    static std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> habbanyaErgButterFly = std::make_shared<const EdgeGraph<BasicNode, Identifiable>>(
    EdgeGraph<BasicNode, Identifiable>(std::vector<std::pair<BasicNode, std::vector<Identifiable>>>
    {
        {{1, 0}, {4, 6}},
        {{2, 0}, {4, 5}},
        {{3, 0}, {5, 8}},
        {{4, 0}, {1, 2, 5, 6, 7, 9}},
        {{5, 0}, {2, 3, 4, 7, 8, 10}},
        {{6, 0}, {1, 4, 9, 11}},
        {{7, 0}, {4, 5, 9, 10}},
        {{8, 0}, {3, 5, 10, 13}},
        {{9, 0}, {4, 6, 7, 10, 11, 12}},
        {{10, 0}, {5, 7, 8, 9, 12, 13}},
        {{11, 0}, {6, 9}},
        {{12, 0}, {9, 10}},
        {{13, 0}, {8, 10}}
    }
    ));

    static std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> longTriangulated = std::make_shared<const EdgeGraph<BasicNode, Identifiable>>(
    EdgeGraph<BasicNode, Identifiable>(std::vector<std::pair<BasicNode, std::vector<Identifiable>>>
    {
        {{0, 0}, {1, 2, 3, 4}},
        {{1, 0}, {0, 2, 5, 6}},
        {{2, 0}, {0, 1, 3, 6, 7}},
        {{3, 0}, {0, 2, 4, 7, 8}},
        {{4, 0}, {0, 3, 8, 9}},
        {{5, 0}, {1, 6, 10}},
        {{6, 0}, {1, 2, 5, 7, 10}},
        {{7, 0}, {2, 3, 6, 8, 10}},
        {{8, 0}, {3, 4, 7, 9, 10}},
        {{9, 0}, {4, 8, 10, 11}},
        {{10, 0}, {5, 6, 7, 8, 9, 11, 12, 13}},
        {{11, 0}, {9, 10, 13, 14}},
        {{12, 0}, {10, 13, 15}},
        {{13, 0}, {10, 11, 12, 14, 15}},
        {{14, 0}, {11, 13, 15}},
        {{15, 0}, {12, 13, 14}}
    }
    ));


    static std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> jebusCross = std::make_shared<const EdgeGraph<BasicNode, Identifiable>>(
    EdgeGraph<BasicNode, Identifiable>(std::vector<std::pair<BasicNode, std::vector<Identifiable>>>
        {
            {{0, 0}, {4}},
            {{1, 0}, {4}},
            {{2, 0}, {4}},
            {{3, 0}, {4}},
            {{4, 0}, {0, 1, 2, 3}}
        }
    ));

    static std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> testy = std::make_shared<const EdgeGraph<BasicNode, Identifiable>>(
    EdgeGraph<BasicNode, Identifiable>(std::vector<std::pair<BasicNode, std::vector<Identifiable>>>
        {
            {{0, 0}, {1, 2, 3}},
            {{1, 0}, {0, 2, 4}},
            {{2, 0}, {0, 1, 3}},
            {{3, 0}, {0, 2, 4}},
            {{4, 0}, {1, 3}}
        }
    ));

    static std::shared_ptr<const EdgeGraph<BasicNode, Identifiable>> jebusCrossMagnetic = std::make_shared<const EdgeGraph<BasicNode, Identifiable>>(
    EdgeGraph<BasicNode, Identifiable>(std::vector<std::pair<BasicNode, std::vector<Identifiable>>>
        {
            {{0, 0}, {4}},
            {{1, 0}, {4}},
            {{2, 0}, {4}},
            {{3, 0}, {4}},
            {{4, 1}, {0, 1, 2, 3}}
        }
    ));

    }
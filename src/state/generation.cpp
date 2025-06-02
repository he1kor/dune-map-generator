#include "generation.h"
#include <iostream>

Generation::Generation() : plane(128.0, 128.0){};
void Generation::generate(){
    plane.clear();
    plane.initEmbed(&defaultGraph);
    iterateNext = true;
}

void Generation::runIteration(){
    if (!plane.stepForceDirected())
        iterateNext = false;
}

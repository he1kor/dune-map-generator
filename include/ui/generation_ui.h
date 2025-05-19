#pragma once
#include "generation.h"
#include "map_render.h"
#include <array>

class GenerationUI{
    public:
        GenerationUI(Generation* generationPtr, MapRenderer* mapRendererPtr);
        void build();
        
    private:
        Generation* generationPtr;
        MapRenderer* mapRendererPtr;
};
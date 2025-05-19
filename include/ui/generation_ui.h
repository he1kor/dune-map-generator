#pragma once
#include "generation.h"
#include "map_render.h"
#include <array>

class GenerationUI{
    public:
        GenerationUI(Generation* generationPtr, MapRenderer* mapRendererPtr);
        void build();
        
    private:
        void showMapPreview();
        Generation* generationPtr;
        MapRenderer* mapRendererPtr;
};
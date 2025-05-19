#pragma once
#include "generation.h"
#include <array>

class GenerationUI{
    public:
        GenerationUI(Generation* generationPtr);
        void build();
        
    private:
        void showMapPreview();
        Generation* generationPtr;
};
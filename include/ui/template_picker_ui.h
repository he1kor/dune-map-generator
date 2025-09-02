#pragma once
#include "template_picker.h"
#include "generation.h"

#include "map_render.h"

class TemplatePickerUI{
    public:
        TemplatePickerUI(TemplatePicker* templatePickerPtr, Generation* generationPtr, MapRenderer& mapRenderer);
        void buildError();
        void build();

    private:
        void initError(const std::string& text);
        void buildErrors();
        void buildGenerateButton();
        void buildTemplatePickButton();
        bool buildFilePickButton(const std::string& name, const std::string& value, const std::string& id);
        
        TemplatePicker* templatePickerPtr;
        MapRenderer& mapRenderer;
        Generation* generationPtr;

        std::string errorText = "";
};
#pragma once
#include "template_picker.h"
#include "generation.h"

class TemplatePickerUI{
    public:
        TemplatePickerUI(TemplatePicker* templatePickerPtr, Generation* generationPtr);
        void build();
        
        
    private:
        
        void buildGenerateButton();
        void buildTemplatePickButton();
        bool buildFilePickButton(std::string name, std::string default_value, std::string id);
        
        TemplatePicker* templatePickerPtr;
        Generation* generationPtr;
};
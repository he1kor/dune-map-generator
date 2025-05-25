#pragma once
#include "template_picker.h"
#include "generation.h"

class TemplatePickerUI{
    public:
        TemplatePickerUI(TemplatePicker* templatePickerPtr, Generation* generationPtr);
        void buildGenerateButton();
        void build();

    private:
        TemplatePicker* templatePickerPtr;
        Generation* generationPtr;
};
#pragma once
#include "template_picker.h"

class TemplatePickerUI{
    public:
        TemplatePickerUI(TemplatePicker* templatePickerPtr);
        void build();
    private:
        TemplatePicker* templatePickerPtr;
};
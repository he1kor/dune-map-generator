#include "app_layout.h"
#include "imgui.h"
#include "quick_settings_ui.h"
#include "template_picker_ui.h"
#include "generation_ui.h"
#include <algorithm>

void appLayout::build(QuickSettingsUI& settings, TemplatePickerUI& files, GenerationUI& generation) {
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(windowPadding, windowPadding));
    ImGui::Begin("##Main Window", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    const float middleWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - 2 * sidebarWidth - 2 * panelSpacing);
    if (ImGui::BeginChild("settings_panel", ImVec2(sidebarWidth, 0))) settings.build();
    ImGui::EndChild();
    ImGui::SameLine(0, panelSpacing);
    if (ImGui::BeginChild("generation_panel", ImVec2(middleWidth, 0))) generation.build();
    ImGui::EndChild();
    ImGui::SameLine(0, panelSpacing);
    if (ImGui::BeginChild("files_panel", ImVec2(sidebarWidth, 0))) files.build();
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleVar();
}

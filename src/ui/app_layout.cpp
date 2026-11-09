#include "app_layout.h"
#include "colors.h"
#include "imgui.h"
#include "quick_settings_ui.h"
#include "template_picker_ui.h"
#include "generation_ui.h"
#include "template_editor_ui.h"
#include <algorithm>

namespace {
bool beginWorkspaceTab(const char* label, const char* shortcut, bool select) {
    // ImGui uses the hover color for held tabs too; give mouse presses their own gold.
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImGui::IsMouseDown(ImGuiMouseButton_Left)
        ? VARIANT2_ULTRA : ImGui::GetStyleColorVec4(ImGuiCol_TabHovered));
    const bool visible = ImGui::BeginTabItem(label, nullptr, select ? ImGuiTabItemFlags_SetSelected : 0);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s (%s)", label, shortcut);
    return visible;
}
}

void appLayout::build(QuickSettingsUI& settings, TemplatePickerUI& files, GenerationUI& generation, TemplateEditorUI& editor) {
    const auto& io = ImGui::GetIO();
    const bool canSwitchTabs = !io.KeyCtrl && !io.KeyAlt && !io.KeyShift && !io.KeySuper
        && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    const bool selectGenerator = canSwitchTabs && ImGui::IsKeyPressed(ImGuiKey_F1, false);
    const bool selectEditor = canSwitchTabs && ImGui::IsKeyPressed(ImGuiKey_F2, false);
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(windowPadding, windowPadding));
    ImGui::Begin("##Main Window", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    // BeginTabBar draws its divider with the selected tab color.
    ImGui::PushStyleColor(ImGuiCol_TabSelected, ImGui::GetStyleColorVec4(ImGuiCol_Separator));
    const bool workspacesVisible = ImGui::BeginTabBar("Workspaces", ImGuiTabBarFlags_DrawSelectedOverline);
    ImGui::PopStyleColor();
    if (workspacesVisible) {
        if (beginWorkspaceTab("Generator", "F1", selectGenerator)) {
            const float middleWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - 2 * sidebarWidth - 2 * panelSpacing);
            if (ImGui::BeginChild("settings_panel", ImVec2(sidebarWidth, 0))) settings.build();
            ImGui::EndChild();
            ImGui::SameLine(0, panelSpacing);
            if (ImGui::BeginChild("generation_panel", ImVec2(middleWidth, 0))) generation.build();
            ImGui::EndChild();
            ImGui::SameLine(0, panelSpacing);
            if (ImGui::BeginChild("files_panel", ImVec2(sidebarWidth, 0))) files.build();
            ImGui::EndChild();

            ImGui::EndTabItem();
        }
        if (beginWorkspaceTab("Template editor", "F2", selectEditor)) {
            editor.build();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    editor.buildDialogs();

    ImGui::End();
    ImGui::PopStyleVar();
}

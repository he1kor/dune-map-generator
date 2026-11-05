#include "imgui.h"
#include "quick_settings_ui.h"
#include "slider.h"
#include "int_field.h"
#include <optional>
#include <string>
#include <vector>

namespace {
std::optional<int> sidePicker(const char* id, int current, const std::vector<int>& choices) {
    std::optional<int> selected;
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::BeginDisabled(choices.size() == 1);
    if (ImGui::BeginCombo(id, std::to_string(current).c_str())) {
        for (int side : choices) {
            const bool active = side == current;
            if (ImGui::Selectable(std::to_string(side).c_str(), active)) selected = side;
            if (active) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    return selected;
}
}

QuickSettingsUI::QuickSettingsUI(QuickSettings* quickSettingsPtr) : quickSettingsPtr(quickSettingsPtr) {}

void QuickSettingsUI::build() {
    ImGui::Text("Map size");
    ImGui::Dummy(ImVec2(0, 5));
    const auto& limits = quickSettingsPtr->dimensionLimits;
    if (ImGui::BeginTable("##map_dimensions", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Width");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted("Height");
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        if (auto value = sidePicker("##width", quickSettingsPtr->width.getValue(), limits.allowedSides(true)))
            quickSettingsPtr->setWidth(*value);
        ImGui::TableSetColumnIndex(1);
        if (auto value = sidePicker("##height", quickSettingsPtr->height.getValue(), limits.allowedSides(false)))
            quickSettingsPtr->setHeight(*value);
        ImGui::EndTable();
    }
    ImGui::TextWrapped("Even numbers, 32-128 cells.");
    if (limits != MapDimensionLimits{}) {
        ImGui::Spacing();
        ImGui::TextUnformatted("Template limits:");
        ImGui::Text("Width: %d-%d", limits.minWidth, limits.maxWidth);
        ImGui::Text("Height: %d-%d", limits.minHeight, limits.maxHeight);
        if (limits.squareOnly()) {
            ImGui::TextWrapped("Square map: both sides change together.");
        } else if (limits.minAspectRatio != 0.25 || limits.maxAspectRatio != 4.0) {
            ImGui::TextWrapped("Width / height: %.3g to %.3g.", limits.minAspectRatio, limits.maxAspectRatio);
            ImGui::TextWrapped("The other side adjusts to keep this shape.");
        }
    }
    if (quickSettingsPtr->sizeAdjustedToTemplate) {
        ImGui::Spacing();
        ImGui::TextWrapped("Size adjusted to fit this template.");
    }

    ImGui::Dummy(ImVec2(0, 10));
    ImGui::Separator();
    if (!ImGui::CollapsingHeader("Other settings")) return;
    ImGui::TextWrapped("These settings aren't available yet.");
    ImGui::BeginDisabled();
    buildIntField(quickSettingsPtr->spotCount, "Number of spots", "##spots");
    ImGui::Dummy(ImVec2(0, 10));
    buildSlider(quickSettingsPtr->amountOfSpice, "Amount of spice", "##spice");
    ImGui::Dummy(ImVec2(0, 10));
    buildSlider(quickSettingsPtr->amountOfDunes, "Amount of dunes", "##dunes");
    ImGui::Dummy(ImVec2(0, 10));
    buildSlider(quickSettingsPtr->amountOfInfantryCliffs, "Amount of infantry cliffs", "##inf_cliffs");
    ImGui::EndDisabled();
}

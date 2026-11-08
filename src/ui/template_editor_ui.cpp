#include "template_editor_ui.h"
#include "template_editor_descriptions.h"
#include "template_picker.h"
#include "template_validation.h"
#include "generation.h"
#include "quick_settings.h"
#include "filedialog.h"
#include "resources.h"
#include "colors.h"
#include "imgui_internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cfloat>
#include <vector>

namespace {
using Json = nlohmann::json;
constexpr ImU32 accent = IM_COL32(255, 202, 104, 255);
constexpr ImU32 textColor = IM_COL32(255, 255, 255, 255);
constexpr float gridSpacing = 64;
constexpr double toastDuration = 4;
constexpr double toastFadeDuration = 0.4;

void descriptionTooltip(const char* text) {
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

void helpMarker(const char* text) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) descriptionTooltip(text);
}

void inlineValueColumn() {
    ImGui::SameLine();
    const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    const float column = (ImGui::GetCursorStartPos().x + right) * 0.5f;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), column));
    ImGui::SetNextItemWidth(-1);
}

void fieldLabel(const char* label) {
    ImGui::TextUnformatted(label);
    helpMarker(uiDescriptions().parameter(label).c_str());
    ImGui::SetNextItemWidth(-1);
}

bool checkboxField(const char* label, bool* value) {
    ImGui::PushID(label);
    const bool changed = ImGui::Checkbox(label, value);
    helpMarker(uiDescriptions().parameter(label).c_str());
    ImGui::PopID();
    return changed;
}

bool stringField(const char* label, Json& value, bool inlineField = false) {
    ImGui::PushID(label);
    if (inlineField) inlineValueColumn();
    else fieldLabel(label);
    const auto original = value.get<std::string>();
    std::vector<char> buffer(std::max(size_t(512), original.size() + 256), 0);
    std::copy(original.begin(), original.end(), buffer.begin());
    const bool changed = ImGui::InputText("##value", buffer.data(), buffer.size(), ImGuiInputTextFlags_NoUndoRedo);
    if (changed) value = std::string(buffer.data());
    if (inlineField && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        descriptionTooltip(uiDescriptions().parameter(label).c_str());
    ImGui::PopID();
    return changed;
}

void numberField(const char* label, Json& value, bool whole = false, bool inlineField = false) {
    ImGui::PushID(label);
    if (inlineField) inlineValueColumn();
    else fieldLabel(label);
    ImGui::PushItemFlag(ImGuiItemFlags_LiveEditOnInputScalar, true);
    if (whole) {
        int number = value.get<int>();
        if (ImGui::InputScalar("##value", ImGuiDataType_S32, &number, nullptr, nullptr, nullptr, ImGuiInputTextFlags_NoUndoRedo)) value = number;
    } else {
        double number = value.get<double>();
        if (ImGui::InputScalar("##value", ImGuiDataType_Double, &number, nullptr, nullptr, "%.6g", ImGuiInputTextFlags_NoUndoRedo)) value = number;
    }
    if (inlineField && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        descriptionTooltip(uiDescriptions().parameter(label).c_str());
    ImGui::PopItemFlag();
    ImGui::PopID();
}

void overrideNumberField(const char* label, Json* value, Json defaultValue, bool whole = false) {
    // Fallbacks are display-only: viewing or applying a disabled override must
    // never insert its default into the template.
    ImGui::BeginDisabled(value == nullptr);
    Json& shown = value ? *value : defaultValue;
    if (shown.is_string()) stringField(label, shown, true);
    else numberField(label, shown, whole, true);
    ImGui::EndDisabled();
}

void resourceField(const char* label, Json& value, bool inlineField = false) {
    ImGui::PushID(label);
    if (inlineField) inlineValueColumn();
    else fieldLabel(label);
    static constexpr const char* names[] = {"rock", "dunes", "sand", "spice", "thick_spice", "cliff_rock"};
    const auto current = value.get<std::string>();
    const auto parsed = resourceFromString(current);
    if (ImGui::BeginCombo("##value", uiDescriptions().materialName(current).c_str())) {
        for (const auto* name : names) {
            ImGui::PushID(name);
            const auto resource = *resourceFromString(name);
            ImGui::ColorButton("##color", ImGui::ColorConvertU32ToFloat4(resourceColor(resource)),
                ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
            ImGui::SameLine();
            if (ImGui::Selectable(uiDescriptions().materialName(name).c_str(), parsed == resource)) value = name;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    if (inlineField && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
        descriptionTooltip(uiDescriptions().parameter(label).c_str());
    ImGui::PopID();
}

float pointDistance(ImVec2 point, ImVec2 a, ImVec2 b) {
    const float dx = b.x - a.x, dy = b.y - a.y;
    const double length2 = double(dx) * dx + double(dy) * dy;
    const float t = length2 > 0 ? static_cast<float>(std::clamp(
        ((double(point.x) - a.x) * dx + (double(point.y) - a.y) * dy) / length2, 0.0, 1.0)) : 0;
    return std::hypot(point.x - a.x - t * dx, point.y - a.y - t * dy);
}

// Clip before emitting dashes so offscreen nodes cannot create millions of
// draw commands. The phase stays anchored to the first endpoint when panning.
void patternedEdge(ImDrawList* draw, ImVec2 a, ImVec2 b, const Json& passes, ImVec2 clipMin, ImVec2 clipMax, bool highlighted) {
    const float length = std::hypot(b.x - a.x, b.y - a.y);
    if (length < 0.01f) return;
    const ImVec2 direction((b.x - a.x) / length, (b.y - a.y) / length);
    float start = 0, end = length;
    auto clip = [&](float origin, float delta, float minimum, float maximum) {
        if (std::abs(delta) < 1e-8f) return origin >= minimum && origin <= maximum;
        float lo = (minimum - origin) / delta, hi = (maximum - origin) / delta;
        if (lo > hi) std::swap(lo, hi);
        start = std::max(start, lo);
        end = std::min(end, hi);
        return start < end;
    };
    if (!clip(a.x, direction.x, clipMin.x - 4, clipMax.x + 4)
        || !clip(a.y, direction.y, clipMin.y - 4, clipMax.y + 4)) return;
    // Two screen pixels per unit, independent of zoom or map resolution.
    const std::array<double, 4> pattern{2 * passes["minWallLength"].get<double>(),
        2 * passes["minPassWidth"].get<double>(), 2 * passes["maxWallLength"].get<double>(),
        2 * passes["maxPassWidth"].get<double>()};
    const double period = pattern[0] + pattern[1] + pattern[2] + pattern[3];
    if (period <= 0) return;
    double phase = std::fmod(start, period);
    int index = 0;
    while (index < 3 && phase >= pattern[index]) phase -= pattern[index++];
    double at = start;
    while (at < end) {
        const double next = std::min(double(end), at + pattern[index] - phase);
        if (index % 2 == 0 && next > at) {
            const ImVec2 wallStart(a.x + direction.x * at, a.y + direction.y * at);
            const ImVec2 wallEnd(a.x + direction.x * next, a.y + direction.y * next);
            if (highlighted) draw->AddLine(wallStart, wallEnd, accent, 7);
            draw->AddLine(wallStart, wallEnd, IM_COL32(0, 0, 0, 255), 3.5f);
        }
        at = next;
        phase = 0;
        index = (index + 1) % 4;
    }
}

Json* findNode(Json& document, int id) {
    for (auto& node : document["nodes"]) if (node["id"] == id) return &node;
    return nullptr;
}

std::string pathText(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {value.begin(), value.end()};
}
} // namespace

TemplateEditorUI::TemplateEditorUI(TemplateEditor& editor, TemplatePicker& picker, Generation& generation, QuickSettings& settings)
    : editor_(editor), picker_(picker), generation_(generation), settings_(settings) {}

void TemplateEditorUI::guardChanges(std::function<void()> action) {
    if (!editor_.dirty()) { action(); return; }
    pendingAction_ = std::move(action);
    openDiscard_ = true;
}

bool TemplateEditorUI::requestClose() {
    if (!editor_.dirty()) return true;
    guardChanges([this] { closeApproved_ = true; });
    return false;
}

bool TemplateEditorUI::save(bool saveAs) {
    finishMove();
    auto path = editor_.path();
    if (saveAs || path.empty()) {
        auto picked = fileDialog.tryGetSavePathFromDialog(path.empty() ? "template.json" : pathText(path.filename()));
        if (!picked) {
            if (picked.error() == FileDialogStatus::ERROR) message_ = "Could not open the save dialog.";
            return false;
        }
        path = *picked;
        if (path.extension().empty()) path += ".json";
    }
    if (!editor_.save(path)) { message_ = editor_.error(); return false; }
    notify(uiDescriptions().format("saved", {{"filename", pathText(path.filename())}}));
    message_.clear();
    return true;
}

void TemplateEditorUI::toolbar() {
    const auto& topologyWarning = editor_.topologyWarning();
    const float menuHeight = ImGui::GetFrameHeight() + 2 * ImGui::GetStyle().FramePadding.y;
    if (ImGui::BeginChild("editor_commands", {0, menuHeight}, 0,
            ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New template", "Ctrl+N")) createNew();
                if (ImGui::MenuItem("Open...", "Ctrl+O")) load();
                ImGui::Separator();
                if (ImGui::MenuItem("Save", "Ctrl+S")) save(false);
                if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S")) save(true);
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem(("Undo " + editor_.undoLabel()).c_str(), "Ctrl+Z", false, editor_.canUndo())) history(false);
                if (ImGui::MenuItem(("Redo " + editor_.redoLabel()).c_str(), "Ctrl+Y", false, editor_.canRedo())) history(true);
                ImGui::Separator();
                if (ImGui::MenuItem("Copy data", "Ctrl+C", false, selectedNode_ >= 0 || selectedEdge_ >= 0)) copyData();
                if (ImGui::MenuItem("Paste data", "Ctrl+V", false, canPasteData())) pasteData();
                ImGui::Separator();
                if (ImGui::MenuItem("Edit selection", "Enter", false, selectedNode_ >= 0 || selectedEdge_ >= 0))
                    edit(selectedNode_ >= 0 ? Dialog::Node : Dialog::Edge, selectedNode_ >= 0 ? selectedNode_ : selectedEdge_);
                if (ImGui::MenuItem("Delete selection", "Delete", false, !selectedNodes_.empty() || selectedEdge_ >= 0)) deleteSelection();
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Fit graph", "F / Home")) fit_ = true;
                if (ImGui::MenuItem("Selection mode", "V", mode_ == Mode::Selection)) mode_ = Mode::Selection;
                if (ImGui::MenuItem("Edge mode", "E / hold Shift", mode_ == Mode::Edge)) mode_ = Mode::Edge;
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Settings")) edit(Dialog::Globals);
            if (ImGui::MenuItem("Help", "F2")) openHelp_ = true;
            if (!topologyWarning.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(accent));
                if (ImGui::MenuItem("!##topology_warning")) ImGui::OpenPopup("Graph topology warning");
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) descriptionTooltip(uiDescriptions().text("topology_warning").c_str());
                if (ImGui::BeginPopup("Graph topology warning")) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25);
                    ImGui::TextUnformatted(uiDescriptions().text("topology_warning").c_str());
                    ImGui::Separator();
                    ImGui::TextUnformatted(topologyWarning.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndPopup();
                }
            }
            const auto title = std::string(editor_.dirty() ? "* " : "") + editor_.document()["name"].get<std::string>();
            const auto start = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            if (width > 30) {
                ImGui::GetWindowDrawList()->PushClipRect(start, {start.x + width, start.y + ImGui::GetFrameHeight()}, true);
                ImGui::GetWindowDrawList()->AddText({start.x + 12, start.y + ImGui::GetStyle().FramePadding.y}, accent, title.c_str());
                ImGui::GetWindowDrawList()->PopClipRect();
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::EndChild();

    auto modeButton = [&](const char* id, bool edge, bool selected, const char* hint) {
        ImGui::PushStyleColor(ImGuiCol_Button, selected ? RED_EXTRA : HARD_BROWN);
        const bool clicked = ImGui::Button(id, {32, ImGui::GetFrameHeight()});
        ImGui::PopStyleColor();
        const auto p = ImGui::GetItemRectMin();
        auto* draw = ImGui::GetWindowDrawList();
        const ImVec2 center(p.x + 16, p.y + ImGui::GetFrameHeight() * 0.5f);
        if (edge) {
            draw->AddLine({center.x - 7, center.y + 5}, {center.x + 7, center.y - 5}, textColor, 1.5f);
            draw->AddRectFilled({center.x - 10, center.y + 2}, {center.x - 4, center.y + 8}, textColor);
            draw->AddRectFilled({center.x + 4, center.y - 8}, {center.x + 10, center.y - 2}, textColor);
        } else {
            draw->AddTriangleFilled({center.x - 5, center.y - 9}, {center.x - 5, center.y + 8}, {center.x + 7, center.y + 3}, textColor);
            draw->AddLine({center.x - 1, center.y + 1}, {center.x + 5, center.y + 10}, textColor, 2);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", hint);
        return clicked;
    };
    const bool edgeMode = mode_ == Mode::Edge || ImGui::GetIO().KeyShift;
    if (modeButton("##selection_mode", false, !edgeMode, uiDescriptions().text("selection_mode").c_str())) {
        mode_ = Mode::Selection; connectionStart_ = -1;
    }
    ImGui::SameLine();
    if (modeButton("##edge_mode", true, edgeMode, uiDescriptions().text("edge_mode").c_str())) {
        mode_ = Mode::Edge; connectionStart_ = -1;
    }
    ImGui::SameLine(0, 16);
    if (ImGui::Button("+ Zone")) {
        selectedNode_ = editor_.addNode({camera_.x, camera_.y});
        selectedNodes_ = {selectedNode_}; selectedEdge_ = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Fit")) fit_ = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", uiDescriptions().text("fit_graph").c_str());
    ImGui::SameLine();
    ImGui::BeginDisabled(!topologyWarning.empty() || editor_.document()["nodes"].empty());
    if (ImGui::Button("Auto place")) autoPlace();
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", uiDescriptions().text(topologyWarning.empty() ? "auto_place" : "auto_place_disabled").c_str());
    const float buttonWidth = ImGui::CalcTextSize("Use for generation").x + 2 * ImGui::GetStyle().FramePadding.x;
    const float remainingWidth = ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x
        - (ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x);
    if (remainingWidth > buttonWidth + ImGui::GetStyle().ItemSpacing.x * 2 + 56) {
        ImGui::SameLine();
        ImGui::TextDisabled("%.0f%%", zoom_ * 100);
    }
    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - buttonWidth));
    ImGui::BeginDisabled(generation_.isRunning());
    if (ImGui::Button("Use for generation")) useForGeneration();
    ImGui::EndDisabled();
    if (!message_.empty()) ImGui::TextWrapped("%s", message_.c_str());
}

void TemplateEditorUI::createNew() {
    finishMove();
    guardChanges([this] { editor_.createNew(); resetInteraction(); fit_ = true; message_.clear(); });
}

void TemplateEditorUI::load() {
    finishMove();
    auto picked = fileDialog.tryGetPathFromDialog();
    if (picked) guardChanges([this, path = *picked] {
        if (editor_.load(path)) { resetInteraction(); fit_ = true; message_.clear(); }
        else message_ = editor_.error();
    });
    else if (picked.error() == FileDialogStatus::ERROR) message_ = "Could not open the file dialog.";
}

void TemplateEditorUI::useForGeneration() {
    if (generation_.isRunning()) return;
    const auto error = templateValidationError(editor_.document());
    if (!error.empty()) message_ = error;
    else if (picker_.tryReadEdgeGraph(editor_.document())) {
        picker_.templateName = editor_.document()["name"].get<std::string>();
        settings_.applyDimensionLimits(picker_.mapTemplate->dimensionLimits);
        message_.clear();
        notify(uiDescriptions().text("generation_selected"));
    }
}

void TemplateEditorUI::finishMove() {
    if (moving_) { editor_.endAction(); moving_ = false; }
    draggingNode_ = -1; dragPositions_.clear(); toggleOnRelease_ = -1;
}

void TemplateEditorUI::resetInteraction() {
    finishMove();
    selectedNode_ = selectedEdge_ = connectionStart_ = -1;
    selectedNodes_.clear(); selecting_ = false; mode_ = Mode::Selection;
}

void TemplateEditorUI::history(bool redo) {
    finishMove();
    const auto previousPath = editor_.path();
    const auto result = redo ? editor_.redo() : editor_.undo();
    if (result.empty()) return;
    notify(result);
    const auto previousMode = mode_;
    resetInteraction();
    mode_ = previousMode;
    if (previousPath != editor_.path()) fit_ = true;
    message_.clear();
}

void TemplateEditorUI::shortcuts() {
    const auto& io = ImGui::GetIO();
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) || io.WantTextInput) return;
    if (io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) save(io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_O, false)) load();
        if (ImGui::IsKeyPressed(ImGuiKey_N, false)) createNew();
        if (ImGui::IsKeyPressed(ImGuiKey_Comma, false)) edit(Dialog::Globals);
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) useForGeneration();
        if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) history(io.KeyShift);
        if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) history(true);
        if (ImGui::IsKeyPressed(ImGuiKey_C, false)) copyData();
        if (ImGui::IsKeyPressed(ImGuiKey_V, false)) pasteData();
    } else {
        if (ImGui::IsKeyPressed(ImGuiKey_V, false)) { mode_ = Mode::Selection; connectionStart_ = -1; }
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) { mode_ = Mode::Edge; connectionStart_ = -1; }
        if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) openHelp_ = true;
    }
}

void TemplateEditorUI::copyData() {
    finishMove();
    const auto payload = selectedNode_ >= 0 ? editor_.copyNodeData(selectedNode_)
        : selectedEdge_ >= 0 ? editor_.copyConnectionData(selectedEdge_) : Json();
    if (payload.is_null()) return;
    ImGui::SetClipboardText(payload.dump().c_str());
    notify(uiDescriptions().text(selectedNode_ >= 0 ? "node_copied" : "edge_copied"));
}

bool TemplateEditorUI::canPasteData() const {
    const char* clipboard = ImGui::GetClipboardText();
    if (!clipboard || (!selectedNodes_.size() && selectedEdge_ < 0)) return false;
    const auto payload = Json::parse(clipboard, nullptr, false);
    return TemplateEditor::isDataClipboard(payload, selectedNodes_.empty() ? "edge" : "node");
}

void TemplateEditorUI::pasteData() {
    if (selectedNodes_.empty() && selectedEdge_ < 0) return;
    finishMove();
    const char* clipboard = ImGui::GetClipboardText();
    const auto payload = Json::parse(clipboard ? clipboard : "", nullptr, false);
    const bool pasted = !selectedNodes_.empty()
        ? editor_.pasteNodeData(payload, {selectedNodes_.begin(), selectedNodes_.end()})
        : editor_.pasteConnectionData(payload, selectedEdge_);
    if (!pasted) { notify(editor_.error()); return; }
    message_.clear();
    notify(uiDescriptions().format(selectedNodes_.empty() ? "edge_pasted" : "node_pasted",
        {{"count", std::to_string(selectedNodes_.size())}}));
}

ImVec2 TemplateEditorUI::screen(TemplatePoint point) const {
    return {canvasOrigin_.x + canvasSize_.x * 0.5f + (point.x - camera_.x) * zoom_,
        canvasOrigin_.y + canvasSize_.y * 0.5f + (point.y - camera_.y) * zoom_};
}

TemplatePoint TemplateEditorUI::world(ImVec2 point) const {
    return {camera_.x + (point.x - canvasOrigin_.x - canvasSize_.x * 0.5f) / zoom_,
        camera_.y + (point.y - canvasOrigin_.y - canvasSize_.y * 0.5f) / zoom_};
}

void TemplateEditorUI::fitView() {
    fit_ = false;
    if (editor_.document()["nodes"].empty()) { camera_ = {0, 0}; zoom_ = 1; return; }
    ImVec2 minimum(1e20f, 1e20f), maximum(-1e20f, -1e20f);
    for (const auto& node : editor_.document()["nodes"]) {
        const auto point = editor_.position(node["id"]);
        const float half = TemplateEditor::halfSize(node);
        minimum.x = std::min(minimum.x, point.x - half); minimum.y = std::min(minimum.y, point.y - half);
        maximum.x = std::max(maximum.x, point.x + half); maximum.y = std::max(maximum.y, point.y + half);
    }
    camera_ = {(minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f};
    zoom_ = std::clamp(std::min(std::max(20.0f, canvasSize_.x - 64) / std::max(1.0f, maximum.x - minimum.x),
        std::max(20.0f, canvasSize_.y - 64) / std::max(1.0f, maximum.y - minimum.y)), 0.02f, 4.0f);
}

void TemplateEditorUI::autoPlace() {
    finishMove();
    if (!editor_.autoPlace()) { message_ = editor_.error(); return; }
    connectionStart_ = -1;
    fitView();
    message_.clear();
    notify(uiDescriptions().text("auto_placed"));
}

void TemplateEditorUI::deleteSelection() {
    finishMove();
    editor_.beginAction(selectedNodes_.empty() ? "Delete connection" : "Delete " + std::to_string(selectedNodes_.size()) + " zones");
    if (!selectedNodes_.empty()) for (int id : selectedNodes_) editor_.removeNode(id);
    else if (selectedEdge_ >= 0) editor_.removeConnection(selectedEdge_);
    editor_.endAction();
    selectedNodes_.clear();
    selectedNode_ = selectedEdge_ = connectionStart_ = draggingNode_ = -1;
}

void TemplateEditorUI::canvas() {
    if (!ImGui::BeginChild("graph_canvas", ImVec2(0, 0), ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) { ImGui::EndChild(); return; }
    canvasOrigin_ = ImGui::GetCursorScreenPos();
    canvasSize_ = ImGui::GetContentRegionAvail();
    canvasSize_.x = std::max(1.0f, canvasSize_.x); canvasSize_.y = std::max(1.0f, canvasSize_.y);
    if (fit_) fitView();
    ImGui::InvisibleButton("##graph", canvasSize_, ImGuiButtonFlags_MouseButtonLeft
        | ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);
    const bool hovered = ImGui::IsItemHovered();
    auto& io = ImGui::GetIO();
    if (hovered && io.MouseWheel != 0) {
        const auto before = world(io.MousePos);
        zoom_ = std::clamp(zoom_ * std::pow(1.15f, io.MouseWheel), 0.02f, 4.0f);
        const auto after = world(io.MousePos);
        camera_.x += before.x - after.x; camera_.y += before.y - after.y;
    }
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0)
            || ImGui::IsMouseDragging(ImGuiMouseButton_Right))) {
        camera_.x -= io.MouseDelta.x / zoom_; camera_.y -= io.MouseDelta.y / zoom_;
    }

    int hitNode = -1, hitEdge = -1;
    const auto& nodes = editor_.document()["nodes"];
    if (hovered) for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        const auto center = screen(editor_.position((*it)["id"]));
        const float half = TemplateEditor::halfSize(*it) * zoom_;
        if (std::abs(io.MousePos.x - center.x) <= half && std::abs(io.MousePos.y - center.y) <= half) {
            hitNode = (*it)["id"]; break;
        }
    }
    const auto& edges = editor_.document()["symmetrical_edges"];
    if (hovered && hitNode < 0) {
        float closest = 8;
        for (size_t i = 0; i < edges.size(); ++i) {
            const auto& edge = edges[i];
            const float distance = pointDistance(io.MousePos, screen(editor_.position(edge["ID_first"])),
                screen(editor_.position(edge["ID_second"])));
            if (distance < closest) { hitEdge = static_cast<int>(i); closest = distance; }
        }
    }
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        toggleOnRelease_ = -1;
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (hitNode >= 0) {
                selectedNode_ = hitNode; selectedNodes_ = {hitNode}; selectedEdge_ = -1;
                edit(Dialog::Node, hitNode);
            }
            else if (hitEdge >= 0) {
                selectedNodes_.clear(); selectedNode_ = -1; selectedEdge_ = hitEdge;
                edit(Dialog::Edge, hitEdge);
            }
            else {
                selectedNode_ = editor_.addNode(world(io.MousePos));
                selectedNodes_ = {selectedNode_}; selectedEdge_ = -1;
            }
        } else if (hitNode >= 0 && (mode_ == Mode::Edge || io.KeyShift)) {
            const int source = connectionStart_ >= 0 ? connectionStart_
                : io.KeyShift && selectedNode_ >= 0 ? selectedNode_ : -1;
            if (source < 0 || source == hitNode) connectionStart_ = hitNode;
            else {
                editor_.connect(source, hitNode);
                connectionStart_ = -1;
            }
            selectedNode_ = hitNode; selectedNodes_ = {hitNode}; selectedEdge_ = -1;
        } else if (hitNode >= 0) {
            if (io.KeyCtrl) {
                if (selectedNodes_.contains(hitNode)) toggleOnRelease_ = hitNode;
                else selectedNodes_.insert(hitNode);
            } else if (!selectedNodes_.contains(hitNode)) selectedNodes_ = {hitNode};
            selectedNode_ = hitNode; selectedEdge_ = -1; connectionStart_ = -1;
            draggingNode_ = hitNode;
            dragStart_ = world(io.MousePos);
            dragPositions_.clear();
            for (int id : selectedNodes_) dragPositions_[id] = editor_.position(id);
        } else {
            connectionStart_ = -1;
            selectedEdge_ = hitEdge;
            if (!io.KeyCtrl) { selectedNodes_.clear(); selectedNode_ = -1; }
            if (hitEdge < 0 && mode_ == Mode::Selection && !io.KeyShift) {
                selecting_ = true; selectionStart_ = world(io.MousePos); selectionBase_ = selectedNodes_;
            }
        }
    }
    if (draggingNode_ >= 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (!moving_) {
            editor_.beginAction("Move " + std::to_string(dragPositions_.size()) + " zones");
            moving_ = true; toggleOnRelease_ = -1;
        }
        const auto cursor = world(io.MousePos);
        TemplatePoint delta{cursor.x - dragStart_.x, cursor.y - dragStart_.y};
        if (io.KeyCtrl) {
            const auto anchor = dragPositions_.at(draggingNode_);
            const TemplatePoint target{anchor.x + delta.x, anchor.y + delta.y};
            const TemplatePoint cross{std::round(target.x / gridSpacing) * gridSpacing,
                std::round(target.y / gridSpacing) * gridSpacing};
            const float half = gridSpacing * 0.5f;
            const TemplatePoint center{std::round((target.x - half) / gridSpacing) * gridSpacing + half,
                std::round((target.y - half) / gridSpacing) * gridSpacing + half};
            const auto snapped = std::hypot(target.x - center.x, target.y - center.y)
                < std::hypot(target.x - cross.x, target.y - cross.y) ? center : cross;
            delta = {snapped.x - anchor.x, snapped.y - anchor.y};
        }
        float minX = -2e7f, maxX = 2e7f, minY = -2e7f, maxY = 2e7f;
        for (const auto& [id, point] : dragPositions_) {
            minX = std::max(minX, -1e7f - point.x); maxX = std::min(maxX, 1e7f - point.x);
            minY = std::max(minY, -1e7f - point.y); maxY = std::min(maxY, 1e7f - point.y);
        }
        delta.x = std::clamp(delta.x, minX, maxX); delta.y = std::clamp(delta.y, minY, maxY);
        for (const auto& [id, point] : dragPositions_) editor_.setPosition(id, {point.x + delta.x, point.y + delta.y});
    }
    if (selecting_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const auto point = world(io.MousePos);
        selectedNodes_ = selectionBase_;
        for (const auto& node : editor_.document()["nodes"]) {
            const int id = node["id"];
            const auto center = editor_.position(id);
            const float half = TemplateEditor::halfSize(node);
            if (center.x + half >= std::min(selectionStart_.x, point.x) && center.x - half <= std::max(selectionStart_.x, point.x)
                && center.y + half >= std::min(selectionStart_.y, point.y) && center.y - half <= std::max(selectionStart_.y, point.y))
                selectedNodes_.insert(id);
        }
        selectedNode_ = selectedNodes_.empty() ? -1 : *selectedNodes_.begin();
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (toggleOnRelease_ >= 0 && !moving_) {
            selectedNodes_.erase(toggleOnRelease_);
            selectedNode_ = selectedNodes_.empty() ? -1 : *selectedNodes_.begin();
        }
        finishMove(); selecting_ = false;
    }
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right)
            && io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] < io.MouseDragThreshold * io.MouseDragThreshold) {
        contextPoint_ = world(io.MousePos); contextNode_ = hitNode; contextEdge_ = hitEdge;
        if (contextNode_ >= 0) {
            if (!selectedNodes_.contains(contextNode_)) selectedNodes_ = {contextNode_};
            selectedNode_ = contextNode_; selectedEdge_ = -1;
        } else if (contextEdge_ >= 0) {
            selectedNodes_.clear(); selectedNode_ = -1; selectedEdge_ = contextEdge_;
        }
        ImGui::OpenPopup("Canvas actions");
    }
    if (ImGui::BeginPopup("Canvas actions")) {
        if (ImGui::MenuItem("Add node here")) {
            selectedNode_ = editor_.addNode(contextPoint_); selectedNodes_ = {selectedNode_}; selectedEdge_ = -1;
        }
        if (contextNode_ >= 0 || contextEdge_ >= 0) {
            if (ImGui::MenuItem("Copy data", "Ctrl+C")) copyData();
            if (ImGui::MenuItem("Paste data", "Ctrl+V", false, canPasteData())) pasteData();
            ImGui::Separator();
            if (ImGui::MenuItem("Edit")) edit(contextNode_ >= 0 ? Dialog::Node : Dialog::Edge,
                contextNode_ >= 0 ? contextNode_ : contextEdge_);
            if (contextNode_ >= 0 && ImGui::MenuItem("Connect from this node")) {
                mode_ = Mode::Edge; connectionStart_ = contextNode_;
            }
            if (ImGui::MenuItem("Delete")) {
                selectedNode_ = contextNode_; selectedEdge_ = contextEdge_;
                if (contextNode_ >= 0 && !selectedNodes_.contains(contextNode_)) selectedNodes_ = {contextNode_};
                if (contextNode_ < 0) selectedNodes_.clear();
                deleteSelection();
            }
        }
        if (ImGui::MenuItem("Fit graph")) fit_ = true;
        ImGui::EndPopup();
    }
    if (ImGui::IsWindowFocused() && !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deleteSelection();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { finishMove(); connectionStart_ = -1; selecting_ = false; selectedNodes_.clear(); selectedNode_ = selectedEdge_ = -1; }
        if (ImGui::IsKeyPressed(ImGuiKey_F) || ImGui::IsKeyPressed(ImGuiKey_Home)) fit_ = true;
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
            selectedNodes_.clear();
            for (const auto& node : editor_.document()["nodes"]) selectedNodes_.insert(node["id"].get<int>());
            selectedNode_ = selectedNodes_.empty() ? -1 : *selectedNodes_.begin(); selectedEdge_ = -1;
        }
        if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter) && (selectedNode_ >= 0 || selectedEdge_ >= 0))
            edit(selectedNode_ >= 0 ? Dialog::Node : Dialog::Edge, selectedNode_ >= 0 ? selectedNode_ : selectedEdge_);
    }

    auto* draw = ImGui::GetWindowDrawList();
    const ImVec2 maximum(canvasOrigin_.x + canvasSize_.x, canvasOrigin_.y + canvasSize_.y);
    draw->PushClipRect(canvasOrigin_, maximum, true);
    draw->AddRectFilled(canvasOrigin_, maximum, IM_COL32(128, 125, 113, 255));
    const float grid = gridSpacing * zoom_;
    if (grid >= 12) {
        const auto zero = screen({0, 0});
        float x = std::fmod(zero.x - canvasOrigin_.x, grid); if (x < 0) x += grid;
        float y = std::fmod(zero.y - canvasOrigin_.y, grid); if (y < 0) y += grid;
        for (x += canvasOrigin_.x; x < maximum.x; x += grid)
            draw->AddLine({x, canvasOrigin_.y}, {x, maximum.y}, IM_COL32(255, 255, 255, 18));
        for (y += canvasOrigin_.y; y < maximum.y; y += grid)
            draw->AddLine({canvasOrigin_.x, y}, {maximum.x, y}, IM_COL32(255, 255, 255, 18));
        float centerX = std::fmod(zero.x + grid * 0.5f - canvasOrigin_.x, grid);
        float centerY = std::fmod(zero.y + grid * 0.5f - canvasOrigin_.y, grid);
        if (centerX < 0) centerX += grid;
        if (centerY < 0) centerY += grid;
        for (float cx = canvasOrigin_.x + centerX; cx < maximum.x; cx += grid)
            for (float cy = canvasOrigin_.y + centerY; cy < maximum.y; cy += grid)
                draw->AddCircleFilled({cx, cy}, 1.5f, IM_COL32(255, 255, 255, io.KeyCtrl ? 110 : 35), 4);
    }
    // Re-read arrays after canvas actions, which may have erased entries.
    for (size_t i = 0; i < editor_.document()["symmetrical_edges"].size(); ++i) {
        const auto& edge = editor_.document()["symmetrical_edges"][i];
        const int first = edge["ID_first"], second = edge["ID_second"];
        auto a = screen(editor_.position(first)), b = screen(editor_.position(second));
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float norm = std::max(std::abs(dx), std::abs(dy));
        if (norm < 0.01f) continue;
        const float firstFraction = TemplateEditor::halfSize(*editor_.node(first)) * zoom_ / norm;
        const float secondFraction = TemplateEditor::halfSize(*editor_.node(second)) * zoom_ / norm;
        if (firstFraction + secondFraction >= 1) continue;
        b = {b.x - dx * secondFraction, b.y - dy * secondFraction};
        a = {a.x + dx * firstFraction, a.y + dy * firstFraction};
        patternedEdge(draw, a, b, edge["data"]["passes"], canvasOrigin_, maximum,
            static_cast<int>(i) == selectedEdge_ || static_cast<int>(i) == hitEdge);
    }
    const int source = connectionStart_ >= 0 ? connectionStart_ : io.KeyShift ? selectedNode_ : -1;
    if (hovered && source >= 0 && editor_.node(source)) draw->AddLine(screen(editor_.position(source)), io.MousePos, accent, 2);
    for (const auto& node : editor_.document()["nodes"]) {
        const int id = node["id"];
        const auto center = screen(editor_.position(id));
        const float half = TemplateEditor::halfSize(node) * zoom_;
        const ImVec2 minimum(center.x - half, center.y - half), end(center.x + half, center.y + half);
        if (end.x < canvasOrigin_.x || minimum.x > maximum.x || end.y < canvasOrigin_.y || minimum.y > maximum.y) continue;
        const float rounding = std::min(6.0f * zoom_, half * 0.15f);
        draw->AddRectFilled(minimum, end, IM_COL32(50, 50, 50, 255), rounding);
        draw->PathRect(minimum, end, rounding);
        const std::vector<ImVec2> outline(draw->_Path.begin(), draw->_Path.end());
        draw->PathClear();
        for (const auto& entry : node["data"]["resources"]["content"]) {
            const float start = entry["startThreshold"], finish = entry["endThreshold"];
            if (finish <= start) continue;
            // Clip each band to the same rounded outline, including very thin
            // bands at the top/bottom. Per-band rounding would distort corners.
            auto polygon = outline;
            for (const auto [height, lower] : {std::pair{end.y - 2 * half * finish, true},
                    std::pair{end.y - 2 * half * start, false}}) {
                std::vector<ImVec2> clipped;
                if (polygon.empty()) break;
                auto previous = polygon.back();
                bool previousInside = lower ? previous.y >= height : previous.y <= height;
                for (auto point : polygon) {
                    const bool inside = lower ? point.y >= height : point.y <= height;
                    if (inside != previousInside) {
                        const float t = (height - previous.y) / (point.y - previous.y);
                        clipped.emplace_back(previous.x + t * (point.x - previous.x), height);
                    }
                    if (inside) clipped.push_back(point);
                    previous = point; previousInside = inside;
                }
                polygon = std::move(clipped);
            }
            if (polygon.size() >= 3) draw->AddConvexPolyFilled(polygon.data(), static_cast<int>(polygon.size()),
                resourceColor(*resourceFromString(entry["name"])));
        }
        draw->AddRect(minimum, end, selectedNodes_.contains(id) || id == hitNode || id == connectionStart_ ? accent
            : IM_COL32(28, 24, 19, 255), rounding, 0, selectedNodes_.contains(id) ? 3 : 1.5f);
        char label[128];
        std::snprintf(label, sizeof(label), "ID %d\nS: %.4g", id, node["data"]["susceptibility"].get<double>());
        const float normalSize = ImGui::GetFontSize();
        const auto normalBounds = ImGui::GetFont()->CalcTextSizeA(normalSize, 1e8f, 0, label);
        const float available = std::max(1.0f, 2 * half - 8);
        const float fontSize = std::max(1.0f, normalSize * std::min({1.0f,
            available / std::max(1.0f, normalBounds.x), available / std::max(1.0f, normalBounds.y)}));
        const auto textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, 1e8f, 0, label);
        const ImVec2 textPosition(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
        draw->AddText(ImGui::GetFont(), fontSize, {textPosition.x + 1, textPosition.y + 1}, IM_COL32(0, 0, 0, 255), label);
        draw->AddText(ImGui::GetFont(), fontSize, textPosition, textColor, label);
    }
    if (editor_.document()["nodes"].empty())
        draw->AddText({canvasOrigin_.x + 16, canvasOrigin_.y + 16}, textColor, uiDescriptions().text("empty_canvas").c_str());
    if (selecting_ && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        const auto start = screen(selectionStart_);
        const ImVec2 lo(std::min(start.x, io.MousePos.x), std::min(start.y, io.MousePos.y));
        const ImVec2 hi(std::max(start.x, io.MousePos.x), std::max(start.y, io.MousePos.y));
        draw->AddRectFilled(lo, hi, IM_COL32(255, 202, 104, 35)); draw->AddRect(lo, hi, accent);
    }
    const auto status = uiDescriptions().format("canvas_status", {{"count", std::to_string(selectedNodes_.size())},
        {"mode", mode_ == Mode::Edge || io.KeyShift ? "Edge mode" : "Selection mode"}});
    draw->AddRectFilled({canvasOrigin_.x, maximum.y - ImGui::GetTextLineHeight() - 8}, maximum, IM_COL32(35, 28, 25, 210));
    draw->AddText({canvasOrigin_.x + 8, maximum.y - ImGui::GetTextLineHeight() - 4}, textColor, status.c_str());
    draw->PopClipRect();
    if (hovered && hitNode >= 0 && editor_.node(hitNode) && !ImGui::IsMouseDown(0))
        ImGui::SetTooltip("%s", uiDescriptions().format("node_tooltip", {{"id", std::to_string(hitNode)},
            {"radius", editor_.node(hitNode)->at("data").at("radius").dump()},
            {"susceptibility", editor_.node(hitNode)->at("data").at("susceptibility").dump()}}).c_str());
    if (hovered && hitEdge >= 0 && hitEdge < static_cast<int>(editor_.document()["symmetrical_edges"].size())) {
        const auto& edge = editor_.document()["symmetrical_edges"][hitEdge];
        ImGui::SetTooltip("%s", uiDescriptions().format("edge_tooltip", {{"first", edge["ID_first"].dump()},
            {"second", edge["ID_second"].dump()}}).c_str());
    }
    ImGui::EndChild();
}

void TemplateEditorUI::edit(Dialog dialog, int index) {
    dialog_ = dialog;
    editingNode_ = dialog == Dialog::Node ? index : -1;
    editedId_ = editingNode_;
    editingEdge_ = dialog == Dialog::Edge ? index : -1;
    draft_ = editor_.document();
    dialogInitial_ = draft_;
    draftUndo_.clear(); draftRedo_.clear(); draftGroup_ = 0;
    dismissDialog_ = false;
    dialogError_.clear();
    openDialog_ = true;
    finishMove();
    connectionStart_ = -1;
}

void TemplateEditorUI::globalFields() {
    stringField("Template name", draft_["name"]);
    bool walls = draft_.contains("walls");
    if (checkboxField("Custom wall resource", &walls)) {
        if (walls) draft_["walls"] = {{"resource", "cliff_rock"}};
        else draft_.erase("walls");
    }
    Json defaultWall = "cliff_rock";
    ImGui::BeginDisabled(!walls);
    resourceField("Wall material", walls ? draft_["walls"]["resource"] : defaultWall, true);
    ImGui::EndDisabled();
    ImGui::SeparatorText("Noise octaves");
    int count = static_cast<int>(draft_["octaves"].size());
    fieldLabel("Octave count");
    if (ImGui::SliderInt("##octave_count", &count, 1, 3)) {
        for (int i = 1; i <= 3; ++i) {
            const auto key = "octave" + std::to_string(i);
            if (i <= count && !draft_["octaves"].contains(key))
                draft_["octaves"][key] = {{"minRadius", 2.0 * i}, {"maxRadius", 3.0 * i}};
            if (i > count) {
                draft_["octaves"].erase(key);
                for (auto& node : draft_["nodes"]) node["data"]["resources"].erase(key + "_weight");
            }
        }
    }
    for (int i = 1; i <= count; ++i) {
        ImGui::PushID(i);
        ImGui::Spacing();
        ImGui::SeparatorText(("Octave " + std::to_string(i)).c_str());
        ImGui::Spacing();
        auto& octave = draft_["octaves"]["octave" + std::to_string(i)];
        numberField("Minimum radius", octave["minRadius"]);
        numberField("Maximum radius", octave["maxRadius"]);
        ImGui::PopID();
    }
    ImGui::SeparatorText("Map dimensions");
    bool dimensions = draft_.contains("dimensions");
    if (checkboxField("Restrict map dimensions", &dimensions)) {
        if (dimensions) draft_["dimensions"] = {{"width", {{"min", 32}, {"max", 128}}},
            {"height", {{"min", 32}, {"max", 128}}}, {"aspectRatio", {{"min", 0.25}, {"max", 4.0}}}};
        else draft_.erase("dimensions");
    }
    if (dimensions) for (const auto* axis : {"width", "height", "aspectRatio"}) {
        ImGui::PushID(axis);
        ImGui::TextUnformatted(axis);
        auto range = draft_["dimensions"].value(axis, Json::object());
        const auto before = range;
        for (const auto* bound : {"min", "max"}) {
            bool enabled = range.contains(bound);
            const bool aspectRatio = std::string(axis) == "aspectRatio";
            const Json defaultValue = aspectRatio ? Json(std::string(bound) == "min" ? 0.25 : 4.0)
                : Json(std::string(bound) == "min" ? 32 : 128);
            const auto label = std::string("Set ") + bound;
            if (checkboxField(label.c_str(), &enabled)) {
                if (enabled) range[bound] = defaultValue;
                else range.erase(bound);
            }
            overrideNumberField(bound, enabled ? &range[bound] : nullptr, defaultValue, !aspectRatio);
        }
        if (range != before) draft_["dimensions"][axis] = range;
        ImGui::PopID();
    }
    ImGui::TextWrapped("%s", uiDescriptions().text("dimensions").c_str());
}

void TemplateEditorUI::nodeFields() {
    auto* node = findNode(draft_, editingNode_);
    if (!node) return;
    Json id = editedId_;
    numberField("Node ID", id, true);
    editedId_ = id.get<int>();
    auto& data = (*node)["data"];
    numberField("Radius", data["radius"]);
    numberField("Susceptibility", data["susceptibility"]);
    ImGui::SeparatorText("Noise weights");
    auto& resources = data["resources"];
    for (size_t i = 1; i <= draft_["octaves"].size(); ++i) {
        const auto key = "octave" + std::to_string(i) + "_weight";
        bool enabled = resources.contains(key);
        ImGui::PushID(key.c_str());
        ImGui::Spacing();
        ImGui::SeparatorText(("Octave " + std::to_string(i)).c_str());
        ImGui::Spacing();
        if (checkboxField("Override", &enabled)) {
            if (enabled) resources[key] = 0.0;
            else resources.erase(key);
        }
        overrideNumberField("Weight", enabled ? &resources[key] : nullptr, 0.0);
        ImGui::PopID();
    }
    ImGui::SeparatorText("Resources (bottom to top)");
    ImGui::TextWrapped("%s", uiDescriptions().text("resource_intervals").c_str());
    auto& content = resources["content"];
    int remove = -1, split = -1;
    for (size_t i = 0; i < content.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text("Row %zu", i + 1);
        resourceField("Material", content[i]["name"]);
        numberField("Start threshold", content[i]["startThreshold"]);
        numberField("End threshold", content[i]["endThreshold"]);
        if (ImGui::Button("Split row")) split = static_cast<int>(i);
        ImGui::SameLine();
        ImGui::BeginDisabled(content.size() == 1);
        if (ImGui::Button("Remove row")) remove = static_cast<int>(i);
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::PopID();
    }
    if (split >= 0) {
        const double middle = (content[split]["startThreshold"].get<double>() + content[split]["endThreshold"].get<double>()) * 0.5;
        auto next = content[split];
        next["startThreshold"] = middle;
        content[split]["endThreshold"] = middle;
        content.insert(content.begin() + split + 1, next);
    }
    if (remove >= 0) {
        // Extend the touching neighbor, independent of JSON row ordering.
        const auto removed = content[remove];
        for (size_t i = 0; i < content.size(); ++i) if (i != static_cast<size_t>(remove)) {
            if (content[i]["endThreshold"] == removed["startThreshold"]) {
                content[i]["endThreshold"] = removed["endThreshold"]; break;
            }
            if (content[i]["startThreshold"] == removed["endThreshold"]) {
                content[i]["startThreshold"] = removed["startThreshold"]; break;
            }
        }
        content.erase(content.begin() + remove);
    }
}

void TemplateEditorUI::edgeFields() {
    auto& edge = draft_["symmetrical_edges"][editingEdge_];
    const int first = edge["ID_first"], second = edge["ID_second"];
    ImGui::Text("Connection %d - %d", first, second);
    ImGui::SeparatorText("Symmetrical settings");
    auto& data = edge["data"];
    auto& passes = data["passes"];
    numberField("Minimum pass width", passes["minPassWidth"], true);
    numberField("Maximum pass width", passes["maxPassWidth"], true);
    numberField("Minimum wall length", passes["minWallLength"], true);
    numberField("Maximum wall length", passes["maxWallLength"], true);
    bool blend = data.contains("resourceBlend");
    if (checkboxField("Resource blending", &blend)) {
        if (blend) data["resourceBlend"] = {{"distance", 0}};
        else data.erase("resourceBlend");
    }
    overrideNumberField("Blend distance", blend ? &data["resourceBlend"]["distance"] : nullptr, 0, true);
    ImGui::SeparatorText("Optional generation geometry");
    bool distance = data.contains("geometry") && data["geometry"].contains("preferredCenterDistance");
    if (checkboxField("Preferred center distance", &distance)) {
        if (distance) data["geometry"]["preferredCenterDistance"] = 15.0;
        else data["geometry"].erase("preferredCenterDistance");
    }
    overrideNumberField("Center distance", distance ? &data["geometry"]["preferredCenterDistance"] : nullptr,
        uiDescriptions().text("automatic_value"));
    bool weight = data.contains("geometry") && data["geometry"].contains("distanceWeight");
    if (checkboxField("Custom distance weight", &weight)) {
        if (weight) data["geometry"]["distanceWeight"] = 1.0;
        else data["geometry"].erase("distanceWeight");
    }
    overrideNumberField("Distance weight", weight ? &data["geometry"]["distanceWeight"] : nullptr, 1.0);
    ImGui::SeparatorText("Asymmetrical settings");
    auto& directed = draft_["asymmetrical_edges"];
    for (int reverse = 0; reverse < 2; ++reverse) {
        const int from = reverse ? second : first, to = reverse ? first : second;
        ImGui::PushID(reverse);
        ImGui::Text("%d -> %d", from, to);
        int index = -1;
        for (size_t i = 0; i < directed.size(); ++i)
            if (directed[i]["ID_from"] == from && directed[i]["ID_to"] == to) index = static_cast<int>(i);
        bool enabled = index >= 0;
        if (checkboxField("Enable directed rule", &enabled)) {
            if (enabled) {
                directed.push_back({{"ID_from", from}, {"ID_to", to},
                    {"data", {{"intakeDistance", 0}, {"areaGuaranteed", 0}, {"bonusValue", 0.0}}}});
                index = static_cast<int>(directed.size() - 1);
            } else directed.erase(directed.begin() + index);
        }
        if (enabled) {
            auto& rule = directed[index]["data"];
            numberField("Intake distance", rule["intakeDistance"], true);
            numberField("Guaranteed area", rule["areaGuaranteed"], true);
            numberField("Bonus value", rule["bonusValue"]);
        }
        ImGui::PopID();
        ImGui::Separator();
    }
}

bool TemplateEditorUI::dialogChanged() const {
    return draft_ != dialogInitial_ || (dialog_ == Dialog::Node && editedId_ != editingNode_);
}

bool TemplateEditorUI::applyDialog() {
    auto candidate = draft_;
    if (dialog_ == Dialog::Node && !editor_.renameNode(editingNode_, editedId_, candidate)) {
        dialogError_ = "Node ID must be non-negative and unique.";
        return false;
    }
    const std::string label = dialog_ == Dialog::Globals ? "Edit global settings"
        : dialog_ == Dialog::Node ? "Edit zone " + std::to_string(editingNode_) : "Edit connection settings";
    if (!editor_.replace(std::move(candidate), label)) { dialogError_ = editor_.error(); return false; }
    if (dialog_ == Dialog::Node) {
        selectedNodes_.erase(editingNode_); selectedNodes_.insert(editedId_);
        selectedNode_ = editedId_; connectionStart_ = -1;
    }
    dialog_ = Dialog::None;
    dismissDialog_ = false;
    return true;
}

void TemplateEditorUI::draftHistory(bool redo) {
    auto& source = redo ? draftRedo_ : draftUndo_;
    if (source.empty()) return;
    auto change = std::move(source.back()); source.pop_back();
    const auto& from = redo ? change.before : change.after;
    const auto& to = redo ? change.after : change.before;
    auto historyDocument = [&](const DraftState& state) {
        auto value = state.document;
        if (dialog_ == Dialog::Node) if (auto* node = findNode(value, editingNode_)) (*node)["id"] = state.id;
        return value;
    };
    notify(std::string(redo ? "Redo" : "Undo") + " settings edit"
        + TemplateEditor::describeChanges(historyDocument(from), historyDocument(to)));
    ImGui::ClearActiveID();
    draft_ = to.document; editedId_ = to.id;
    (redo ? draftUndo_ : draftRedo_).push_back(std::move(change));
    draftGroup_ = 0;
    dialogError_.clear();
}

void TemplateEditorUI::buildDialogs() {
    ImGui::PushStyleColor(ImGuiCol_PopupBg, SOFT_BROWN);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, HARD_BROWN);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, MEDIUM_BROWN);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, MEDIUM_BROWN);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16, 14});
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {8, 5});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4);
    if (openDiscard_) { ImGui::OpenPopup("Unsaved template"); openDiscard_ = false; }
    if (ImGui::BeginPopupModal("Unsaved template", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(uiDescriptions().text("save_changes").c_str());
        bool proceed = false;
        if (ImGui::Button("Save changes")) proceed = save(false);
        ImGui::SameLine();
        if (ImGui::Button("Discard changes")) proceed = true;
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { pendingAction_ = {}; ImGui::CloseCurrentPopup(); }
        if (proceed) {
            auto action = std::move(pendingAction_);
            pendingAction_ = {};
            ImGui::CloseCurrentPopup();
            if (action) action();
        }
        if (!message_.empty()) ImGui::TextWrapped("%s", message_.c_str());
        ImGui::EndPopup();
    }
    const char* title = dialog_ == Dialog::Globals ? "Global template settings" : dialog_ == Dialog::Node ? "Node settings" : "Connection settings";
    if (openDialog_) { ImGui::OpenPopup(title); openDialog_ = false; }
    const auto* viewport = ImGui::GetMainViewport();
    const float width = std::min(540.0f, viewport->Size.x - 24);
    ImGui::SetNextWindowSize({width, 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({width, 0}, {width, viewport->Size.y - 24});
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(title);
        ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - ImGui::GetFrameHeight());
        if (ImGui::Button("x##close_settings", {ImGui::GetFrameHeight(), ImGui::GetFrameHeight()})) dismissDialog_ = true;
        ImGui::Separator();
        const bool confirmingAtStart = ImGui::IsPopupOpen("Discard settings changes?");
        const auto& io = ImGui::GetIO();
        if (!confirmingAtStart && io.KeyCtrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) draftHistory(io.KeyShift);
            if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) draftHistory(true);
        }
        const DraftState beforeFields{draft_, editedId_};
        const bool wasChanged = dialogChanged();
        // ImGui text inputs normally revert their value on Escape. Keep the
        // live draft intact until the user answers our discard confirmation.
        const bool preserveDraft = ImGui::IsKeyPressed(ImGuiKey_Escape, false)
            && !ImGui::IsPopupOpen("Discard settings changes?");
        const auto beforeEscape = preserveDraft ? draft_ : Json();
        const int idBeforeEscape = editedId_;
        const float bodyMaximum = std::max(100.0f, viewport->Size.y - 200 - (!dialogError_.empty() ? 64 : 0));
        ImGui::SetNextWindowSizeConstraints({0, 0}, {FLT_MAX, bodyMaximum});
        if (ImGui::BeginChild("fields", {0, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysAutoResize)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8, 7});
            if (dialog_ == Dialog::Globals) globalFields();
            if (dialog_ == Dialog::Node) nodeFields();
            if (dialog_ == Dialog::Edge) edgeFields();
            ImGui::PopStyleVar();
        }
        ImGui::EndChild();
        if (preserveDraft) { draft_ = beforeEscape; editedId_ = idBeforeEscape; }
        const ImGuiID activeWidget = ImGui::GetActiveID();
        if (draft_ != beforeFields.document || editedId_ != beforeFields.id) {
            if (!draftUndo_.empty() && activeWidget != 0 && activeWidget == draftGroup_
                    && draftUndo_.back().widget == activeWidget)
                draftUndo_.back().after = {draft_, editedId_};
            else draftUndo_.push_back({beforeFields, {draft_, editedId_}, activeWidget});
            draftRedo_.clear();
        }
        draftGroup_ = activeWidget;
        if (!dialogError_.empty()) {
            if (ImGui::BeginChild("validation_message", {0, 54})) ImGui::TextWrapped("%s", dialogError_.c_str());
            ImGui::EndChild();
        }
        ImGui::Separator();
        const float buttonsWidth = 2 * 92 + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x - buttonsWidth);
        if (ImGui::Button("Cancel", {92, 0})) dismissDialog_ = true;
        ImGui::SameLine();
        bool apply = ImGui::Button("Apply", {92, 0});
        const bool confirming = ImGui::IsPopupOpen("Discard settings changes?");
        if (!confirming) {
            apply |= ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) dismissDialog_ = true;
        }
        if (apply && applyDialog()) ImGui::CloseCurrentPopup();
        bool closeParent = false;
        if (dismissDialog_) {
            if (wasChanged || dialogChanged()) ImGui::OpenPopup("Discard settings changes?");
            else { closeParent = true; dialog_ = Dialog::None; }
            dismissDialog_ = false;
        }
        if (ImGui::BeginPopupModal("Discard settings changes?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted(uiDescriptions().text("unapplied_changes").c_str());
            ImGui::TextUnformatted(uiDescriptions().text("discard_changes").c_str());
            if (ImGui::Button("Keep editing") || (!ImGui::IsWindowAppearing() && ImGui::IsKeyPressed(ImGuiKey_Escape, false)))
                ImGui::CloseCurrentPopup();
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Discard changes")) {
                ImGui::CloseCurrentPopup(); closeParent = true; dialog_ = Dialog::None;
            }
            ImGui::EndPopup();
        }
        if (closeParent) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    help();
    ImGui::PopStyleVar(4);
    ImGui::PopStyleColor(4);
    notifications();
}

void TemplateEditorUI::build() {
    shortcuts();
    toolbar();
    ImGui::Separator();
    canvas();
}

void TemplateEditorUI::notify(std::string text) {
    if (text.empty()) return;
    toasts_.push_back({nextToastId_++, std::move(text), ImGui::GetTime() + toastDuration});
    if (toasts_.size() > 4) toasts_.erase(toasts_.begin());
}

void TemplateEditorUI::notifications() {
    const auto* viewport = ImGui::GetMainViewport();
    const double now = ImGui::GetTime();
    std::erase_if(toasts_, [&](const Toast& toast) { return toast.expires <= now; });
    float bottom = viewport->Pos.y + viewport->Size.y - 12;
    const float width = std::min(470.0f, viewport->Size.x - 24);
    const float closeSize = ImGui::GetFrameHeight();
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollWithMouse;
    ImGuiWindow* popup = nullptr;
    for (const auto& opened : GImGui->OpenPopupStack)
        if (opened.Window && opened.Window->Active) { popup = opened.Window; break; }
    const auto raise = [&] {
        if (popup) ImGui::BringWindowToDisplayBehind(ImGui::GetCurrentWindow(), popup);
        else ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    };
    for (size_t i = toasts_.size(); i-- > 0;) {
        auto& toast = toasts_[i];
        const float textWidth = width - 24 - closeSize - 8;
        const float textHeight = ImGui::CalcTextSize(toast.text.c_str(), nullptr, false, textWidth).y;
        const float height = std::min(textHeight + 4, std::max(48.0f, viewport->Size.y / 5 - 28));
        const float alpha = static_cast<float>(std::clamp((toast.expires - now) / toastFadeDuration, 0.0, 1.0));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * alpha);
        ImGui::SetNextWindowPos({viewport->Pos.x + viewport->Size.x - 12, bottom}, ImGuiCond_Always, {1, 1});
        ImGui::SetNextWindowSize({width, height + 24});
        ImGui::SetNextWindowBgAlpha(0.93f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12, 10});
        const auto name = "##editor_toast_" + std::to_string(i);
        if (ImGui::Begin(name.c_str(), nullptr, flags | ImGuiWindowFlags_NoInputs)) {
            raise();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textWidth);
            ImGui::TextUnformatted(toast.text.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // A separate, tightly sized window captures only the dismiss button.
        // The message body stays click-through even over Generate or inputs.
        // Stable IDs prevent expiration of another toast from changing the
        // target while the user is holding this button down.
        ImGui::SetNextWindowPos({viewport->Pos.x + viewport->Size.x - 18 - closeSize, bottom - height - 18});
        ImGui::SetNextWindowSize({closeSize, closeSize});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, {1, 1});
        const auto closeName = "##editor_toast_close_" + std::to_string(toast.id);
        bool dismissed = false;
        if (ImGui::Begin(closeName.c_str(), nullptr, flags | ImGuiWindowFlags_NoBackground)) {
            raise();
            dismissed = ImGui::Button("x##dismiss", {closeSize, closeSize});
        }
        ImGui::End();
        ImGui::PopStyleVar(3);
        if (dismissed) toasts_.erase(toasts_.begin() + i);
        bottom -= height + 32;
    }
}

void TemplateEditorUI::help() {
    if (openHelp_) { ImGui::OpenPopup("Template editor help"); openHelp_ = false; }
    const auto* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize({std::min(620.0f, viewport->Size.x - 24), std::min(670.0f, viewport->Size.y - 24)}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, {0.5f, 0.5f});
    if (ImGui::BeginPopupModal("Template editor help", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        if (ImGui::BeginChild("help_body", {0, -ImGui::GetFrameHeightWithSpacing()})) {
            if (!uiDescriptions().error().empty()) ImGui::TextWrapped("%s", uiDescriptions().error().c_str());
            for (const auto& section : uiDescriptions().help()) {
                ImGui::SeparatorText(section.at("title").get_ref<const std::string&>().c_str());
                for (const auto& paragraph : section.at("paragraphs"))
                    ImGui::TextWrapped("%s", paragraph.get_ref<const std::string&>().c_str());
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("Close", {100, 0}) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

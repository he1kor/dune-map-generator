#pragma once

class QuickSettingsUI;
class TemplatePickerUI;
class GenerationUI;
class TemplateEditorUI;

namespace appLayout {
inline constexpr float sidebarWidth = 192.0f;
inline constexpr float panelSpacing = 8.0f;
inline constexpr float windowPadding = 8.0f;
inline constexpr float minWindowWidth = 2 * sidebarWidth + 128 + 2 * panelSpacing + 2 * windowPadding;
inline constexpr float maxWindowWidth = 2 * sidebarWidth + 512 + 2 * panelSpacing + 2 * windowPadding;
inline constexpr float minWindowHeight = 500.0f;
inline constexpr float maxWindowHeight = 730.0f;

void build(QuickSettingsUI& settings, TemplatePickerUI& files, GenerationUI& generation, TemplateEditorUI& editor);
}

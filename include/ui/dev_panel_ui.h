#pragma once

#include <vector>
#include <optional>
#include <string>
#include "template_picker.h"
#include "generation.h"
#include "map_render.h"
#include "colors.h"

enum class DisplayMode {
    SPICE_MAP,
    ZONE_MAP,
    INFLUENCE_MASK,
    BONUS_MASK,
    ZONE_BORDERS,
    ZONE_NOISE
};

class DevPanelUI{
private:
    bool showDevPanel = false;
    std::optional<int> selectedZoneId = std::nullopt;
    DisplayMode currentMode = DisplayMode::SPICE_MAP;
    
    const char* getModeName(DisplayMode mode) const {
        switch(mode) {
            case DisplayMode::SPICE_MAP: return "Spice Map";
            case DisplayMode::ZONE_MAP: return "Zone Map";
            case DisplayMode::INFLUENCE_MASK: return "Influence Mask";
            case DisplayMode::ZONE_BORDERS: return "Zone Borders";
            case DisplayMode::BONUS_MASK: return "Bonus Mask";
            case DisplayMode::ZONE_NOISE: return "Zone Noise";
            default: return "Unknown";
        }
    }
    
    void applyMode(Generation& generation, MapRenderer& mapRenderer) {
        switch(currentMode) {
            case DisplayMode::SPICE_MAP:
                mapRenderer.updateMap(
                    mapRenderer.convertResourceMatrix(generation.spiceMap),
                    generation.noiseMap.getWidth(),
                    generation.noiseMap.getHeight()
                );
                break;
                
            case DisplayMode::ZONE_MAP:
                if (generation.grid) {
                    mapRenderer.updateMap(
                        mapRenderer.convertGrid(*(generation.grid)),
                        generation.plane.getWidth(),
                        generation.plane.getHeight()
                    );
                }
                break;
                
            case DisplayMode::INFLUENCE_MASK:
                if (selectedZoneId.has_value() && 
                    generation.zoneMasks.influenceMask.find(Identifiable(selectedZoneId.value())) != generation.zoneMasks.influenceMask.end()) {
                    mapRenderer.updateMap(
                        mapRenderer.convertMatrix(generation.zoneMasks.influenceMask[Identifiable(selectedZoneId.value())]),
                        generation.plane.getWidth(),
                        generation.plane.getHeight()
                    );
                }
                break;
                
            case DisplayMode::BONUS_MASK:
                if (selectedZoneId.has_value() && 
                    generation.zoneMasks.bonusMask.find(Identifiable(selectedZoneId.value())) != generation.zoneMasks.bonusMask.end()) {
                    mapRenderer.updateMap(
                        mapRenderer.convertMatrix(generation.zoneMasks.bonusMask[Identifiable(selectedZoneId.value())]),
                        generation.plane.getWidth(),
                        generation.plane.getHeight()
                    );
                }
                break;
                
            case DisplayMode::ZONE_BORDERS:
                mapRenderer.updateMap(
                    mapRenderer.convertBorders(
                        generation.edgeToborderMap,
                        static_cast<size_t>(generation.plane.getWidth()),
                        static_cast<size_t>(generation.plane.getHeight())
                    ),
                    generation.plane.getWidth(),
                    generation.plane.getHeight()
                );
                break;
                
            case DisplayMode::ZONE_NOISE:
                if (selectedZoneId.has_value()) {
                    try {
                        const auto& noiseMatrix = generation.resourceGenerators.at(Identifiable(selectedZoneId.value())).getNoise();
                        
                        mapRenderer.updateMap(
                            mapRenderer.convertMatrix(noiseMatrix),
                            generation.plane.getWidth(),
                            generation.plane.getHeight()
                        );
                    } catch (const std::out_of_range&) {
                        ImGui::TextColored(ImVec4(1,0,0,1), "Zone %d not found in resource generators!", selectedZoneId.value());
                    }
                }
                break;
        }
    }

public:
    void handleInput() {
        if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
            showDevPanel = !showDevPanel;
        }
    }
    
    void render(Generation& generation, MapRenderer& mapRenderer, TemplatePicker& templatePicker) {
        if (!showDevPanel) return;
        
        if (ImGui::Begin("Dev Panel", &showDevPanel, ImGuiWindowFlags_AlwaysAutoResize)) {
            
            ImGui::Text("Display Mode:");
            ImGui::Separator();
            
            bool spiceSelected = (currentMode == DisplayMode::SPICE_MAP);
            if (ImGui::RadioButton("Spice Map", spiceSelected)) {
                currentMode = DisplayMode::SPICE_MAP;
                applyMode(generation, mapRenderer);
            }
            
            bool zoneSelected = (currentMode == DisplayMode::ZONE_MAP);
            if (ImGui::RadioButton("Zone Map", zoneSelected)) {
                currentMode = DisplayMode::ZONE_MAP;
                applyMode(generation, mapRenderer);
            }
            
            bool bordersSelected = (currentMode == DisplayMode::ZONE_BORDERS);
            if (ImGui::RadioButton("Zone Borders", bordersSelected)) {
                currentMode = DisplayMode::ZONE_BORDERS;
                applyMode(generation, mapRenderer);
            }
            
            bool influenceSelected = (currentMode == DisplayMode::INFLUENCE_MASK);
            if (ImGui::RadioButton("Influence Mask", influenceSelected)) {
                currentMode = DisplayMode::INFLUENCE_MASK;
                if (selectedZoneId.has_value()) {
                    applyMode(generation, mapRenderer);
                }
            }
            
            bool bonusSelected = (currentMode == DisplayMode::BONUS_MASK);
            if (ImGui::RadioButton("Bonus Mask", bonusSelected)) {
                currentMode = DisplayMode::BONUS_MASK;
                if (selectedZoneId.has_value()) {
                    applyMode(generation, mapRenderer);
                }
            }
            
            bool noiseSelected = (currentMode == DisplayMode::ZONE_NOISE);
            if (ImGui::RadioButton("Zone Noise", noiseSelected)) {
                currentMode = DisplayMode::ZONE_NOISE;
                if (selectedZoneId.has_value()) {
                    applyMode(generation, mapRenderer);
                }
            }
            
            ImGui::Separator();
            
            if (currentMode == DisplayMode::INFLUENCE_MASK || 
                currentMode == DisplayMode::BONUS_MASK || 
                currentMode == DisplayMode::ZONE_NOISE) {
                ImGui::Text("Select Zone:");
                ImGui::Separator();
                
                if (!templatePicker.mapTemplate || !templatePicker.mapTemplate->zoneGraph) {
                    ImGui::TextColored(ImVec4(1,0,0,1), "No template selected!");
                } else {
                    const auto& ids = templatePicker.mapTemplate->zoneGraph->getIDs();
                    
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 4));
                    
                    const int buttonsPerRow = 4;
                    float windowWidth = ImGui::GetContentRegionAvail().x;
                    float buttonSpacing = ImGui::GetStyle().ItemSpacing.x;
                    float buttonWidth = (windowWidth - (buttonsPerRow - 1) * buttonSpacing) / buttonsPerRow;
                    
                    for (size_t i = 0; i < ids.size(); i++) {
                        int id = ids[i].getID();
                        bool isSelected = selectedZoneId.has_value() && id == selectedZoneId.value();
                        
                        if (isSelected) {
                            ImGui::PushStyleColor(ImGuiCol_Button, VARIANT_BASIC);
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, VARIANT_EXTRA);
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, VARIANT_ULTRA);
                        }
                        
                        if (ImGui::Button(std::to_string(id).c_str(), ImVec2(buttonWidth, 32))) {
                            selectedZoneId = id;
                            if (currentMode == DisplayMode::INFLUENCE_MASK || 
                                currentMode == DisplayMode::BONUS_MASK ||
                                currentMode == DisplayMode::ZONE_NOISE) {
                                applyMode(generation, mapRenderer);
                            }
                        }
                        
                        if (isSelected) {
                            ImGui::PopStyleColor(3);
                        }
                        
                        if ((i + 1) % buttonsPerRow != 0 && i < ids.size() - 1) {
                            ImGui::SameLine();
                        }
                    }
                    
                    ImGui::PopStyleVar(2);
                }
            }
            
            ImGui::Separator();
            ImGui::TextUnformatted("Press F1 to close");
        }
        ImGui::End();
    }
    
    void onGenerationComplete(Generation& generation, MapRenderer& mapRenderer) {
        applyMode(generation, mapRenderer);
    }
    
    bool shouldOverrideRender() const {
        return showDevPanel;
    }
    
    DisplayMode getCurrentMode() const {
        return currentMode;
    }
    
    std::optional<int> getSelectedZoneId() const {
        return selectedZoneId;
    }
};
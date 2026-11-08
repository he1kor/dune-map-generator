#include "app_layout.h"
#include "colors.h"
#include "generation_ui.h"
#include "dev_panel_ui.h"
#include "imgui_internal.h"
#include "quick_settings_ui.h"
#include "setup.h"
#include "template_picker_ui.h"
#include "template_editor_ui.h"
#include "template_editor_descriptions.h"
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

static constexpr GLuint mapTexture = 42;
static std::string clipboard;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// Exercise the actual UI and renderer without opening a native window or using
// a GPU. Texture uploads are irrelevant to the ImGui layout being measured.
static void APIENTRY textures(GLsizei count, GLuint* names) {
    for (int i = 0; i < count; ++i) names[i] = mapTexture;
}
static void APIENTRY deleteTextures(GLsizei, const GLuint*) {}
static void APIENTRY bindTexture(GLenum, GLuint) {}
static void APIENTRY textureParameter(GLenum, GLenum, GLint) {}
static void APIENTRY textureImage(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) {}
static void sampler(const ImDrawList*, const ImDrawCmd*) {}

static std::vector<float> geometry() {
    const auto* root = ImGui::FindWindowByName("##Main Window");
    require(root && root->DC.ChildWindows.Size == 3, "The three panels do not have independent layout areas");
    require(root->ScrollMax.x == 0 && root->ScrollMax.y == 0, "Main window overflow changes the panel widths");
    const auto* settings = root->DC.ChildWindows[0];
    const auto* generation = root->DC.ChildWindows[1];
    const auto* files = root->DC.ChildWindows[2];
    require(settings->Pos.y == generation->Pos.y && generation->Pos.y == files->Pos.y,
        "Panel headings are not on the same row");
    require(generation->Pos.x >= settings->Pos.x + settings->Size.x
        && files->Pos.x >= generation->Pos.x + generation->Size.x, "Panels overlap");
    require(files->Pos.x + files->Size.x <= root->Pos.x + root->Size.x, "Files panel leaves the window");
    require(generation->ScrollMax.x == 0 && generation->ScrollMax.y == 0,
        "The map does not leave enough room for seed and progress controls");
    require(files->ScrollMax.x == 0 && files->ScrollMax.y == 0, "Generate button is outside the visible panel");
    std::vector<float> result;
    for (auto* panel : root->DC.ChildWindows) {
        result.insert(result.end(), {panel->Pos.x, panel->Pos.y, panel->Size.x, panel->Size.y});
    }
    bool foundMap = false;
    for (const auto* list : ImGui::GetDrawData()->CmdLists) {
        for (const auto& command : list->CmdBuffer) {
            if (command.UserCallback || command.ElemCount == 0 || command.GetTexID() != mapTexture) continue;
            ImVec2 minimum(1e9f, 1e9f), maximum(-1e9f, -1e9f);
            for (unsigned int i = 0; i < command.ElemCount; ++i) {
                const auto& point = list->VtxBuffer[list->IdxBuffer[command.IdxOffset + i] + command.VtxOffset].pos;
                minimum.x = std::min(minimum.x, point.x); minimum.y = std::min(minimum.y, point.y);
                maximum.x = std::max(maximum.x, point.x); maximum.y = std::max(maximum.y, point.y);
            }
            require(minimum.x >= generation->InnerRect.Min.x && maximum.x <= generation->InnerRect.Max.x
                && minimum.y >= generation->InnerRect.Min.y && maximum.y <= generation->InnerRect.Max.y,
                "Map preview is clipped");
            result.insert(result.end(), {minimum.x, minimum.y, maximum.x, maximum.y});
            foundMap = true;
        }
    }
    require(foundMap, "Map preview disappeared at a supported window size");
    return result;
}

static void savePreview(const std::string& prefix) {
    using Json = nlohmann::json;
    auto& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    Json output = {{"width", io.DisplaySize.x}, {"height", io.DisplaySize.y},
        {"textureWidth", width}, {"textureHeight", height}, {"lists", Json::array()}};
    for (const auto* list : ImGui::GetDrawData()->CmdLists) {
        Json vertices = Json::array(), indices = Json::array(), commands = Json::array();
        for (const auto& vertex : list->VtxBuffer)
            vertices.push_back({vertex.pos.x, vertex.pos.y, vertex.uv.x, vertex.uv.y, vertex.col});
        for (auto index : list->IdxBuffer) indices.push_back(index);
        for (const auto& command : list->CmdBuffer) {
            if (command.UserCallback) continue;
            commands.push_back({command.ClipRect.x, command.ClipRect.y, command.ClipRect.z, command.ClipRect.w,
                command.IdxOffset, command.ElemCount, command.VtxOffset, command.GetTexID()});
        }
        output["lists"].push_back({{"vertices", vertices}, {"indices", indices}, {"commands", commands}});
    }
    std::ofstream(prefix + ".json") << output.dump();
    std::ofstream texture(prefix + ".rgba", std::ios::binary);
    texture.write(reinterpret_cast<const char*>(pixels), width * height * 4);
}

int main(int argc, char** argv) {
    try {
        ImGui::CreateContext();
        ImGui::GetPlatformIO().Platform_GetClipboardTextFn = [](ImGuiContext*) { return clipboard.c_str(); };
        ImGui::GetPlatformIO().Platform_SetClipboardTextFn = [](ImGuiContext*, const char* text) { clipboard = text; };
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DeltaTime = 1.0f / 60.0f;
        ImGui::StyleColorsDark();
        if (std::filesystem::exists("C:\\Windows\\Fonts\\Verdana.ttf")) setFont();
        else {
            ImFontConfig config;
            config.SizePixels = 18.0f;
            io.Fonts->AddFontDefault(&config);
        }
        setColors(); setShape();
        require(ImGui::GetColorU32(ImGuiCol_Header) == ImGui::GetColorU32(RED_BASIC)
            && ImGui::GetColorU32(ImGuiCol_HeaderHovered) == ImGui::GetColorU32(RED_EXTRA)
            && ImGui::GetColorU32(ImGuiCol_HeaderActive) == ImGui::GetColorU32(RED_ULTRA),
            "Settings headers do not use the application palette");
        unsigned char* pixels;
        int atlasWidth, atlasHeight;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &atlasWidth, &atlasHeight);
        io.Fonts->SetTexID(static_cast<ImTextureID>(1));
        ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest = sampler;
        ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear = sampler;
        glad_glGenTextures = textures;
        glad_glDeleteTextures = deleteTextures;
        glad_glBindTexture = bindTexture;
        glad_glTexParameteri = textureParameter;
        glad_glTexImage2D = textureImage;
        {
            TemplateEditorDescriptions descriptions;
            const auto original = descriptions.parameter("Radius");
            require(descriptions.materialName("cliff_rock") == "Cliff rock"
                && descriptions.materialName("thick_spice") == "Thick spice", "Material names are not user friendly");
            const auto path = std::filesystem::current_path() / "ui-descriptions-regression.json";
            std::ofstream(path) << R"({"parameters":{"Radius":"Custom size description"},"materials":{"cliff_rock":"Mountain cliff"},"text":{"node_tooltip":"{id} uses 100% {radius}"},"help":[{"title":"Custom help","paragraphs":["Loaded from JSON."]}]})";
            require(descriptions.load(path) && descriptions.parameter("Radius") == "Custom size description",
                "Parameter descriptions are not data driven");
            require(descriptions.format("node_tooltip", {{"id", "{radius}"}, {"radius", "5"}}) == "{radius} uses 100% 5",
                "Description substitution interpreted user text or printf placeholders");
            require(descriptions.help()[0]["title"] == "Custom help", "Help sections did not load from data");
            require(descriptions.materialName("cliff_rock") == "Mountain cliff" && descriptions.materialName("sand") == "Sand"
                && descriptions.materialName("unknown_extension") == "unknown_extension", "Data-driven material names or fallback did not load");
            for (const auto* bad : {"{", R"({"parameters":{"Radius":42}})", R"({"text":null})", R"({"materials":{"rock":42}})", R"({"help":[{"title":"Missing paragraphs"}]})"}) {
                std::ofstream(path) << bad;
                require(!descriptions.load(path) && descriptions.parameter("Radius") == "Custom size description",
                    "Malformed descriptions replaced the last valid catalog");
            }
            std::filesystem::remove(path);
        }
        {
            QuickSettings settings;
            TemplatePicker picker;
            Generation generation;
            MapRenderer renderer(nullptr);
            renderer.updateMap(std::vector<uint32_t>(128 * 128, 0xff000000), 128, 128);
            QuickSettingsUI settingsUI(&settings);
            GenerationUI generationUI(&generation, &renderer);
            TemplatePickerUI filesUI(&picker, &generation, renderer, &settings);
            TemplateEditor editor;
            TemplateEditorUI editorUI(editor, picker, generation, settings);
            int cases = 0;
            for (int width : {static_cast<int>(appLayout::minWindowWidth), 662, 663, 672, 800, 927,
                static_cast<int>(appLayout::maxWindowWidth)}) {
                for (int height : {500, 730}) {
                    for (int shape : {0, 1, 2}) {
                        MapDimensionLimits limits;
                        if (shape != 0) {
                            limits.minWidth = limits.minHeight = 64;
                            limits.minAspectRatio = shape == 1 ? 1.0 : 0.5;
                            limits.maxAspectRatio = shape == 1 ? 1.0 : 2.0;
                        }
                        settings.applyDimensionLimits(limits);
                        for (bool expanded : {false, true}) {
                            std::vector<float> previous;
                            io.DisplaySize = ImVec2(width, height);
                            for (int frame = 0; frame < 12; ++frame) {
                                if (const auto* root = ImGui::FindWindowByName("##Main Window"); root && root->DC.ChildWindows.Size == 3) {
                                    auto* panel = root->DC.ChildWindows[0];
                                    panel->StateStorage.SetInt(ImHashStr("Other settings", 0, panel->ID), expanded);
                                }
                                ImGui::NewFrame();
                                appLayout::build(settingsUI, filesUI, generationUI, editorUI);
                                ImGui::Render();
                                if (frame < 3) continue;
                                const auto current = geometry();
                                if (!previous.empty()) require(current == previous, "Layout oscillates between frames");
                                previous = current;
                            }
                            if (argc > 1 && width == 663 && height == 500 && !expanded)
                                savePreview(std::string(argv[1]) + "-" + std::to_string(shape));
                            ++cases;
                        }
                    }
                }
            }
            // Large windows must use the largest fitting integer scale for
            // each actual map size, without sacrificing seed/progress controls.
            for (const ImVec2 size : {ImVec2(1920, 1080), ImVec2(2560, 1387), ImVec2(3840, 2160)}) {
                io.DisplaySize = size;
                for (const auto [mapWidth, mapHeight] : {std::pair{32, 32}, {64, 64}, {128, 128}, {64, 128}, {128, 64}}) {
                    renderer.updateMap(std::vector<uint32_t>(mapWidth * mapHeight, 0xff000000), mapWidth, mapHeight);
                    for (int frame = 0; frame < 4; ++frame) {
                        ImGui::NewFrame();
                        appLayout::build(settingsUI, filesUI, generationUI, editorUI);
                        ImGui::Render();
                    }
                    const auto bounds = geometry();
                    const float imageWidth = bounds[14] - bounds[12], imageHeight = bounds[15] - bounds[13];
                    const float scale = imageWidth / mapWidth;
                    require(scale == std::floor(scale) && imageHeight / mapHeight == scale,
                        "Map pixels are not scaled uniformly by a whole number");
                    require(std::max(imageWidth, imageHeight) > 512, "Fullscreen preview still has the old size cap");
                    const auto* panel = ImGui::FindWindowByName("##Main Window")->DC.ChildWindows[1];
                    const float spacing = ImGui::GetStyle().ItemSpacing.y;
                    const float controlsHeight = 5 + 2 * spacing + ImGui::GetTextLineHeightWithSpacing()
                        + 2 * ImGui::GetFrameHeightWithSpacing() + 30;
                    require(imageWidth + mapWidth > panel->WorkRect.GetWidth()
                        || bounds[15] + mapHeight > panel->WorkRect.Max.y - controlsHeight,
                        "Preview leaves space for another full integer scale step");
                    if (argc > 1 && size.x == 2560 && mapWidth == 64 && mapHeight == 64)
                        savePreview(std::string(argv[1]) + "-fullscreen");
                    ++cases;
                }
            }
            renderer.updateMap(std::vector<uint32_t>(128 * 128, 0xff000000), 128, 128);
            // Status labels must remain usable even in the narrowest map panel.
            io.DisplaySize = ImVec2(appLayout::minWindowWidth, appLayout::minWindowHeight);
            for (auto stage : {GenerationStage::EMBED, GenerationStage::ZONE_BLOAT, GenerationStage::POST_PROCESS,
                GenerationStage::BORDERS, GenerationStage::NOISE, GenerationStage::BLEND,
                GenerationStage::RESOURCES, GenerationStage::FINISH}) {
                generation.generationStage = stage;
                generation.generationProgress = 0.5f;
                require(ImGui::CalcTextSize(generation.statusText()).x <= 128.0f, "Status label does not fit the minimum panel width");
                for (int frame = 0; frame < 3; ++frame) {
                    ImGui::NewFrame();
                    appLayout::build(settingsUI, filesUI, generationUI, editorUI);
                    ImGui::Render();
                }
                geometry();
            }
            generation.generationStage = GenerationStage::NONE;
            generation.generationProgress = 0.0f;

            DevPanelUI devPanel;
            auto devFrame = [&] {
                ImGui::NewFrame();
                devPanel.handleInput();
                appLayout::build(settingsUI, filesUI, generationUI, editorUI);
                devPanel.render(generation, renderer, picker);
                ImGui::Render();
            };
            io.AddKeyEvent(ImGuiKey_F1, true);
            devFrame();
            io.AddKeyEvent(ImGuiKey_F1, false);
            devFrame();
            const auto* devWindow = ImGui::FindWindowByName("Dev Panel");
            require(devWindow && devWindow->Active, "F1 did not open the developer panel");
            io.AddMousePosEvent(devWindow->DC.CursorStartPos.x + ImGui::GetFrameHeight() * 0.5f,
                devWindow->DC.CursorStartPos.y + ImGui::GetFrameHeight() * 0.5f);
            devFrame();
            for (bool enabled : {true, false}) {
                io.AddMouseButtonEvent(0, true);
                devFrame();
                io.AddMouseButtonEvent(0, false);
                devFrame();
                require(generation.isAnimationEnabled() == enabled, "Animate checkbox did not update generation");
            }
            io.AddKeyEvent(ImGuiKey_F1, true); devFrame();
            io.AddKeyEvent(ImGuiKey_F1, false); devFrame();

            auto editorFrame = [&] {
                ImGui::NewFrame();
                appLayout::build(settingsUI, filesUI, generationUI, editorUI);
                ImGui::Render();
            };
            auto* root = ImGui::FindWindowByName("##Main Window");
            auto* tabs = GImGui->TabBars.GetByKey(root->GetID("Workspaces"));
            require(tabs && tabs->Tabs.Size == 2, "Editor tab is missing");
            tabs->NextSelectedTabId = tabs->Tabs[1].ID;
            for (int i = 0; i < 3; ++i) editorFrame();
            auto canvasWindow = [&]() -> ImGuiWindow* {
                for (auto* child : root->DC.ChildWindows)
                    if (std::string(child->Name).find("graph_canvas") != std::string::npos) return child;
                throw std::runtime_error("Graph canvas is missing");
            };
            for (int width : {544, 672, 928, 1440}) for (int height : {500, 900}) {
                io.DisplaySize = ImVec2(width, height);
                for (int i = 0; i < 4; ++i) editorFrame();
                auto* canvas = canvasWindow();
                require(root->ScrollMax.x == 0 && root->ScrollMax.y == 0, "Editor toolbar overflows window");
                require(canvas->Size.x > 400 && canvas->Size.y > 100, "Editor canvas collapsed");
                require(canvas->ScrollMax.x == 0 && canvas->ScrollMax.y == 0, "Canvas uses scrollbars instead of its camera");
            }
            io.DisplaySize = ImVec2(1024, 730);
            for (int i = 0; i < 4; ++i) editorFrame();
            const auto* canvas = canvasWindow();
            const ImVec2 firstPoint(canvas->DC.CursorStartPos.x + 100, canvas->DC.CursorStartPos.y + 80);
            const ImVec2 secondPoint(firstPoint.x + 260, firstPoint.y);
            auto click = [&](ImVec2 point) {
                io.AddMousePosEvent(point.x, point.y); editorFrame();
                io.AddMouseButtonEvent(0, true); editorFrame();
                io.AddMouseButtonEvent(0, false); editorFrame();
            };
            auto settleClicks = [&] { for (int i = 0; i < 24; ++i) editorFrame(); };
            auto doubleClick = [&](ImVec2 point) {
                settleClicks(); click(point); click(point);
                for (int i = 0; i < 5; ++i) editorFrame();
            };
            auto key = [&](ImGuiKey value, bool ctrl = false, bool shift = false) {
                if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, true);
                if (shift) io.AddKeyEvent(ImGuiMod_Shift, true);
                io.AddKeyEvent(value, true); editorFrame();
                io.AddKeyEvent(value, false); editorFrame();
                if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, false);
                if (shift) io.AddKeyEvent(ImGuiMod_Shift, false);
                editorFrame();
            };
            auto drag = [&](ImVec2 from, ImVec2 to, bool ctrl = false) {
                settleClicks();
                if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, true);
                io.AddMousePosEvent(from.x, from.y); editorFrame();
                io.AddMouseButtonEvent(0, true); editorFrame();
                io.AddMousePosEvent(to.x, to.y); editorFrame();
                io.AddMouseButtonEvent(0, false); editorFrame();
                if (ctrl) io.AddKeyEvent(ImGuiMod_Ctrl, false);
                editorFrame();
            };
            doubleClick(firstPoint);
            require(editor.document()["nodes"].size() == 1, "Double-click did not create a node");
            doubleClick(secondPoint);
            require(editor.document()["nodes"].size() == 2, "Second canvas node was not created");
            const int firstId = editor.document()["nodes"][0]["id"], secondId = editor.document()["nodes"][1]["id"];
            settleClicks();
            click(firstPoint);
            io.AddKeyEvent(ImGuiMod_Shift, true); editorFrame();
            click(secondPoint);
            io.AddKeyEvent(ImGuiMod_Shift, false); editorFrame();
            require(editor.document()["symmetrical_edges"].size() == 1, "Shift-click did not connect nodes");
            const auto beforeDrag = editor.position(firstId);
            settleClicks();
            io.AddMousePosEvent(firstPoint.x, firstPoint.y); editorFrame();
            io.AddMouseButtonEvent(0, true); editorFrame();
            io.AddMousePosEvent(firstPoint.x + 30, firstPoint.y + 50); editorFrame();
            io.AddMouseButtonEvent(0, false); editorFrame();
            require(editor.position(firstId) == TemplatePoint{beforeDrag.x + 30, beforeDrag.y + 50}, "Dragging did not move node");
            const auto beforeGroup = editor.document();
            const auto firstPosition = editor.position(firstId), secondPosition = editor.position(secondId);
            const ImVec2 movedFirst(firstPoint.x + 30, firstPoint.y + 50);
            // Marquee selection and movement must produce a single history step.
            drag({firstPoint.x - 60, firstPoint.y - 50}, {secondPoint.x + 60, firstPoint.y + 100});
            drag(movedFirst, {movedFirst.x + 40, movedFirst.y + 20});
            require(editor.position(firstId) == TemplatePoint{firstPosition.x + 40, firstPosition.y + 20}
                && editor.position(secondId) == TemplatePoint{secondPosition.x + 40, secondPosition.y + 20}, "Marquee group drag failed");
            const auto afterGroup = editor.document();
            key(ImGuiKey_Z, true);
            require(editor.document() == beforeGroup, "Undo did not restore a whole group move");
            key(ImGuiKey_Y, true);
            require(editor.document() == afterGroup, "Redo did not restore a group move");
            key(ImGuiKey_Z, true);
            require(ImGui::FindWindowByName("##editor_toast_0") && ImGui::FindWindowByName("##editor_toast_1"), "History notifications do not stack");
            auto drawOrder = [&](ImGuiWindow* window) {
                for (int i = 0; i < ImGui::GetDrawData()->CmdListsCount; ++i)
                    if (ImGui::GetDrawData()->CmdLists[i] == window->DrawList) return i;
                return -1;
            };
            require(drawOrder(ImGui::FindWindowByName("##editor_toast_0")) > drawOrder(root),
                "History notifications are hidden behind the fullscreen workspace");
            if (argc > 1) savePreview(std::string(argv[1]) + "-history");
            // Ctrl-click adds a zone; Ctrl-drag snaps the anchor without changing spacing.
            settleClicks();
            click(movedFirst);
            io.AddKeyEvent(ImGuiMod_Ctrl, true); editorFrame(); click(secondPoint);
            io.AddKeyEvent(ImGuiMod_Ctrl, false); editorFrame();
            drag(movedFirst, {movedFirst.x + 31, movedFirst.y + 11}, true);
            const auto snappedFirst = editor.position(firstId), snappedSecond = editor.position(secondId);
            const auto gridRemainder = [](float value) { return std::fmod(std::fmod(value, 64) + 64, 64); };
            require((gridRemainder(snappedFirst.x) == 0 && gridRemainder(snappedFirst.y) == 0)
                || (gridRemainder(snappedFirst.x) == 32 && gridRemainder(snappedFirst.y) == 32), "Ctrl-drag did not snap to cross or center");
            require(snappedSecond.x - snappedFirst.x == secondPosition.x - firstPosition.x
                && snappedSecond.y - snappedFirst.y == secondPosition.y - firstPosition.y, "Grid snapping changed group spacing");
            key(ImGuiKey_Z, true);
            require(editor.document() == beforeGroup, "Snapped group move is not atomic");
            const TemplatePoint centerTarget{std::round((firstPosition.x - 32) / 64) * 64 + 32,
                std::round((firstPosition.y - 32) / 64) * 64 + 32};
            drag(movedFirst, {movedFirst.x + centerTarget.x - firstPosition.x,
                movedFirst.y + centerTarget.y - firstPosition.y}, true);
            require(editor.position(firstId) == centerTarget, "Ctrl-drag cannot align to a grid cell center");
            key(ImGuiKey_Z, true);
            doubleClick(secondPoint);
            const auto* nodePopup = ImGui::FindWindowByName("Node settings");
            require(nodePopup && nodePopup->Active, "Double-click node did not open settings");
            require(nodePopup->Pos.y >= 0 && nodePopup->Pos.y + nodePopup->Size.y <= io.DisplaySize.y,
                "Settings footer is outside the window");
            if (argc > 1) {
                for (int i = 0; i < 5; ++i) editorFrame();
                savePreview(std::string(argv[1]) + "-node-menu");
            }
            const auto nodeFields = nodePopup->DC.ChildWindows[0]->DC.CursorStartPos;
            const auto beforeEdit = editor.document();
            // Enter a radius using actual text input, then apply the modal.
            const float inputHeight = ImGui::GetFontSize() + 10;
            const float fieldHeight = ImGui::GetFontSize() + 7 + inputHeight + 7;
            auto radiusPoint = [&] {
                const auto start = ImGui::FindWindowByName("Node settings")->DC.ChildWindows[0]->DC.CursorStartPos;
                return ImVec2(start.x + 60, start.y + fieldHeight + ImGui::GetFontSize() + 7 + inputHeight * 0.5f);
            };
            click(radiusPoint());
            key(ImGuiKey_A, true);
            io.AddInputCharactersUTF8("3"); editorFrame();
            require(editor.document() == beforeEdit, "Node modal mutated document before Apply");
            key(ImGuiKey_Z, true);
            key(ImGuiKey_Y, true);
            key(ImGuiKey_Enter);
            require(editor.node(secondId)->at("data").at("radius") == 3, "Applying node settings did not save edited radius");
            require(!editor.node(secondId)->at("data").at("resources").contains("octave1_weight"),
                "Displaying the disabled default weight inserted an override into the template");
            doubleClick(secondPoint);
            click(radiusPoint()); key(ImGuiKey_A, true);
            io.AddInputCharactersUTF8("-1"); editorFrame();
            key(ImGuiKey_Enter);
            require(ImGui::FindWindowByName("Node settings")->Active && editor.node(secondId)->at("data").at("radius") == 3,
                "Invalid settings were applied or closed the menu");
            key(ImGuiKey_Z, true);
            click(radiusPoint()); key(ImGuiKey_A, true);
            io.AddInputCharactersUTF8("5"); editorFrame();
            key(ImGuiKey_Escape);
            auto* confirm = ImGui::FindWindowByName("Discard settings changes?");
            require(confirm && confirm->Active, "Escape discarded edited settings without confirmation");
            key(ImGuiKey_Escape);
            require(ImGui::FindWindowByName("Node settings")->Active, "Keep editing closed settings");
            auto* modal = ImGui::FindWindowByName("Node settings");
            click({modal->Pos.x + modal->Size.x - 16 - 192 + 46, modal->DC.CursorMaxPos.y - inputHeight * 0.5f});
            require(ImGui::FindWindowByName("Discard settings changes?")->Active, "Cancel discarded edited settings without confirmation");
            key(ImGuiKey_Escape);
            modal = ImGui::FindWindowByName("Node settings");
            click({modal->Pos.x + modal->Size.x - 16 - inputHeight * 0.5f, modal->DC.CursorStartPos.y + inputHeight * 0.5f});
            require(ImGui::FindWindowByName("Discard settings changes?")->Active, "Close icon discarded settings without confirmation");
            confirm = ImGui::FindWindowByName("Discard settings changes?");
            const float keepWidth = ImGui::CalcTextSize("Keep editing").x + 16;
            click({confirm->DC.CursorStartPos.x + keepWidth + 8 + 50, confirm->DC.CursorMaxPos.y - inputHeight * 0.5f});
            require(editor.node(secondId)->at("data").at("radius") == 3, "Discard committed settings changes");
            settleClicks();
            const ImVec2 midpoint((firstPoint.x + 30 + secondPoint.x) * 0.5f,
                (firstPoint.y + 50 + secondPoint.y) * 0.5f);
            doubleClick(midpoint);
            const auto* edgePopup = ImGui::FindWindowByName("Connection settings");
            require(edgePopup && edgePopup->Active, "Double-click edge did not open connection settings");
            if (argc > 1) {
                for (int i = 0; i < 5; ++i) editorFrame();
                savePreview(std::string(argv[1]) + "-edge-menu");
            }
            // Cancel the modal; its controls must not mutate the editor draft.
            key(ImGuiKey_Escape);
            // Copy/paste changes selected data, preserves graph/layout, and is
            // one undo step through the actual keyboard shortcut route.
            settleClicks(); click(secondPoint);
            key(ImGuiKey_C, true);
            require(nlohmann::json::parse(clipboard)["kind"] == "node", "Ctrl+C did not copy node data");
            settleClicks(); click(movedFirst);
            const auto beforePaste = editor.document();
            key(ImGuiKey_V, true);
            require(editor.node(firstId)->at("data") == editor.node(secondId)->at("data"), "Ctrl+V did not paste node settings");
            require(editor.position(firstId) == firstPosition
                && editor.document()["symmetrical_edges"] == beforePaste["symmetrical_edges"], "Keyboard paste changed position or connections");
            key(ImGuiKey_Z, true);
            require(editor.document() == beforePaste, "Keyboard paste undo did not restore the document");
            // Edge clipboard includes both directions and has its own type.
            settleClicks(); click(midpoint); key(ImGuiKey_C, true);
            require(nlohmann::json::parse(clipboard)["kind"] == "edge", "Ctrl+C did not copy connection data");
            key(ImGuiKey_V, true);
            require(editor.document() == beforePaste, "Pasting unchanged edge data changed the graph");
            const auto beforeCamera = editor.document();
            io.AddMousePosEvent(midpoint.x, midpoint.y + 100); editorFrame();
            io.AddMouseWheelEvent(0, 2); editorFrame();
            io.AddMouseButtonEvent(2, true); editorFrame();
            io.AddMousePosEvent(midpoint.x + 90, midpoint.y + 130); editorFrame();
            io.AddMouseButtonEvent(2, false); editorFrame();
            require(editor.document() == beforeCamera, "Camera movement changed template coordinates");
            require(editor.node(secondId), "UI interactions removed an unrelated node");
            // Exercise globals and applying an editor snapshot to generation.
            key(ImGuiKey_Comma, true);
            const auto* globals = ImGui::FindWindowByName("Global template settings");
            require(globals && globals->Active, "Global settings did not open");
            const auto globalFields = globals->DC.ChildWindows[0]->DC.CursorStartPos;
            click({globalFields.x + 60, globalFields.y + ImGui::GetFontSize() + 7 + inputHeight * 0.5f});
            key(ImGuiKey_A, true);
            io.AddInputCharactersUTF8("UI template"); editorFrame();
            key(ImGuiKey_A, true); key(ImGuiKey_C, true);
            require(clipboard == "UI template", "Graph shortcut intercepted native text copy in a settings field");
            key(ImGuiKey_V, true);
            click({globalFields.x + 14, globalFields.y + fieldHeight + inputHeight * 0.5f});
            key(ImGuiKey_Z, true); key(ImGuiKey_Y, true);
            key(ImGuiKey_Enter);
            require(editor.document()["name"] == "UI template", "Global setting edits were not applied");
            require(!editor.document().contains("walls"), "Draft undo/redo did not restore checkbox changes");
            key(ImGuiKey_Enter, true);
            require(picker.mapTemplate && picker.templateName == "UI template", "Use for generation did not select editor snapshot");
            // Notifications must not intercept the real Generate button. Only
            // their small X region should capture clicks; hover must not keep
            // them alive, and the final part of their lifetime must fade.
            io.DeltaTime = 5; editorFrame(); io.DeltaTime = 1.0f / 60; editorFrame();
            key(ImGuiKey_Enter, true); key(ImGuiKey_Enter, true);
            const auto dismissButtons = [&] {
                std::vector<ImGuiWindow*> windows;
                for (auto* window : GImGui->Windows)
                    if (window->Active && std::string(window->Name).starts_with("##editor_toast_close_")) windows.push_back(window);
                return windows;
            };
            require(dismissButtons().size() == 2, "Notifications do not each have a dismiss button");
            tabs->NextSelectedTabId = tabs->Tabs[0].ID;
            for (int i = 0; i < 3; ++i) editorFrame();
            const auto beforeDismiss = editor.document();
            click(dismissButtons().back()->Rect().GetCenter());
            editorFrame(); // The dismissed overlay disappears on the next redraw.
            if (argc > 1) savePreview(std::string(argv[1]) + "-dismiss");
            require(dismissButtons().size() == 1, "The notification X did not dismiss exactly one message");
            require(editor.document() == beforeDismiss, "Dismissing a notification changed the editor document");
            require(!generation.isRunning(), "The notification X activated Generate underneath it");
            const auto* filesPanel = root->DC.ChildWindows[2];
            const ImVec2 generatePoint(filesPanel->WorkRect.GetCenter().x, filesPanel->DC.CursorMaxPos.y - 15);
            const auto* coveringToast = ImGui::FindWindowByName("##editor_toast_0");
            require(coveringToast && coveringToast->Active && coveringToast->Rect().Contains(generatePoint),
                "Notification test does not cover Generate");
            click(generatePoint);
            require(generation.isRunning(), "Clicking Generate through a notification did not start generation");
            const auto generationDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (generation.isRunning() && std::chrono::steady_clock::now() < generationDeadline) {
                generation.poll(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            require(!generation.isRunning(), "Notification click-through generation did not finish");
            tabs->NextSelectedTabId = tabs->Tabs[1].ID;
            for (int i = 0; i < 3; ++i) editorFrame();
            key(ImGuiKey_Enter, true);
            auto maxToastAlpha = [&] {
                const auto* window = ImGui::FindWindowByName("##editor_toast_0");
                int alpha = 0;
                if (window && window->Active)
                    for (const auto& vertex : window->DrawList->VtxBuffer)
                        alpha = std::max(alpha, int((vertex.col >> IM_COL32_A_SHIFT) & 255));
                return alpha;
            };
            const int freshAlpha = maxToastAlpha();
            coveringToast = ImGui::FindWindowByName("##editor_toast_0");
            io.AddMousePosEvent(coveringToast->Pos.x + 30, coveringToast->Pos.y + 20);
            io.DeltaTime = 3.75f; editorFrame(); io.DeltaTime = 1.0f / 60;
            require(maxToastAlpha() > 0 && maxToastAlpha() < freshAlpha, "Notifications do not fade before expiring");
            io.DeltaTime = 0.5f; editorFrame(); io.DeltaTime = 1.0f / 60;
            require(dismissButtons().empty() && !ImGui::FindWindowByName("##editor_toast_0")->Active,
                "Notifications survive their four-second lifetime while hovered");
            const auto savedPath = std::filesystem::current_path() / "editor-shortcuts-regression.json";
            require(editor.save(savedPath), "Cannot prepare keyboard save test");
            editor.setPosition(firstId, {13, 27});
            key(ImGuiKey_S, true);
            require(!editor.dirty(), "Ctrl+S did not save");
            nlohmann::json saved;
            std::ifstream(savedPath) >> saved;
            require(saved == editor.document(), "Ctrl+S saved stale values");
            key(ImGuiKey_N, true);
            require(editor.document()["nodes"].empty(), "Ctrl+N did not create new template");
            key(ImGuiKey_Z, true);
            require(editor.document() == saved, "Undo new template lost document");
            key(ImGuiKey_Z, true, true);
            require(editor.document()["nodes"].empty(), "Ctrl+Shift+Z did not redo");
            key(ImGuiKey_Z, true);
            key(ImGuiKey_F2);
            require(ImGui::FindWindowByName("Template editor help")->Active, "Help shortcut failed");
            if (argc > 1) {
                for (int i = 0; i < 5; ++i) editorFrame();
                savePreview(std::string(argv[1]) + "-help");
            }
            key(ImGuiKey_Escape);
            require(ImGui::GetStyle().Colors[ImGuiCol_ModalWindowDimBg].w == 0, "Modal background is opaque");
            std::filesystem::remove(savedPath);

            const auto beforeLayoutChecks = editor.document();
            TemplateEditor nonPlanar;
            for (int id = 0; id < 6; ++id) nonPlanar.addNode({0, 0});
            for (int a = 1; a <= 3; ++a) for (int b = 4; b <= 6; ++b) nonPlanar.connect(a, b);
            require(editor.replace(nonPlanar.document()), "Cannot prepare non-planar editor draft");
            auto loggedFrame = [&] {
                ImGui::NewFrame();
                ImGui::LogToBuffer();
                appLayout::build(settingsUI, filesUI, generationUI, editorUI);
                const std::string text = GImGui->LogBuffer.c_str();
                ImGui::LogFinish(); ImGui::Render();
                return text;
            };
            require(loggedFrame().find('!') != std::string::npos, "Non-planar graph does not display the top menu warning icon");
            const auto badGraph = editor.document();
            const ImGuiID autoPlaceButton = ImHashStr("Auto place", 0, tabs->Tabs[1].ID);
            ImGui::ActivateItemByID(autoPlaceButton); editorFrame();
            require(editor.document() == badGraph, "Disabled Auto place button changed a non-planar draft");
            ImGuiWindow* commands = nullptr;
            for (auto* child : root->DC.ChildWindows)
                if (std::string(child->Name).find("editor_commands") != std::string::npos) commands = child;
            require(commands != nullptr, "Top menu is missing");
            const ImGuiID menuBar = ImHashStr("##MenuBar", 0, commands->ID);
            ImGui::ActivateItemByID(ImHashStr("!##topology_warning", 0, menuBar));
            for (int i = 0; i < 4; ++i) editorFrame();
            require(!GImGui->OpenPopupStack.empty(), "Topology warning icon did not open its explanation");
            require(drawOrder(GImGui->OpenPopupStack.back().Window) > drawOrder(ImGui::FindWindowByName("##editor_toast_0")),
                "History notifications cover the topology explanation");
            if (argc > 1) savePreview(std::string(argv[1]) + "-topology-warning");
            key(ImGuiKey_Escape);
            editor.removeConnection(0);
            const auto planarGraph = editor.document();
            require(loggedFrame().find('!') == std::string::npos, "Topology warning icon persisted after removing an obstruction");
            io.DeltaTime = 13; editorFrame(); io.DeltaTime = 1.0f / 60;
            ImGui::ActivateItemByID(autoPlaceButton);
            for (int i = 0; i < 4; ++i) editorFrame();
            require(editor.document() != planarGraph && editor.topologyWarning().empty(), "Auto place button did not arrange a planar graph");
            if (argc > 1) savePreview(std::string(argv[1]) + "-auto-place");
            key(ImGuiKey_Z, true);
            require(editor.document() == planarGraph, "UI auto placement is not one undo step");
            require(editor.replace(beforeLayoutChecks), "Cannot restore editor after layout checks");
            if (argc > 1) {
                auto settingsPreview = editor.document();
                settingsPreview["walls"] = {{"resource", "cliff_rock"}};
                settingsPreview["octaves"]["octave2"] = {{"minRadius", 4}, {"maxRadius", 6}};
                settingsPreview["octaves"]["octave3"] = {{"minRadius", 6}, {"maxRadius", 9}};
                settingsPreview["dimensions"] = {{"width", {{"min", 32}, {"max", 128}}},
                    {"height", {{"min", 32}, {"max", 128}}}, {"aspectRatio", {{"min", 0.25}, {"max", 4}}}};
                require(editor.replace(settingsPreview), "Cannot prepare global settings preview");
                key(ImGuiKey_Comma, true);
                for (int i = 0; i < 4; ++i) editorFrame();
                savePreview(std::string(argv[1]) + "-octave-dividers");
                auto* fields = ImGui::FindWindowByName("Global template settings")->DC.ChildWindows[0];
                ImGui::SetScrollY(fields, fields->ScrollMax.y);
                for (int i = 0; i < 4; ++i) editorFrame();
                savePreview(std::string(argv[1]) + "-inline-fields");
                key(ImGuiKey_Escape);
                require(editor.replace(beforeLayoutChecks), "Cannot restore editor after settings preview");
            }
            if (argc > 1) {
                const auto source = std::filesystem::path(__FILE__).parent_path().parent_path();
                require(editor.load(source / "assets/templates/habbanya erg true 2.json"), "Cannot load editor preview");
                const auto origin = canvasWindow()->DC.CursorStartPos;
                click({origin.x + 20, origin.y + 20});
                io.AddKeyEvent(ImGuiKey_F, true); editorFrame();
                io.AddKeyEvent(ImGuiKey_F, false); editorFrame();
                io.AddMousePosEvent(-100, -100);
                for (int i = 0; i < 3; ++i) editorFrame();
                savePreview(std::string(argv[1]) + "-editor");
                io.DisplaySize = ImVec2(2560, 1387);
                for (int i = 0; i < 4; ++i) editorFrame();
                const ImGuiID autoPlace = ImHashStr("Auto place", 0, tabs->Tabs[1].ID);
                ImGui::ActivateItemByID(autoPlace);
                for (int i = 0; i < 4; ++i) editorFrame();
                io.DeltaTime = 13; editorFrame(); io.DeltaTime = 1.0f / 60;
                editorFrame();
                savePreview(std::string(argv[1]) + "-compact-diagram");
                editor.setPosition(1, {1, 1});
            }
            const int lastId = editor.document()["nodes"][0]["id"];
            const auto lastPosition = editor.position(lastId);
            editor.setPosition(lastId, {lastPosition.x + 1, lastPosition.y});
            editorUI.requestClose(); editorFrame();
            const auto* unsaved = ImGui::FindWindowByName("Unsaved template");
            require(unsaved && unsaved->Active && !editorUI.closeApproved(), "Closing loses unsaved template changes");
            std::cout << cases << " full-window scenarios remained stable across consecutive frames\n"
                << "Window sizes, template limits, status labels, F1 Animate and editor canvas interactions passed\n";
        }
        ImGui::DestroyContext();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

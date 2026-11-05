#include "app_layout.h"
#include "colors.h"
#include "generation_ui.h"
#include "dev_panel_ui.h"
#include "imgui_internal.h"
#include "quick_settings_ui.h"
#include "setup.h"
#include "template_picker_ui.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

static constexpr GLuint mapTexture = 42;

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
            QuickSettings settings;
            TemplatePicker picker;
            Generation generation;
            MapRenderer renderer(nullptr);
            renderer.updateMap(std::vector<uint32_t>(128 * 128, 0xff000000), 128, 128);
            QuickSettingsUI settingsUI(&settings);
            GenerationUI generationUI(&generation, &renderer);
            TemplatePickerUI filesUI(&picker, &generation, renderer, &settings);
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
                                appLayout::build(settingsUI, filesUI, generationUI);
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
                    appLayout::build(settingsUI, filesUI, generationUI);
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
                appLayout::build(settingsUI, filesUI, generationUI);
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
            std::cout << cases << " full-window scenarios remained stable across consecutive frames\n"
                << "Window sizes, template limits, generation status labels and F1 Animate checkbox passed\n";
        }
        ImGui::DestroyContext();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

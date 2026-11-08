#include "imgui.h"
#include <glad/glad.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include "quick_settings.h"
#include "quick_settings_ui.h"
#include "app_layout.h"
#include "setup.h"
#include "template_picker_ui.h"
#include "template_editor_ui.h"
#include "generation.h"
#include "generation_ui.h"
#include "icon.h"
#include "map_render.h"
#include "tileset_properties.h"
#include "tileset_json_i.h"
#include "map_bin_file_io.h"
#include <palette.h>


#include "test.h"
#include "plane.h"
#include "graph.h"
#include "embedding.h"
#include "edge_graph.h"
#include <dev_panel_ui.h>

int main(){

    if (!glfwInit())
        return -1;

    GLFWwindow* window = glfwCreateWindow(672, 500, "Dune 2000 Map Generator", nullptr, nullptr);
    glfwSetWindowSizeLimits(window, appLayout::minWindowWidth, appLayout::minWindowHeight,
        GLFW_DONT_CARE, GLFW_DONT_CARE);
    if (!window){
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return -1;
    }

    GLFWimage gl_icon;
    gl_icon.width = ICON_WIDTH;
    gl_icon.height = ICON_WIDTH;
    gl_icon.pixels = icon;
    glfwSetWindowIcon(window, 1, &gl_icon);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui::StyleColorsDark();
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    setFont();
    setColors();
    setShape();

    MapBinFileIO map_bin_file;
    //map_bin_file.open("test.map");
    //auto map = SmartMap::fromMap(map_bin_file.load());

    TilesetProperties tilesetProperties = load("tileset.json");

    MapRenderer mapRenderer(&tilesetProperties.palette);
    
    QuickSettings quickSettings;
    QuickSettingsUI quickSettingsUI(&quickSettings);
    
    TemplatePicker templatePicker;
    Generation generation;
    TemplatePickerUI templatePickerUI(&templatePicker, &generation, mapRenderer, &quickSettings);


    GenerationUI generationUI(&generation, &mapRenderer);
    TemplateEditor templateEditor;
    TemplateEditorUI templateEditorUI(templateEditor, templatePicker, generation, quickSettings);
    
    DevPanelUI devPanelUI;

    //mapRenderer.updateMap(mapRenderer.convertMap(map), map.width(), map.height());
    mapRenderer.updateMap(mapRenderer.convertPlane(generation.plane, {}), generation.plane.getWidth(), generation.plane.getHeight());

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        if (glfwWindowShouldClose(window)) {
            if (templateEditorUI.requestClose()) break;
            glfwSetWindowShouldClose(window, GLFW_FALSE);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        devPanelUI.handleInput();

        const bool wasRunning = generation.isRunning();
        generation.poll();
        if (wasRunning && !generation.isRunning() && !generation.generationError.empty() && generation.mapTemplate) {
            mapRenderer.setDistinctColors(*generation.mapTemplate->zoneGraph);
            devPanelUI.onGenerationComplete(generation, mapRenderer);
        }
        if (auto preview = generation.takePreview(); preview && generation.isAnimationEnabled())
            mapRenderer.updateMap(mapRenderer.convertPreview(*preview), preview->width, preview->height);
        if (generation.is_done){
            generation.is_done = false;

            if (devPanelUI.shouldOverrideRender()) {
                devPanelUI.onGenerationComplete(generation, mapRenderer);
            } else {
                mapRenderer.updateMap(
                    mapRenderer.convertResourceMatrix(generation.spiceMap), 
                    generation.noiseMap.getWidth(), 
                    generation.noiseMap.getHeight()
                );
            }
        }


        appLayout::build(quickSettingsUI, templatePickerUI, generationUI, templateEditorUI);
        devPanelUI.render(generation, mapRenderer, templatePicker);

        // Рендеринг
        //ImGui::ShowStyleEditor();
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        if (templateEditorUI.closeApproved()) break;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

#include "imgui.h"
#include <glad/glad.h>
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include "quick_settings.h"
#include "quick_settings_ui.h"
#include "setup.h"
#include "template_picker_ui.h"
#include "generation.h"
#include "generation_ui.h"
#include "icon.h"
#include "map_render.h"

constexpr float left_column_width = 200.0f;
constexpr float min_map_size = 128.0f;
constexpr float min_middle_column_width = min_map_size + 16;
constexpr float max_middle_column_width = min_map_size * 4 + 16;
constexpr float right_column_width = 200.0f;

constexpr float min_window_width = left_column_width + min_middle_column_width + right_column_width;
constexpr float max_window_width = left_column_width + max_middle_column_width + right_column_width;

constexpr float min_window_height = 500.0f;
constexpr float max_window_height = 800.0f;

void buildLayout(QuickSettingsUI& quickSettingsUI, TemplatePickerUI& templatePickerUI, GenerationUI& generationUI){


    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    
    ImGui::Begin("##Main Window", nullptr, 
        ImGuiWindowFlags_AlwaysAutoResize | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse
    );

    ImGui::Columns(3, "MyColumns", false);
    
    ImGui::SetColumnWidth(0, left_column_width);
    
    quickSettingsUI.build();
    ImGui::NextColumn();
    
    ImGui::SetColumnWidth(1, ImGui::GetMainViewport()->Size.x - left_column_width - right_column_width);
    generationUI.build();
    
    ImGui::NextColumn();

    ImGui::SetColumnWidth(2, right_column_width);
    templatePickerUI.build();
    
    ImGui::Columns(1);
    ImGui::End();
}

int main(){
    if (!glfwInit())
        return -1;

    GLFWwindow* window = glfwCreateWindow(672, 500, "Dune 2000 Map Generator", nullptr, nullptr);
    glfwSetWindowSizeLimits(window, min_window_width, min_window_height, max_window_width, max_window_height);
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

    MapRenderer mapRenderer;

    QuickSettings quickSettings;
    QuickSettingsUI quickSettingsUI(&quickSettings);

    TemplatePicker templatePicker;
    TemplatePickerUI templatePickerUI(&templatePicker);

    Generation generation;
    GenerationUI generationUI(&generation, &mapRenderer);
    



    int width = 64;
    int height = 64;


    std::vector<uint32_t> mapPixels(height * width);
    // Example: Set some tiles (replace with your actual tile mapping)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            mapPixels[y * width + x] = 0xFF00FFFF;
            if (x + y < width)
                mapPixels[y * width + x] = 0xFF0033FF;
        }
    }

    mapRenderer.updateMap(mapPixels, width, height);
    
    
    
    
    
    
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.ChildRounding = 1.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;
    
    
    
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        buildLayout(quickSettingsUI, templatePickerUI, generationUI);

        // Рендеринг
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
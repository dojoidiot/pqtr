// DESK - Desktop GUI for LABS
// Bare application scaffold

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>

// Layout constants
static const float LEFT_PANEL_WIDTH = 250.0f;   // Project files
static const float RIGHT_PANEL_WIDTH = 300.0f;  // Tune controls
static const float DIALS_PANEL_HEIGHT = 200.0f; // Bottom dials

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    // Setup GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        return 1;
    }

    // OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "DESK", nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Get window dimensions
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        // Menu bar height
        float menu_bar_height = 0.0f;

        // Main Menu Bar
        if (ImGui::BeginMainMenuBar()) {
            menu_bar_height = ImGui::GetWindowSize().y;

            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Project...", "Ctrl+N")) {
                    // TODO
                }
                if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {
                    // TODO
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Ctrl+Q")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        // Calculate layout dimensions
        float content_height = display_h - menu_bar_height;
        float center_width = display_w - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;
        float image_height = content_height - DIALS_PANEL_HEIGHT;

        // === LEFT PANEL: Project Files ===
        ImGui::SetNextWindowPos(ImVec2(0, menu_bar_height));
        ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, content_height));
        ImGui::Begin("##ProjectPanel", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Project Files");
        ImGui::Separator();

        ImGui::End();

        // === CENTER TOP: Image Viewer ===
        ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH, menu_bar_height));
        ImGui::SetNextWindowSize(ImVec2(center_width, image_height));
        ImGui::Begin("##ImagePanel", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Image");
        ImGui::Separator();

        ImGui::End();

        // === CENTER BOTTOM: Dials Panel ===
        ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH, menu_bar_height + image_height));
        ImGui::SetNextWindowSize(ImVec2(center_width, DIALS_PANEL_HEIGHT));
        ImGui::Begin("##DialsPanel", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Dials");
        ImGui::Separator();

        ImGui::End();

        // === RIGHT PANEL: Tune Controls ===
        ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH + center_width, menu_bar_height));
        ImGui::SetNextWindowSize(ImVec2(RIGHT_PANEL_WIDTH, content_height));
        ImGui::Begin("##TunePanel", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);

        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Tune");
        ImGui::Separator();

        ImGui::End();

        // Render
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

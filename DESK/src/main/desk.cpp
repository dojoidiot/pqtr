// DESK - Desktop GUI for LABS
// Project management interface for RAW image processing

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <string>
#include <filesystem>

#include "part/state.hpp"
#include "part/files.hpp"
#include "part/projects.hpp"
#include "part/workarea.hpp"
#include "part/linkeditor.hpp"

// Layout constants
static const float LEFT_PANEL_WIDTH = 250.0f;   // Projects panel
static const float RIGHT_PANEL_WIDTH = 300.0f;  // Link editor

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char** argv) {
    // Parse command line for root folder
    // Default to "var/" for development
    std::string initial_root = "var";
    if (argc > 1) {
        initial_root = argv[1];
    }

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
    GLFWwindow* window = glfwCreateWindow(1280, 720, "DESK - LABS Project Manager", nullptr, nullptr);
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

    // Application state
    desk::State state;

    // Set initial root folder from command line
    namespace fs = std::filesystem;
    fs::path root_path = fs::absolute(initial_root);
    if (fs::exists(root_path) && fs::is_directory(root_path)) {
        state.root_folder = root_path;
        state.root_folder_set = true;
        desk::scan_projects(state);
    }

    // Track previous selection for image loading
    int prev_project = -1;

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
                if (ImGui::MenuItem("Select Root Folder...", "Ctrl+O")) {
                    desk::open_folder_dialog();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Refresh", "F5")) {
                    desk::scan_projects(state);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Ctrl+Q")) {
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                }
                ImGui::EndMenu();
            }

            // Status on right side of menu bar
            if (!state.status_message.empty()) {
                float status_width = ImGui::CalcTextSize(state.status_message.c_str()).x;
                ImGui::SameLine(display_w - status_width - 20);
                ImGui::TextDisabled("%s", state.status_message.c_str());
            }

            ImGui::EndMainMenuBar();
        }

        // Calculate layout dimensions
        float content_height = display_h - menu_bar_height;
        float center_width = display_w - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;

        // === LEFT PANEL: Projects ===
        ImGui::SetNextWindowPos(ImVec2(0, menu_bar_height));
        ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, content_height));
        ImGui::Begin("##ProjectPanel", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);

        bool selection_changed = desk::render_projects_panel(state);

        ImGui::End();

        // === CENTER: Work Area (Image) ===
        ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH, menu_bar_height));
        ImGui::SetNextWindowSize(ImVec2(center_width, content_height));
        ImGui::Begin("##WorkArea", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);

        desk::render_work_area(state);

        ImGui::End();

        // === RIGHT PANEL: Link Editor ===
        ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH + center_width, menu_bar_height));
        ImGui::SetNextWindowSize(ImVec2(RIGHT_PANEL_WIDTH, content_height));
        ImGui::Begin("##LinkEditor", nullptr,
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar);

        desk::render_link_editor(state);

        ImGui::End();

        // === File Dialogs ===

        // Folder selection dialog
        if (ImGuiFileDialog::Instance()->Display("ChooseFolderDlg",
            ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                state.root_folder = ImGuiFileDialog::Instance()->GetCurrentPath();
                state.root_folder_set = true;
                desk::scan_projects(state);
            }
            ImGuiFileDialog::Instance()->Close();
        }

        // RAW file selection dialog
        if (ImGuiFileDialog::Instance()->Display("ChooseRawDlg",
            ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                desk::create_project(state, path);
            }
            ImGuiFileDialog::Instance()->Close();
        }

        // Load image when project selection changes
        if (selection_changed || state.selected_project != prev_project) {
            prev_project = state.selected_project;
            if (state.selected_project >= 0 && state.selected_project < (int)state.projects.size()) {
                const auto& proj = state.projects[state.selected_project];

                // Render if PNG missing or reprocess requested
                if (!fs::exists(proj.png_path) || state.needs_reprocess) {
                    desk::render_project(state, proj);
                    state.needs_reprocess = false;
                }

                desk::load_texture(state, proj.png_path);
            } else {
                desk::unload_texture(state);
            }
        }

        // Render
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    desk::unload_texture(state);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

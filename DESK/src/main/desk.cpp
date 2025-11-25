// desk.cpp - DESK application entry point
// Project management interface for LABS

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "ImGuiFileDialog.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <string>
#include <filesystem>

#include "part/state.hpp"
#include "part/theme.hpp"
#include "part/files.hpp"
#include "part/projects.hpp"
#include "part/workarea.hpp"
#include "part/linkeditor.hpp"

namespace {

// ============================================================
// GLFW Error Callback
// ============================================================

void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// ============================================================
// Menu Bar Rendering
// ============================================================

void render_menu_bar(desk::State& state, int display_w) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(display_w), desk::MENU_BAR_HEIGHT));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 0));

    ImGui::Begin("##MenuBar", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar);

    // Project buttons (always enabled)
    if (ImGui::Button("New Project")) {
        desk::open_raw_file_dialog(state);
    }
    ImGui::SameLine();
    if (ImGui::Button("Open Folder")) {
        desk::open_folder_dialog();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Panel toggle buttons (disabled until project folder is set)
    bool folder_set = state.project_folder_set;
    ImGui::BeginDisabled(!folder_set);

    if (ImGui::Button(state.panels.projects ? "Projects [x]" : "Projects [ ]")) {
        state.panels.projects = !state.panels.projects;
    }
    ImGui::SameLine();

    if (ImGui::Button(state.panels.info ? "Info [x]" : "Info [ ]")) {
        state.panels.info = !state.panels.info;
    }
    ImGui::SameLine();

    if (ImGui::Button(state.panels.link_editor ? "Editor [x]" : "Editor [ ]")) {
        state.panels.link_editor = !state.panels.link_editor;
    }

    ImGui::EndDisabled();

    // Status message on right side
    if (!state.status_message.empty()) {
        float status_width = ImGui::CalcTextSize(state.status_message.c_str()).x;
        ImGui::SameLine(display_w - status_width - 20);
        ImGui::TextDisabled("%s", state.status_message.c_str());
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

// ============================================================
// Image Background Rendering
// ============================================================

void render_image_background(desk::State& state, int display_w, int display_h) {
    float content_y = desk::MENU_BAR_HEIGHT;
    float content_h = display_h - desk::MENU_BAR_HEIGHT;

    ImGui::SetNextWindowPos(ImVec2(0, content_y));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(display_w), content_h));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

    ImGui::Begin("##ImageBackground", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    desk::render_work_area(state);

    ImGui::End();
    ImGui::PopStyleVar();
}

// ============================================================
// Floating Panels Rendering
// ============================================================

bool render_floating_panels(desk::State& state, int display_w, int display_h) {
    bool selection_changed = false;
    float content_y = desk::MENU_BAR_HEIGHT;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, desk::PANEL_ALPHA);

    // Projects Panel
    if (state.panels.projects) {
        ImGui::SetNextWindowPos(ImVec2(10, content_y + 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(desk::PROJECTS_PANEL_WIDTH, desk::PROJECTS_PANEL_HEIGHT),
            ImGuiCond_FirstUseEver);

        ImGui::Begin("Projects", &state.panels.projects, ImGuiWindowFlags_NoCollapse);
        selection_changed = desk::render_projects_panel(state);
        ImGui::End();
    }

    // Info Panel
    if (state.panels.info) {
        ImGui::SetNextWindowPos(
            ImVec2(10, content_y + desk::PROJECTS_PANEL_HEIGHT + 20),
            ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(desk::INFO_PANEL_WIDTH, desk::INFO_PANEL_HEIGHT),
            ImGuiCond_FirstUseEver);

        ImGui::Begin("RAW Info", &state.panels.info, ImGuiWindowFlags_NoCollapse);
        desk::render_info_panel(state);
        ImGui::End();
    }

    // Link Editor Panel
    if (state.panels.link_editor) {
        float editor_x = (display_w - desk::LINK_EDITOR_WIDTH) / 2.0f;
        float editor_y = display_h - desk::LINK_EDITOR_HEIGHT - 20;
        ImGui::SetNextWindowPos(ImVec2(editor_x, editor_y), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(
            ImVec2(desk::LINK_EDITOR_WIDTH, desk::LINK_EDITOR_HEIGHT),
            ImGuiCond_FirstUseEver);

        ImGui::Begin("Link Editor", &state.panels.link_editor, ImGuiWindowFlags_NoCollapse);
        desk::render_module_menus(state);
        ImGui::End();
    }

    ImGui::PopStyleVar();

    return selection_changed;
}

// ============================================================
// File Dialogs Processing
// ============================================================

void process_file_dialogs(desk::State& state) {
    namespace fs = std::filesystem;

    // Folder selection dialog (changes project_folder)
    if (ImGuiFileDialog::Instance()->Display("ChooseFolderDlg",
        ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {

        if (ImGuiFileDialog::Instance()->IsOk()) {
            state.project_folder = ImGuiFileDialog::Instance()->GetCurrentPath();
            state.project_folder_set = true;
            desk::scan_projects(state);
            if (!state.projects.empty()) {
                state.panels.projects = true;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // RAW file selection dialog
    if (ImGuiFileDialog::Instance()->Display("ChooseRawDlg",
        ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {

        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            desk::create_project(state, path);
            desk::scan_projects(state);  // Refresh project list
            state.panels.projects = true;
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

// ============================================================
// Project Selection Change Handler
// ============================================================

void handle_selection_change(desk::State& state, int& prev_project, bool selection_changed) {
    namespace fs = std::filesystem;

    if (!selection_changed && state.selection.project == prev_project) {
        return;
    }

    prev_project = state.selection.project;

    if (state.has_project()) {
        const auto& proj = state.current_project();

        // Load RAW metadata
        desk::load_raw_info(state, proj);

        // Render if PNG missing or reprocess requested
        if (!fs::exists(proj.png_path) || state.needs_reprocess) {
            desk::render_project(state, proj);
            state.needs_reprocess = false;
        }

        desk::load_texture(state, proj.png_path);
    } else {
        desk::unload_texture(state);
        state.raw_info.clear();
    }
}

} // anonymous namespace

// ============================================================
// Main Entry Point
// ============================================================

int main(int argc, char** argv) {
    namespace fs = std::filesystem;

    // Get executable directory for relative paths
    fs::path exe_path = fs::canonical("/proc/self/exe").parent_path();
    fs::path desk_root = exe_path.parent_path();  // DESK/
    fs::path labs_root = desk_root.parent_path() / "LABS";  // ../LABS/

    // Default paths
    fs::path project_folder = desk_root / "var";           // DESK/var/
    fs::path raw_source_folder = labs_root / "var" / "pics";  // LABS/var/pics/

    // Override project folder from command line if provided
    if (argc > 1) {
        project_folder = fs::absolute(argv[1]);
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
    glfwSwapInterval(1);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    desk::apply_theme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Application state
    desk::State state;

    // Set RAW source folder (where file dialog starts)
    state.raw_source_folder = raw_source_folder;

    // Set project folder (where projects are stored)
    if (fs::exists(project_folder) && fs::is_directory(project_folder)) {
        state.project_folder = project_folder;
        state.project_folder_set = true;
        desk::scan_projects(state);
    }

    int prev_project = -1;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        // Render UI components
        render_menu_bar(state, display_w);
        render_image_background(state, display_w, display_h);
        bool selection_changed = render_floating_panels(state, display_w, display_h);

        // Process dialogs and handle selection changes
        process_file_dialogs(state);
        handle_selection_change(state, prev_project, selection_changed);

        // Render frame
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

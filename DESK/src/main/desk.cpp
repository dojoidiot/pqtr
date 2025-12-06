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
#include "part/geos.hpp"

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

    // Workspace selector (dropdown of RAW files) - FIRST
    bool folder_set = state.project_folder_set;

    ImGui::Text("RAW:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);

    ImGui::BeginDisabled(!folder_set || state.projects.empty());
    const char* preview = state.has_project() ? state.current_project().name.c_str() : "Select...";
    if (ImGui::BeginCombo("##workspace", preview, ImGuiComboFlags_None)) {
        for (int i = 0; i < static_cast<int>(state.projects.size()); i++) {
            bool selected = (state.selection.project == i);
            if (ImGui::Selectable(state.projects[i].name.c_str(), selected)) {
                if (state.selection.project != i) {
                    state.selection.project = i;
                    state.selection.link = -1;
                    state.is_working = true;  // Show loading immediately
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    // Import RAW button
    if (ImGui::Button("Import RAW")) {
        desk::open_raw_file_dialog(state);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add a RAW file to workspace");
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Panel toggle buttons
    ImGui::BeginDisabled(!folder_set || !state.has_project());

    if (ImGui::Button(state.panels.pipe ? "Pipe [x]" : "Pipe [ ]")) {
        state.panels.pipe = !state.panels.pipe;
    }
    ImGui::SameLine();

    if (ImGui::Button(state.panels.info ? "Info [x]" : "Info [ ]")) {
        state.panels.info = !state.panels.info;
    }
    ImGui::SameLine();

    if (ImGui::Button(state.panels.link_editor ? "Editor [x]" : "Editor [ ]")) {
        state.panels.link_editor = !state.panels.link_editor;
    }

    // Embedded preview button (only enabled if RAW has embedded)
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.has_embedded);
    if (ImGui::Button(state.panels.embedded ? "Embedded [x]" : "Embedded [ ]")) {
        state.panels.embedded = !state.panels.embedded;
    }
    ImGui::EndDisabled();

    ImGui::EndDisabled();  // End folder_set disable

    // GeoS dome button (always available)
    ImGui::SameLine();
    if (ImGui::Button(state.panels.geos ? "GeoS [x]" : "GeoS [ ]")) {
        state.panels.geos = !state.panels.geos;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Geodesic Spectrum visualization");
    }

    // Separator
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Working size selector
    ImGui::Text("Preview:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);

    // Find current index
    int size_idx = 1;  // Default to 1024
    for (int i = 0; i < desk::State::WORKING_SIZE_COUNT; i++) {
        if (desk::State::WORKING_SIZES[i] == state.working_size) {
            size_idx = i;
            break;
        }
    }

    const char* size_labels[] = {"512", "1024", "2048", "4096", "Full"};
    if (ImGui::BeginCombo("##worksize", size_labels[size_idx], ImGuiComboFlags_NoArrowButton)) {
        for (int i = 0; i < desk::State::WORKING_SIZE_COUNT; i++) {
            bool selected = (i == size_idx);
            if (ImGui::Selectable(size_labels[i], selected)) {
                state.working_size = desk::State::WORKING_SIZES[i];
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Tune button (run optimizer to match camera preview)
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.has_project() || state.is_working || state.is_tuning || !state.has_embedded);
    if (ImGui::Button("Tune")) {
        desk::start_tune_async(state, state.current_project());
        state.panels.geos = true;  // Show geos panel for visualization
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
        if (!state.has_embedded) {
            ImGui::SetTooltip("No embedded preview available");
        } else if (state.is_tuning) {
            ImGui::SetTooltip("Tuning in progress...");
        } else {
            ImGui::SetTooltip("Match camera preview (create Base link)");
        }
    }

    // Export button (full resolution render)
    ImGui::SameLine();
    ImGui::BeginDisabled(!state.has_project() || state.is_working || state.is_tuning);
    if (ImGui::Button("Export")) {
        state.needs_export = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered() && !state.is_working) {
        ImGui::SetTooltip("Render at full resolution");
    }

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

// Margin from window edges
constexpr float PANEL_MARGIN = 10.0f;

// Panel sizes as fraction of window
constexpr float PIPE_WIDTH_FRAC = 0.15f;     // 15% of width
constexpr float PIPE_HEIGHT_FRAC = 0.45f;    // 45% of height
constexpr float INFO_WIDTH_FRAC = 0.15f;     // 15% of width
constexpr float INFO_HEIGHT_FRAC = 0.35f;    // 35% of height
constexpr float EDITOR_WIDTH_FRAC = 0.40f;   // 40% of width
constexpr float EDITOR_HEIGHT_FRAC = 0.35f;  // 35% of height
constexpr float EMBEDDED_WIDTH_FRAC = 0.26f; // 26% of width
constexpr float EMBEDDED_HEIGHT_FRAC = 0.455f;// 45.5% of height

bool render_floating_panels(desk::State& state, int display_w, int display_h) {
    bool selection_changed = false;
    float content_y = desk::MENU_BAR_HEIGHT;
    float content_h = display_h - content_y;

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, desk::PANEL_ALPHA);

    // Pipe Panel (Links/Modules/Dials tree) - TOP LEFT
    if (state.panels.pipe) {
        ImVec2 size(display_w * state.panel_sizes.pipe_w, content_h * state.panel_sizes.pipe_h);
        if (size.x < 150) size.x = 150;
        if (size.y < 150) size.y = 150;

        ImVec2 pos(PANEL_MARGIN, content_y + PANEL_MARGIN);

        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        ImGui::Begin("Pipe", &state.panels.pipe,
                     ImGuiWindowFlags_NoCollapse);
        selection_changed |= desk::render_pipe_panel(state);

        // Update fractions if user resized
        ImVec2 win_size = ImGui::GetWindowSize();
        state.panel_sizes.pipe_w = win_size.x / display_w;
        state.panel_sizes.pipe_h = win_size.y / content_h;

        ImGui::End();
    }

    // Info Panel - BOTTOM LEFT
    if (state.panels.info) {
        ImVec2 size(display_w * state.panel_sizes.info_w, content_h * state.panel_sizes.info_h);
        if (size.x < 150) size.x = 150;
        if (size.y < 100) size.y = 100;

        ImVec2 pos(PANEL_MARGIN, display_h - size.y - PANEL_MARGIN);

        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        ImGui::Begin("RAW Info", &state.panels.info,
                     ImGuiWindowFlags_NoCollapse);
        desk::render_info_panel(state);

        // Update fractions if user resized
        ImVec2 win_size = ImGui::GetWindowSize();
        state.panel_sizes.info_w = win_size.x / display_w;
        state.panel_sizes.info_h = win_size.y / content_h;

        ImGui::End();
    }

    // Link Editor Panel - BOTTOM RIGHT
    if (state.panels.link_editor) {
        std::string editor_title = "Link Editor";
        if (state.has_project() && state.selection.link >= 0) {
            const auto& proj = state.current_project();
            if (state.selection.link < static_cast<int>(proj.links.size())) {
                editor_title = proj.name + " > " + proj.links[state.selection.link].name;
            }
        }

        ImVec2 size(display_w * state.panel_sizes.editor_w, content_h * state.panel_sizes.editor_h);
        if (size.x < 300) size.x = 300;
        if (size.y < 150) size.y = 150;

        ImVec2 pos(display_w - size.x - PANEL_MARGIN, display_h - size.y - PANEL_MARGIN);

        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        ImGui::Begin(editor_title.c_str(), &state.panels.link_editor,
                     ImGuiWindowFlags_NoCollapse);
        selection_changed |= desk::render_module_menus(state);

        // Update fractions if user resized
        ImVec2 win_size = ImGui::GetWindowSize();
        state.panel_sizes.editor_w = win_size.x / display_w;
        state.panel_sizes.editor_h = win_size.y / content_h;

        ImGui::End();
    }

    // Embedded Preview Panel - TOP RIGHT
    if (state.panels.embedded && state.has_embedded && state.embedded_texture.loaded) {
        std::string emb_title = "Embedded Preview";
        if (state.has_project()) {
            emb_title = "Embedded: " + state.current_project().name;
        }

        ImVec2 size(display_w * state.panel_sizes.embedded_w, content_h * state.panel_sizes.embedded_h);
        if (size.x < 200) size.x = 200;
        if (size.y < 150) size.y = 150;

        ImVec2 pos(display_w - size.x - PANEL_MARGIN, content_y + PANEL_MARGIN);

        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        ImGui::Begin(emb_title.c_str(), &state.panels.embedded,
                     ImGuiWindowFlags_NoCollapse);

        // Display embedded image scaled to fit
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float img_aspect = static_cast<float>(state.embedded_texture.width) / state.embedded_texture.height;
        float win_aspect = avail.x / avail.y;

        float disp_w, disp_h;
        if (img_aspect > win_aspect) {
            disp_w = avail.x;
            disp_h = avail.x / img_aspect;
        } else {
            disp_h = avail.y;
            disp_w = avail.y * img_aspect;
        }

        // Center the image
        float offset_x = (avail.x - disp_w) * 0.5f;
        float offset_y = (avail.y - disp_h) * 0.5f;
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offset_x, ImGui::GetCursorPosY() + offset_y));

        ImGui::Image((ImTextureID)(intptr_t)state.embedded_texture.id, ImVec2(disp_w, disp_h));

        // Update fractions if user resized
        ImVec2 win_size = ImGui::GetWindowSize();
        state.panel_sizes.embedded_w = win_size.x / display_w;
        state.panel_sizes.embedded_h = win_size.y / content_h;

        ImGui::End();
    }

    // GeoS Dome Panel - CENTER RIGHT (between embedded and editor)
    if (state.panels.geos) {
        ImVec2 size(display_w * state.panel_sizes.geos_w, content_h * state.panel_sizes.geos_h);
        if (size.x < 200) size.x = 200;
        if (size.y < 250) size.y = 250;

        // Position: right side, centered vertically
        ImVec2 pos(display_w - size.x - PANEL_MARGIN,
                   content_y + (content_h - size.y) * 0.5f);

        ImGui::SetNextWindowPos(pos, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);

        ImGui::Begin("GeoS", &state.panels.geos,
                     ImGuiWindowFlags_NoCollapse);

        desk::render_geos_panel();

        // Update fractions if user resized
        ImVec2 win_size = ImGui::GetWindowSize();
        state.panel_sizes.geos_w = win_size.x / display_w;
        state.panel_sizes.geos_h = win_size.y / content_h;

        ImGui::End();
    }

    ImGui::PopStyleVar();

    // Working overlay (drawn outside panel alpha)
    if (state.is_working) {
        ImVec2 center(display_w * 0.5f, display_h * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(200, 60));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
        ImGui::Begin("##Working", nullptr,
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoCollapse);

        ImGui::SetCursorPosX((200 - ImGui::CalcTextSize("Working...").x) * 0.5f);
        ImGui::SetCursorPosY(20);
        ImGui::Text("Working...");

        ImGui::End();
        ImGui::PopStyleColor();
    }

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
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // RAW file selection dialog
    if (ImGuiFileDialog::Instance()->Display("ChooseRawDlg",
        ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {

        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            desk::create_project(state, path);
            // Get the name of the imported file
            std::string name = fs::path(path).stem().string();
            desk::scan_projects(state);  // Refresh project list
            // Select the newly imported project
            for (int i = 0; i < static_cast<int>(state.projects.size()); i++) {
                if (state.projects[i].name == name) {
                    state.selection.project = i;
                    state.selection.link = -1;
                    state.is_working = true;
                    break;
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Save link preset dialog
    if (ImGuiFileDialog::Instance()->Display("SaveLinkDlg",
        ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {

        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            if (state.has_project() && state.selection.link >= 0) {
                desk::Project& proj = state.current_project();
                if (state.selection.link < static_cast<int>(proj.links.size())) {
                    desk::save_link_json(proj.links[state.selection.link], path);
                    state.status_message = "Saved link preset";
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Load link preset dialog
    if (ImGuiFileDialog::Instance()->Display("LoadLinkDlg",
        ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {

        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
            if (state.has_project() && state.selection.link >= 0) {
                desk::Project& proj = state.current_project();
                if (state.selection.link < static_cast<int>(proj.links.size())) {
                    desk::load_link_json(proj.links[state.selection.link], path);
                    desk::save_pipe_json(proj);
                    state.needs_reprocess = true;
                    state.status_message = "Loaded link preset";
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}

// ============================================================
// Project Selection Change Handler
// ============================================================

void handle_selection_change(desk::State& state, int& prev_project, bool /*selection_changed*/) {
    // Only reload metadata when the PROJECT actually changes, not on any selection change
    bool project_changed = (state.selection.project != prev_project);

    // Handle full-size export request (saves PNG)
    if (state.needs_export && state.has_project()) {
        const auto& proj = state.current_project();
        desk::export_project(state, proj);
        state.needs_export = false;
    }

    // Handle reprocess request (e.g., slider released) - render to texture
    // Don't reprocess during tune - wait until tune is complete
    if (state.needs_reprocess && state.has_project() && !state.is_tuning) {
        const auto& proj = state.current_project();
        desk::render_to_texture(state, proj, state.working_size);
        state.needs_reprocess = false;
        state.is_working = false;
    }

    if (!project_changed) {
        return;
    }

    prev_project = state.selection.project;

    if (state.has_project()) {
        const auto& proj = state.current_project();

        // Load RAW metadata
        desk::load_raw_info(state, proj);

        // Load embedded preview (if available)
        desk::load_embedded_preview(state, proj);

        // Show all panels when project is selected
        state.panels.pipe = true;
        state.panels.info = true;
        state.panels.link_editor = true;
        state.panels.embedded = state.has_embedded;

        // Defer render to next frame so "Loading..." can display
        state.is_working = true;
        state.needs_reprocess = true;
    } else {
        desk::unload_texture(state);
        desk::unload_embedded_texture(state);
        state.raw_info.clear();

        // Hide panels when no project
        state.panels.pipe = false;
        state.panels.info = false;
        state.panels.link_editor = false;
        state.panels.embedded = false;
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

        // Poll for async tune completion
        desk::poll_tune_complete(state);

        // Handle Ctrl+Z for undo
        if (ImGui::IsKeyDown(ImGuiMod_Ctrl) && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            if (state.can_undo()) {
                desk::UndoEntry entry = state.pop_undo();
                desk::apply_undo(state, entry);
            }
        }

        // Render frame
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.3f, 0.3f, 0.3f, 1.0f);  // Neutral gray background
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    desk::unload_texture(state);
    desk::unload_base_texture(state);
    desk::unload_embedded_texture(state);
    desk::cleanup_geos();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

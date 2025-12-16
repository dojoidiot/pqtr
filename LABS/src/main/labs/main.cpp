// main.cpp - LABS WASM application entry point

#include "desk.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

// Global state
AppState g_state;

// Upload preview RGB data to OpenGL texture
void uploadPreviewTexture() {
    if (!g_state.preview_data || g_state.preview_width <= 0 || g_state.preview_height <= 0) {
        return;
    }

    if (g_state.preview_texture) {
        glDeleteTextures(1, &g_state.preview_texture);
        g_state.preview_texture = 0;
    }

    glGenTextures(1, &g_state.preview_texture);
    glBindTexture(GL_TEXTURE_2D, g_state.preview_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_state.preview_width, g_state.preview_height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, g_state.preview_data);

    glBindTexture(GL_TEXTURE_2D, 0);

    char msg[64];
    snprintf(msg, sizeof(msg), "Preview texture: %dx%d", g_state.preview_width, g_state.preview_height);
    LOG(msg);
}

// Run HEAD pipeline: blc → wb → demosaic → cst → crop
// Takes GEAR output, produces scene-linear RGB
void runHeadPipeline(pipe::Data& data) {
    // Build HEAD pipe
    auto head = pipe::make();
    head->link(pipe::blc());
    head->link(pipe::wb());
    head->link(pipe::demosaic());
    head->link(pipe::cst());
    head->link(pipe::crop());

    // Run HEAD pipeline
    data = head->flow(std::move(data));

    // Check for error
    if (data.info.text("error")[0] != '\0') {
        LOG(data.info.text("error").c_str());
        return;
    }

    // Extract RGB output
    struct RgbF32 { float* data; int width; int height; };
    auto* rgb = static_cast<RgbF32*>(data.page);
    if (!rgb || !rgb->data) {
        LOG("HEAD: no output data");
        return;
    }

    // Store in global state
    if (g_state.head_rgb) free(g_state.head_rgb);
    size_t count = static_cast<size_t>(rgb->width) * rgb->height * 3;
    g_state.head_rgb = (float*)malloc(count * sizeof(float));
    memcpy(g_state.head_rgb, rgb->data, count * sizeof(float));
    g_state.head_width = rgb->width;
    g_state.head_height = rgb->height;
    g_state.head_done = true;

    // Clean up pipe output
    delete[] rgb->data;
    delete rgb;
    data.page = nullptr;

    char msg[64];
    snprintf(msg, sizeof(msg), "HEAD done: %dx%d", g_state.head_width, g_state.head_height);
    LOG(msg);
}

// Convert HEAD scene-linear RGB to 8-bit and upload as texture
void createHeadTexture() {
    if (!g_state.head_rgb || g_state.head_width <= 0 || g_state.head_height <= 0) {
        return;
    }

    int w = g_state.head_width;
    int h = g_state.head_height;
    size_t count = static_cast<size_t>(w) * h;

    // Allocate 8-bit buffer
    if (g_state.head_rgb8) free(g_state.head_rgb8);
    g_state.head_rgb8 = (uint8_t*)malloc(count * 3);

    // Convert scene-linear [0,1+] to sRGB 8-bit
    // Simple gamma 2.2 for now (proper sRGB would use piecewise function)
    for (size_t i = 0; i < count * 3; i++) {
        float v = g_state.head_rgb[i];
        // Clamp and apply gamma
        if (v < 0) v = 0;
        if (v > 1) v = 1;
        v = powf(v, 1.0f / 2.2f);
        g_state.head_rgb8[i] = static_cast<uint8_t>(v * 255.0f + 0.5f);
    }

    // Upload to OpenGL texture
    if (g_state.head_texture) {
        glDeleteTextures(1, &g_state.head_texture);
        g_state.head_texture = 0;
    }

    glGenTextures(1, &g_state.head_texture);
    glBindTexture(GL_TEXTURE_2D, g_state.head_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, g_state.head_rgb8);
    glBindTexture(GL_TEXTURE_2D, 0);

    char msg[64];
    snprintf(msg, sizeof(msg), "HEAD texture: %dx%d", w, h);
    LOG(msg);
}

// Main loop
static void main_loop() {
    glfwPollEvents();

    int display_w, display_h;
    glfwGetFramebufferSize(g_state.window, &display_w, &display_h);
    if (display_w <= 0 || display_h <= 0) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    switch (g_state.screen) {
        case Screen::Login:
            render_login_screen();
            break;
        case Screen::OTP:
            render_otp_screen();
            break;
        case Screen::Desktop:
            render_desktop_screen();
            break;
    }

    ImGui::Render();

    glViewport(0, 0, display_w, display_h);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(g_state.window);
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

EMSCRIPTEN_KEEPALIVE
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    LOG("LABS: Starting...");

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        LOG("LABS: glfwInit failed!");
        return 1;
    }

#ifdef __EMSCRIPTEN__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    const char* glsl_version = "#version 300 es";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const char* glsl_version = "#version 330";
#endif

    g_state.window = glfwCreateWindow(1024, 768, "PQTR", nullptr, nullptr);
    if (!g_state.window) {
        LOG("LABS: glfwCreateWindow failed!");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(g_state.window);
#ifndef __EMSCRIPTEN__
    glfwSwapInterval(1);
#endif

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef __EMSCRIPTEN__
    io.IniFilename = nullptr;
#endif

    // Contemporary photo-app theme
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    // Geometry
    s.WindowRounding    = 6.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 4.0f;
    s.GrabRounding      = 4.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.TabRounding       = 4.0f;
    s.FramePadding      = ImVec2(8, 4);
    s.ItemSpacing       = ImVec2(8, 6);
    s.WindowPadding     = ImVec2(10, 10);
    s.WindowBorderSize  = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 1.0f;

    // Palette - neutral darks
    ImVec4 bg      = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
    ImVec4 panel   = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
    ImVec4 surface = ImVec4(0.18f, 0.18f, 0.19f, 1.0f);
    ImVec4 hover   = ImVec4(0.24f, 0.24f, 0.26f, 1.0f);
    ImVec4 active  = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
    ImVec4 accent  = ImVec4(0.35f, 0.55f, 0.90f, 1.0f);
    ImVec4 text    = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
    ImVec4 dim     = ImVec4(0.45f, 0.45f, 0.48f, 1.0f);
    ImVec4 border  = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);

    // Backgrounds
    c[ImGuiCol_WindowBg]       = panel;
    c[ImGuiCol_ChildBg]        = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]        = ImVec4(0.14f, 0.14f, 0.15f, 0.96f);
    c[ImGuiCol_FrameBg]        = surface;
    c[ImGuiCol_FrameBgHovered] = hover;
    c[ImGuiCol_FrameBgActive]  = active;

    // Title bar
    c[ImGuiCol_TitleBg]          = bg;
    c[ImGuiCol_TitleBgActive]    = bg;
    c[ImGuiCol_TitleBgCollapsed] = bg;
    c[ImGuiCol_MenuBarBg]        = bg;

    // Headers
    c[ImGuiCol_Header]        = surface;
    c[ImGuiCol_HeaderHovered] = hover;
    c[ImGuiCol_HeaderActive]  = active;

    // Buttons
    c[ImGuiCol_Button]        = surface;
    c[ImGuiCol_ButtonHovered] = hover;
    c[ImGuiCol_ButtonActive]  = accent;

    // Sliders
    c[ImGuiCol_SliderGrab]           = accent;
    c[ImGuiCol_SliderGrabActive]     = accent;
    c[ImGuiCol_ScrollbarBg]          = bg;
    c[ImGuiCol_ScrollbarGrab]        = surface;
    c[ImGuiCol_ScrollbarGrabHovered] = hover;
    c[ImGuiCol_ScrollbarGrabActive]  = active;

    // Checks
    c[ImGuiCol_CheckMark]      = accent;
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);

    // Tabs
    c[ImGuiCol_Tab]                = surface;
    c[ImGuiCol_TabHovered]         = hover;
    c[ImGuiCol_TabActive]          = active;
    c[ImGuiCol_TabUnfocused]       = surface;
    c[ImGuiCol_TabUnfocusedActive] = surface;

    // Text
    c[ImGuiCol_Text]         = text;
    c[ImGuiCol_TextDisabled] = dim;

    // Misc
    c[ImGuiCol_Border]            = border;
    c[ImGuiCol_Separator]         = border;
    c[ImGuiCol_SeparatorHovered]  = hover;
    c[ImGuiCol_SeparatorActive]   = accent;
    c[ImGuiCol_ResizeGrip]        = surface;
    c[ImGuiCol_ResizeGripHovered] = hover;
    c[ImGuiCol_ResizeGripActive]  = accent;

    ImGui_ImplGlfw_InitForOpenGL(g_state.window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Try to restore saved auth session
    if (loadAuthJwt(g_state.jwt, sizeof(g_state.jwt))) {
        loadAuthItag(g_state.itag, sizeof(g_state.itag));
        loadAuthRole(g_state.role, sizeof(g_state.role));
        loadAuthUserId(g_state.user_id, sizeof(g_state.user_id));
        updateAuthHeader(g_state.jwt);
        g_state.screen = Screen::Desktop;

        char debug_msg[64];
        snprintf(debug_msg, sizeof(debug_msg), "jwt=%zu itag=%s", strlen(g_state.jwt), g_state.itag);
        postNote("auth.restore", debug_msg);

        postNote("labs.open", g_state.itag);
        LOG("LABS: Restored auth session");
    }

#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(g_state.window, "#canvas");
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    while (!glfwWindowShouldClose(g_state.window)) {
        main_loop();
    }
#endif

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(g_state.window);
    glfwTerminate();

    return 0;
}

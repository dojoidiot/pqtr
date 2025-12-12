// login.cpp - LABS WASM login page
// Minimal ImGui app for login/account creation

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdio>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/console.h>
#define LOG(msg) emscripten_console_log(msg)
#else
#define LOG(msg) printf("%s\n", msg)
#endif

// Application state
struct AppState {
    GLFWwindow* window = nullptr;

    // Login form
    char email[128] = "";
    char password[128] = "";
    char confirm_password[128] = "";
    char error_message[256] = "";

    // Mode: false = login, true = create account
    bool create_mode = false;
    bool show_password = false;
};

static AppState g_state;

// Render the login/create account dialog
void render_login_dialog() {
    ImGuiIO& io = ImGui::GetIO();

    // Center the dialog
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 dialog_size(360, g_state.create_mode ? 320 : 260);

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(dialog_size);

    const char* title = g_state.create_mode ? "Create Account" : "Login";

    ImGui::Begin(title, nullptr,
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse);

    // Logo/branding
    ImGui::SetCursorPosX((dialog_size.x - ImGui::CalcTextSize("PQTR").x * 2) * 0.5f);
    ImGui::PushFont(nullptr);  // Would use larger font if available
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "PQTR");
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Email field
    ImGui::Text("Email");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##email", g_state.email, sizeof(g_state.email));

    ImGui::Spacing();

    // Password field
    ImGui::Text("Password");
    ImGui::SetNextItemWidth(-1);
    ImGuiInputTextFlags pw_flags = g_state.show_password ? 0 : ImGuiInputTextFlags_Password;
    ImGui::InputText("##password", g_state.password, sizeof(g_state.password), pw_flags);

    // Confirm password (create mode only)
    if (g_state.create_mode) {
        ImGui::Spacing();
        ImGui::Text("Confirm Password");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##confirm", g_state.confirm_password, sizeof(g_state.confirm_password), pw_flags);
    }

    // Show password checkbox
    ImGui::Checkbox("Show password", &g_state.show_password);

    ImGui::Spacing();

    // Error message
    if (g_state.error_message[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_state.error_message);
        ImGui::Spacing();
    }

    // Submit button
    const char* button_text = g_state.create_mode ? "Create Account" : "Login";
    if (ImGui::Button(button_text, ImVec2(-1, 32))) {
        // Validate
        if (strlen(g_state.email) == 0) {
            strcpy(g_state.error_message, "Email is required");
        } else if (strlen(g_state.password) < 8) {
            strcpy(g_state.error_message, "Password must be at least 8 characters");
        } else if (g_state.create_mode && strcmp(g_state.password, g_state.confirm_password) != 0) {
            strcpy(g_state.error_message, "Passwords do not match");
        } else {
            // TODO: Send to BASE API
            strcpy(g_state.error_message, "Not implemented yet");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Toggle mode link
    if (g_state.create_mode) {
        ImGui::Text("Already have an account?");
        ImGui::SameLine();
        if (ImGui::SmallButton("Login")) {
            g_state.create_mode = false;
            g_state.error_message[0] = '\0';
            g_state.confirm_password[0] = '\0';
        }
    } else {
        ImGui::Text("Don't have an account?");
        ImGui::SameLine();
        if (ImGui::SmallButton("Create one")) {
            g_state.create_mode = true;
            g_state.error_message[0] = '\0';
        }
    }

    ImGui::End();
}

static int frame_count = 0;

// Main loop iteration (called by Emscripten or native loop)
void main_loop() {
    glfwPollEvents();

    int display_w, display_h;
    glfwGetFramebufferSize(g_state.window, &display_w, &display_h);

    if (frame_count < 3) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Frame %d: framebuffer=%dx%d", frame_count, display_w, display_h);
        LOG(buf);
        frame_count++;
    }

    if (display_w <= 0 || display_h <= 0) return;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Debug: log ImGui display size on first frame
    if (frame_count == 1) {
        ImGuiIO& io = ImGui::GetIO();
        char buf[128];
        snprintf(buf, sizeof(buf), "ImGui DisplaySize: %.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);
        LOG(buf);
    }

    // Simple test: draw a visible window
    ImGui::SetNextWindowPos(ImVec2(50, 50));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Text("If you see this, ImGui works!");
    ImGui::End();

    render_login_dialog();

    ImGui::Render();

    glViewport(0, 0, display_w, display_h);
    glClearColor(0.3f, 0.1f, 0.1f, 1.0f);  // Red tint to confirm we're rendering
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(g_state.window);
}

void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int /*argc*/, char** /*argv*/) {
    EM_ASM(console.log('=== C++ MAIN CALLED ==='));
    LOG("LABS: Starting...");

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        LOG("LABS: glfwInit failed!");
        return 1;
    }
    LOG("LABS: GLFW initialized");

    // OpenGL ES 3.0 for Emscripten, OpenGL 3.3 for native
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

    g_state.window = glfwCreateWindow(800, 600, "PQTR - Login", nullptr, nullptr);
    if (!g_state.window) {
        LOG("LABS: glfwCreateWindow failed!");
        glfwTerminate();
        return 1;
    }
    LOG("LABS: Window created");

    glfwMakeContextCurrent(g_state.window);
    glfwSwapInterval(1);
    LOG("LABS: Context ready");

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef __EMSCRIPTEN__
    io.IniFilename = nullptr;  // No filesystem access in browser
#endif

    // Dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(g_state.window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

#ifdef __EMSCRIPTEN__
    // Install Emscripten canvas callbacks for proper event handling
    LOG("LABS: Installing Emscripten callbacks");
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(g_state.window, "#canvas");
    LOG("LABS: Starting main loop");
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

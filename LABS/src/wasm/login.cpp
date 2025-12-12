// login.cpp - LABS WASM application
// Login via OTP, then desktop with menu bar

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/console.h>
#include <emscripten/fetch.h>
#define LOG(msg) emscripten_console_log(msg)
#else
#define LOG(msg) printf("%s\n", msg)
#endif

// Application screens
enum class Screen {
    Login,      // Email entry
    OTP,        // OTP entry (after login/register)
    Desktop     // Main app after auth
};

// Task status
enum class TaskStatus {
    Pending,
    Running,
    Done,
    Error
};

// Task in queue
struct Task {
    char name[64] = "";
    char message[128] = "";
    TaskStatus status = TaskStatus::Pending;
};

// Application state
struct AppState {
    GLFWwindow* window = nullptr;
    Screen screen = Screen::Login;

    // Login form
    char email[128] = "";
    char otp[16] = "";
    char error_message[256] = "";
    char status_message[256] = "";

    // Auth state
    char jwt[2048] = "";
    char refresh_token[256] = "";
    char user_id[64] = "";
    char itag[16] = "";
    char role[32] = "";

    // Desktop state
    bool show_labs_panel = false;
    bool list_loaded = false;
    bool list_loading = false;
    static constexpr int MAX_PIPES = 64;
    char pipes[MAX_PIPES][64] = {};
    int pipe_count = 0;

    // Current RAW file
    uint8_t* raws_data = nullptr;
    size_t raws_size = 0;
    char raws_name[256] = "";

    // Task queue
    bool show_task_view = false;
    static constexpr int MAX_TASKS = 8;
    Task tasks[MAX_TASKS] = {};
    int task_count = 0;
    int current_task = -1;

    // Async request state
    bool request_pending = false;
};

static AppState g_state;

// Task helpers
static void clearTasks() {
    g_state.task_count = 0;
    g_state.current_task = -1;
    for (int i = 0; i < AppState::MAX_TASKS; i++) {
        g_state.tasks[i].name[0] = '\0';
        g_state.tasks[i].message[0] = '\0';
        g_state.tasks[i].status = TaskStatus::Pending;
    }
}

static int addTask(const char* name) {
    if (g_state.task_count >= AppState::MAX_TASKS) return -1;
    int idx = g_state.task_count++;
    strncpy(g_state.tasks[idx].name, name, sizeof(g_state.tasks[idx].name) - 1);
    g_state.tasks[idx].message[0] = '\0';
    g_state.tasks[idx].status = TaskStatus::Pending;
    return idx;
}

static void setTaskStatus(int idx, TaskStatus status, const char* message = nullptr) {
    if (idx < 0 || idx >= g_state.task_count) return;
    g_state.tasks[idx].status = status;
    if (message) {
        strncpy(g_state.tasks[idx].message, message, sizeof(g_state.tasks[idx].message) - 1);
    }
}

static void startNextTask();  // Forward declaration

#ifdef __EMSCRIPTEN__
// C callback for file load - called from JavaScript
extern "C" {
EMSCRIPTEN_KEEPALIVE
void onRawFileLoaded(const char* name, uint8_t* data, int size) {
    LOG("RAW file loaded");
    char msg[128];
    snprintf(msg, sizeof(msg), "File: %s, Size: %d bytes", name, size);
    LOG(msg);

    // Free previous data
    if (g_state.raws_data) {
        free(g_state.raws_data);
    }

    // Store new data
    g_state.raws_data = (uint8_t*)malloc(size);
    if (g_state.raws_data) {
        memcpy(g_state.raws_data, data, size);
        g_state.raws_size = size;
        strncpy(g_state.raws_name, name, sizeof(g_state.raws_name) - 1);
        g_state.raws_name[sizeof(g_state.raws_name) - 1] = '\0';

        // Start task queue
        clearTasks();
        addTask("Check existing");
        addTask("Upload RAW");
        g_state.show_task_view = true;
        g_state.current_task = 0;
        startNextTask();
    }
}
}

// JavaScript function to open file picker
EM_JS(void, openRawFilePicker, (), {
    // Create hidden file input if needed
    let input = document.getElementById('rawFileInput');
    if (!input) {
        input = document.createElement('input');
        input.type = 'file';
        input.id = 'rawFileInput';
        input.accept = '.arw,.ARW';
        input.style.display = 'none';
        document.body.appendChild(input);

        input.addEventListener('change', function(e) {
            const file = e.target.files[0];
            if (!file) return;

            const reader = new FileReader();
            reader.onload = function(event) {
                const data = new Uint8Array(event.target.result);
                const namePtr = stringToNewUTF8(file.name);
                const dataPtr = _malloc(data.length);
                HEAPU8.set(data, dataPtr);
                _onRawFileLoaded(namePtr, dataPtr, data.length);
                _free(namePtr);
                _free(dataPtr);
            };
            reader.readAsArrayBuffer(file);
            input.value = "";  // Reset for next use
        });
    }
    input.click();
});
#else
static void openRawFilePicker() {
    LOG("File picker not implemented for native build");
}
#endif

// JSON helpers
static const char* extractJsonString(const char* json, const char* key, char* out, size_t out_size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* start = strstr(json, pattern);
    if (!start) { out[0] = '\0'; return out; }
    start += strlen(pattern);
    const char* end = strchr(start, '"');
    if (!end) { out[0] = '\0'; return out; }
    size_t len = end - start;
    if (len >= out_size) len = out_size - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return out;
}

static bool extractJsonBool(const char* json, const char* key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":true", key);
    return strstr(json, pattern) != nullptr;
}

#ifdef __EMSCRIPTEN__
// Fetch callbacks
static void onLoginSuccess(emscripten_fetch_t* fetch) {
    LOG("Login response received");
    char buf[512];
    size_t len = fetch->numBytes < 511 ? fetch->numBytes : 511;
    strncpy(buf, fetch->data, len);
    buf[len] = '\0';
    LOG(buf);

    if (extractJsonBool(buf, "ok")) {
        g_state.screen = Screen::OTP;
        strcpy(g_state.status_message, "Check console for OTP");
        g_state.error_message[0] = '\0';
    } else {
        strcpy(g_state.error_message, "Login failed");
    }
    g_state.request_pending = false;
    emscripten_fetch_close(fetch);
}

static void onLoginFail(emscripten_fetch_t* fetch) {
    LOG("Login request failed");
    strcpy(g_state.error_message, "Network error");
    g_state.request_pending = false;
    emscripten_fetch_close(fetch);
}

static void onVerifySuccess(emscripten_fetch_t* fetch) {
    LOG("Verify response received");
    char buf[4096];
    size_t len = fetch->numBytes < 4095 ? fetch->numBytes : 4095;
    strncpy(buf, fetch->data, len);
    buf[len] = '\0';
    LOG(buf);

    // Check for error
    if (strstr(buf, "\"error\"")) {
        strcpy(g_state.error_message, "Invalid OTP");
        g_state.request_pending = false;
        emscripten_fetch_close(fetch);
        return;
    }

    // Extract tokens
    extractJsonString(buf, "jwt", g_state.jwt, sizeof(g_state.jwt));
    extractJsonString(buf, "refresh_token", g_state.refresh_token, sizeof(g_state.refresh_token));
    extractJsonString(buf, "user_id", g_state.user_id, sizeof(g_state.user_id));
    extractJsonString(buf, "itag", g_state.itag, sizeof(g_state.itag));
    extractJsonString(buf, "role", g_state.role, sizeof(g_state.role));

    if (g_state.jwt[0] != '\0') {
        g_state.screen = Screen::Desktop;
        g_state.error_message[0] = '\0';
        g_state.list_loaded = false;  // Trigger list load
        LOG("Login successful, entering desktop");
    } else {
        strcpy(g_state.error_message, "Verification failed");
    }
    g_state.request_pending = false;
    emscripten_fetch_close(fetch);
}

static void onVerifyFail(emscripten_fetch_t* fetch) {
    LOG("Verify request failed");
    strcpy(g_state.error_message, "Network error");
    g_state.request_pending = false;
    emscripten_fetch_close(fetch);
}

static void sendLoginRequest() {
    if (g_state.request_pending) return;
    g_state.request_pending = true;

    char body[512];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"register\",\"params\":{\"email\":\"%s\"},\"id\":1}",
        g_state.email);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onLoginSuccess;
    attr.onerror = onLoginFail;

    const char* headers[] = {"Content-Type", "application/json", nullptr};
    attr.requestHeaders = headers;
    attr.requestData = body;
    attr.requestDataSize = strlen(body);

    emscripten_fetch(&attr, "/jrpc");
}

static void sendVerifyRequest() {
    if (g_state.request_pending) return;
    g_state.request_pending = true;

    char body[512];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"verify\",\"params\":{\"email\":\"%s\",\"otp\":\"%s\"},\"id\":1}",
        g_state.email, g_state.otp);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onVerifySuccess;
    attr.onerror = onVerifyFail;

    const char* headers[] = {"Content-Type", "application/json", nullptr};
    attr.requestHeaders = headers;
    attr.requestData = body;
    attr.requestDataSize = strlen(body);

    emscripten_fetch(&attr, "/jrpc");
}

static void onListSuccess(emscripten_fetch_t* fetch) {
    LOG("List response received");
    char buf[4096];
    size_t len = fetch->numBytes < 4095 ? fetch->numBytes : 4095;
    strncpy(buf, fetch->data, len);
    buf[len] = '\0';
    LOG(buf);

    g_state.pipe_count = 0;

    // Parse pipes array - simple extraction
    const char* pipes_start = strstr(buf, "\"pipes\":[");
    if (pipes_start) {
        pipes_start += 9;  // Skip "pipes":[
        while (*pipes_start && g_state.pipe_count < AppState::MAX_PIPES) {
            if (*pipes_start == '"') {
                pipes_start++;
                const char* end = strchr(pipes_start, '"');
                if (end) {
                    size_t plen = end - pipes_start;
                    if (plen < 64) {
                        strncpy(g_state.pipes[g_state.pipe_count], pipes_start, plen);
                        g_state.pipes[g_state.pipe_count][plen] = '\0';
                        g_state.pipe_count++;
                    }
                    pipes_start = end + 1;
                } else break;
            } else if (*pipes_start == ']') break;
            else pipes_start++;
        }
    }

    g_state.list_loaded = true;
    g_state.list_loading = false;
    emscripten_fetch_close(fetch);
}

static void onListFail(emscripten_fetch_t* fetch) {
    LOG("List request failed");
    g_state.list_loaded = true;
    g_state.list_loading = false;
    emscripten_fetch_close(fetch);
}

static void sendListRequest() {
    if (g_state.list_loading) return;
    g_state.list_loading = true;

    static char body[2560];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"list\",\"params\":{\"jwt\":\"%s\"},\"id\":1}",
        g_state.jwt);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onListSuccess;
    attr.onerror = onListFail;

    const char* headers[] = {"Content-Type", "application/json", nullptr};
    attr.requestHeaders = headers;
    attr.requestData = body;
    attr.requestDataSize = strlen(body);

    emscripten_fetch(&attr, "/jrpc");
}

// Test API - check if pipe folder exists
static void onTestSuccess(emscripten_fetch_t* fetch) {
    LOG("Test response received");
    char buf[512];
    size_t len = fetch->numBytes < 511 ? fetch->numBytes : 511;
    strncpy(buf, fetch->data, len);
    buf[len] = '\0';
    LOG(buf);

    bool exists = extractJsonBool(buf, "exists");

    if (exists) {
        // File already exists - error state
        setTaskStatus(0, TaskStatus::Error, "RAWS already exists");
        setTaskStatus(1, TaskStatus::Pending);  // Cancel upload task
        g_state.current_task = -1;  // Stop queue
    } else {
        // File doesn't exist - proceed to upload
        setTaskStatus(0, TaskStatus::Done, "New file");
        g_state.current_task = 1;
        startNextTask();
    }
    emscripten_fetch_close(fetch);
}

static void onTestFail(emscripten_fetch_t* fetch) {
    LOG("Test request failed");
    setTaskStatus(0, TaskStatus::Error, "Network error");
    g_state.current_task = -1;
    emscripten_fetch_close(fetch);
}

static void sendTestRequest() {
    setTaskStatus(0, TaskStatus::Running, "Checking...");

    // Extract base name without extension for folder name
    char base_name[256];
    strncpy(base_name, g_state.raws_name, sizeof(base_name) - 1);
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    static char body[2560];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"test\",\"params\":{\"jwt\":\"%s\",\"name\":\"%s\"},\"id\":1}",
        g_state.jwt, base_name);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onTestSuccess;
    attr.onerror = onTestFail;

    const char* headers[] = {"Content-Type", "application/json", nullptr};
    attr.requestHeaders = headers;
    attr.requestData = body;
    attr.requestDataSize = strlen(body);

    emscripten_fetch(&attr, "/jrpc");
}

// Push API - upload RAW file (binary)
static void onPushSuccess(emscripten_fetch_t* fetch) {
    LOG("Push response received");
    char buf[512];
    size_t len = fetch->numBytes < 511 ? fetch->numBytes : 511;
    strncpy(buf, fetch->data, len);
    buf[len] = '\0';
    LOG(buf);

    if (extractJsonBool(buf, "ok")) {
        setTaskStatus(1, TaskStatus::Done, "Uploaded");
        g_state.list_loaded = false;  // Refresh list
    } else {
        char error[128];
        extractJsonString(buf, "error", error, sizeof(error));
        setTaskStatus(1, TaskStatus::Error, error[0] ? error : "Upload failed");
    }
    g_state.current_task = -1;  // Done
    emscripten_fetch_close(fetch);
}

static void onPushFail(emscripten_fetch_t* fetch) {
    LOG("Push request failed");
    setTaskStatus(1, TaskStatus::Error, "Network error");
    g_state.current_task = -1;
    emscripten_fetch_close(fetch);
}

static void sendPushRequest() {
    setTaskStatus(1, TaskStatus::Running, "Uploading...");

    // Extract base name without extension
    char base_name[256];
    strncpy(base_name, g_state.raws_name, sizeof(base_name) - 1);
    char* dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    // Build URL with query params: /push?name=xxx&file=xxx
    // JWT goes in Authorization header
    static char url[512];
    snprintf(url, sizeof(url), "/push?name=%s&file=%s", base_name, g_state.raws_name);

    static char auth_header[2200];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_state.jwt);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onPushSuccess;
    attr.onerror = onPushFail;

    const char* headers[] = {
        "Content-Type", "application/octet-stream",
        "Authorization", auth_header,
        nullptr
    };
    attr.requestHeaders = headers;
    attr.requestData = (const char*)g_state.raws_data;
    attr.requestDataSize = g_state.raws_size;

    emscripten_fetch(&attr, url);
}

// Start next task in queue
static void startNextTask() {
    if (g_state.current_task < 0 || g_state.current_task >= g_state.task_count) return;

    switch (g_state.current_task) {
        case 0: sendTestRequest(); break;
        case 1: sendPushRequest(); break;
        default: break;
    }
}

#else
// Native stubs
static void sendLoginRequest() {
    strcpy(g_state.error_message, "Native login not implemented");
}
static void sendVerifyRequest() {
    strcpy(g_state.error_message, "Native verify not implemented");
}
static void sendListRequest() {
    g_state.list_loaded = true;
}
static void startNextTask() {}
#endif

// Render login screen (email entry)
static void render_login_screen() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 dialog_size(360, 220);

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(dialog_size);

    ImGui::Begin("Login", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Logo
    ImGui::SetCursorPosX((dialog_size.x - ImGui::CalcTextSize("PQTR").x * 2) * 0.5f);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "PQTR");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Email field
    ImGui::Text("Email");
    ImGui::SetNextItemWidth(-1);
    bool enter_pressed = ImGui::InputText("##email", g_state.email, sizeof(g_state.email),
        ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::Spacing();

    // Error/status messages
    if (g_state.error_message[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_state.error_message);
    }

    ImGui::Spacing();

    // Submit button
    bool can_submit = strlen(g_state.email) > 0 && !g_state.request_pending;
    if (!can_submit) ImGui::BeginDisabled();
    if (ImGui::Button(g_state.request_pending ? "Sending..." : "Continue", ImVec2(-1, 32)) || (enter_pressed && can_submit)) {
        sendLoginRequest();
    }
    if (!can_submit) ImGui::EndDisabled();

    ImGui::End();
}

// Render OTP screen
static void render_otp_screen() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImVec2 dialog_size(360, 260);

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(dialog_size);

    ImGui::Begin("Enter OTP", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Logo
    ImGui::SetCursorPosX((dialog_size.x - ImGui::CalcTextSize("PQTR").x * 2) * 0.5f);
    ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "PQTR");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Status
    if (g_state.status_message[0] != '\0') {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", g_state.status_message);
        ImGui::Spacing();
    }

    ImGui::Text("Email: %s", g_state.email);
    ImGui::Spacing();

    // OTP field
    ImGui::Text("One-Time Password");
    ImGui::SetNextItemWidth(-1);
    bool enter_pressed = ImGui::InputText("##otp", g_state.otp, sizeof(g_state.otp),
        ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::Spacing();

    // Error message
    if (g_state.error_message[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_state.error_message);
    }

    ImGui::Spacing();

    // Submit button
    bool can_submit = strlen(g_state.otp) > 0 && !g_state.request_pending;
    if (!can_submit) ImGui::BeginDisabled();
    if (ImGui::Button(g_state.request_pending ? "Verifying..." : "Verify", ImVec2(-1, 32)) || (enter_pressed && can_submit)) {
        sendVerifyRequest();
    }
    if (!can_submit) ImGui::EndDisabled();

    ImGui::Spacing();

    // Back button
    if (ImGui::SmallButton("< Back")) {
        g_state.screen = Screen::Login;
        g_state.otp[0] = '\0';
        g_state.error_message[0] = '\0';
        g_state.status_message[0] = '\0';
    }

    ImGui::End();
}

// Render desktop screen with menu bar
static void render_desktop_screen() {
    ImGuiIO& io = ImGui::GetIO();

    // Load list on first render
    if (!g_state.list_loaded && !g_state.list_loading) {
        sendListRequest();
    }

    // Main menu bar
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load RAW File...")) {
                openRawFilePicker();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Logout")) {
                g_state.screen = Screen::Login;
                g_state.jwt[0] = '\0';
                g_state.email[0] = '\0';
                g_state.otp[0] = '\0';
                g_state.itag[0] = '\0';
                g_state.list_loaded = false;
                g_state.pipe_count = 0;
                // Free RAW data on logout
                if (g_state.raws_data) {
                    free(g_state.raws_data);
                    g_state.raws_data = nullptr;
                    g_state.raws_size = 0;
                    g_state.raws_name[0] = '\0';
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("LABS", nullptr, &g_state.show_labs_panel);
            ImGui::EndMenu();
        }

        // Right-aligned user info
        float user_width = ImGui::CalcTextSize(g_state.itag).x + 20;
        ImGui::SetCursorPosX(io.DisplaySize.x - user_width);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", g_state.itag);

        ImGui::EndMainMenuBar();
    }

    // LABS floating panel
    if (g_state.show_labs_panel) {
        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        if (ImGui::Begin("LABS", &g_state.show_labs_panel)) {
            // Current RAW file section
            if (g_state.raws_data) {
                ImGui::Text("Current RAW:");
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s", g_state.raws_name);
                ImGui::Text("Size: %.2f MB", g_state.raws_size / (1024.0f * 1024.0f));
                ImGui::Separator();
            }

            // Pipelines section
            if (g_state.list_loading) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Loading...");
            } else if (g_state.pipe_count == 0 && !g_state.raws_data) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.4f, 1.0f), "Load a RAW file to begin");
            } else if (g_state.pipe_count > 0) {
                ImGui::Text("Pipelines:");
                ImGui::Separator();
                for (int i = 0; i < g_state.pipe_count; i++) {
                    if (ImGui::Selectable(g_state.pipes[i])) {
                        // TODO: Select pipeline
                    }
                }
            }
        }
        ImGui::End();
    }

    // Task View pane
    if (g_state.show_task_view) {
        ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::Begin("Task View", nullptr, ImGuiWindowFlags_NoCollapse)) {
            // Show tasks
            for (int i = 0; i < g_state.task_count; i++) {
                Task& task = g_state.tasks[i];

                // Status indicator
                ImVec4 color;
                const char* icon;
                switch (task.status) {
                    case TaskStatus::Pending:  color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); icon = "[ ]"; break;
                    case TaskStatus::Running:  color = ImVec4(0.4f, 0.7f, 1.0f, 1.0f); icon = "[~]"; break;
                    case TaskStatus::Done:     color = ImVec4(0.4f, 0.9f, 0.4f, 1.0f); icon = "[+]"; break;
                    case TaskStatus::Error:    color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); icon = "[!]"; break;
                }

                ImGui::TextColored(color, "%s %s", icon, task.name);
                if (task.message[0]) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "- %s", task.message);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // OK button - enabled when all tasks done or error
            bool all_done = g_state.current_task < 0;
            if (!all_done) ImGui::BeginDisabled();
            if (ImGui::Button("OK", ImVec2(80, 0))) {
                g_state.show_task_view = false;
            }
            if (!all_done) ImGui::EndDisabled();
        }
        ImGui::End();
    }
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

    // Render appropriate screen
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

int main(int /*argc*/, char** /*argv*/) {
#ifdef __EMSCRIPTEN__
    EM_ASM(console.log('=== LABS Starting ==='));
#endif
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
    glfwSwapInterval(1);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef __EMSCRIPTEN__
    io.IniFilename = nullptr;
#endif

    // Dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;

    ImGui_ImplGlfw_InitForOpenGL(g_state.window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

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

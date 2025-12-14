// labs.cpp - LABS WASM application
// Login via OTP, then desktop with menu bar

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "gear.hpp"
#include "part/json.hpp"
#include "part/tree.hpp"

#include <GLFW/glfw3.h>
#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#endif
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

// ============================================================
// Fetch Context System - Safe async request handling
// ============================================================
// Each fetch request gets a heap-allocated context that:
// - Holds all data needed for the request (body, headers, etc.)
// - Tracks session ID to detect stale responses after auth changes
// - Is freed in the callback (success or error)

// Global session counter - incremented on auth changes
static uint32_t g_auth_session = 0;

// Request types for different handlers
enum class FetchType {
    Login,
    Verify,
    List,
    Files,
    Push,
    Pull,
    Json
};

// Context for each fetch request - heap allocated, freed in callback
struct FetchContext {
    FetchType type;
    uint32_t session;           // Session ID when request was made
    char* body;                 // Heap-allocated request body
    size_t body_size;
    char* url;                  // Heap-allocated URL (for push/pull)
    uint8_t* data;              // Heap-allocated binary data (for push)
    size_t data_size;
    char extra[256];            // Extra context (e.g., basename for chained ops)

    FetchContext(FetchType t) : type(t), session(g_auth_session),
                                 body(nullptr), body_size(0),
                                 url(nullptr), data(nullptr), data_size(0) {
        extra[0] = '\0';
    }

    ~FetchContext() {
        if (body) free(body);
        if (url) free(url);
        if (data) free(data);
    }

    // Check if this request is still valid (session hasn't changed)
    bool isValid() const { return session == g_auth_session; }

    // Allocate and copy body
    void setBody(const char* src) {
        body_size = strlen(src);
        body = (char*)malloc(body_size + 1);
        memcpy(body, src, body_size + 1);
    }

    // Allocate and copy URL
    void setUrl(const char* src) {
        size_t len = strlen(src);
        url = (char*)malloc(len + 1);
        memcpy(url, src, len + 1);
    }

    // Allocate and copy binary data
    void setData(const uint8_t* src, size_t size) {
        data_size = size;
        data = (uint8_t*)malloc(size);
        memcpy(data, src, size);
    }
};

// Static headers - persist for lifetime of program
static const char* g_json_headers[] = {"Content-Type", "application/json", nullptr};

// Auth header buffer - updated when JWT changes, used by push/pull
static char g_auth_header[2200] = "";
static const char* g_auth_headers[] = {"Authorization", g_auth_header, nullptr};

// Helper to update auth header when JWT changes
static void updateAuthHeader(const char* jwt) {
    snprintf(g_auth_header, sizeof(g_auth_header), "Bearer %s", jwt);
}

// Helper to invalidate all in-flight requests (call on logout/auth change)
static void invalidateRequests() {
    g_auth_session++;
    LOG("Auth session invalidated - stale requests will be ignored");
}

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

// Note system - simple event queue
struct Note {
    char event[32] = "";
    char data[256] = "";
};

static constexpr int MAX_NOTES = 16;
static Note g_notes[MAX_NOTES];
static int g_note_count = 0;

// Note history for display
static constexpr int MAX_NOTE_HISTORY = 64;
static Note g_note_history[MAX_NOTE_HISTORY];
static int g_note_history_count = 0;
static bool g_note_history_scroll = false;  // Flag to scroll to bottom

static void postNote(const char* event, const char* data = "") {
    // Add to active queue
    if (g_note_count >= MAX_NOTES) {
        // Shift notes down, drop oldest
        for (int i = 0; i < MAX_NOTES - 1; i++) {
            g_notes[i] = g_notes[i + 1];
        }
        g_note_count = MAX_NOTES - 1;
    }
    Note& note = g_notes[g_note_count++];
    strncpy(note.event, event, sizeof(note.event) - 1);
    strncpy(note.data, data, sizeof(note.data) - 1);

    // Add to history
    if (g_note_history_count >= MAX_NOTE_HISTORY) {
        // Shift history down, drop oldest
        for (int i = 0; i < MAX_NOTE_HISTORY - 1; i++) {
            g_note_history[i] = g_note_history[i + 1];
        }
        g_note_history_count = MAX_NOTE_HISTORY - 1;
    }
    Note& hist = g_note_history[g_note_history_count++];
    strncpy(hist.event, event, sizeof(hist.event) - 1);
    strncpy(hist.data, data, sizeof(hist.data) - 1);
    g_note_history_scroll = true;
}

static bool checkNote(const char* event, char* data_out = nullptr, size_t data_size = 0) {
    for (int i = 0; i < g_note_count; i++) {
        if (strcmp(g_notes[i].event, event) == 0) {
            if (data_out && data_size > 0) {
                strncpy(data_out, g_notes[i].data, data_size - 1);
                data_out[data_size - 1] = '\0';
            }
            // Remove note (shift remaining)
            for (int j = i; j < g_note_count - 1; j++) {
                g_notes[j] = g_notes[j + 1];
            }
            g_note_count--;
            return true;
        }
    }
    return false;
}

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
    bool show_note_pane = false;
    bool show_tune_pane = false;
    bool layout_loaded = false;
    bool list_loaded = false;
    bool list_loading = false;
    static constexpr int MAX_PIPES = 64;
    char pipes[MAX_PIPES][64] = {};
    int pipe_count = 0;
    char current_pipe[64] = "";  // Selected pipeline

    // Current RAW file (name only - data lives on BASE)
    char raws_name[256] = "";

    // Files in current pipe
    static constexpr int MAX_FILES = 64;
    char files[MAX_FILES][64] = {};
    int file_count = 0;
    bool files_loaded = false;
    bool files_loading = false;

    // GEAR decoded data (from BASE pull)
    uint16_t* bayer_data = nullptr;
    int bayer_width = 0;
    int bayer_height = 0;
    int bayer_black = 0;
    int bayer_white = 0;
    pipe::Info* gear_info = nullptr;  // Camera metadata from GEAR decode (heap allocated)
    bool gear_decoded = false;

    // Preview from GEAR (embedded JPEG, RGB 8-bit)
    uint8_t* preview_data = nullptr;
    int preview_width = 0;
    int preview_height = 0;
    unsigned int preview_texture = 0;  // OpenGL texture ID

    // Stage textures (each stage's output)
    uint8_t* gear_rgb = nullptr;       // Flat demosaiced RAW
    unsigned int gear_texture = 0;
    unsigned int lute_texture = 0;
    unsigned int drum_texture = 0;
    unsigned int diff_texture = 0;

    // Task queue
    bool show_task_view = false;
    static constexpr int MAX_TASKS = 16;
    Task tasks[MAX_TASKS] = {};
    int task_count = 0;
    int current_task = -1;

    // Tune pipeline state
    bool tune_running = false;
    int tune_step = 0;

    // Async request state
    bool request_pending = false;

    // JSON tree viewer state
    char json_file[64] = "";           // Currently selected JSON file
    json::ValuePtr json_tree;          // Parsed JSON for tree view
    bool json_loading = false;
    bool json_loaded = false;
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
static void runTuneStep();    // Forward declaration
static const char* extractJsonString(const char* json, const char* key, char* out, size_t out_size);  // Forward declaration

// Upload preview RGB data to OpenGL texture
static void uploadPreviewTexture() {
    if (!g_state.preview_data || g_state.preview_width <= 0 || g_state.preview_height <= 0) {
        return;
    }

    // Delete old texture if exists
    if (g_state.preview_texture) {
        glDeleteTextures(1, &g_state.preview_texture);
        g_state.preview_texture = 0;
    }

    // Create new texture
    glGenTextures(1, &g_state.preview_texture);
    glBindTexture(GL_TEXTURE_2D, g_state.preview_texture);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Upload RGB data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, g_state.preview_width, g_state.preview_height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, g_state.preview_data);

    glBindTexture(GL_TEXTURE_2D, 0);

    char msg[64];
    snprintf(msg, sizeof(msg), "Preview texture: %dx%d", g_state.preview_width, g_state.preview_height);
    LOG(msg);
}

// Demosaic Bayer data to flat RGB and upload as gear_texture
// Simple bilinear interpolation, RGGB pattern, no tone curve
static void createGearTexture() {
    if (!g_state.bayer_data || g_state.bayer_width <= 0 || g_state.bayer_height <= 0) {
        return;
    }

    int w = g_state.bayer_width;
    int h = g_state.bayer_height;
    int black = g_state.bayer_black;
    int white = g_state.bayer_white;
    float scale = 255.0f / (white - black);

    // Allocate RGB buffer (half resolution to avoid edge artifacts)
    int out_w = w / 2;
    int out_h = h / 2;

    if (g_state.gear_rgb) free(g_state.gear_rgb);
    g_state.gear_rgb = (uint8_t*)malloc(out_w * out_h * 3);

    // Simple 2x2 Bayer block sampling (RGGB)
    // Each 2x2 block: [R  Gr]
    //                 [Gb B ]
    for (int y = 0; y < out_h; y++) {
        for (int x = 0; x < out_w; x++) {
            int bx = x * 2;
            int by = y * 2;

            // Get raw values from 2x2 block
            int r  = g_state.bayer_data[by * w + bx];           // R
            int gr = g_state.bayer_data[by * w + bx + 1];       // Gr
            int gb = g_state.bayer_data[(by + 1) * w + bx];     // Gb
            int b  = g_state.bayer_data[(by + 1) * w + bx + 1]; // B

            // Average greens
            int g = (gr + gb) / 2;

            // Normalize to 0-255 (linear, no gamma)
            auto norm = [black, scale](int v) -> uint8_t {
                float f = (v - black) * scale;
                if (f < 0) f = 0;
                if (f > 255) f = 255;
                return (uint8_t)f;
            };

            int idx = (y * out_w + x) * 3;
            g_state.gear_rgb[idx + 0] = norm(r);
            g_state.gear_rgb[idx + 1] = norm(g);
            g_state.gear_rgb[idx + 2] = norm(b);
        }
    }

    // Delete old texture
    if (g_state.gear_texture) {
        glDeleteTextures(1, &g_state.gear_texture);
        g_state.gear_texture = 0;
    }

    // Create texture
    glGenTextures(1, &g_state.gear_texture);
    glBindTexture(GL_TEXTURE_2D, g_state.gear_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, out_w, out_h, 0, GL_RGB, GL_UNSIGNED_BYTE, g_state.gear_rgb);
    glBindTexture(GL_TEXTURE_2D, 0);

    char msg[64];
    snprintf(msg, sizeof(msg), "GEAR texture: %dx%d (flat RAW)", out_w, out_h);
    LOG(msg);
}

// Start tune pipeline for current_pipe
static void startTunePipeline() {
    if (!g_state.current_pipe[0]) return;

    clearTasks();
    addTask("gear.load");
    addTask("wgpu.open");
    addTask("pipe.view");   // GEAR stage
    addTask("lute.tune");
    addTask("pipe.view");   // LUTE stage
    addTask("drum.tune");
    addTask("pipe.view");   // DRUM stage
    addTask("pipe.make");
    addTask("pipe.save");
    addTask("wgpu.shut");

    g_state.tune_running = true;
    g_state.tune_step = 0;
    g_state.show_task_view = true;
    g_state.current_task = 0;

    postNote("tune.begin", g_state.current_pipe);
    runTuneStep();
}

// Run current tune step (placeholder - will connect to PIPE/WGPU)
static void runTuneStep() {
    if (!g_state.tune_running || g_state.tune_step >= g_state.task_count) {
        g_state.tune_running = false;
        g_state.current_task = -1;
        postNote("tune.done", g_state.current_pipe);
        return;
    }

    int step = g_state.tune_step;
    setTaskStatus(step, TaskStatus::Running);

    // Placeholder: immediately complete each step
    // TODO: Connect to actual PIPE/WGPU calls
    const char* task_name = g_state.tasks[step].name;

    if (strcmp(task_name, "gear.load") == 0) {
        // Use current_pipe to find the pipe.json file
        if (!g_state.current_pipe[0]) {
            setTaskStatus(step, TaskStatus::Error, "no pipe selected");
            g_state.tune_running = false;
            postNote("tune.error", "select a pipeline first");
            return;
        }

        // First fetch pipe.json to get the RAW filename
        setTaskStatus(step, TaskStatus::Running, "loading pipe...");

        // Build URL: GET /pull?name={pipe}&file={pipe}.pipe.json
        static char pipe_url[512];
        snprintf(pipe_url, sizeof(pipe_url), "/pull?name=%s&file=%s.pipe.json",
                 g_state.current_pipe, g_state.current_pipe);

        // Build Authorization header
        static char auth_header[2100];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", g_state.jwt);
        static const char* headers[] = {"Authorization", auth_header, nullptr};

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.requestHeaders = headers;
        attr.userData = reinterpret_cast<void*>(static_cast<intptr_t>(step));

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            int step = static_cast<int>(reinterpret_cast<intptr_t>(fetch->userData));

            // Parse pipe.json to get raw filename from info.file
            char raw_filename[256] = "";
            extractJsonString(fetch->data, "file", raw_filename, sizeof(raw_filename));
            emscripten_fetch_close(fetch);

            if (!raw_filename[0]) {
                setTaskStatus(step, TaskStatus::Error, "no file in pipe.json");
                g_state.tune_running = false;
                postNote("tune.error", "pipe.json missing info.file");
                return;
            }

            // Store raw filename for later use (pipe.json updates)
            strncpy(g_state.raws_name, raw_filename, sizeof(g_state.raws_name) - 1);
            g_state.raws_name[sizeof(g_state.raws_name) - 1] = '\0';

            // Now fetch the actual RAW file
            setTaskStatus(step, TaskStatus::Running, "pulling RAW...");

            // Build URL: GET /pull?name={pipe}&file={raw_filename}
            static char raw_url[512];
            snprintf(raw_url, sizeof(raw_url), "/pull?name=%s&file=%s",
                     g_state.current_pipe, raw_filename);

            // Reuse static auth header from outer scope
            static char raw_auth[2100];
            snprintf(raw_auth, sizeof(raw_auth), "Bearer %s", g_state.jwt);
            static const char* raw_headers[] = {"Authorization", raw_auth, nullptr};

            emscripten_fetch_attr_t raw_attr;
            emscripten_fetch_attr_init(&raw_attr);
            strcpy(raw_attr.requestMethod, "GET");
            raw_attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
            raw_attr.requestHeaders = raw_headers;
            raw_attr.userData = reinterpret_cast<void*>(static_cast<intptr_t>(step));

            raw_attr.onsuccess = [](emscripten_fetch_t* fetch) {
                int step = static_cast<int>(reinterpret_cast<intptr_t>(fetch->userData));

                // Decode the RAW data
                pipe::Data result = gear::sony::decode(fetch->data, fetch->numBytes);
                emscripten_fetch_close(fetch);

                // Check for errors
                if (result.info.text("error")[0] != '\0') {
                    setTaskStatus(step, TaskStatus::Error, result.info.text("error").c_str());
                    g_state.tune_running = false;
                    postNote("tune.error", result.info.text("error").c_str());
                    return;
                }

                // Store decoded Bayer data
                if (result.page) {
                    auto* buf = static_cast<gear::sony::BayerBuffer*>(result.page);

                    // Free previous if exists
                    if (g_state.bayer_data) free(g_state.bayer_data);

                    // Copy Bayer data
                    size_t bayer_size = buf->width * buf->height * sizeof(uint16_t);
                    g_state.bayer_data = (uint16_t*)malloc(bayer_size);
                    memcpy(g_state.bayer_data, buf->data.data(), bayer_size);
                    g_state.bayer_width = buf->width;
                    g_state.bayer_height = buf->height;
                    g_state.bayer_black = buf->black_level;
                    g_state.bayer_white = buf->white_level;

                    // Store metadata
                    if (g_state.gear_info) delete g_state.gear_info;
                    g_state.gear_info = new pipe::Info(std::move(result.info));
                    g_state.gear_decoded = true;

                    // Create flat RAW texture from Bayer data
                    createGearTexture();

                    // Copy preview data
                    if (g_state.preview_data) free(g_state.preview_data);
                    g_state.preview_data = nullptr;
                    g_state.preview_width = 0;
                    g_state.preview_height = 0;

                    if (buf->preview_width > 0 && buf->preview_height > 0 && !buf->preview.empty()) {
                        size_t preview_size = buf->preview_width * buf->preview_height * 3;
                        g_state.preview_data = (uint8_t*)malloc(preview_size);
                        memcpy(g_state.preview_data, buf->preview.data(), preview_size);
                        g_state.preview_width = buf->preview_width;
                        g_state.preview_height = buf->preview_height;

                        // Upload to texture
                        uploadPreviewTexture();
                    }

                    char msg[128];
                    snprintf(msg, sizeof(msg), "%dx%d", buf->width, buf->height);
                    setTaskStatus(step, TaskStatus::Running, "pushing pipe.json...");

                    // Build updated pipe.json with camera metadata
                    // Get camera info from gear_info
                    std::string camera_model_str = g_state.gear_info ?
                        g_state.gear_info->text("camera_model") : "";
                    const char* camera_model = camera_model_str.c_str();
                    int width = buf->width;
                    int height = buf->height;

                    static char updated_pipe[2048];
                    snprintf(updated_pipe, sizeof(updated_pipe),
                        "{\"info\":{\"file\":\"%s\",\"camera_model\":\"%s\","
                        "\"width\":%d,\"height\":%d},\"tune\":{\"step\":[\"gear\"]}}",
                        g_state.raws_name, camera_model, width, height);

                    delete buf;

                    // Push updated pipe.json
                    static char pipe_push_url[512];
                    snprintf(pipe_push_url, sizeof(pipe_push_url), "/push?name=%s&file=%s.pipe.json",
                             g_state.current_pipe, g_state.current_pipe);

                    static char pipe_auth[2100];
                    snprintf(pipe_auth, sizeof(pipe_auth), "Bearer %s", g_state.jwt);
                    static const char* pipe_headers[] = {"Authorization", pipe_auth, nullptr};

                    emscripten_fetch_attr_t pipe_attr;
                    emscripten_fetch_attr_init(&pipe_attr);
                    strcpy(pipe_attr.requestMethod, "POST");
                    pipe_attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
                    pipe_attr.requestHeaders = pipe_headers;
                    pipe_attr.requestData = updated_pipe;
                    pipe_attr.requestDataSize = strlen(updated_pipe);
                    pipe_attr.userData = reinterpret_cast<void*>(static_cast<intptr_t>(step));

                    pipe_attr.onsuccess = [](emscripten_fetch_t* fetch) {
                        int step = static_cast<int>(reinterpret_cast<intptr_t>(fetch->userData));
                        emscripten_fetch_close(fetch);

                        char msg[128];
                        snprintf(msg, sizeof(msg), "%dx%d", g_state.bayer_width, g_state.bayer_height);
                        setTaskStatus(step, TaskStatus::Done, msg);
                        postNote("gear.load", msg);

                        // Continue to next step
                        g_state.tune_step++;
                        runTuneStep();
                    };

                    pipe_attr.onerror = [](emscripten_fetch_t* fetch) {
                        int step = static_cast<int>(reinterpret_cast<intptr_t>(fetch->userData));
                        setTaskStatus(step, TaskStatus::Error, "pipe.json push failed");
                        g_state.tune_running = false;
                        postNote("tune.error", "failed to push pipe.json");
                        emscripten_fetch_close(fetch);
                    };

                    emscripten_fetch(&pipe_attr, pipe_push_url);
                } else {
                    setTaskStatus(step, TaskStatus::Error, "no page");
                    g_state.tune_running = false;
                    postNote("tune.error", "decode failed");
                }
            };

            raw_attr.onerror = [](emscripten_fetch_t* fetch) {
                int step = static_cast<int>(reinterpret_cast<intptr_t>(fetch->userData));
                setTaskStatus(step, TaskStatus::Error, "RAW fetch failed");
                g_state.tune_running = false;
                postNote("tune.error", "failed to pull RAW from BASE");
                emscripten_fetch_close(fetch);
            };

            emscripten_fetch(&raw_attr, raw_url);
        };

        attr.onerror = [](emscripten_fetch_t* fetch) {
            int step = static_cast<int>(reinterpret_cast<intptr_t>(fetch->userData));
            setTaskStatus(step, TaskStatus::Error, "pipe.json not found");
            g_state.tune_running = false;
            postNote("tune.error", "no pipe.json - load RAW first");
            emscripten_fetch_close(fetch);
        };

        emscripten_fetch(&attr, pipe_url);
        return;  // Async - will continue in callback
    } else if (strcmp(task_name, "wgpu.open") == 0) {
        postNote("wgpu.open", "");
        setTaskStatus(step, TaskStatus::Done, "ready");
    } else if (strcmp(task_name, "pipe.view") == 0) {
        // Which stage? 0=GEAR, 1=LUTE, 2=DROP
        int view_num = 0;
        for (int i = 0; i < step; i++) {
            if (strcmp(g_state.tasks[i].name, "pipe.view") == 0) view_num++;
        }
        const char* stage_names[] = {"GEAR", "LUTE", "DRUM"};
        postNote("pipe.view", stage_names[view_num]);
        setTaskStatus(step, TaskStatus::Done, stage_names[view_num]);
    } else if (strcmp(task_name, "lute.tune") == 0) {
        postNote("lute.tune", "");
        setTaskStatus(step, TaskStatus::Done, "tuned");
    } else if (strcmp(task_name, "drum.tune") == 0) {
        postNote("drum.tune", "");
        setTaskStatus(step, TaskStatus::Done, "tuned");
    } else if (strcmp(task_name, "pipe.make") == 0) {
        postNote("pipe.make", "tune.json");
        setTaskStatus(step, TaskStatus::Done, "json");
    } else if (strcmp(task_name, "pipe.save") == 0) {
        postNote("pipe.save", "png");
        setTaskStatus(step, TaskStatus::Done, "saved");
    } else if (strcmp(task_name, "wgpu.shut") == 0) {
        postNote("wgpu.shut", "");
        setTaskStatus(step, TaskStatus::Done, "closed");
    }

    // Move to next step
    g_state.tune_step++;
    g_state.current_task = g_state.tune_step < g_state.task_count ? g_state.tune_step : -1;

    // Continue to next step (in real impl, this would be async)
    if (g_state.tune_step < g_state.task_count) {
        runTuneStep();
    } else {
        g_state.tune_running = false;
        postNote("tune.done", g_state.current_pipe);
    }
}

#ifdef __EMSCRIPTEN__
// Layout persistence via localStorage
EM_JS(void, saveLayout, (int labs, int note, int tune), {
    localStorage.setItem('pqtr_layout', JSON.stringify({labs: labs, note: note, tune: tune}));
});

EM_JS(int, loadLayoutLabs, (), {
    try {
        const data = localStorage.getItem('pqtr_layout');
        if (data) return JSON.parse(data).labs ? 1 : 0;
    } catch(e) {}
    return 1;  // Default: LABS on
});

EM_JS(int, loadLayoutNote, (), {
    try {
        const data = localStorage.getItem('pqtr_layout');
        if (data) return JSON.parse(data).note ? 1 : 0;
    } catch(e) {}
    return 0;  // Default: Note off
});

EM_JS(int, loadLayoutTune, (), {
    try {
        const data = localStorage.getItem('pqtr_layout');
        if (data) return JSON.parse(data).tune ? 1 : 0;
    } catch(e) {}
    return 0;  // Default: Tune off
});

// Auth persistence via localStorage
EM_JS(void, saveAuth, (const char* jwt, const char* itag, const char* role, const char* user_id), {
    localStorage.setItem('pqtr_auth', JSON.stringify({
        jwt: UTF8ToString(jwt),
        itag: UTF8ToString(itag),
        role: UTF8ToString(role),
        user_id: UTF8ToString(user_id)
    }));
});

EM_JS(void, clearAuth, (), {
    localStorage.removeItem('pqtr_auth');
});

EM_JS(int, loadAuthJwt, (char* out, int maxLen), {
    try {
        const data = localStorage.getItem('pqtr_auth');
        if (data) {
            const jwt = JSON.parse(data).jwt || "";
            if (jwt.length > 0 && jwt.length < maxLen) {
                stringToUTF8(jwt, out, maxLen);
                return 1;
            }
        }
    } catch(e) {}
    return 0;
});

EM_JS(void, loadAuthItag, (char* out, int maxLen), {
    try {
        const data = localStorage.getItem('pqtr_auth');
        if (data) stringToUTF8(JSON.parse(data).itag || "", out, maxLen);
    } catch(e) {}
});

EM_JS(void, loadAuthRole, (char* out, int maxLen), {
    try {
        const data = localStorage.getItem('pqtr_auth');
        if (data) stringToUTF8(JSON.parse(data).role || "", out, maxLen);
    } catch(e) {}
});

EM_JS(void, loadAuthUserId, (char* out, int maxLen), {
    try {
        const data = localStorage.getItem('pqtr_auth');
        if (data) stringToUTF8(JSON.parse(data).user_id || "", out, maxLen);
    } catch(e) {}
});

// Forward declarations for fetch callbacks (defined later, used by onRawFileLoaded)
static void onFetchSuccess(emscripten_fetch_t* fetch);
static void onFetchError(emscripten_fetch_t* fetch);

// C callback for file load - called from JavaScript
extern "C" {
EMSCRIPTEN_KEEPALIVE
void onRawFileLoaded(const char* name, uint8_t* data, int size) {
    LOG("RAW file loaded - pushing to BASE");
    char msg[128];
    snprintf(msg, sizeof(msg), "File: %s, Size: %d bytes", name, size);
    LOG(msg);

    // Store full filename
    strncpy(g_state.raws_name, name, sizeof(g_state.raws_name) - 1);
    g_state.raws_name[sizeof(g_state.raws_name) - 1] = '\0';

    // Extract basename (without extension)
    char basename[256];
    strncpy(basename, name, sizeof(basename) - 1);
    char* dot = strrchr(basename, '.');
    if (dot) *dot = '\0';

    // Update auth header in case it changed
    updateAuthHeader(g_state.jwt);

    // Create context with binary data and URL
    FetchContext* ctx = new FetchContext(FetchType::Push);
    ctx->setData(data, size);

    char url[512];
    snprintf(url, sizeof(url), "/push?name=%s&file=%s", basename, name);
    ctx->setUrl(url);

    // Store basename in extra for gear.json chaining
    strncpy(ctx->extra, basename, sizeof(ctx->extra) - 1);
    ctx->extra[sizeof(ctx->extra) - 1] = '\0';

    // Push RAW to BASE
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = g_auth_headers;
    attr.requestData = reinterpret_cast<const char*>(ctx->data);
    attr.requestDataSize = ctx->data_size;

    emscripten_fetch(&attr, ctx->url);
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
// ============================================================
// Unified fetch callback - handles all request types with context
// ============================================================
static void onFetchSuccess(emscripten_fetch_t* fetch);
static void onFetchError(emscripten_fetch_t* fetch);

// Helper to cleanup fetch and context
static void cleanupFetch(emscripten_fetch_t* fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    if (ctx) delete ctx;
    emscripten_fetch_close(fetch);
}

// Check if response is stale (auth changed since request was made)
static bool isStaleResponse(emscripten_fetch_t* fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    if (!ctx || !ctx->isValid()) {
        LOG("Ignoring stale response (auth session changed)");
        cleanupFetch(fetch);
        return true;
    }
    return false;
}

static void onFetchSuccess(emscripten_fetch_t* fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    if (!ctx) {
        LOG("ERROR: fetch success with null context");
        emscripten_fetch_close(fetch);
        return;
    }

    // Check for stale response
    if (!ctx->isValid()) {
        LOG("Ignoring stale response (auth session changed)");
        cleanupFetch(fetch);
        return;
    }

    // Parse response
    char buf[4096];
    size_t len = fetch->numBytes < 4095 ? fetch->numBytes : 4095;
    strncpy(buf, fetch->data, len);
    buf[len] = '\0';

    switch (ctx->type) {
        case FetchType::Login: {
            LOG("Login response received");
            LOG(buf);
            if (extractJsonBool(buf, "ok")) {
                g_state.screen = Screen::OTP;
                strcpy(g_state.status_message, "Check console for OTP");
                g_state.error_message[0] = '\0';
            } else {
                strcpy(g_state.error_message, "Login failed");
            }
            g_state.request_pending = false;
            break;
        }

        case FetchType::Verify: {
            LOG("Verify response received");
            LOG(buf);
            if (strstr(buf, "\"error\"")) {
                strcpy(g_state.error_message, "Invalid OTP");
                g_state.request_pending = false;
                break;
            }

            // Extract tokens
            extractJsonString(buf, "jwt", g_state.jwt, sizeof(g_state.jwt));
            extractJsonString(buf, "refresh_token", g_state.refresh_token, sizeof(g_state.refresh_token));
            extractJsonString(buf, "user_id", g_state.user_id, sizeof(g_state.user_id));
            extractJsonString(buf, "itag", g_state.itag, sizeof(g_state.itag));
            extractJsonString(buf, "role", g_state.role, sizeof(g_state.role));

            if (g_state.jwt[0] != '\0') {
                // Update auth header for future requests
                updateAuthHeader(g_state.jwt);
                g_state.screen = Screen::Desktop;
                g_state.error_message[0] = '\0';
                saveAuth(g_state.jwt, g_state.itag, g_state.role, g_state.user_id);
                postNote("labs.open", g_state.itag);
                LOG("Login successful, entering desktop");
            } else {
                strcpy(g_state.error_message, "Verification failed");
            }
            g_state.request_pending = false;
            break;
        }

        case FetchType::List: {
            LOG("List response received");
            LOG(buf);

            // Check for auth failure (401/403 or "Unauthorized" or JSON-RPC error)
            if (fetch->status == 401 || fetch->status == 403 ||
                strstr(buf, "Unauthorized") || strstr(buf, "\"error\"")) {
                LOG("List auth failed - session expired");
                postNote("auth.expired", "");
                clearAuth();
                invalidateRequests();  // Invalidate any other in-flight requests
                g_state.screen = Screen::Login;
                g_state.jwt[0] = '\0';
                g_state.itag[0] = '\0';
                g_state.email[0] = '\0';  // Clear email too for fresh start
                g_state.error_message[0] = '\0';  // Clear any stale error
                strcpy(g_state.status_message, "Session expired - please login again");
                g_state.list_loaded = false;
                g_state.list_loading = false;
                break;
            }

            g_state.pipe_count = 0;

            // Parse items array
            const char* items_start = strstr(buf, "\"items\":[");
            if (items_start) {
                items_start += 9;
                while (*items_start && g_state.pipe_count < AppState::MAX_PIPES) {
                    if (*items_start == '"') {
                        items_start++;
                        const char* end = strchr(items_start, '"');
                        if (end) {
                            size_t plen = end - items_start;
                            if (plen < 64) {
                                strncpy(g_state.pipes[g_state.pipe_count], items_start, plen);
                                g_state.pipes[g_state.pipe_count][plen] = '\0';
                                g_state.pipe_count++;
                            }
                            items_start = end + 1;
                        } else break;
                    } else if (*items_start == ']') break;
                    else items_start++;
                }
            }

            g_state.list_loaded = true;
            g_state.list_loading = false;

            char count_msg[32];
            snprintf(count_msg, sizeof(count_msg), "%d pipes", g_state.pipe_count);
            postNote("labs.list", count_msg);
            break;
        }

        case FetchType::Files: {
            LOG("Files response received");
            g_state.file_count = 0;

            const char* items_start = strstr(buf, "\"items\":[");
            if (items_start) {
                items_start += 9;
                while (*items_start && g_state.file_count < AppState::MAX_FILES) {
                    if (*items_start == '"') {
                        items_start++;
                        const char* end = strchr(items_start, '"');
                        if (end) {
                            size_t plen = end - items_start;
                            if (plen < 64) {
                                strncpy(g_state.files[g_state.file_count], items_start, plen);
                                g_state.files[g_state.file_count][plen] = '\0';
                                g_state.file_count++;
                            }
                            items_start = end + 1;
                        } else break;
                    } else if (*items_start == ']') break;
                    else items_start++;
                }
            }

            g_state.files_loaded = true;
            g_state.files_loading = false;
            break;
        }

        case FetchType::Push: {
            LOG("Push response received");
            // Check if this was RAW push (has extra with basename)
            if (ctx->extra[0] != '\0') {
                LOG("RAW pushed to BASE - creating pipe.json");
                // Chain to pipe.json creation - but only if session still valid
                if (ctx->isValid()) {
                    // Build pipe.json request
                    FetchContext* pipeCtx = new FetchContext(FetchType::Push);
                    char pipe_json[512];
                    snprintf(pipe_json, sizeof(pipe_json),
                        "{\"info\":{\"file\":\"%s\"},\"tune\":{\"step\":[]}}",
                        g_state.raws_name);
                    pipeCtx->setBody(pipe_json);

                    char pipe_url[512];
                    snprintf(pipe_url, sizeof(pipe_url), "/push?name=%s&file=%s.pipe.json",
                             ctx->extra, ctx->extra);
                    pipeCtx->setUrl(pipe_url);

                    // Mark as pipe.json (no extra = don't chain further)
                    pipeCtx->extra[0] = '\0';

                    emscripten_fetch_attr_t attr;
                    emscripten_fetch_attr_init(&attr);
                    strcpy(attr.requestMethod, "POST");
                    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
                    attr.onsuccess = onFetchSuccess;
                    attr.onerror = onFetchError;
                    attr.userData = pipeCtx;
                    attr.requestHeaders = g_auth_headers;
                    attr.requestData = pipeCtx->body;
                    attr.requestDataSize = pipeCtx->body_size;

                    emscripten_fetch(&attr, pipe_url);
                }
            } else {
                // pipe.json push completed
                LOG("pipe.json created");
                // Extract basename from raws_name for note
                char basename[256];
                strncpy(basename, g_state.raws_name, sizeof(basename) - 1);
                char* dot = strrchr(basename, '.');
                if (dot) *dot = '\0';
                postNote("raws.load", basename);
            }
            break;
        }

        case FetchType::Pull: {
            LOG("Pull response received");
            // Handle pull response (binary data)
            break;
        }

        case FetchType::Json: {
            LOG("JSON response received");
            // Parse JSON and store for tree view
            g_state.json_tree = json::parse(fetch->data, fetch->numBytes);
            g_state.json_loaded = true;
            g_state.json_loading = false;

            if (g_state.json_tree) {
                postNote("json.load", g_state.json_file);
            } else {
                postNote("json.error", "parse failed");
            }
            break;
        }
    }

    cleanupFetch(fetch);
}

static void onFetchError(emscripten_fetch_t* fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    if (!ctx) {
        LOG("ERROR: fetch error with null context");
        emscripten_fetch_close(fetch);
        return;
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "Fetch failed: HTTP %d (type %d)", fetch->status, (int)ctx->type);
    LOG(msg);

    // Only update state if session still valid
    if (ctx->isValid()) {
        switch (ctx->type) {
            case FetchType::Login:
            case FetchType::Verify:
                strcpy(g_state.error_message, "Network error");
                g_state.request_pending = false;
                break;

            case FetchType::List:
                g_state.list_loaded = true;  // Prevent retry spam
                g_state.list_loading = false;
                postNote("labs.list", msg);
                break;

            case FetchType::Files:
                g_state.files_loaded = true;
                g_state.files_loading = false;
                break;

            case FetchType::Push:
                postNote("raws.error", msg);
                break;

            case FetchType::Pull:
                break;

            case FetchType::Json:
                g_state.json_loaded = false;
                g_state.json_loading = false;
                postNote("json.error", msg);
                break;
        }
    }

    cleanupFetch(fetch);
}

// Fetch JSON file from BASE for tree view
static void sendJsonRequest(const char* pipe_name, const char* file_name) {
    if (g_state.json_loading) return;
    g_state.json_loading = true;
    g_state.json_loaded = false;
    g_state.json_tree.reset();

    strncpy(g_state.json_file, file_name, sizeof(g_state.json_file) - 1);

    FetchContext* ctx = new FetchContext(FetchType::Json);

    char url[512];
    snprintf(url, sizeof(url), "/pull?name=%s&file=%s", pipe_name, file_name);
    ctx->setUrl(url);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "GET");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = g_auth_headers;

    emscripten_fetch(&attr, ctx->url);
}

static void sendLoginRequest() {
    if (g_state.request_pending) return;
    g_state.request_pending = true;

    FetchContext* ctx = new FetchContext(FetchType::Login);
    char body[512];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"register\",\"params\":{\"email\":\"%s\"},\"id\":1}",
        g_state.email);
    ctx->setBody(body);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = g_json_headers;
    attr.requestData = ctx->body;
    attr.requestDataSize = ctx->body_size;

    emscripten_fetch(&attr, "/jrpc");
}

static void sendVerifyRequest() {
    if (g_state.request_pending) return;
    g_state.request_pending = true;

    FetchContext* ctx = new FetchContext(FetchType::Verify);
    char body[512];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"verify\",\"params\":{\"email\":\"%s\",\"otp\":\"%s\"},\"id\":1}",
        g_state.email, g_state.otp);
    ctx->setBody(body);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = g_json_headers;
    attr.requestData = ctx->body;
    attr.requestDataSize = ctx->body_size;

    emscripten_fetch(&attr, "/jrpc");
}

static void sendListRequest() {
    if (g_state.list_loading) return;
    g_state.list_loading = true;

    // Debug: log JWT length
    char jwt_debug[64];
    snprintf(jwt_debug, sizeof(jwt_debug), "sendListRequest jwt_len=%zu", strlen(g_state.jwt));
    LOG(jwt_debug);

    FetchContext* ctx = new FetchContext(FetchType::List);
    char body[2560];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"list\",\"params\":{\"jwt\":\"%s\"},\"id\":1}",
        g_state.jwt);
    ctx->setBody(body);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = g_json_headers;
    attr.requestData = ctx->body;
    attr.requestDataSize = ctx->body_size;

    emscripten_fetch(&attr, "/jrpc");
}

static void sendFilesRequest(const char* pipe_name) {
    if (g_state.files_loading) return;
    g_state.files_loading = true;
    g_state.files_loaded = false;
    g_state.file_count = 0;

    FetchContext* ctx = new FetchContext(FetchType::Files);
    char body[2560];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"list\",\"params\":{\"jwt\":\"%s\",\"name\":\"%s\"},\"id\":1}",
        g_state.jwt, pipe_name);
    ctx->setBody(body);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = g_json_headers;
    attr.requestData = ctx->body;
    attr.requestDataSize = ctx->body_size;

    emscripten_fetch(&attr, "/jrpc");
}


#else
// Native stubs
static void saveLayout(int, int, int) {}
static int loadLayoutLabs() { return 1; }
static int loadLayoutNote() { return 0; }
static int loadLayoutTune() { return 0; }
static void saveAuth(const char*, const char*, const char*, const char*) {}
static void clearAuth() {}
static int loadAuthJwt(char*, int) { return 0; }
static void loadAuthItag(char*, int) {}
static void loadAuthRole(char*, int) {}
static void loadAuthUserId(char*, int) {}
static void sendLoginRequest() {
    strcpy(g_state.error_message, "Native login not implemented");
}
static void sendVerifyRequest() {
    strcpy(g_state.error_message, "Native verify not implemented");
}
static void sendListRequest() {
    g_state.list_loaded = true;
}
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

    // Status message (info - blue/grey)
    if (g_state.status_message[0] != '\0') {
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "%s", g_state.status_message);
    }

    // Error message (red)
    if (g_state.error_message[0] != '\0') {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", g_state.error_message);
    }

    ImGui::Spacing();

    // Submit button
    bool can_submit = strlen(g_state.email) > 0 && !g_state.request_pending;
    if (!can_submit) ImGui::BeginDisabled();
    if (ImGui::Button(g_state.request_pending ? "Sending..." : "Continue", ImVec2(-1, 32)) || (enter_pressed && can_submit)) {
        g_state.status_message[0] = '\0';  // Clear status on new login attempt
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
    if (g_state.otp[0] == '\0') {
        ImGui::SetKeyboardFocusHere();
    }
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

    // Back button - always works, clears all OTP state
    if (ImGui::SmallButton("< Back")) {
        g_state.screen = Screen::Login;
        g_state.otp[0] = '\0';
        g_state.error_message[0] = '\0';
        g_state.status_message[0] = '\0';
        g_state.request_pending = false;  // Clear any stuck request
    }

    ImGui::End();
}

// Render desktop screen with menu bar
static void render_desktop_screen() {
    ImGuiIO& io = ImGui::GetIO();

    // Load layout on first render
    if (!g_state.layout_loaded) {
        g_state.show_labs_panel = loadLayoutLabs() != 0;
        g_state.show_note_pane = loadLayoutNote() != 0;
        g_state.show_tune_pane = loadLayoutTune() != 0;
        g_state.layout_loaded = true;
    }

    // Track layout changes
    bool prev_labs = g_state.show_labs_panel;
    bool prev_note = g_state.show_note_pane;
    bool prev_tune = g_state.show_tune_pane;

    // Web-style menu bar
    if (ImGui::BeginMainMenuBar()) {
        // Task: section
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Task:");
        ImGui::SameLine(0, 10);

        if (ImGui::SmallButton("Load RAW")) {
            openRawFilePicker();
        }
        ImGui::SameLine(0, 10);

        if (ImGui::SmallButton("Logout")) {
            invalidateRequests();  // Mark any in-flight requests as stale
            g_state.screen = Screen::Login;
            g_state.jwt[0] = '\0';
            g_state.email[0] = '\0';
            g_state.otp[0] = '\0';
            g_state.itag[0] = '\0';
            g_state.list_loaded = false;
            g_state.pipe_count = 0;
            g_state.raws_name[0] = '\0';
            clearAuth();
        }

        // Divider
        ImGui::SameLine(0, 20);
        ImGui::TextColored(ImVec4(0.3f, 0.3f, 0.3f, 1.0f), "|");
        ImGui::SameLine(0, 20);

        // View: section
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "View:");
        ImGui::SameLine(0, 10);

        // LABS toggle
        if (g_state.show_labs_panel) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::SmallButton("[x] LABS")) g_state.show_labs_panel = false;
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            if (ImGui::SmallButton("[ ] LABS")) g_state.show_labs_panel = true;
            ImGui::PopStyleColor();
        }

        ImGui::SameLine(0, 10);

        // Note toggle
        if (g_state.show_note_pane) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::SmallButton("[x] Note")) g_state.show_note_pane = false;
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            if (ImGui::SmallButton("[ ] Note")) g_state.show_note_pane = true;
            ImGui::PopStyleColor();
        }

        ImGui::SameLine(0, 10);

        // Tune toggle
        if (g_state.show_tune_pane) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            if (ImGui::SmallButton("[x] Tune")) g_state.show_tune_pane = false;
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            if (ImGui::SmallButton("[ ] Tune")) g_state.show_tune_pane = true;
            ImGui::PopStyleColor();
        }

        // Right-aligned user info
        float user_width = ImGui::CalcTextSize(g_state.itag).x + 20;
        ImGui::SetCursorPosX(io.DisplaySize.x - user_width);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", g_state.itag);

        ImGui::EndMainMenuBar();
    }

    // Save layout if changed
    if (g_state.show_labs_panel != prev_labs || g_state.show_note_pane != prev_note || g_state.show_tune_pane != prev_tune) {
        saveLayout(g_state.show_labs_panel ? 1 : 0, g_state.show_note_pane ? 1 : 0, g_state.show_tune_pane ? 1 : 0);
    }

    // LABS floating panel - check for labs.open note to trigger list
    if (checkNote("labs.open")) {
        g_state.list_loaded = false;  // Force reload
        if (!g_state.list_loading) {
            sendListRequest();
            postNote("labs.list", "loading");
        }
    }

    // Check for tune.start note to trigger pipeline
    if (checkNote("tune.start")) {
        if (!g_state.tune_running && g_state.current_pipe[0]) {
            startTunePipeline();
        }
    }

    // Check for raws.load note - file was pushed to BASE, refresh list
    if (checkNote("raws.load")) {
        g_state.list_loaded = false;  // Refresh list to show new file
        g_state.files_loaded = false;  // Reload files
        // Extract base name for auto-select
        char base_name[256];
        strncpy(base_name, g_state.raws_name, sizeof(base_name) - 1);
        char* dot = strrchr(base_name, '.');
        if (dot) *dot = '\0';
        strncpy(g_state.current_pipe, base_name, sizeof(g_state.current_pipe) - 1);
        postNote("pipe.select", base_name);
    }

    // Fallback: ensure list loads if not loaded
    if (!g_state.list_loaded && !g_state.list_loading) {
        sendListRequest();
        postNote("labs.list", "loading");
    }

    if (g_state.show_labs_panel) {
        // Check for push.done note to auto-select
        char pushed_name[64];
        if (checkNote("push.done", pushed_name, sizeof(pushed_name))) {
            strncpy(g_state.current_pipe, pushed_name, sizeof(g_state.current_pipe) - 1);
            g_state.files_loaded = false;  // Reload files
            postNote("pipe.select", pushed_name);
        }

        ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

        if (ImGui::Begin("LABS", &g_state.show_labs_panel)) {
            // Pipelines section
            if (g_state.list_loading) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Loading...");
            } else if (g_state.pipe_count == 0 && !g_state.raws_name[0]) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.4f, 1.0f), "Load a RAW file to begin");
            } else if (g_state.pipe_count > 0) {
                ImGui::Text("Pipelines:");
                ImGui::Separator();
                for (int i = 0; i < g_state.pipe_count; i++) {
                    bool is_selected = strcmp(g_state.pipes[i], g_state.current_pipe) == 0;
                    if (ImGui::Selectable(g_state.pipes[i], is_selected)) {
                        if (!is_selected) {
                            strncpy(g_state.current_pipe, g_state.pipes[i], sizeof(g_state.current_pipe) - 1);
                            g_state.files_loaded = false;  // Trigger files reload
                            // Clear JSON viewer when switching pipes
                            g_state.json_file[0] = '\0';
                            g_state.json_tree.reset();
                            g_state.json_loaded = false;
                            postNote("pipe.select", g_state.current_pipe);
                        }
                    }

                    // Show files under selected pipe
                    if (is_selected && g_state.current_pipe[0]) {
                        // Load files if not loaded
                        if (!g_state.files_loaded && !g_state.files_loading) {
                            sendFilesRequest(g_state.current_pipe);
                        }

                        // Display files indented
                        if (g_state.files_loading) {
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "    ...");
                        } else if (g_state.file_count > 0) {
                            for (int f = 0; f < g_state.file_count; f++) {
                                const char* fname = g_state.files[f];
                                bool is_json = strstr(fname, ".json") != nullptr;
                                bool is_selected = strcmp(fname, g_state.json_file) == 0;

                                ImGui::Indent(20.0f);
                                if (is_json) {
                                    // Make JSON files clickable
                                    if (ImGui::Selectable(fname, is_selected)) {
                                        if (!is_selected) {
                                            // Fetch and parse JSON
                                            sendJsonRequest(g_state.current_pipe, fname);
                                        } else {
                                            // Clicking again collapses
                                            g_state.json_file[0] = '\0';
                                            g_state.json_tree.reset();
                                            g_state.json_loaded = false;
                                        }
                                    }

                                    // Show tree view under selected JSON file
                                    if (is_selected && g_state.json_loaded && g_state.json_tree) {
                                        ImGui::Indent(10.0f);
                                        tree::render(g_state.json_tree.get());
                                        ImGui::Unindent(10.0f);
                                    } else if (is_selected && g_state.json_loading) {
                                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "      ...");
                                    }
                                } else {
                                    // Non-JSON files just display
                                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", fname);
                                }
                                ImGui::Unindent(20.0f);
                            }
                        }
                    }
                }
            }
        }
        ImGui::End();
    }

    // Note pane - shows note history
    if (g_state.show_note_pane) {
        ImGui::SetNextWindowSize(ImVec2(350, 250), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20, io.DisplaySize.y - 270),
            ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Note", &g_state.show_note_pane)) {
            // Clear button
            if (ImGui::SmallButton("Clear")) {
                g_note_history_count = 0;
            }
            ImGui::Separator();

            // Scrollable list
            ImGui::BeginChild("NoteScroll", ImVec2(0, 0), false);
            for (int i = 0; i < g_note_history_count; i++) {
                Note& n = g_note_history[i];
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "%s", n.event);
                if (n.data[0]) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", n.data);
                }
            }
            // Auto-scroll to bottom on new note
            if (g_note_history_scroll) {
                ImGui::SetScrollHereY(1.0f);
                g_note_history_scroll = false;
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    // Tune pane - horizontal pipeline stages
    if (g_state.show_tune_pane) {
        // Size based on 4 images: each ~240px wide, 3:2 aspect ratio (~160px tall)
        float stage_width = 240.0f;
        float stage_gap = 10.0f;
        float padding = 32.0f;  // Window padding
        float tune_width = (stage_width * 4) + (stage_gap * 3) + padding;
        float stage_height = stage_width * 2.0f / 3.0f;  // 3:2 aspect
        float header_height = 50.0f;  // Header + separator + labels
        float tune_height = stage_height + header_height + padding;

        ImGui::SetNextWindowSize(ImVec2(tune_width, tune_height), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2((io.DisplaySize.x - tune_width) * 0.5f, 50), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Tune", &g_state.show_tune_pane)) {
            float pane_width = ImGui::GetContentRegionAvail().x;

            // Header: file name left, Tune button right
            if (g_state.current_pipe[0]) {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "%s", g_state.current_pipe);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(no file)");
            }
            ImGui::SameLine(pane_width - 50);
            bool can_tune = g_state.current_pipe[0] != '\0';
            if (!can_tune) ImGui::BeginDisabled();
            if (ImGui::SmallButton("Tune")) {
                postNote("tune.start", g_state.current_pipe);
            }
            if (!can_tune) ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::Spacing();

            // Fixed stage dimensions for horizontal layout
            float stage_gap = 10.0f;
            float stage_width = (pane_width - (3 * stage_gap)) / 4.0f;
            float stage_height = stage_width * 2.0f / 3.0f;  // 3:2 aspect

            // Helper lambda to render a stage with texture
            auto renderStage = [&](const char* label, const char* id, ImVec4 label_color,
                                   unsigned int texture, int tex_w, int tex_h, const char* placeholder) {
                ImGui::BeginGroup();
                ImGui::TextColored(label_color, "%s", label);
                ImGui::BeginChild(id, ImVec2(stage_width, stage_height), true);
                if (texture && tex_w > 0 && tex_h > 0) {
                    float aspect = (float)tex_w / (float)tex_h;
                    float child_width = ImGui::GetContentRegionAvail().x;
                    float child_height = ImGui::GetContentRegionAvail().y;

                    float img_width = child_width;
                    float img_height = img_width / aspect;
                    if (img_height > child_height) {
                        img_height = child_height;
                        img_width = img_height * aspect;
                    }
                    // Center the image
                    float offset_x = (child_width - img_width) * 0.5f;
                    float offset_y = (child_height - img_height) * 0.5f;
                    if (offset_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
                    if (offset_y > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset_y);
                    ImGui::Image((ImTextureID)(intptr_t)texture, ImVec2(img_width, img_height));
                } else if (g_state.current_pipe[0]) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", placeholder);
                } else {
                    ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "—");
                }
                ImGui::EndChild();
                ImGui::EndGroup();
            };

            // Stage 1: GEAR - flat RAW (or preview fallback)
            unsigned int gear_tex = g_state.gear_texture ? g_state.gear_texture : g_state.preview_texture;
            int gear_w = g_state.gear_texture ? g_state.bayer_width / 2 : g_state.preview_width;
            int gear_h = g_state.gear_texture ? g_state.bayer_height / 2 : g_state.preview_height;
            renderStage("GEAR", "stage_gear", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), gear_tex, gear_w, gear_h, "Press Tune");

            ImGui::SameLine(0, stage_gap);

            // Stage 2: LUTE
            renderStage("LUTE", "stage_lute", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), g_state.lute_texture, 0, 0, "[camera profile]");

            ImGui::SameLine(0, stage_gap);

            // Stage 3: DRUM
            renderStage("DRUM", "stage_drum", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), g_state.drum_texture, 0, 0, "[dynamic range]");

            ImGui::SameLine(0, stage_gap);

            // Stage 4: DIFF
            renderStage("DIFF", "stage_diff", ImVec4(0.4f, 0.7f, 1.0f, 1.0f), g_state.diff_texture, 0, 0, "[diff from camera]");
        }
        ImGui::End();
    }

    // Task View pane
    if (g_state.show_task_view) {
        bool all_done = g_state.current_task < 0;
        bool has_error = false;
        for (int i = 0; i < g_state.task_count; i++) {
            if (g_state.tasks[i].status == TaskStatus::Error) {
                has_error = true;
                break;
            }
        }

        // Auto-hide on success
        if (all_done && !has_error) {
            g_state.show_task_view = false;
        } else {
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

                // Shut button - only shown on error
                if (has_error) {
                    if (ImGui::Button("Shut", ImVec2(80, 0))) {
                        g_state.show_task_view = false;
                    }
                }
            }
            ImGui::End();
        }
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

EMSCRIPTEN_KEEPALIVE
int main(int argc, char** argv) {
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
    glfwSwapInterval(1);  // Emscripten uses requestAnimationFrame for vsync
#endif

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

    // Try to restore saved auth session
    if (loadAuthJwt(g_state.jwt, sizeof(g_state.jwt))) {
        loadAuthItag(g_state.itag, sizeof(g_state.itag));
        loadAuthRole(g_state.role, sizeof(g_state.role));
        loadAuthUserId(g_state.user_id, sizeof(g_state.user_id));
        updateAuthHeader(g_state.jwt);  // Set auth header for push/pull
        g_state.screen = Screen::Desktop;

        // Debug: show jwt length and itag
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

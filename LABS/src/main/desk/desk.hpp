// desk.hpp - Desktop app types and shared state
#pragma once

#include "imgui.h"
#include "json.hpp"
#include "pipe.hpp"
#include <GLFW/glfw3.h>
#include <cstdint>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/fetch.h>
#define LOG(msg) emscripten_console_log(msg)
#else
#define LOG(msg) printf("%s\n", msg)
#endif

// Application screens
enum class Screen { Login, OTP, Desktop };

// Task status
enum class TaskStatus { Pending, Running, Done, Error };

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

// Fetch request types
enum class FetchType { Login, Register, Verify, List, Files, Push, Pull, Json, Drop };

// Context for each fetch request
struct FetchContext {
    FetchType type;
    uint32_t session;
    char* body;
    size_t body_size;
    char* url;
    uint8_t* data;
    size_t data_size;
    char extra[256];

    FetchContext(FetchType t);
    ~FetchContext();
    bool isValid() const;
    void setBody(const char* src);
    void setUrl(const char* src);
    void setData(const uint8_t* src, size_t size);
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
    bool show_note_pane = false;
    bool show_tune_pane = false;
    bool layout_loaded = false;
    bool list_loaded = false;
    bool list_loading = false;
    static constexpr int MAX_PIPES = 64;
    char pipes[MAX_PIPES][64] = {};
    int pipe_count = 0;
    char current_pipe[64] = "";

    // Current RAW file
    char raws_name[256] = "";

    // RAW upload dialog
    bool show_raw_dialog = false;
    uint8_t* pending_raw_data = nullptr;
    int pending_raw_size = 0;
    char pending_raw_filename[256] = "";
    char raw_dialog_name[128] = "";
    char raw_dialog_find[512] = "";

    // Files in current pipe
    static constexpr int MAX_FILES = 64;
    char files[MAX_FILES][64] = {};
    int file_count = 0;
    bool files_loaded = false;
    bool files_loading = false;

    // GEAR decoded data (embedded preview)
    pipe::Info* gear_info = nullptr;
    bool gear_decoded = false;
    uint8_t* preview_data = nullptr;
    int preview_width = 0;
    int preview_height = 0;
    unsigned int preview_texture = 0;

    // HEAD output (scene-linear RGB float)
    float* head_rgb = nullptr;
    int head_width = 0;
    int head_height = 0;
    bool head_done = false;

    // Stage textures (8-bit for display)
    uint8_t* head_rgb8 = nullptr;
    unsigned int head_texture = 0;
    unsigned int lute_texture = 0;
    unsigned int drum_texture = 0;
    unsigned int diff_texture = 0;

    // Task queue
    bool show_task_view = false;
    static constexpr int MAX_TASKS = 16;
    Task tasks[MAX_TASKS] = {};
    int task_count = 0;
    int current_task = -1;

    // Tune pipeline
    bool tune_running = false;
    int tune_step = 0;

    // Async request state
    bool request_pending = false;

    // JSON tree viewer
    char json_file[64] = "";
    json::ValuePtr json_tree;
    bool json_loading = false;
    bool json_loaded = false;
};

// Globals
extern AppState g_state;
extern uint32_t g_auth_session;
extern char g_auth_header[2200];
extern const char* g_auth_headers[];
extern const char* g_json_headers[];

// Note constants
static constexpr int MAX_NOTES = 16;
static constexpr int MAX_NOTE_HISTORY = 64;
extern Note g_notes[MAX_NOTES];
extern int g_note_count;
extern Note g_note_history[MAX_NOTE_HISTORY];
extern int g_note_history_count;
extern bool g_note_history_scroll;

// Note functions
void postNote(const char* event, const char* data = "");
bool checkNote(const char* event, char* data_out = nullptr, size_t data_size = 0);

// Task functions
void clearTasks();
int addTask(const char* name);
void setTaskStatus(int idx, TaskStatus status, const char* message = nullptr);

// Auth functions
void updateAuthHeader(const char* jwt);
void invalidateRequests();

// Tune functions
void startTunePipeline();
void runTuneStep();

// Pipe functions
void createPipeJson(const char* pipe_name, const char* raw_filename);

// Texture functions
void uploadPreviewTexture();
void runHeadPipeline(pipe::Flow& data);
void createHeadTexture();

// REST functions
void sendLoginRequest();
void sendRegisterRequest();
void sendVerifyRequest();
void sendListRequest();
void sendFilesRequest(const char* pipe_name);
void sendJsonRequest(const char* pipe_name, const char* file_name);
void sendDropRequest(const char* pipe_name);
void sendPushJsonRequest(const char* pipe_name, const char* filename, const char* json);
void confirmRawUpload();
void cancelRawUpload();
#ifdef __EMSCRIPTEN__
void onFetchSuccess(emscripten_fetch_t* fetch);
void onFetchError(emscripten_fetch_t* fetch);
#endif

// JSON helpers
const char* extractJsonString(const char* json, const char* key, char* out, size_t out_size);
bool extractJsonBool(const char* json, const char* key);

// View functions
void render_login_screen();
void render_otp_screen();
void render_desktop_screen();

// Tile functions
void render_base_tile(float x, float y, float width, float height);
void render_pipe_tile(float x, float y, float width, float height);
void render_note_tile(float x, float y, float width, float height);
void render_head_tile(float x, float y, float width, float height);

// Layout persistence (Emscripten only)
#ifdef __EMSCRIPTEN__
extern "C" {
void saveLayout(int labs, int note, int tune);
int loadLayoutLabs();
int loadLayoutNote();
int loadLayoutTune();
void saveAuth(const char* jwt, const char* itag, const char* role, const char* user_id);
void clearAuth();
int loadAuthJwt(char* out, int maxLen);
void loadAuthItag(char* out, int maxLen);
void loadAuthRole(char* out, int maxLen);
void loadAuthUserId(char* out, int maxLen);
void openRawFilePicker();
}
#else
inline void saveLayout(int, int, int) {}
inline int loadLayoutLabs() { return 1; }
inline int loadLayoutNote() { return 0; }
inline int loadLayoutTune() { return 0; }
inline void saveAuth(const char*, const char*, const char*, const char*) {}
inline void clearAuth() {}
inline int loadAuthJwt(char*, int) { return 0; }
inline void loadAuthItag(char*, int) {}
inline void loadAuthRole(char*, int) {}
inline void loadAuthUserId(char*, int) {}
inline void openRawFilePicker() { LOG("File picker not available"); }
#endif

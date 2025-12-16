// base.cpp - BASE server communication (all emscripten fetch code)

#include "desk.hpp"
#include "gear.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>

// Global auth state
uint32_t g_auth_session = 0;
char g_auth_header[2200] = "";
const char* g_auth_headers[] = {"Authorization", g_auth_header, nullptr};
const char* g_json_headers[] = {"Content-Type", "application/json", nullptr};

// FetchContext implementation
FetchContext::FetchContext(FetchType t) : type(t), session(g_auth_session),
                             body(nullptr), body_size(0),
                             url(nullptr), data(nullptr), data_size(0) {
    extra[0] = '\0';
}

FetchContext::~FetchContext() {
    if (body) free(body);
    if (url) free(url);
    if (data) free(data);
}

bool FetchContext::isValid() const { return session == g_auth_session; }

void FetchContext::setBody(const char* src) {
    body_size = strlen(src);
    body = (char*)malloc(body_size + 1);
    memcpy(body, src, body_size + 1);
}

void FetchContext::setUrl(const char* src) {
    size_t len = strlen(src);
    url = (char*)malloc(len + 1);
    memcpy(url, src, len + 1);
}

void FetchContext::setData(const uint8_t* src, size_t size) {
    data_size = size;
    data = (uint8_t*)malloc(size);
    memcpy(data, src, size);
}

// Auth helpers
void updateAuthHeader(const char* jwt) {
    snprintf(g_auth_header, sizeof(g_auth_header), "Bearer %s", jwt);
}

void invalidateRequests() {
    g_auth_session++;
    LOG("Auth session invalidated");
}

// JSON helpers
const char* extractJsonString(const char* json, const char* key, char* out, size_t out_size) {
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

bool extractJsonBool(const char* json, const char* key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":true", key);
    return strstr(json, pattern) != nullptr;
}

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#include <emscripten/html5.h>

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

// File picker for RAW files
EM_JS(void, openRawFilePicker, (), {
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
            input.value = "";
        });
    }
    input.click();
});

static void cleanupFetch(emscripten_fetch_t* fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    if (ctx) delete ctx;
    emscripten_fetch_close(fetch);
}

void onFetchSuccess(emscripten_fetch_t* fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    if (!ctx) {
        LOG("ERROR: fetch success with null context");
        emscripten_fetch_close(fetch);
        return;
    }

    if (!ctx->isValid()) {
        LOG("Ignoring stale response");
        cleanupFetch(fetch);
        return;
    }

    char buf[4096];
    size_t len = fetch->numBytes < 4095 ? fetch->numBytes : 4095;
    strncpy(buf, fetch->data, len);
    buf[len] = '\0';

    switch (ctx->type) {
        case FetchType::Login: {
            LOG("Login response received");
            if (strstr(buf, "\"ok\":true")) {
                g_state.screen = Screen::OTP;
                strcpy(g_state.status_message, "Code sent! Check your email");
                g_state.error_message[0] = '\0';
            } else {
                extractJsonString(buf, "message", g_state.error_message, sizeof(g_state.error_message));
                if (g_state.error_message[0] == '\0') {
                    strcpy(g_state.error_message, "Login failed. Please try again.");
                }
            }
            g_state.request_pending = false;
            break;
        }

        case FetchType::Register: {
            LOG("Register response received");
            if (strstr(buf, "\"ok\":true")) {
                g_state.screen = Screen::OTP;
                strcpy(g_state.status_message, "Registered! Check your email for code");
                g_state.error_message[0] = '\0';
            } else {
                extractJsonString(buf, "message", g_state.error_message, sizeof(g_state.error_message));
                if (g_state.error_message[0] == '\0') {
                    strcpy(g_state.error_message, "Registration failed. Please try again.");
                }
            }
            g_state.request_pending = false;
            break;
        }

        case FetchType::Verify: {
            LOG("Verify response received");
            if (strstr(buf, "\"error\"")) {
                extractJsonString(buf, "message", g_state.error_message, sizeof(g_state.error_message));
                if (g_state.error_message[0] == '\0') {
                    strcpy(g_state.error_message, "Invalid OTP or verification failed");
                }
                g_state.request_pending = false;
                break;
            }

            extractJsonString(buf, "jwt", g_state.jwt, sizeof(g_state.jwt));
            extractJsonString(buf, "refresh_token", g_state.refresh_token, sizeof(g_state.refresh_token));
            extractJsonString(buf, "user_id", g_state.user_id, sizeof(g_state.user_id));
            extractJsonString(buf, "itag", g_state.itag, sizeof(g_state.itag));
            extractJsonString(buf, "role", g_state.role, sizeof(g_state.role));

            if (g_state.jwt[0] != '\0') {
                updateAuthHeader(g_state.jwt);
                g_state.screen = Screen::Desktop;
                g_state.error_message[0] = '\0';
                saveAuth(g_state.jwt, g_state.itag, g_state.role, g_state.user_id);
                postNote("labs.open", g_state.itag);
                LOG("Login successful");
            } else {
                strcpy(g_state.error_message, "Verification failed");
            }
            g_state.request_pending = false;
            break;
        }

        case FetchType::List: {
            LOG("List response received");
            if (fetch->status == 401 || fetch->status == 403 ||
                strstr(buf, "Unauthorized") || strstr(buf, "\"error\"")) {
                LOG("List auth failed");
                postNote("auth.expired", "");
                clearAuth();
                invalidateRequests();
                g_state.screen = Screen::Login;
                g_state.jwt[0] = '\0';
                g_state.itag[0] = '\0';
                g_state.email[0] = '\0';
                g_state.error_message[0] = '\0';
                strcpy(g_state.status_message, "Session expired. Please login again.");
                g_state.list_loaded = false;
                g_state.list_loading = false;
                break;
            }

            g_state.pipe_count = 0;
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
            if (ctx->extra[0] != '\0') {
                // RAW pushed - create initial pipe.json
                LOG("RAW pushed - creating pipe.json");
                if (ctx->isValid()) {
                    createPipeJson(ctx->extra, g_state.raws_name);
                }
            } else {
                postNote("push.done", "");
                g_state.list_loaded = false;
            }
            break;
        }

        case FetchType::Pull: {
            LOG("Pull response received");
            break;
        }

        case FetchType::Json: {
            LOG("JSON response received");
            g_state.json_tree = json::parse(fetch->data, fetch->numBytes);
            g_state.json_loaded = true;
            g_state.json_loading = false;
            strncpy(g_state.json_file, ctx->extra, sizeof(g_state.json_file) - 1);
            break;
        }

        case FetchType::Drop: {
            LOG("Drop response received");
            if (strstr(buf, "\"ok\":true")) {
                postNote("drop.done", ctx->extra);
                // Clear current pipe if it was the dropped one
                if (strcmp(g_state.current_pipe, ctx->extra) == 0) {
                    g_state.current_pipe[0] = '\0';
                    g_state.files_loaded = false;
                    g_state.json_file[0] = '\0';
                    g_state.json_tree.reset();
                    g_state.json_loaded = false;
                }
                // Refresh list
                g_state.list_loaded = false;
            } else {
                postNote("drop.error", ctx->extra);
            }
            break;
        }
    }

    cleanupFetch(fetch);
}

void onFetchError(emscripten_fetch_t* fetch) {
    FetchContext* ctx = static_cast<FetchContext*>(fetch->userData);
    char msg[256];
    snprintf(msg, sizeof(msg), "Fetch error: %d %s", fetch->status, fetch->statusText);
    LOG(msg);

    if (ctx) {
        switch (ctx->type) {
            case FetchType::Login:
            case FetchType::Register:
            case FetchType::Verify:
                strcpy(g_state.error_message, "Network error. Please try again.");
                g_state.request_pending = false;
                break;
            case FetchType::List:
                g_state.list_loading = false;
                break;
            case FetchType::Files:
                g_state.files_loading = false;
                break;
            case FetchType::Json:
                g_state.json_loading = false;
                break;
            default:
                break;
        }
    }

    cleanupFetch(fetch);
}

void sendLoginRequest() {
    g_state.request_pending = true;

    FetchContext* ctx = new FetchContext(FetchType::Login);

    char body[256];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"login\",\"params\":{\"email\":\"%s\"},\"id\":1}",
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

void sendRegisterRequest() {
    g_state.request_pending = true;

    FetchContext* ctx = new FetchContext(FetchType::Register);

    char body[256];
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

void sendVerifyRequest() {
    g_state.request_pending = true;

    FetchContext* ctx = new FetchContext(FetchType::Verify);

    char body[256];
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

void sendListRequest() {
    g_state.list_loading = true;

    FetchContext* ctx = new FetchContext(FetchType::List);

    char body[512];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"list\",\"params\":{},\"id\":1}");
    ctx->setBody(body);

    static char auth_list[2200];
    snprintf(auth_list, sizeof(auth_list), "Bearer %s", g_state.jwt);
    static const char* list_headers[] = {"Content-Type", "application/json", "Authorization", auth_list, nullptr};

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = list_headers;
    attr.requestData = ctx->body;
    attr.requestDataSize = ctx->body_size;

    emscripten_fetch(&attr, "/jrpc");
}

void sendFilesRequest(const char* pipe_name) {
    g_state.files_loading = true;

    FetchContext* ctx = new FetchContext(FetchType::Files);

    char body[512];
    snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"method\":\"list\",\"params\":{\"name\":\"%s\"},\"id\":1}",
        pipe_name);
    ctx->setBody(body);

    static char auth_files[2200];
    snprintf(auth_files, sizeof(auth_files), "Bearer %s", g_state.jwt);
    static const char* files_headers[] = {"Content-Type", "application/json", "Authorization", auth_files, nullptr};

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = files_headers;
    attr.requestData = ctx->body;
    attr.requestDataSize = ctx->body_size;

    emscripten_fetch(&attr, "/jrpc");
}

void sendJsonRequest(const char* pipe_name, const char* file_name) {
    g_state.json_loading = true;
    g_state.json_loaded = false;
    g_state.json_tree.reset();

    FetchContext* ctx = new FetchContext(FetchType::Json);
    strncpy(ctx->extra, file_name, sizeof(ctx->extra) - 1);

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

void sendDropRequest(const char* pipe_name) {
    FetchContext* ctx = new FetchContext(FetchType::Drop);
    strncpy(ctx->extra, pipe_name, sizeof(ctx->extra) - 1);

    char url[512];
    snprintf(url, sizeof(url), "/drop?name=%s", pipe_name);
    ctx->setUrl(url);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "DELETE");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = g_auth_headers;

    emscripten_fetch(&attr, ctx->url);
}

void sendPushJsonRequest(const char* pipe_name, const char* filename, const char* json) {
    FetchContext* ctx = new FetchContext(FetchType::Push);
    ctx->setBody(json);

    char url[512];
    snprintf(url, sizeof(url), "/push?name=%s&file=%s", pipe_name, filename);
    ctx->setUrl(url);

    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    strcpy(attr.requestMethod, "POST");
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = onFetchSuccess;
    attr.onerror = onFetchError;
    attr.userData = ctx;
    attr.requestHeaders = g_auth_headers;
    attr.requestData = ctx->body;
    attr.requestDataSize = ctx->body_size;

    emscripten_fetch(&attr, ctx->url);
}

// File upload callback - shows dialog for name/find
extern "C" {
EMSCRIPTEN_KEEPALIVE
void onRawFileLoaded(const char* name, uint8_t* data, int size) {
    LOG("RAW file loaded - showing dialog");

    // Store pending data
    if (g_state.pending_raw_data) {
        free(g_state.pending_raw_data);
    }
    g_state.pending_raw_data = (uint8_t*)malloc(size);
    memcpy(g_state.pending_raw_data, data, size);
    g_state.pending_raw_size = size;

    strncpy(g_state.pending_raw_filename, name, sizeof(g_state.pending_raw_filename) - 1);
    g_state.pending_raw_filename[sizeof(g_state.pending_raw_filename) - 1] = '\0';

    // Default name from filename (without extension)
    strncpy(g_state.raw_dialog_name, name, sizeof(g_state.raw_dialog_name) - 1);
    char* dot = strrchr(g_state.raw_dialog_name, '.');
    if (dot) *dot = '\0';

    g_state.raw_dialog_find[0] = '\0';
    g_state.show_raw_dialog = true;
}
}

void confirmRawUpload() {
    if (!g_state.pending_raw_data || g_state.raw_dialog_name[0] == '\0') {
        return;
    }

    LOG("Confirming RAW upload");

    strncpy(g_state.raws_name, g_state.pending_raw_filename, sizeof(g_state.raws_name) - 1);
    g_state.raws_name[sizeof(g_state.raws_name) - 1] = '\0';

    updateAuthHeader(g_state.jwt);

    FetchContext* ctx = new FetchContext(FetchType::Push);
    ctx->setData(g_state.pending_raw_data, g_state.pending_raw_size);

    char url[512];
    snprintf(url, sizeof(url), "/push?name=%s&file=%s", g_state.raw_dialog_name, g_state.pending_raw_filename);
    ctx->setUrl(url);

    strncpy(ctx->extra, g_state.raw_dialog_name, sizeof(ctx->extra) - 1);
    ctx->extra[sizeof(ctx->extra) - 1] = '\0';

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

    // Clean up pending state
    free(g_state.pending_raw_data);
    g_state.pending_raw_data = nullptr;
    g_state.pending_raw_size = 0;
    g_state.show_raw_dialog = false;
}

void cancelRawUpload() {
    if (g_state.pending_raw_data) {
        free(g_state.pending_raw_data);
        g_state.pending_raw_data = nullptr;
    }
    g_state.pending_raw_size = 0;
    g_state.pending_raw_filename[0] = '\0';
    g_state.raw_dialog_name[0] = '\0';
    g_state.raw_dialog_find[0] = '\0';
    g_state.show_raw_dialog = false;
}

#else
// Native stubs
void sendLoginRequest() { LOG("Login not available in native"); }
void sendRegisterRequest() { LOG("Register not available in native"); }
void sendVerifyRequest() { LOG("Verify not available in native"); }
void sendListRequest() { LOG("List not available in native"); }
void sendFilesRequest(const char*) { LOG("Files not available in native"); }
void sendJsonRequest(const char*, const char*) { LOG("JSON fetch not available in native"); }
void sendDropRequest(const char*) { LOG("Drop not available in native"); }
void sendPushJsonRequest(const char*, const char*, const char*) { LOG("Push JSON not available in native"); }
void confirmRawUpload() { LOG("Upload not available in native"); }
void cancelRawUpload() { g_state.show_raw_dialog = false; }
#endif

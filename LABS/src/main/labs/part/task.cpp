// task.cpp - Task management and tune pipeline

#include "desk.hpp"
#include "gear.hpp"
#include <cstring>
#include <cstdio>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif

void clearTasks() {
    g_state.task_count = 0;
    g_state.current_task = -1;
    for (int i = 0; i < AppState::MAX_TASKS; i++) {
        g_state.tasks[i].name[0] = '\0';
        g_state.tasks[i].message[0] = '\0';
        g_state.tasks[i].status = TaskStatus::Pending;
    }
}

int addTask(const char* name) {
    if (g_state.task_count >= AppState::MAX_TASKS) return -1;
    int idx = g_state.task_count++;
    strncpy(g_state.tasks[idx].name, name, sizeof(g_state.tasks[idx].name) - 1);
    g_state.tasks[idx].message[0] = '\0';
    g_state.tasks[idx].status = TaskStatus::Pending;
    return idx;
}

void setTaskStatus(int idx, TaskStatus status, const char* message) {
    if (idx < 0 || idx >= g_state.task_count) return;
    g_state.tasks[idx].status = status;
    if (message) {
        strncpy(g_state.tasks[idx].message, message, sizeof(g_state.tasks[idx].message) - 1);
    }
}

void startTunePipeline() {
    if (!g_state.current_pipe[0]) return;

    clearTasks();
    addTask("gear.load");
    addTask("pipe.post");
    addTask("wgpu.open");
    addTask("pipe.view");
    addTask("lute.tune");
    addTask("pipe.view");
    addTask("drum.tune");
    addTask("pipe.view");
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

#ifdef __EMSCRIPTEN__
void runTuneStep() {
    if (!g_state.tune_running || g_state.tune_step >= g_state.task_count) {
        g_state.tune_running = false;
        g_state.current_task = -1;
        postNote("tune.done", g_state.current_pipe);
        return;
    }

    int step = g_state.tune_step;
    setTaskStatus(step, TaskStatus::Running);

    const char* task_name = g_state.tasks[step].name;

    if (strcmp(task_name, "gear.load") == 0) {
        if (!g_state.current_pipe[0]) {
            setTaskStatus(step, TaskStatus::Error, "no pipe selected");
            g_state.tune_running = false;
            postNote("tune.error", "select a pipeline first");
            return;
        }

        setTaskStatus(step, TaskStatus::Running, "loading pipe...");

        static char pipe_url[512];
        snprintf(pipe_url, sizeof(pipe_url), "/pull?name=%s&file=%s.pipe.json",
                 g_state.current_pipe, g_state.current_pipe);

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

            char raw_filename[256] = "";
            extractJsonString(fetch->data, "file", raw_filename, sizeof(raw_filename));
            emscripten_fetch_close(fetch);

            if (!raw_filename[0]) {
                setTaskStatus(step, TaskStatus::Error, "no file in pipe.json");
                g_state.tune_running = false;
                postNote("tune.error", "pipe.json missing info.file");
                return;
            }

            strncpy(g_state.raws_name, raw_filename, sizeof(g_state.raws_name) - 1);
            g_state.raws_name[sizeof(g_state.raws_name) - 1] = '\0';

            setTaskStatus(step, TaskStatus::Running, "pulling RAW...");

            static char raw_url[512];
            snprintf(raw_url, sizeof(raw_url), "/pull?name=%s&file=%s",
                     g_state.current_pipe, raw_filename);

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

                pipe::Data result = gear::sony::decode(fetch->data, fetch->numBytes);
                emscripten_fetch_close(fetch);

                if (result.info.text("error")[0] != '\0') {
                    setTaskStatus(step, TaskStatus::Error, result.info.text("error").c_str());
                    g_state.tune_running = false;
                    postNote("tune.error", result.info.text("error").c_str());
                    return;
                }

                if (result.page) {
                    auto* buf = static_cast<gear::sony::BayerBuffer*>(result.page);

                    // Extract preview before running HEAD
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
                        uploadPreviewTexture();
                    }

                    g_state.gear_decoded = true;

                    // Run HEAD pipeline (blc → wb → demosaic → cst → crop)
                    runHeadPipeline(result);
                    createHeadTexture();

                    // Store gear info (move from result after HEAD pipeline)
                    if (g_state.gear_info) delete g_state.gear_info;
                    g_state.gear_info = new pipe::Info(std::move(result.info));

                    // HEAD complete - pipe.json already created by rest.cpp on RAW push
                    char msg[128];
                    snprintf(msg, sizeof(msg), "%dx%d", g_state.head_width, g_state.head_height);
                    setTaskStatus(step, TaskStatus::Done, msg);
                    postNote("head.done", msg);

                    g_state.tune_step++;
                    runTuneStep();
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
        return;

    } else if (strcmp(task_name, "pipe.post") == 0) {
        if (!g_state.preview_data || g_state.preview_width <= 0 || g_state.preview_height <= 0) {
            setTaskStatus(step, TaskStatus::Error, "no preview data");
            g_state.tune_running = false;
            postNote("tune.error", "no preview to push");
            return;
        }

        setTaskStatus(step, TaskStatus::Running, "encoding PNG...");

        static std::vector<uint8_t> png_data;
        png_data = pipe::encodePng(g_state.preview_data,
            g_state.preview_width, g_state.preview_height);

        if (png_data.empty()) {
            setTaskStatus(step, TaskStatus::Error, "PNG encode failed");
            g_state.tune_running = false;
            postNote("tune.error", "PNG encoding failed");
            return;
        }

        char msg[64];
        snprintf(msg, sizeof(msg), "PNG %zu bytes", png_data.size());
        setTaskStatus(step, TaskStatus::Running, msg);
        postNote("pipe.post", msg);

        static char post_url[512];
        snprintf(post_url, sizeof(post_url), "/push?name=%s&file=%s.png",
                 g_state.current_pipe, g_state.current_pipe);

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "POST");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.requestHeaders = g_auth_headers;
        attr.requestData = reinterpret_cast<const char*>(png_data.data());
        attr.requestDataSize = png_data.size();
        attr.userData = reinterpret_cast<void*>(static_cast<intptr_t>(step));

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            int step = static_cast<int>(reinterpret_cast<intptr_t>(fetch->userData));
            emscripten_fetch_close(fetch);

            setTaskStatus(step, TaskStatus::Done, "pushed");
            postNote("pipe.post", "done");

            g_state.tune_step++;
            g_state.current_task = g_state.tune_step < g_state.task_count ? g_state.tune_step : -1;
            runTuneStep();
        };

        attr.onerror = [](emscripten_fetch_t* fetch) {
            int step = static_cast<int>(reinterpret_cast<intptr_t>(fetch->userData));
            setTaskStatus(step, TaskStatus::Error, "push failed");
            g_state.tune_running = false;
            postNote("tune.error", "PNG push failed");
            emscripten_fetch_close(fetch);
        };

        emscripten_fetch(&attr, post_url);
        return;

    } else if (strcmp(task_name, "wgpu.open") == 0) {
        postNote("wgpu.open", "");
        setTaskStatus(step, TaskStatus::Done, "ready");
    } else if (strcmp(task_name, "pipe.view") == 0) {
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

    g_state.tune_step++;
    g_state.current_task = g_state.tune_step < g_state.task_count ? g_state.tune_step : -1;

    if (g_state.tune_step < g_state.task_count) {
        runTuneStep();
    } else {
        g_state.tune_running = false;
        postNote("tune.done", g_state.current_pipe);
    }
}
#else
void runTuneStep() {
    LOG("Tune pipeline not available in native build");
    g_state.tune_running = false;
}
#endif

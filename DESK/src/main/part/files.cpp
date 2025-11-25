// files.cpp - File operations for DESK

#include "files.hpp"
#include "ImGuiFileDialog.h"

#include <fstream>
#include <sstream>
#include <regex>

// LABS pipe
#include <pipe.hpp>
#include <tool.hpp>

// stb_image for PNG loading
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <GLFW/glfw3.h>

namespace desk {

namespace fs = std::filesystem;

// ============================================================
// JSON Helpers (minimal implementation for desk/pipe formats)
// ============================================================

namespace json {

std::string read_file(const fs::path& path) {
    std::ifstream f(path);
    if (!f) return "";
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool write_file(const fs::path& path, const std::string& content) {
    std::ofstream f(path);
    if (!f) return false;
    f << content;
    return true;
}

std::string extract_string(const std::string& json, const std::string& key) {
    std::regex re("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch match;
    if (std::regex_search(json, match, re)) {
        return match[1].str();
    }
    return "";
}

bool extract_bool(const std::string& json, const std::string& key, bool def = false) {
    std::regex re("\"" + key + "\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (std::regex_search(json, match, re)) {
        return match[1].str() == "true";
    }
    return def;
}

float extract_float(const std::string& json, const std::string& key, float def = 0.5f) {
    std::regex re("\"" + key + "\"\\s*:\\s*([0-9.]+)");
    std::smatch match;
    if (std::regex_search(json, match, re)) {
        return std::stof(match[1].str());
    }
    return def;
}

} // namespace json

// ============================================================
// Project Discovery
// ============================================================

void scan_projects(State& state) {
    state.projects.clear();
    state.selection.project = -1;
    state.selection.link = -1;

    if (!state.root_folder_set || !fs::exists(state.root_folder)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(state.root_folder)) {
        if (!entry.is_regular_file()) continue;

        auto path = entry.path();
        auto ext = path.extension().string();

        // Look for .ARW files (case insensitive)
        if (ext != ".ARW" && ext != ".arw") continue;

        Project proj;
        proj.name = path.stem().string();
        proj.raw_path = path;
        proj.desk_path = state.root_folder / (proj.name + ".desk.json");
        proj.pipe_path = state.root_folder / (proj.name + ".pipe.json");
        proj.png_path = state.root_folder / (proj.name + ".png");

        // Load or create desk.json
        if (fs::exists(proj.desk_path)) {
            load_desk_json(proj);
        } else {
            save_desk_json(proj);
        }

        // Skip hidden projects
        if (proj.hidden) continue;

        // Load or create pipe.json
        if (fs::exists(proj.pipe_path)) {
            load_pipe_json(proj);
        } else {
            save_pipe_json(proj);
        }

        state.projects.push_back(proj);
    }

    state.status_message = "Found " + std::to_string(state.projects.size()) + " projects";
}

// ============================================================
// Sidecar File Operations
// ============================================================

bool load_desk_json(Project& project) {
    std::string content = json::read_file(project.desk_path);
    if (content.empty()) return false;

    project.hidden = json::extract_bool(content, "hidden", false);
    return true;
}

bool save_desk_json(const Project& project) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": \"1.0\",\n";
    ss << "  \"hidden\": " << (project.hidden ? "true" : "false") << "\n";
    ss << "}\n";
    return json::write_file(project.desk_path, ss.str());
}

bool load_pipe_json(Project& project) {
    std::string content = json::read_file(project.pipe_path);
    if (content.empty()) return false;

    project.decoder = json::extract_string(content, "decoder");
    if (project.decoder.empty()) {
        project.decoder = "sony_arw2";
    }

    project.tail_output = json::extract_string(content, "output");
    if (project.tail_output.empty()) {
        project.tail_output = project.name + ".png";
    }

    // Parse links array
    project.links.clear();

    size_t links_start = content.find("\"links\"");
    if (links_start == std::string::npos) return true;

    size_t arr_start = content.find('[', links_start);
    if (arr_start == std::string::npos) return true;

    // Find matching bracket
    int bracket_count = 1;
    size_t pos = arr_start + 1;
    while (pos < content.size() && bracket_count > 0) {
        if (content[pos] == '[') bracket_count++;
        else if (content[pos] == ']') bracket_count--;
        pos++;
    }
    size_t arr_end = pos;

    std::string links_content = content.substr(arr_start + 1, arr_end - arr_start - 2);

    // Parse each link object
    size_t link_start = 0;
    while ((link_start = links_content.find('{', link_start)) != std::string::npos) {
        int brace_count = 1;
        size_t link_end = link_start + 1;
        while (link_end < links_content.size() && brace_count > 0) {
            if (links_content[link_end] == '{') brace_count++;
            else if (links_content[link_end] == '}') brace_count--;
            link_end++;
        }

        std::string link_json = links_content.substr(link_start, link_end - link_start);
        std::string name = json::extract_string(link_json, "name");
        if (name.empty()) name = "Link";

        Link link(name);

        // Parse modules if present
        if (link_json.find("\"geometric\"") != std::string::npos) {
            link.geometric.dials["crop_top"] = json::extract_float(link_json, "top", 0.0f);
            link.geometric.dials["crop_right"] = json::extract_float(link_json, "right", 0.0f);
            link.geometric.dials["crop_bottom"] = json::extract_float(link_json, "bottom", 0.0f);
            link.geometric.dials["crop_left"] = json::extract_float(link_json, "left", 0.0f);
            link.geometric.dials["scale"] = json::extract_float(link_json, "scale", 0.5f);
            link.geometric.dials["tilt_angle"] = json::extract_float(link_json, "tilt_angle", 0.5f);
        }

        if (link_json.find("\"color_correction\"") != std::string::npos) {
            link.color_correction.dials["temperature"] = json::extract_float(link_json, "temperature", 0.5f);
            link.color_correction.dials["tint"] = json::extract_float(link_json, "tint", 0.5f);
            link.color_correction.dials["exposure"] = json::extract_float(link_json, "value", 0.5f);
        }

        if (link_json.find("\"tone_mapping\"") != std::string::npos) {
            link.tone_mapping.dials["contrast"] = json::extract_float(link_json, "contrast", 0.5f);
            link.tone_mapping.dials["highlights"] = json::extract_float(link_json, "highlights", 0.5f);
            link.tone_mapping.dials["shadows"] = json::extract_float(link_json, "shadows", 0.5f);
            link.tone_mapping.dials["black"] = json::extract_float(link_json, "black", 0.15f);
            link.tone_mapping.dials["white"] = json::extract_float(link_json, "white", 0.85f);
        }

        if (link_json.find("\"global_color\"") != std::string::npos) {
            link.global_color.dials["vibrance"] = json::extract_float(link_json, "vibrance", 0.5f);
            link.global_color.dials["saturation"] = json::extract_float(link_json, "saturation", 0.5f);
            link.global_color.dials["color_density"] = json::extract_float(link_json, "color_density", 0.5f);
        }

        if (link_json.find("\"detail\"") != std::string::npos) {
            link.detail.dials["sharpen_amount"] = json::extract_float(link_json, "amount", 0.5f);
            link.detail.dials["sharpen_radius"] = json::extract_float(link_json, "radius", 0.5f);
            link.detail.dials["denoise_luminance"] = json::extract_float(link_json, "luminance", 0.5f);
            link.detail.dials["denoise_chroma"] = json::extract_float(link_json, "chroma", 0.5f);
        }

        project.links.push_back(link);
        link_start = link_end;
    }

    return true;
}

bool save_pipe_json(const Project& project) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": \"1.0\",\n";
    ss << "  \"decoder\": \"" << project.decoder << "\",\n";
    ss << "  \"links\": [";

    for (size_t i = 0; i < project.links.size(); i++) {
        const auto& link = project.links[i];
        if (i > 0) ss << ",";
        ss << "\n    {\n";
        ss << "      \"name\": \"" << link.name << "\",\n";
        ss << "      \"modules\": {\n";

        // Geometric
        ss << "        \"geometric\": {\n";
        ss << "          \"crop\": {\n";
        ss << "            \"top\": " << link.geometric.dials.at("crop_top") << ",\n";
        ss << "            \"right\": " << link.geometric.dials.at("crop_right") << ",\n";
        ss << "            \"bottom\": " << link.geometric.dials.at("crop_bottom") << ",\n";
        ss << "            \"left\": " << link.geometric.dials.at("crop_left") << "\n";
        ss << "          },\n";
        ss << "          \"zoom\": { \"scale\": " << link.geometric.dials.at("scale") << " },\n";
        ss << "          \"rotation\": { \"tilt_angle\": " << link.geometric.dials.at("tilt_angle") << " }\n";
        ss << "        },\n";

        // Color correction
        ss << "        \"color_correction\": {\n";
        ss << "          \"white_balance\": {\n";
        ss << "            \"temperature\": " << link.color_correction.dials.at("temperature") << ",\n";
        ss << "            \"tint\": " << link.color_correction.dials.at("tint") << "\n";
        ss << "          },\n";
        ss << "          \"exposure\": { \"value\": " << link.color_correction.dials.at("exposure") << " }\n";
        ss << "        },\n";

        // Tone mapping
        ss << "        \"tone_mapping\": {\n";
        ss << "          \"contrast\": { \"value\": " << link.tone_mapping.dials.at("contrast") << " },\n";
        ss << "          \"curve_adjustment\": {\n";
        ss << "            \"highlights\": " << link.tone_mapping.dials.at("highlights") << ",\n";
        ss << "            \"shadows\": " << link.tone_mapping.dials.at("shadows") << "\n";
        ss << "          },\n";
        ss << "          \"clipping_point\": {\n";
        ss << "            \"black\": " << link.tone_mapping.dials.at("black") << ",\n";
        ss << "            \"white\": " << link.tone_mapping.dials.at("white") << "\n";
        ss << "          }\n";
        ss << "        },\n";

        // Global color
        ss << "        \"global_color\": {\n";
        ss << "          \"vibrance\": " << link.global_color.dials.at("vibrance") << ",\n";
        ss << "          \"saturation\": " << link.global_color.dials.at("saturation") << ",\n";
        ss << "          \"color_density\": " << link.global_color.dials.at("color_density") << "\n";
        ss << "        },\n";

        // Selective color
        ss << "        \"selective_color\": {\n";
        const char* colors[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
        for (int c = 0; c < 8; c++) {
            std::string color = colors[c];
            ss << "          \"" << color << "\": {\n";
            ss << "            \"hue\": " << link.selective_color.dials.at(color + "_hue") << ",\n";
            ss << "            \"saturation\": " << link.selective_color.dials.at(color + "_saturation") << ",\n";
            ss << "            \"luminance\": " << link.selective_color.dials.at(color + "_luminance") << "\n";
            ss << "          }" << (c < 7 ? "," : "") << "\n";
        }
        ss << "        },\n";

        // Detail
        ss << "        \"detail\": {\n";
        ss << "          \"sharpen\": {\n";
        ss << "            \"amount\": " << link.detail.dials.at("sharpen_amount") << ",\n";
        ss << "            \"radius\": " << link.detail.dials.at("sharpen_radius") << "\n";
        ss << "          },\n";
        ss << "          \"denoise\": {\n";
        ss << "            \"luminance\": " << link.detail.dials.at("denoise_luminance") << ",\n";
        ss << "            \"chroma\": " << link.detail.dials.at("denoise_chroma") << "\n";
        ss << "          }\n";
        ss << "        }\n";

        ss << "      }\n";
        ss << "    }";
    }

    ss << "\n  ],\n";
    ss << "  \"tail\": {\n";
    ss << "    \"output\": \"" << (project.tail_output.empty() ? project.name + ".png" : project.tail_output) << "\"\n";
    ss << "  }\n";
    ss << "}\n";

    return json::write_file(project.pipe_path, ss.str());
}

// ============================================================
// Project Operations
// ============================================================

bool create_project(State& state, const fs::path& raw_file) {
    if (!fs::exists(raw_file)) {
        state.error_message = "File not found: " + raw_file.string();
        return false;
    }

    std::string name = raw_file.stem().string();
    fs::path dest = state.root_folder / raw_file.filename();

    if (raw_file != dest) {
        try {
            fs::copy_file(raw_file, dest, fs::copy_options::skip_existing);
        } catch (const std::exception& e) {
            state.error_message = "Failed to copy file: " + std::string(e.what());
            return false;
        }
    }

    Project proj;
    proj.name = name;
    proj.raw_path = dest;
    proj.desk_path = state.root_folder / (name + ".desk.json");
    proj.pipe_path = state.root_folder / (name + ".pipe.json");
    proj.png_path = state.root_folder / (name + ".png");

    save_desk_json(proj);
    save_pipe_json(proj);

    state.projects.push_back(proj);
    state.status_message = "Created project: " + name;
    state.needs_reprocess = true;

    return true;
}

bool render_project(State& state, const Project& project) {
    state.status_message = "Rendering: " + project.name;

    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(project.raw_path.string()));
    if (!sink) {
        state.error_message = "Failed to read: " + project.name;
        return false;
    }

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));
    if (!head) {
        state.error_message = "Failed to decode: " + project.name;
        return false;
    }

    if (!head->body().tail().save(project.png_path.string())) {
        state.error_message = "Failed to save: " + project.name;
        return false;
    }

    state.status_message = "Rendered: " + project.name;
    return true;
}

// ============================================================
// Texture Operations
// ============================================================

bool load_texture(State& state, const fs::path& png_path) {
    unload_texture(state);

    if (!fs::exists(png_path)) {
        return false;
    }

    int width, height, channels;
    unsigned char* data = stbi_load(png_path.string().c_str(), &width, &height, &channels, 4);
    if (!data) {
        state.error_message = "Failed to load image: " + png_path.string();
        return false;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    state.texture.id = texture;
    state.texture.width = width;
    state.texture.height = height;
    state.texture.loaded = true;

    return true;
}

void unload_texture(State& state) {
    if (state.texture.loaded && state.texture.id != 0) {
        GLuint tex = state.texture.id;
        glDeleteTextures(1, &tex);
    }
    state.texture.reset();
}

// ============================================================
// RAW Metadata
// ============================================================

bool load_raw_info(State& state, const Project& project) {
    state.raw_info.clear();

    if (!fs::exists(project.raw_path)) {
        return false;
    }

    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(project.raw_path.string()));
    if (!sink) {
        return false;
    }

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));
    if (!head) {
        return false;
    }

    pipe::Info info = head->data().info();
    for (const auto& [key, value] : info) {
        state.raw_info[key] = value;
    }

    return true;
}

// ============================================================
// File Dialogs
// ============================================================

void open_folder_dialog() {
    IGFD::FileDialogConfig config;
    config.path = ".";
    ImGuiFileDialog::Instance()->OpenDialog(
        "ChooseFolderDlg",
        "Select Root Folder",
        nullptr,
        config
    );
}

void open_raw_file_dialog() {
    IGFD::FileDialogConfig config;
    config.path = ".";
    ImGuiFileDialog::Instance()->OpenDialog(
        "ChooseRawDlg",
        "Select RAW File",
        ".ARW,.arw",
        config
    );
}

} // namespace desk

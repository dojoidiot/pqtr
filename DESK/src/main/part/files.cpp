// files.cpp - File operations for DESK

#include "files.hpp"
#include "geos.hpp"  // desk::geos for dome animation
#include "ImGuiFileDialog.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>

// LABS pipe
#include <pipe.hpp>
#include <tool.hpp>
#include <geos.hpp>  // geos:: optimizer
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

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
// Tuning Thread State
// ============================================================

static std::thread g_tune_thread;
static std::atomic<bool> g_tune_running{false};
static std::atomic<bool> g_tune_finished{false};
static std::mutex g_tune_mutex;
static Link g_tune_result;
static float g_tune_final_loss = 0.0f;

// ============================================================
// Project Discovery
// ============================================================

void scan_projects(State& state) {
    state.projects.clear();
    state.selection.project = -1;
    state.selection.link = -1;

    if (!state.project_folder_set || !fs::exists(state.project_folder)) {
        return;
    }

    for (const auto& entry : fs::directory_iterator(state.project_folder)) {
        if (!entry.is_regular_file()) continue;

        auto path = entry.path();
        auto ext = path.extension().string();

        // Look for .ARW files (case insensitive)
        if (ext != ".ARW" && ext != ".arw") continue;

        Project proj;
        proj.name = path.stem().string();
        proj.raw_path = path;
        proj.desk_path = state.project_folder / (proj.name + ".desk.json");
        proj.pipe_path = state.project_folder / (proj.name + ".pipe.json");
        proj.png_path = state.project_folder / (proj.name + ".png");

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

// Helper: Write link modules to stream (shared by save_pipe_json and save_link_json)
static void write_link_modules(std::ostringstream& ss, const Link& link, const std::string& indent) {
    // Geometric
    ss << indent << "\"geometric\": {\n";
    ss << indent << "  \"crop\": {\n";
    ss << indent << "    \"top\": " << link.geometric.dials.at("crop_top") << ",\n";
    ss << indent << "    \"right\": " << link.geometric.dials.at("crop_right") << ",\n";
    ss << indent << "    \"bottom\": " << link.geometric.dials.at("crop_bottom") << ",\n";
    ss << indent << "    \"left\": " << link.geometric.dials.at("crop_left") << "\n";
    ss << indent << "  },\n";
    ss << indent << "  \"zoom\": { \"scale\": " << link.geometric.dials.at("scale") << " },\n";
    ss << indent << "  \"rotation\": { \"tilt_angle\": " << link.geometric.dials.at("tilt_angle") << " }\n";
    ss << indent << "},\n";

    // Color correction
    ss << indent << "\"color_correction\": {\n";
    ss << indent << "  \"white_balance\": {\n";
    ss << indent << "    \"temperature\": " << link.color_correction.dials.at("temperature") << ",\n";
    ss << indent << "    \"tint\": " << link.color_correction.dials.at("tint") << "\n";
    ss << indent << "  },\n";
    ss << indent << "  \"exposure\": { \"value\": " << link.color_correction.dials.at("exposure") << " }\n";
    ss << indent << "},\n";

    // Tone mapping
    ss << indent << "\"tone_mapping\": {\n";
    ss << indent << "  \"contrast\": { \"value\": " << link.tone_mapping.dials.at("contrast") << " },\n";
    ss << indent << "  \"curve_adjustment\": {\n";
    ss << indent << "    \"highlights\": " << link.tone_mapping.dials.at("highlights") << ",\n";
    ss << indent << "    \"shadows\": " << link.tone_mapping.dials.at("shadows") << "\n";
    ss << indent << "  },\n";
    ss << indent << "  \"clipping_point\": {\n";
    ss << indent << "    \"black\": " << link.tone_mapping.dials.at("black") << ",\n";
    ss << indent << "    \"white\": " << link.tone_mapping.dials.at("white") << "\n";
    ss << indent << "  }\n";
    ss << indent << "},\n";

    // Global color
    ss << indent << "\"global_color\": {\n";
    ss << indent << "  \"vibrance\": " << link.global_color.dials.at("vibrance") << ",\n";
    ss << indent << "  \"saturation\": " << link.global_color.dials.at("saturation") << ",\n";
    ss << indent << "  \"color_density\": " << link.global_color.dials.at("color_density") << "\n";
    ss << indent << "},\n";

    // Selective color
    ss << indent << "\"selective_color\": {\n";
    const char* colors[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
    for (int c = 0; c < 8; c++) {
        std::string color = colors[c];
        ss << indent << "  \"" << color << "\": {\n";
        ss << indent << "    \"hue\": " << link.selective_color.dials.at(color + "_hue") << ",\n";
        ss << indent << "    \"saturation\": " << link.selective_color.dials.at(color + "_saturation") << ",\n";
        ss << indent << "    \"luminance\": " << link.selective_color.dials.at(color + "_luminance") << "\n";
        ss << indent << "  }" << (c < 7 ? "," : "") << "\n";
    }
    ss << indent << "},\n";

    // Detail
    ss << indent << "\"detail\": {\n";
    ss << indent << "  \"sharpen\": {\n";
    ss << indent << "    \"amount\": " << link.detail.dials.at("sharpen_amount") << ",\n";
    ss << indent << "    \"radius\": " << link.detail.dials.at("sharpen_radius") << "\n";
    ss << indent << "  },\n";
    ss << indent << "  \"denoise\": {\n";
    ss << indent << "    \"luminance\": " << link.detail.dials.at("denoise_luminance") << ",\n";
    ss << indent << "    \"chroma\": " << link.detail.dials.at("denoise_chroma") << "\n";
    ss << indent << "  }\n";
    ss << indent << "}\n";
}

bool save_link_json(const Link& link, const fs::path& path) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": \"1.0\",\n";
    ss << "  \"name\": \"" << link.name << "\",\n";
    ss << "  \"modules\": {\n";
    write_link_modules(ss, link, "    ");
    ss << "  }\n";
    ss << "}\n";
    return json::write_file(path, ss.str());
}

bool load_link_json(Link& link, const fs::path& path) {
    std::string content = json::read_file(path);
    if (content.empty()) return false;

    // Get name
    std::string name = json::extract_string(content, "name");
    if (!name.empty()) {
        link.name = name;
    }

    // Parse modules
    if (content.find("\"geometric\"") != std::string::npos) {
        link.geometric.dials["crop_top"] = json::extract_float(content, "top", 0.0f);
        link.geometric.dials["crop_right"] = json::extract_float(content, "right", 0.0f);
        link.geometric.dials["crop_bottom"] = json::extract_float(content, "bottom", 0.0f);
        link.geometric.dials["crop_left"] = json::extract_float(content, "left", 0.0f);
        link.geometric.dials["scale"] = json::extract_float(content, "scale", 0.5f);
        link.geometric.dials["tilt_angle"] = json::extract_float(content, "tilt_angle", 0.5f);
    }

    if (content.find("\"color_correction\"") != std::string::npos) {
        link.color_correction.dials["temperature"] = json::extract_float(content, "temperature", 0.5f);
        link.color_correction.dials["tint"] = json::extract_float(content, "tint", 0.5f);
        link.color_correction.dials["exposure"] = json::extract_float(content, "value", 0.5f);
    }

    if (content.find("\"tone_mapping\"") != std::string::npos) {
        link.tone_mapping.dials["contrast"] = json::extract_float(content, "contrast", 0.5f);
        link.tone_mapping.dials["highlights"] = json::extract_float(content, "highlights", 0.5f);
        link.tone_mapping.dials["shadows"] = json::extract_float(content, "shadows", 0.5f);
        link.tone_mapping.dials["black"] = json::extract_float(content, "black", 0.15f);
        link.tone_mapping.dials["white"] = json::extract_float(content, "white", 0.85f);
    }

    if (content.find("\"global_color\"") != std::string::npos) {
        link.global_color.dials["vibrance"] = json::extract_float(content, "vibrance", 0.5f);
        link.global_color.dials["saturation"] = json::extract_float(content, "saturation", 0.5f);
        link.global_color.dials["color_density"] = json::extract_float(content, "color_density", 0.5f);
    }

    if (content.find("\"detail\"") != std::string::npos) {
        link.detail.dials["sharpen_amount"] = json::extract_float(content, "amount", 0.0f);
        link.detail.dials["sharpen_radius"] = json::extract_float(content, "radius", 0.4f);
        link.detail.dials["denoise_luminance"] = json::extract_float(content, "luminance", 0.0f);
        link.detail.dials["denoise_chroma"] = json::extract_float(content, "chroma", 0.0f);
    }

    if (content.find("\"selective_color\"") != std::string::npos) {
        const char* colors[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};
        for (const char* color : colors) {
            // Look for each color section and extract its values
            std::string color_section = std::string("\"") + color + "\"";
            size_t pos = content.find(color_section);
            if (pos != std::string::npos) {
                // Find the section for this color
                size_t brace_start = content.find('{', pos);
                size_t brace_end = content.find('}', brace_start);
                if (brace_start != std::string::npos && brace_end != std::string::npos) {
                    std::string color_json = content.substr(brace_start, brace_end - brace_start + 1);
                    link.selective_color.dials[std::string(color) + "_hue"] = json::extract_float(color_json, "hue", 0.5f);
                    link.selective_color.dials[std::string(color) + "_saturation"] = json::extract_float(color_json, "saturation", 0.5f);
                    link.selective_color.dials[std::string(color) + "_luminance"] = json::extract_float(color_json, "luminance", 0.5f);
                }
            }
        }
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
    fs::path dest = state.project_folder / raw_file.filename();

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
    proj.desk_path = state.project_folder / (name + ".desk.json");
    proj.pipe_path = state.project_folder / (name + ".pipe.json");
    proj.png_path = state.project_folder / (name + ".png");

    save_desk_json(proj);
    save_pipe_json(proj);

    state.projects.push_back(proj);
    state.status_message = "Created project: " + name;
    state.needs_reprocess = true;

    return true;
}

// Helper to apply Link dials to pipe::Body::Link
static void apply_link_dials(const Link& src, pipe::Body::Link& dst) {
    // Color Correction
    auto get_dial = [](const Module& m, const char* key, float def) {
        auto it = m.dials.find(key);
        return (it != m.dials.end()) ? it->second : def;
    };

    dst.colorCorrection().exposure().set(get_dial(src.color_correction, "exposure", 0.5f));
    dst.colorCorrection().whiteBalance().temperature(get_dial(src.color_correction, "temperature", 0.5f));
    dst.colorCorrection().whiteBalance().tint(get_dial(src.color_correction, "tint", 0.5f));

    // Tone Mapping
    dst.toneMapping().contrast().set(get_dial(src.tone_mapping, "contrast", 0.5f));
    dst.toneMapping().curveAdjustment().highlights().set(get_dial(src.tone_mapping, "highlights", 0.5f));
    dst.toneMapping().curveAdjustment().shadows().set(get_dial(src.tone_mapping, "shadows", 0.5f));
    dst.toneMapping().clippingPoint().black().set(get_dial(src.tone_mapping, "black", 0.15f));
    dst.toneMapping().clippingPoint().white().set(get_dial(src.tone_mapping, "white", 0.85f));

    // Global Color
    dst.globalColor().vibrance().set(get_dial(src.global_color, "vibrance", 0.5f));
    dst.globalColor().saturation().set(get_dial(src.global_color, "saturation", 0.5f));
    dst.globalColor().colourDensity().set(get_dial(src.global_color, "color_density", 0.5f));

    // Selective Color
    auto set_hsl = [&src, &get_dial](pipe::Body::Link::SelectiveColour::HslAdjust& hsl, const char* color) {
        std::string hue_key = std::string(color) + "_hue";
        std::string sat_key = std::string(color) + "_saturation";
        std::string lum_key = std::string(color) + "_luminance";
        hsl.hue(get_dial(src.selective_color, hue_key.c_str(), 0.5f));
        hsl.saturation(get_dial(src.selective_color, sat_key.c_str(), 0.5f));
        hsl.luminance(get_dial(src.selective_color, lum_key.c_str(), 0.5f));
    };

    set_hsl(dst.selectiveColour().red(), "red");
    set_hsl(dst.selectiveColour().orange(), "orange");
    set_hsl(dst.selectiveColour().yellow(), "yellow");
    set_hsl(dst.selectiveColour().green(), "green");
    set_hsl(dst.selectiveColour().cyan(), "cyan");
    set_hsl(dst.selectiveColour().blue(), "blue");
    set_hsl(dst.selectiveColour().purple(), "purple");
    set_hsl(dst.selectiveColour().magenta(), "magenta");

    // Detail
    dst.detail().sharpen().amount(get_dial(src.detail, "sharpen_amount", 0.5f));
    dst.detail().sharpen().radius(get_dial(src.detail, "sharpen_radius", 0.5f));
    dst.detail().denoise().luminance().set(get_dial(src.detail, "denoise_luminance", 0.5f));
    dst.detail().denoise().chroma().set(get_dial(src.detail, "denoise_chroma", 0.5f));

    // Geometric (if needed)
    dst.geometric().crop().crop_top(get_dial(src.geometric, "crop_top", 0.0f));
    dst.geometric().crop().crop_right(get_dial(src.geometric, "crop_right", 0.0f));
    dst.geometric().crop().crop_bottom(get_dial(src.geometric, "crop_bottom", 0.0f));
    dst.geometric().crop().crop_left(get_dial(src.geometric, "crop_left", 0.0f));
    dst.geometric().zoom().scale(get_dial(src.geometric, "scale", 0.0f));
    dst.geometric().rotation().tiltAngle(get_dial(src.geometric, "tilt_angle", 0.5f));

    // LUT Curve (if present)
    if (!src.lut3d.empty()) {
        constexpr int LUT_SIZE = pipe::Body::Link::LutCurve::LUT_SIZE;
        if (static_cast<int>(src.lut3d.size()) == LUT_SIZE) {
            dst.lutCurve().setLut(src.lut3d.data());
            fprintf(stderr, "[apply_link_dials] Applied LUT (%d floats) to link '%s'\n",
                    LUT_SIZE, src.name.c_str());
        }
    } else {
        dst.lutCurve().reset();
        fprintf(stderr, "[apply_link_dials] No LUT for link '%s'\n", src.name.c_str());
    }
}

// Generic helper to upload cv::Mat to an OpenGL texture
static bool upload_to_texture(Texture& tex, const cv::Mat& bgr) {
    if (bgr.empty()) {
        return false;
    }

    // Convert BGR to RGBA for OpenGL
    cv::Mat rgba;
    cv::cvtColor(bgr, rgba, cv::COLOR_BGR2RGBA);

    // Ensure continuous data for OpenGL
    if (!rgba.isContinuous()) {
        rgba = rgba.clone();
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Set pixel unpack alignment (OpenGL default is 4, but our rows may not be aligned)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.cols, rgba.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);  // Restore default

    tex.id = texture;
    tex.width = rgba.cols;
    tex.height = rgba.rows;
    tex.loaded = true;

    return true;
}

// Helper to create and upload OpenGL texture from cv::Mat (main texture)
static bool upload_texture(State& state, const cv::Mat& bgr) {
    unload_texture(state);
    return upload_to_texture(state.texture, bgr);
}

bool render_to_texture(State& state, const Project& project, int size) {
    std::string size_str = (size == 0) ? "full" : std::to_string(size) + "px";
    state.status_message = "Rendering (" + size_str + "): " + project.name;
    state.is_working = true;

    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(project.raw_path.string()));
    if (!sink) {
        state.error_message = "Failed to read: " + project.name;
        state.is_working = false;
        return false;
    }

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));
    if (!head) {
        state.error_message = "Failed to decode: " + project.name;
        state.is_working = false;
        return false;
    }

    // Get body at working size (scales data BEFORE processing for speed)
    pipe::Body& body = head->body(size);

    // Add links and apply dial values
    for (const auto& link : project.links) {
        pipe::Body::Link& pipe_link = body.add(link.name);
        apply_link_dials(link, pipe_link);

        // Debug: print what we applied (all 45 dials)
        fprintf(stderr, "RENDER APPLY [%s]:\n", link.name.c_str());
        auto get_d = [&](const Module& m, const char* k, float def) {
            auto it = m.dials.find(k);
            return (it != m.dials.end()) ? it->second : def;
        };
        fprintf(stderr, "  CC: exp=%.3f temp=%.3f tint=%.3f\n",
            get_d(link.color_correction, "exposure", 0.5f),
            get_d(link.color_correction, "temperature", 0.5f),
            get_d(link.color_correction, "tint", 0.5f));
        fprintf(stderr, "  TM: con=%.3f hi=%.3f sh=%.3f blk=%.3f wht=%.3f\n",
            get_d(link.tone_mapping, "contrast", 0.5f),
            get_d(link.tone_mapping, "highlights", 0.5f),
            get_d(link.tone_mapping, "shadows", 0.5f),
            get_d(link.tone_mapping, "black", 0.15f),
            get_d(link.tone_mapping, "white", 0.85f));
        fprintf(stderr, "  GC: vib=%.3f sat=%.3f den=%.3f\n",
            get_d(link.global_color, "vibrance", 0.5f),
            get_d(link.global_color, "saturation", 0.5f),
            get_d(link.global_color, "color_density", 0.5f));
        fprintf(stderr, "  SC: R(%.2f,%.2f,%.2f) O(%.2f,%.2f,%.2f) Y(%.2f,%.2f,%.2f) G(%.2f,%.2f,%.2f)\n",
            get_d(link.selective_color, "red_hue", 0.5f),
            get_d(link.selective_color, "red_saturation", 0.5f),
            get_d(link.selective_color, "red_luminance", 0.5f),
            get_d(link.selective_color, "orange_hue", 0.5f),
            get_d(link.selective_color, "orange_saturation", 0.5f),
            get_d(link.selective_color, "orange_luminance", 0.5f),
            get_d(link.selective_color, "yellow_hue", 0.5f),
            get_d(link.selective_color, "yellow_saturation", 0.5f),
            get_d(link.selective_color, "yellow_luminance", 0.5f),
            get_d(link.selective_color, "green_hue", 0.5f),
            get_d(link.selective_color, "green_saturation", 0.5f),
            get_d(link.selective_color, "green_luminance", 0.5f));
        fprintf(stderr, "      C(%.2f,%.2f,%.2f) B(%.2f,%.2f,%.2f) P(%.2f,%.2f,%.2f) M(%.2f,%.2f,%.2f)\n",
            get_d(link.selective_color, "cyan_hue", 0.5f),
            get_d(link.selective_color, "cyan_saturation", 0.5f),
            get_d(link.selective_color, "cyan_luminance", 0.5f),
            get_d(link.selective_color, "blue_hue", 0.5f),
            get_d(link.selective_color, "blue_saturation", 0.5f),
            get_d(link.selective_color, "blue_luminance", 0.5f),
            get_d(link.selective_color, "purple_hue", 0.5f),
            get_d(link.selective_color, "purple_saturation", 0.5f),
            get_d(link.selective_color, "purple_luminance", 0.5f),
            get_d(link.selective_color, "magenta_hue", 0.5f),
            get_d(link.selective_color, "magenta_saturation", 0.5f),
            get_d(link.selective_color, "magenta_luminance", 0.5f));
        fprintf(stderr, "  DT: shAmt=%.3f shRad=%.3f dnL=%.3f dnC=%.3f\n",
            get_d(link.detail, "sharpen_amount", 0.5f),
            get_d(link.detail, "sharpen_radius", 0.5f),
            get_d(link.detail, "denoise_luminance", 0.5f),
            get_d(link.detail, "denoise_chroma", 0.5f));
    }

    // Get display-ready view from body (runs pipe at working size, gamma encodes)
    pipe::View display = body.view();

    // Copy to CPU and upload to OpenGL
    cv::Mat cpu;
    display.copyTo(cpu);


    if (!upload_texture(state, cpu)) {
        state.error_message = "Failed to upload texture: " + project.name;
        state.is_working = false;
        return false;
    }

    state.status_message = "Rendered (" + size_str + "): " + project.name;
    state.is_working = false;
    return true;
}

bool export_project(State& state, const Project& project) {
    state.status_message = "Exporting: " + project.name;
    state.is_working = true;

    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(project.raw_path.string()));
    if (!sink) {
        state.error_message = "Failed to read: " + project.name;
        state.is_working = false;
        return false;
    }

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));
    if (!head) {
        state.error_message = "Failed to decode: " + project.name;
        state.is_working = false;
        return false;
    }

    // Get body
    pipe::Body& body = head->body();

    // Add links and apply dial values
    for (const auto& link : project.links) {
        pipe::Body::Link& pipe_link = body.add(link.name);
        apply_link_dials(link, pipe_link);
    }

    // Save full resolution PNG
    if (!body.tail().save(project.png_path.string(), 0)) {
        state.error_message = "Failed to export: " + project.name;
        state.is_working = false;
        return false;
    }

    state.status_message = "Exported: " + project.name;
    state.is_working = false;
    return true;
}

// ============================================================
// Tune - Optimize dials to match camera preview
// ============================================================

// Helper to extract dial values from pipe::Body::Link into desk::Link
static void extract_link_dials(pipe::Body::Link& src, Link& dst) {
    // Color Correction (3 dials)
    dst.color_correction.dials["exposure"] = src.colorCorrection().exposure().get();
    dst.color_correction.dials["temperature"] = src.colorCorrection().whiteBalance().temperature();
    dst.color_correction.dials["tint"] = src.colorCorrection().whiteBalance().tint();

    // Tone Mapping (5 dials)
    dst.tone_mapping.dials["contrast"] = src.toneMapping().contrast().get();
    dst.tone_mapping.dials["highlights"] = src.toneMapping().curveAdjustment().highlights().get();
    dst.tone_mapping.dials["shadows"] = src.toneMapping().curveAdjustment().shadows().get();
    dst.tone_mapping.dials["black"] = src.toneMapping().clippingPoint().black().get();
    dst.tone_mapping.dials["white"] = src.toneMapping().clippingPoint().white().get();

    // Global Color (3 dials)
    dst.global_color.dials["vibrance"] = src.globalColor().vibrance().get();
    dst.global_color.dials["saturation"] = src.globalColor().saturation().get();
    dst.global_color.dials["color_density"] = src.globalColor().colourDensity().get();

    // Selective Color (24 dials: 8 colors × 3 HSL)
    auto& sel = src.selectiveColour();
    dst.selective_color.dials["red_hue"] = sel.red().hue();
    dst.selective_color.dials["red_saturation"] = sel.red().saturation();
    dst.selective_color.dials["red_luminance"] = sel.red().luminance();
    dst.selective_color.dials["orange_hue"] = sel.orange().hue();
    dst.selective_color.dials["orange_saturation"] = sel.orange().saturation();
    dst.selective_color.dials["orange_luminance"] = sel.orange().luminance();
    dst.selective_color.dials["yellow_hue"] = sel.yellow().hue();
    dst.selective_color.dials["yellow_saturation"] = sel.yellow().saturation();
    dst.selective_color.dials["yellow_luminance"] = sel.yellow().luminance();
    dst.selective_color.dials["green_hue"] = sel.green().hue();
    dst.selective_color.dials["green_saturation"] = sel.green().saturation();
    dst.selective_color.dials["green_luminance"] = sel.green().luminance();
    dst.selective_color.dials["cyan_hue"] = sel.cyan().hue();
    dst.selective_color.dials["cyan_saturation"] = sel.cyan().saturation();
    dst.selective_color.dials["cyan_luminance"] = sel.cyan().luminance();
    dst.selective_color.dials["blue_hue"] = sel.blue().hue();
    dst.selective_color.dials["blue_saturation"] = sel.blue().saturation();
    dst.selective_color.dials["blue_luminance"] = sel.blue().luminance();
    dst.selective_color.dials["purple_hue"] = sel.purple().hue();
    dst.selective_color.dials["purple_saturation"] = sel.purple().saturation();
    dst.selective_color.dials["purple_luminance"] = sel.purple().luminance();
    dst.selective_color.dials["magenta_hue"] = sel.magenta().hue();
    dst.selective_color.dials["magenta_saturation"] = sel.magenta().saturation();
    dst.selective_color.dials["magenta_luminance"] = sel.magenta().luminance();

    // Detail (4 dials)
    dst.detail.dials["sharpen_amount"] = src.detail().sharpen().amount();
    dst.detail.dials["sharpen_radius"] = src.detail().sharpen().radius();
    dst.detail.dials["denoise_luminance"] = src.detail().denoise().luminance().get();
    dst.detail.dials["denoise_chroma"] = src.detail().denoise().chroma().get();

    // LUT Curve (if estimated by geos)
    if (src.lutCurve().isEstimated()) {
        constexpr int LUT_SIZE = pipe::Body::Link::LutCurve::LUT_SIZE;
        dst.lut3d.resize(LUT_SIZE);
        const float* lut = src.lutCurve().lut();
        std::copy(lut, lut + LUT_SIZE, dst.lut3d.begin());
    } else {
        dst.lut3d.clear();
    }
}

bool run_tune(State& state, Project& project) {
    state.status_message = "Tuning: " + project.name;
    state.is_working = true;

    // Enable geos tuning mode (shows optimizer position, stops random drift)
    desk::set_geos_tuning(true);

    // Open RAW
    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(project.raw_path.string()));
    if (!sink) {
        state.error_message = "Failed to read: " + project.name;
        state.is_working = false;
        desk::set_geos_tuning(false);
        return false;
    }

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));
    if (!head) {
        state.error_message = "Failed to decode: " + project.name;
        state.is_working = false;
        desk::set_geos_tuning(false);
        return false;
    }

    // Get target: embedded camera preview
    pipe::View preview = head->view().view();
    if (preview.empty()) {
        state.error_message = "No embedded preview: " + project.name;
        state.is_working = false;
        desk::set_geos_tuning(false);
        return false;
    }

    // Get body at preview size for speed
    int preview_size = std::max(preview.cols, preview.rows);
    pipe::Body& body = head->body(preview_size);

    // Create "Base" link
    pipe::Body::Link& tuneLink = body.add("Base");

    // Initialize dials to neutral
    tuneLink.colorCorrection().exposure().set(0.5f);
    tuneLink.colorCorrection().whiteBalance().temperature(0.5f);
    tuneLink.colorCorrection().whiteBalance().tint(0.5f);
    tuneLink.toneMapping().contrast().set(0.5f);
    tuneLink.toneMapping().curveAdjustment().highlights().set(0.5f);
    tuneLink.toneMapping().curveAdjustment().shadows().set(0.5f);
    tuneLink.toneMapping().clippingPoint().black().set(0.5f);
    tuneLink.toneMapping().clippingPoint().white().set(0.5f);
    tuneLink.globalColor().vibrance().set(0.5f);
    tuneLink.globalColor().saturation().set(0.5f);
    tuneLink.globalColor().colourDensity().set(0.5f);

    // Create geos task with preview as target
    pqtr::Hold<geos::Task> task = geos::make(preview);

    // Configure optimizer
    geos::Config config;
    config.skip_edge = false;
    config.skip_lut = false;  // Enable LUT estimation
    config.skip_regional = true;
    config.geos_max_iter = 300;
    config.geos_threshold = 0.01f;  // 1% target
    config.geos_mode = geos::Mode::FULL_35D;
    config.optimizer = geos::Optimizer::HYBRID;

    // Progress callback - update status and geos dome (every 10 iterations)
    auto progress = [&state](const geos::Progress& p) -> bool {
        if (p.stage == geos::Progress::Stage::GEOS && (p.iteration % 10 == 0)) {
            // Update status message
            char buf[64];
            snprintf(buf, sizeof(buf), "Tuning: %.1f%% (iter %d)",
                     p.loss.spectral * 100.0f, p.iteration);
            state.status_message = buf;

            // Update geos dome visualization
            float r = std::min(1.0f, p.loss.spectral * 10.0f);
            float theta = p.iteration * 0.15f;
            desk::set_geos_progress(r, theta, p.loss.spectral);
        }
        return true;  // Continue
    };

    // Run optimization
    geos::Result result = task->run(body, tuneLink, config, progress);

    // Extract optimized dials into a new desk::Link
    Link baseLink("Base");
    extract_link_dials(tuneLink, baseLink);

    // Add or replace "Base" link in project
    bool found = false;
    for (auto& link : project.links) {
        if (link.name == "Base") {
            link = baseLink;
            found = true;
            break;
        }
    }
    if (!found) {
        project.links.insert(project.links.begin(), baseLink);
    }

    // Save pipe.json
    save_pipe_json(project);

    // Update status
    char buf[128];
    snprintf(buf, sizeof(buf), "Tuned: %.1f%% error", result.loss.spectral * 100.0f);
    state.status_message = buf;
    state.is_working = false;
    state.needs_reprocess = true;

    // Disable geos tuning mode
    desk::set_geos_tuning(false);

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

void unload_base_texture(State& state) {
    if (state.base_texture.loaded && state.base_texture.id != 0) {
        GLuint tex = state.base_texture.id;
        glDeleteTextures(1, &tex);
    }
    state.base_texture.reset();
}

void unload_embedded_texture(State& state) {
    if (state.embedded_texture.loaded && state.embedded_texture.id != 0) {
        GLuint tex = state.embedded_texture.id;
        glDeleteTextures(1, &tex);
    }
    state.embedded_texture.reset();
    state.has_embedded = false;
}

// Helper to convert pipe::View to cv::Mat suitable for OpenGL texture upload
static cv::Mat view_to_mat(pipe::View view) {
    if (view.empty()) return cv::Mat();

    cv::Mat cpu;
    view.copyTo(cpu);
    if (cpu.empty()) return cv::Mat();

    // Convert to RGBA for OpenGL
    cv::Mat rgba;
    if (cpu.channels() == 3) {
        cv::cvtColor(cpu, rgba, cv::COLOR_BGR2RGBA);
    } else if (cpu.channels() == 4) {
        rgba = cpu;
    } else {
        return cv::Mat();
    }

    // Convert to 8-bit if needed
    if (rgba.depth() != CV_8U) {
        cv::Mat temp;
        rgba.convertTo(temp, CV_8UC4, 255.0);
        rgba = temp;
    }

    return rgba;
}

bool load_embedded_preview(State& state, const Project& project) {
    unload_embedded_texture(state);
    unload_base_texture(state);
    state.has_embedded = false;

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

    // Load base texture (scene-linear from RAWS)
    // Apply simple gamma for visualization (scene-linear is too dark otherwise)
    pipe::View base_view = head->data().view();
    if (!base_view.empty()) {
        cv::Mat base_cpu;
        base_view.copyTo(base_cpu);
        if (!base_cpu.empty() && base_cpu.channels() == 3) {
            // Apply simple gamma 2.2 for visualization
            cv::Mat base_float;
            base_cpu.convertTo(base_float, CV_32F, 1.0/255.0);
            cv::pow(base_float, 1.0/2.2, base_float);
            cv::Mat base_8u;
            base_float.convertTo(base_8u, CV_8U, 255.0);
            upload_to_texture(state.base_texture, base_8u);
        }
    }

    // Load embedded preview texture (camera JPEG)
    pipe::View view = head->view().view();
    cv::Mat rgba = view_to_mat(view);
    if (rgba.empty()) {
        return false;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba.cols, rgba.rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data);

    state.embedded_texture.id = texture;
    state.embedded_texture.width = rgba.cols;
    state.embedded_texture.height = rgba.rows;
    state.embedded_texture.loaded = true;
    state.has_embedded = true;

    return true;
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

void open_raw_file_dialog(const State& state) {
    IGFD::FileDialogConfig config;
    config.path = state.raw_source_folder.string();
    ImGuiFileDialog::Instance()->OpenDialog(
        "ChooseRawDlg",
        "Select RAW File",
        ".ARW,.arw",
        config
    );
}

void open_link_save_dialog(const State& state) {
    IGFD::FileDialogConfig config;
    config.path = state.project_folder.string();
    // Suggest filename based on current link name if available
    if (state.has_project() && state.selection.link >= 0) {
        const auto& proj = state.current_project();
        if (state.selection.link < static_cast<int>(proj.links.size())) {
            config.fileName = proj.links[state.selection.link].name + ".link.json";
        }
    }
    ImGuiFileDialog::Instance()->OpenDialog(
        "SaveLinkDlg",
        "Save Link Preset",
        ".link.json",
        config
    );
}

void open_link_load_dialog(const State& state) {
    IGFD::FileDialogConfig config;
    config.path = state.project_folder.string();
    ImGuiFileDialog::Instance()->OpenDialog(
        "LoadLinkDlg",
        "Load Link Preset",
        ".link.json,.json",
        config
    );
}

// ============================================================
// Async Tuning (Background Thread)
// ============================================================

// Thread function that runs the optimizer
static void tune_thread_func(const fs::path raw_path, const std::string project_name) {
    // Enable geos tuning mode
    desk::set_geos_tuning(true);

    Link result_link("Base");
    float final_loss = 1.0f;

    // Change to project directory so geos finds etc/aceo_full.json etc.
    fs::path project_dir = raw_path.parent_path();
    fs::current_path(project_dir);

    // Open RAW
    pqtr::Hold<pqtr::Sink> sink(pqtr::Tool::read(raw_path.string()));
    if (!sink) {
        g_tune_finished = true;
        g_tune_running = false;
        desk::set_geos_tuning(false);
        return;
    }

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(sink));
    if (!head) {
        g_tune_finished = true;
        g_tune_running = false;
        desk::set_geos_tuning(false);
        return;
    }

    // Get target: embedded camera preview
    pipe::View preview = head->view().view();
    if (preview.empty()) {
        g_tune_finished = true;
        g_tune_running = false;
        desk::set_geos_tuning(false);
        return;
    }

    // Get body at preview size for speed
    int preview_size = std::max(preview.cols, preview.rows);
    pipe::Body& body = head->body(preview_size);

    // Create "Base" link
    pipe::Body::Link& tuneLink = body.add("Base");

    // Initialize dials to neutral
    tuneLink.colorCorrection().exposure().set(0.5f);
    tuneLink.colorCorrection().whiteBalance().temperature(0.5f);
    tuneLink.colorCorrection().whiteBalance().tint(0.5f);
    tuneLink.toneMapping().contrast().set(0.5f);
    tuneLink.toneMapping().curveAdjustment().highlights().set(0.5f);
    tuneLink.toneMapping().curveAdjustment().shadows().set(0.5f);
    tuneLink.toneMapping().clippingPoint().black().set(0.5f);
    tuneLink.toneMapping().clippingPoint().white().set(0.5f);
    tuneLink.globalColor().vibrance().set(0.5f);
    tuneLink.globalColor().saturation().set(0.5f);
    tuneLink.globalColor().colourDensity().set(0.5f);

    // Save base.png - what image looks like before optimization (neutral dials + sigmoid + gamma)
    std::string stem = raw_path.stem().string();
    fs::path base_path = project_dir / (stem + ".base.png");
    {
        pipe::View baseView = body.view();
        cv::Mat base_cpu;
        baseView.copyTo(base_cpu);
        if (!base_cpu.empty()) {
            cv::imwrite(base_path.string(), base_cpu);
        }
    }

    // Create geos task with preview as target
    pqtr::Hold<geos::Task> task = geos::make(preview);

    // Configure optimizer
    geos::Config config;
    config.skip_edge = false;
    config.skip_lut = false;
    config.skip_regional = true;
    config.geos_max_iter = 300;
    config.geos_threshold = 0.01f;
    config.geos_mode = geos::Mode::FULL_35D;
    config.optimizer = geos::Optimizer::HYBRID;

    // Progress callback - update geos dome (every 10 iterations to reduce overhead)
    auto progress = [](const geos::Progress& p) -> bool {
        if (p.stage == geos::Progress::Stage::GEOS && (p.iteration % 10 == 0)) {
            float r = std::min(1.0f, p.loss.spectral * 10.0f);
            float theta = p.iteration * 0.15f;
            desk::set_geos_progress(r, theta, p.loss.spectral);
        }
        return g_tune_running;  // Allow cancellation
    };

    // Run optimization
    geos::Result result = task->run(body, tuneLink, config, progress);

    // Extract results
    extract_link_dials(tuneLink, result_link);
    final_loss = result.loss.spectral;

    // Debug: print ALL dial values
    fprintf(stderr, "TUNE RESULT (45 dials):\n");
    fprintf(stderr, "  CC: exp=%.3f temp=%.3f tint=%.3f\n",
        result_link.color_correction.dials["exposure"],
        result_link.color_correction.dials["temperature"],
        result_link.color_correction.dials["tint"]);
    fprintf(stderr, "  TM: con=%.3f hi=%.3f sh=%.3f blk=%.3f wht=%.3f\n",
        result_link.tone_mapping.dials["contrast"],
        result_link.tone_mapping.dials["highlights"],
        result_link.tone_mapping.dials["shadows"],
        result_link.tone_mapping.dials["black"],
        result_link.tone_mapping.dials["white"]);
    fprintf(stderr, "  GC: vib=%.3f sat=%.3f den=%.3f\n",
        result_link.global_color.dials["vibrance"],
        result_link.global_color.dials["saturation"],
        result_link.global_color.dials["color_density"]);
    fprintf(stderr, "  SC: R(%.2f,%.2f,%.2f) O(%.2f,%.2f,%.2f) Y(%.2f,%.2f,%.2f) G(%.2f,%.2f,%.2f)\n",
        result_link.selective_color.dials["red_hue"],
        result_link.selective_color.dials["red_saturation"],
        result_link.selective_color.dials["red_luminance"],
        result_link.selective_color.dials["orange_hue"],
        result_link.selective_color.dials["orange_saturation"],
        result_link.selective_color.dials["orange_luminance"],
        result_link.selective_color.dials["yellow_hue"],
        result_link.selective_color.dials["yellow_saturation"],
        result_link.selective_color.dials["yellow_luminance"],
        result_link.selective_color.dials["green_hue"],
        result_link.selective_color.dials["green_saturation"],
        result_link.selective_color.dials["green_luminance"]);
    fprintf(stderr, "      C(%.2f,%.2f,%.2f) B(%.2f,%.2f,%.2f) P(%.2f,%.2f,%.2f) M(%.2f,%.2f,%.2f)\n",
        result_link.selective_color.dials["cyan_hue"],
        result_link.selective_color.dials["cyan_saturation"],
        result_link.selective_color.dials["cyan_luminance"],
        result_link.selective_color.dials["blue_hue"],
        result_link.selective_color.dials["blue_saturation"],
        result_link.selective_color.dials["blue_luminance"],
        result_link.selective_color.dials["purple_hue"],
        result_link.selective_color.dials["purple_saturation"],
        result_link.selective_color.dials["purple_luminance"],
        result_link.selective_color.dials["magenta_hue"],
        result_link.selective_color.dials["magenta_saturation"],
        result_link.selective_color.dials["magenta_luminance"]);
    fprintf(stderr, "  DT: shAmt=%.3f shRad=%.3f dnL=%.3f dnC=%.3f\n",
        result_link.detail.dials["sharpen_amount"],
        result_link.detail.dials["sharpen_radius"],
        result_link.detail.dials["denoise_luminance"],
        result_link.detail.dials["denoise_chroma"]);

    // Save diagnostic images to project folder (reuse project_dir and stem from above)

    // Save view.png - the target (camera preview)
    fs::path view_path = project_dir / (stem + ".view.png");
    cv::Mat tgt_cpu;
    preview.copyTo(tgt_cpu);
    if (!tgt_cpu.empty()) {
        cv::imwrite(view_path.string(), tgt_cpu);
    }

    // Save 0.pipe.png - the first (Base) link output from tune
    // Future links will be 1.pipe.png, 2.pipe.png, etc.
    fs::path step0_path = project_dir / (stem + ".0.pipe.png");
    {
        pipe::View step_view = body.view();
        if (!step_view.empty()) {
            cv::Mat step_cpu;
            step_view.copyTo(step_cpu);
            cv::imwrite(step0_path.string(), step_cpu);
        }
    }

    // Save tail.png - the final output (same as last step)
    // Use social media size (2048px) for fast iteration; full res via Export
    fs::path tail_path = project_dir / (stem + ".tail.png");
    body.tail().save(tail_path.string(), 2048);

    // Save diff.png - difference between view (target) and tail (result)
    fs::path diff_path = project_dir / (stem + ".diff.png");
    pipe::View output = body.view();
    if (!output.empty() && !tgt_cpu.empty()) {
        cv::Mat out_cpu;
        output.copyTo(out_cpu);

        // Resize to match if needed
        if (out_cpu.size() != tgt_cpu.size()) {
            cv::resize(out_cpu, out_cpu, tgt_cpu.size());
        }

        // Compute absolute difference, amplified for visibility
        cv::Mat diff;
        cv::absdiff(out_cpu, tgt_cpu, diff);
        diff *= 4;  // Amplify difference for visibility

        cv::imwrite(diff_path.string(), diff);
    }

    // Store result in thread-safe globals
    {
        std::lock_guard<std::mutex> lock(g_tune_mutex);
        g_tune_result = result_link;
        g_tune_final_loss = final_loss;
    }

    desk::set_geos_tuning(false);
    g_tune_finished = true;
    g_tune_running = false;
}

void start_tune_async(State& state, Project& project) {
    // Don't start if already running
    if (g_tune_running) return;

    // Join previous thread if any
    if (g_tune_thread.joinable()) {
        g_tune_thread.join();
    }

    // Clear existing links - DESK owns the pipe, start fresh
    project.links.clear();
    save_pipe_json(project);

    // Set state
    state.is_tuning = true;
    state.tune_complete = false;
    state.tune_project = state.selection.project;
    state.status_message = "Tuning: " + project.name;

    // Reset flags
    g_tune_running = true;
    g_tune_finished = false;

    // Start thread (copy path and name to avoid dangling refs)
    g_tune_thread = std::thread(tune_thread_func, project.raw_path, project.name);
}

void poll_tune_complete(State& state) {
    if (!state.is_tuning) return;
    if (!g_tune_finished) return;

    // Thread is done
    if (g_tune_thread.joinable()) {
        g_tune_thread.join();
    }

    // Apply result to project
    if (state.tune_project >= 0 && state.tune_project < static_cast<int>(state.projects.size())) {
        Project& proj = state.projects[state.tune_project];

        Link result;
        float loss;
        {
            std::lock_guard<std::mutex> lock(g_tune_mutex);
            result = g_tune_result;
            loss = g_tune_final_loss;
        }

        // Add or replace "Base" link
        bool found = false;
        for (auto& link : proj.links) {
            if (link.name == "Base") {
                link = result;
                found = true;
                break;
            }
        }
        if (!found) {
            proj.links.insert(proj.links.begin(), result);
        }

        // Save pipe.json
        save_pipe_json(proj);

        // Update status
        char buf[128];
        snprintf(buf, sizeof(buf), "Tuned: %.1f%% error", loss * 100.0f);
        state.status_message = buf;
    }

    // Reset state
    state.is_tuning = false;
    state.tune_complete = true;
    state.tune_project = -1;
    state.needs_reprocess = true;

    // Show tail (pipeline output) in work area
    state.selection.pipe_view = PipeView::BODY;
    g_tune_finished = false;
}

} // namespace desk

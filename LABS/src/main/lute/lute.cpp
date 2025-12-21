// lute.cpp - Camera Profile Learning
//
// LUTE learns camera-specific color transforms from RAW + embedded JPEG pairs.
// Each shot improves the LUT for that camera's style.
//
// Model:
//   1. flow::Flow provides flat (scene-linear RGB after HEAD pipeline)
//   2. flow::Flow provides target (embedded JPEG from camera)
//   3. tune() accumulates flat->target mappings into 17^3 LUT
//   4. view() applies learned LUT to produce camera-style output
//
// Profile key: Camera_Model_Style (e.g., "Sony_ILCE-7M4_Standard")

#include "lute.hpp"

#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>

namespace lute {

// ============================================================================
// CameraLut implementation
// ============================================================================

CameraLut::CameraLut() {
    reset();
}

void CameraLut::reset() {
    // 3D LUT
    sum.assign(LUT_SIZE, 0.0);
    prev_avg.assign(LUT_SIZE, 0.0);
    count.assign(CELLS, 0);

    // 1D tone curve
    curve_sum.assign(CURVE_SIZE, 0.0);
    curve_count.assign(CURVE_SIZE, 0);

    sample_count = 0;
    last_delta = 1.0f;
    frozen = false;
    estimated = false;
    camera_make.clear();
    camera_model.clear();
    creative_style.clear();
}

std::string CameraLut::key() const {
    std::string k = camera_make + "_" + camera_model;
    if (!creative_style.empty())
        k += "_" + creative_style;
    return k;
}

void CameraLut::lut(float* out) const {
    for (int ri = 0; ri < GRID_SIZE; ri++) {
        for (int gi = 0; gi < GRID_SIZE; gi++) {
            for (int bi = 0; bi < GRID_SIZE; bi++) {
                int cell_idx = (ri * GRID_SIZE + gi) * GRID_SIZE + bi;
                int lut_base = cell_idx * 3;

                if (count[cell_idx] > 0) {
                    out[lut_base + 0] = static_cast<float>(sum[lut_base + 0] / count[cell_idx]);
                    out[lut_base + 1] = static_cast<float>(sum[lut_base + 1] / count[cell_idx]);
                    out[lut_base + 2] = static_cast<float>(sum[lut_base + 2] / count[cell_idx]);
                } else {
                    // Identity for empty cells
                    out[lut_base + 0] = static_cast<float>(ri) / (GRID_SIZE - 1);
                    out[lut_base + 1] = static_cast<float>(gi) / (GRID_SIZE - 1);
                    out[lut_base + 2] = static_cast<float>(bi) / (GRID_SIZE - 1);
                }
            }
        }
    }
}

void CameraLut::curve(float* out) const {
    for (int i = 0; i < CURVE_SIZE; i++) {
        if (curve_count[i] > 0) {
            out[i] = static_cast<float>(curve_sum[i] / curve_count[i]);
        } else {
            // Identity for empty bins: input = output
            out[i] = static_cast<float>(i) / (CURVE_SIZE - 1);
        }
    }
}

float CameraLut::coverage() const {
    int filled = 0;
    for (int i = 0; i < CELLS; i++)
        if (count[i] > 0) filled++;
    return static_cast<float>(filled) / CELLS;
}

int CameraLut::emptyCells() const {
    int empty = 0;
    for (int i = 0; i < CELLS; i++)
        if (count[i] == 0) empty++;
    return empty;
}

void CameraLut::snapshot() {
    for (int i = 0; i < CELLS; i++) {
        int base = i * 3;
        if (count[i] > 0) {
            prev_avg[base + 0] = sum[base + 0] / count[i];
            prev_avg[base + 1] = sum[base + 1] / count[i];
            prev_avg[base + 2] = sum[base + 2] / count[i];
        } else {
            // Identity for empty cells
            int ri = i / (GRID_SIZE * GRID_SIZE);
            int gi = (i / GRID_SIZE) % GRID_SIZE;
            int bi = i % GRID_SIZE;
            prev_avg[base + 0] = static_cast<double>(ri) / (GRID_SIZE - 1);
            prev_avg[base + 1] = static_cast<double>(gi) / (GRID_SIZE - 1);
            prev_avg[base + 2] = static_cast<double>(bi) / (GRID_SIZE - 1);
        }
    }
}

float CameraLut::computeDelta() const {
    double total_delta = 0.0;
    int cells_with_data = 0;

    for (int i = 0; i < CELLS; i++) {
        if (count[i] > 0) {
            int base = i * 3;
            double curr_r = sum[base + 0] / count[i];
            double curr_g = sum[base + 1] / count[i];
            double curr_b = sum[base + 2] / count[i];

            double dr = std::abs(curr_r - prev_avg[base + 0]);
            double dg = std::abs(curr_g - prev_avg[base + 1]);
            double db = std::abs(curr_b - prev_avg[base + 2]);

            total_delta += (dr + dg + db) / 3.0;
            cells_with_data++;
        }
    }

    if (cells_with_data == 0)
        return 1.0f;

    return static_cast<float>(total_delta / cells_with_data);
}

bool CameraLut::converged(float threshold) const {
    return frozen || (sample_count >= 5 && last_delta < threshold);
}

std::vector<std::string> CameraLut::missing() const {
    std::vector<std::string> suggestions;

    // Count empty cells by region
    int dark_empty = 0, mid_empty = 0, bright_empty = 0;
    int red_empty = 0, green_empty = 0, blue_empty = 0;
    int neutral_empty = 0;

    for (int ri = 0; ri < GRID_SIZE; ri++) {
        for (int gi = 0; gi < GRID_SIZE; gi++) {
            for (int bi = 0; bi < GRID_SIZE; bi++) {
                int cell_idx = (ri * GRID_SIZE + gi) * GRID_SIZE + bi;
                if (count[cell_idx] > 0) continue;

                // Luminance
                int lum = ri + gi + bi;
                if (lum < 15) dark_empty++;
                else if (lum < 33) mid_empty++;
                else bright_empty++;

                // Hue
                int max_ch = std::max({ri, gi, bi});
                int min_ch = std::min({ri, gi, bi});
                int chroma = max_ch - min_ch;

                if (chroma < 3) {
                    neutral_empty++;
                } else if (ri == max_ch) {
                    red_empty++;
                } else if (gi == max_ch) {
                    green_empty++;
                } else {
                    blue_empty++;
                }
            }
        }
    }

    const int threshold = 50;

    if (dark_empty > threshold)
        suggestions.push_back("dark scenes (night, shadows)");
    if (bright_empty > threshold)
        suggestions.push_back("bright scenes (snow, clouds)");
    if (red_empty > threshold)
        suggestions.push_back("reds (flowers, autumn leaves)");
    if (green_empty > threshold)
        suggestions.push_back("greens (foliage, grass)");
    if (blue_empty > threshold)
        suggestions.push_back("blues (sky, water)");
    if (neutral_empty > threshold)
        suggestions.push_back("neutrals (gray cards, concrete)");

    if (suggestions.empty())
        suggestions.push_back("good coverage - no major gaps");

    return suggestions;
}

// ============================================================================
// JSON Persistence
// ============================================================================

bool save(const CameraLut& lut, const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "[lute::save] Failed to open: " << path << "\n";
        return false;
    }

    file << "{\n";
    file << "  \"version\": 1,\n";
    file << "  \"camera_make\": \"" << lut.camera_make << "\",\n";
    file << "  \"camera_model\": \"" << lut.camera_model << "\",\n";
    file << "  \"creative_style\": \"" << lut.creative_style << "\",\n";
    file << "  \"sample_count\": " << lut.sample_count << ",\n";
    file << "  \"coverage\": " << lut.coverage() << ",\n";
    file << "  \"last_delta\": " << lut.last_delta << ",\n";
    file << "  \"frozen\": " << (lut.frozen ? "true" : "false") << ",\n";
    file << "  \"grid_size\": " << GRID_SIZE << ",\n";

    // Save sum array
    file << "  \"sum\": [";
    for (int i = 0; i < LUT_SIZE; i++) {
        if (i > 0) file << ",";
        if (i % 12 == 0) file << "\n    ";
        file << lut.sum[i];
    }
    file << "\n  ],\n";

    // Save count array
    file << "  \"count\": [";
    for (int i = 0; i < CELLS; i++) {
        if (i > 0) file << ",";
        if (i % 20 == 0) file << "\n    ";
        file << lut.count[i];
    }
    file << "\n  ]\n";

    file << "}\n";

    std::cerr << "[lute::save] " << lut.key() << " (" << lut.sample_count << " samples, "
              << (lut.coverage() * 100.0f) << "% coverage) -> " << path << "\n";
    return true;
}

bool load(CameraLut& lut, const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Simple JSON parsing
    auto extractString = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\": \"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return "";
        pos += search.length();
        size_t end = content.find("\"", pos);
        if (end == std::string::npos) return "";
        return content.substr(pos, end - pos);
    };

    auto extractInt = [&](const std::string& key) -> int {
        std::string search = "\"" + key + "\": ";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.length();
        return std::stoi(content.substr(pos));
    };

    auto extractFloat = [&](const std::string& key) -> float {
        std::string search = "\"" + key + "\": ";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return 0.0f;
        pos += search.length();
        return std::stof(content.substr(pos));
    };

    auto extractBool = [&](const std::string& key) -> bool {
        std::string search = "\"" + key + "\": ";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return false;
        pos += search.length();
        return content.substr(pos, 4) == "true";
    };

    auto extractDoubleArray = [&](const std::string& key, std::vector<double>& out) {
        std::string search = "\"" + key + "\": [";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return;
        pos += search.length();

        size_t end = content.find("]", pos);
        if (end == std::string::npos) return;

        std::string arr = content.substr(pos, end - pos);
        std::istringstream iss(arr);
        std::string token;

        out.clear();
        while (std::getline(iss, token, ',')) {
            size_t start = token.find_first_not_of(" \t\n");
            size_t stop = token.find_last_not_of(" \t\n");
            if (start != std::string::npos && stop != std::string::npos) {
                token = token.substr(start, stop - start + 1);
                out.push_back(std::stod(token));
            }
        }
    };

    auto extractIntArray = [&](const std::string& key, std::vector<int>& out) {
        std::string search = "\"" + key + "\": [";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return;
        pos += search.length();

        size_t end = content.find("]", pos);
        if (end == std::string::npos) return;

        std::string arr = content.substr(pos, end - pos);
        std::istringstream iss(arr);
        std::string token;

        out.clear();
        while (std::getline(iss, token, ',')) {
            size_t start = token.find_first_not_of(" \t\n");
            size_t stop = token.find_last_not_of(" \t\n");
            if (start != std::string::npos && stop != std::string::npos) {
                token = token.substr(start, stop - start + 1);
                out.push_back(std::stoi(token));
            }
        }
    };

    lut.reset();
    lut.camera_make = extractString("camera_make");
    lut.camera_model = extractString("camera_model");
    lut.creative_style = extractString("creative_style");
    lut.sample_count = extractInt("sample_count");
    lut.last_delta = extractFloat("last_delta");
    lut.frozen = extractBool("frozen");
    lut.estimated = lut.sample_count > 0;

    extractDoubleArray("sum", lut.sum);
    extractIntArray("count", lut.count);

    // Ensure vectors are correct size
    if (lut.sum.size() != LUT_SIZE) lut.sum.resize(LUT_SIZE, 0.0);
    if (lut.count.size() != CELLS) lut.count.resize(CELLS, 0);
    if (lut.prev_avg.size() != LUT_SIZE) lut.prev_avg.resize(LUT_SIZE, 0.0);

    std::cerr << "[lute::load] " << lut.key() << " (" << lut.sample_count << " samples, "
              << (lut.coverage() * 100.0f) << "% coverage) <- " << path << "\n";
    return true;
}

// ============================================================================
// tune() - Accumulate flat -> target mappings
// ============================================================================
//
// flat: scene-linear RGB from HEAD pipeline (float [0,1])
// target: camera JPEG RGB (uint8 [0,255])
// Both at same resolution.

// sRGB to linear conversion
static float srgb_to_linear(float v) {
    if (v <= 0.04045f)
        return v / 12.92f;
    return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

bool tune(const float* flat, const uint8_t* target, int width, int height, CameraLut& lut, bool direct) {
    if (!flat || !target || width <= 0 || height <= 0) {
        std::cerr << "[lute::tune] Error: Invalid input\n";
        return false;
    }

    if (lut.frozen) {
        std::cerr << "[lute::tune] Profile frozen, skipping\n";
        return true;
    }

    // Snapshot for delta tracking
    lut.snapshot();

    long pixels_added = 0;
    float bin_size = 1.0f / GRID_SIZE;  // [0,1] bins

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx = (static_cast<size_t>(y) * width + x) * 3;

            // Input RGB [0,1+] - clamp to [0,1]
            float fr = std::max(0.0f, std::min(1.0f, flat[idx + 0]));
            float fg = std::max(0.0f, std::min(1.0f, flat[idx + 1]));
            float fb = std::max(0.0f, std::min(1.0f, flat[idx + 2]));

            // Target is sRGB uint8 - convert to linear [0,1]
            float tr = srgb_to_linear(target[idx + 0] / 255.0f);
            float tg = srgb_to_linear(target[idx + 1] / 255.0f);
            float tb = srgb_to_linear(target[idx + 2] / 255.0f);

            float cr, cg, cb;

            if (direct) {
                // Direct mode: input already tone-mapped (ACES), bin directly
                cr = fr;
                cg = fg;
                cb = fb;
            } else {
                // Scene-linear mode: apply ratio adjustment first
                // === 1D Tone Curve ===
                float in_lum = 0.299f * fr + 0.587f * fg + 0.114f * fb;
                float out_lum = 0.299f * tr + 0.587f * tg + 0.114f * tb;

                int curve_bin = std::min(CURVE_SIZE - 1, static_cast<int>(in_lum * CURVE_SIZE));
                lut.curve_sum[curve_bin] += out_lum;
                lut.curve_count[curve_bin]++;

                // Apply tone curve to input (so 3D LUT learns residual color)
                float ratio = (in_lum > 0.001f) ? (out_lum / in_lum) : 1.0f;
                cr = std::max(0.0f, std::min(1.0f, fr * ratio));
                cg = std::max(0.0f, std::min(1.0f, fg * ratio));
                cb = std::max(0.0f, std::min(1.0f, fb * ratio));
            }

            // Quantize to grid cell
            int ri = std::min(GRID_SIZE - 1, static_cast<int>(cr / bin_size));
            int gi = std::min(GRID_SIZE - 1, static_cast<int>(cg / bin_size));
            int bi = std::min(GRID_SIZE - 1, static_cast<int>(cb / bin_size));

            int cell_idx = (ri * GRID_SIZE + gi) * GRID_SIZE + bi;

            // Accumulate target RGB
            lut.sum[cell_idx * 3 + 0] += tr;
            lut.sum[cell_idx * 3 + 1] += tg;
            lut.sum[cell_idx * 3 + 2] += tb;
            lut.count[cell_idx]++;
            pixels_added++;
        }
    }

    lut.estimated = true;
    lut.sample_count++;
    lut.last_delta = lut.computeDelta();

    // Auto-freeze on convergence
    bool just_converged = false;
    if (lut.sample_count >= 10 && lut.last_delta < 0.001f && lut.coverage() > 0.7f) {
        lut.frozen = true;
        just_converged = true;
    }

    std::cerr << "[lute::tune] " << (pixels_added / 1000) << "k pixels"
              << ", coverage " << (lut.coverage() * 100.0f) << "%"
              << ", delta " << (lut.last_delta * 100.0f) << "%"
              << ", sample #" << lut.sample_count
              << (just_converged ? " -> CONVERGED" : "")
              << "\n";

    return true;
}

// ============================================================================
// view() - Apply learned LUT to scene-linear RGB
// ============================================================================
//
// Trilinear interpolation in 17^3 grid.
// Input: scene-linear RGB [0,1]
// Output: camera-style RGB [0,1]

void view(const float* in, float* out, int width, int height, const CameraLut& lut) {
    // Extract tone curve (3D LUT disabled until coverage > 70%)
    std::vector<float> tone(CURVE_SIZE);
    lut.curve(tone.data());

    float curve_scale = static_cast<float>(CURVE_SIZE - 1);

    // Helper: interpolate tone curve
    auto applyCurve = [&](float lum) -> float {
        float idx = lum * curve_scale;
        int i0 = static_cast<int>(idx);
        int i1 = std::min(i0 + 1, CURVE_SIZE - 1);
        i0 = std::min(i0, CURVE_SIZE - 1);
        float t = idx - i0;
        return tone[i0] * (1 - t) + tone[i1] * t;
    };

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx = (static_cast<size_t>(y) * width + x) * 3;

            // Clamp input to [0,1]
            float r = std::max(0.0f, std::min(1.0f, in[idx + 0]));
            float g = std::max(0.0f, std::min(1.0f, in[idx + 1]));
            float b = std::max(0.0f, std::min(1.0f, in[idx + 2]));

            // === Step 1: Apply tone curve ===
            // Compute input luminance
            float in_lum = 0.299f * r + 0.587f * g + 0.114f * b;
            float out_lum = applyCurve(in_lum);

            // Scale RGB by luminance ratio (preserves color, changes brightness)
            float ratio = (in_lum > 0.001f) ? (out_lum / in_lum) : 1.0f;
            r = std::max(0.0f, std::min(1.0f, r * ratio));
            g = std::max(0.0f, std::min(1.0f, g * ratio));
            b = std::max(0.0f, std::min(1.0f, b * ratio));

            // === Step 2: Apply 3D LUT (disabled until coverage > 70%) ===
            // With low coverage, the LUT creates discontinuities.
            // The tone curve alone works better for now.
            out[idx + 0] = r;
            out[idx + 1] = g;
            out[idx + 2] = b;
        }
    }
}

} // namespace lute

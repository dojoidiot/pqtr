// raws.cpp
// RAWS library implementation - loads RAW files to scene-linear RGB
// Auto-detects format and dispatches to appropriate loader (Sony, Canon, Nikon, etc.)
//
// RAWS provides:
//   load() - decode RAW to flat scene-linear
//   tune() - estimate camera LUT from flat/jpeg pairs

#include "raws.hpp"
#include "sony.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <cmath>
#include <cctype>
#include <opencv2/imgproc.hpp>

namespace raws {

// ============================================================
// CameraLut implementation
// ============================================================

void CameraLut::reset()
{
    // Zero accumulators
    for (int i = 0; i < TOTAL; i++) {
        sum[i] = 0.0;
        prev_avg[i] = 0.0;
    }
    for (int i = 0; i < CELLS; i++)
        count[i] = 0;

    estimated = false;
    sample_count = 0;
    last_delta = 1.0f;
    frozen = false;
    camera_make.clear();
    camera_model.clear();
    creative_style.clear();
    dro.clear();
}

void CameraLut::lut(float* out) const
{
    // Compute averages, use identity for empty cells
    for (int ri = 0; ri < GRID_SIZE; ri++)
    {
        for (int gi = 0; gi < GRID_SIZE; gi++)
        {
            for (int bi = 0; bi < GRID_SIZE; bi++)
            {
                int cell_idx = (ri * GRID_SIZE + gi) * GRID_SIZE + bi;
                int lut_base = cell_idx * 3;

                if (count[cell_idx] > 0)
                {
                    out[lut_base + 0] = static_cast<float>(sum[lut_base + 0] / count[cell_idx]);
                    out[lut_base + 1] = static_cast<float>(sum[lut_base + 1] / count[cell_idx]);
                    out[lut_base + 2] = static_cast<float>(sum[lut_base + 2] / count[cell_idx]);
                }
                else
                {
                    // Identity for empty cells
                    out[lut_base + 0] = static_cast<float>(ri) / (GRID_SIZE - 1);
                    out[lut_base + 1] = static_cast<float>(gi) / (GRID_SIZE - 1);
                    out[lut_base + 2] = static_cast<float>(bi) / (GRID_SIZE - 1);
                }
            }
        }
    }
}

bool CameraLut::hasCell(int r, int g, int b) const
{
    int cell_idx = (r * GRID_SIZE + g) * GRID_SIZE + b;
    return count[cell_idx] > 0;
}

float CameraLut::coverage() const
{
    int filled = 0;
    for (int i = 0; i < CELLS; i++)
        if (count[i] > 0) filled++;
    return static_cast<float>(filled) / CELLS;
}

int CameraLut::emptyCells() const
{
    int empty = 0;
    for (int i = 0; i < CELLS; i++)
        if (count[i] == 0) empty++;
    return empty;
}

std::string CameraLut::key() const
{
    // e.g., "Sony_ILCE-7M4_Standard_DRO-Auto"
    std::string k = camera_make + "_" + camera_model;
    if (!creative_style.empty())
        k += "_" + creative_style;
    if (!dro.empty())
        k += "_DRO-" + dro;
    return k;
}

bool CameraLut::matches(const Result& result) const
{
    // Check if result's style matches this LUT
    auto it_make = result.dataInfo.find("camera.make");
    auto it_model = result.dataInfo.find("camera.model");
    auto it_style = result.previewInfo.find("style.creative");
    auto it_dro = result.previewInfo.find("style.dro");

    if (it_make != result.dataInfo.end() && it_make->second != camera_make)
        return false;
    if (it_model != result.dataInfo.end() && it_model->second != camera_model)
        return false;
    if (it_style != result.previewInfo.end() && it_style->second != creative_style)
        return false;
    if (it_dro != result.previewInfo.end() && it_dro->second != dro)
        return false;

    return true;
}

std::vector<std::string> CameraLut::missing() const
{
    std::vector<std::string> suggestions;

    // Divide grid into regions and count empty cells per region
    // Luminance: dark (0-5), mid (6-10), bright (11-16)
    // Hue regions based on dominant channel

    // Count empty cells by luminance level (R+G+B approximation)
    int dark_empty = 0, mid_empty = 0, bright_empty = 0;

    // Count empty cells by hue region
    int red_empty = 0, yellow_empty = 0, green_empty = 0;
    int cyan_empty = 0, blue_empty = 0, magenta_empty = 0;
    int neutral_empty = 0;

    for (int ri = 0; ri < GRID_SIZE; ri++)
    {
        for (int gi = 0; gi < GRID_SIZE; gi++)
        {
            for (int bi = 0; bi < GRID_SIZE; bi++)
            {
                int cell_idx = (ri * GRID_SIZE + gi) * GRID_SIZE + bi;
                if (count[cell_idx] > 0) continue;  // Not empty

                // Luminance (sum of indices as proxy)
                int lum = ri + gi + bi;
                if (lum < 15) dark_empty++;
                else if (lum < 33) mid_empty++;
                else bright_empty++;

                // Hue based on dominant channel
                int max_ch = std::max({ri, gi, bi});
                int min_ch = std::min({ri, gi, bi});
                int chroma = max_ch - min_ch;

                if (chroma < 3) {
                    neutral_empty++;
                } else if (ri == max_ch && bi == min_ch) {
                    if (gi > (ri + bi) / 2) yellow_empty++;
                    else red_empty++;
                } else if (gi == max_ch && bi == min_ch) {
                    green_empty++;
                } else if (gi == max_ch && ri == min_ch) {
                    cyan_empty++;
                } else if (bi == max_ch && ri == min_ch) {
                    blue_empty++;
                } else if (bi == max_ch && gi == min_ch) {
                    if (ri > (bi + gi) / 2) magenta_empty++;
                    else blue_empty++;
                } else if (ri == max_ch && gi == min_ch) {
                    magenta_empty++;
                }
            }
        }
    }

    // Generate suggestions based on counts (threshold: 50+ empty cells in region)
    const int threshold = 50;

    // Luminance suggestions
    if (dark_empty > threshold)
        suggestions.push_back("dark scenes (night, shadows, black objects)");
    if (bright_empty > threshold)
        suggestions.push_back("bright scenes (snow, clouds, white objects)");

    // Hue suggestions with scene examples
    if (red_empty > threshold)
        suggestions.push_back("reds (flowers, autumn leaves, brick, sunset)");
    if (yellow_empty > threshold)
        suggestions.push_back("yellows (sunflowers, sand, golden hour)");
    if (green_empty > threshold)
        suggestions.push_back("greens (foliage, grass, forest)");
    if (cyan_empty > threshold)
        suggestions.push_back("cyans (ocean, pool water, teal objects)");
    if (blue_empty > threshold)
        suggestions.push_back("blues (sky, blue hour, water)");
    if (magenta_empty > threshold)
        suggestions.push_back("magentas (flowers, neon, purple fabrics)");
    if (neutral_empty > threshold)
        suggestions.push_back("neutrals (gray cards, concrete, overcast sky)");

    // If well covered, say so
    if (suggestions.empty())
        suggestions.push_back("good coverage - no major gaps");

    return suggestions;
}

bool CameraLut::converged(float threshold) const
{
    return frozen || (sample_count >= 5 && last_delta < threshold);
}

void CameraLut::snapshot()
{
    // Save current averages for delta comparison after tune()
    for (int i = 0; i < CELLS; i++)
    {
        int base = i * 3;
        if (count[i] > 0)
        {
            prev_avg[base + 0] = sum[base + 0] / count[i];
            prev_avg[base + 1] = sum[base + 1] / count[i];
            prev_avg[base + 2] = sum[base + 2] / count[i];
        }
        else
        {
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

float CameraLut::computeDelta() const
{
    // Average RGB change per cell since snapshot
    double total_delta = 0.0;
    int cells_with_data = 0;

    for (int i = 0; i < CELLS; i++)
    {
        if (count[i] > 0)
        {
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
        return 1.0f;  // No data yet

    return static_cast<float>(total_delta / cells_with_data);
}

bool CameraLut::save(const std::string& path) const
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        std::cerr << "[CameraLut] Failed to open for write: " << path << "\n";
        return false;
    }

    file << "{\n";
    file << "  \"version\": 1,\n";
    file << "  \"camera_make\": \"" << camera_make << "\",\n";
    file << "  \"camera_model\": \"" << camera_model << "\",\n";
    file << "  \"creative_style\": \"" << creative_style << "\",\n";
    file << "  \"dro\": \"" << dro << "\",\n";
    file << "  \"sample_count\": " << sample_count << ",\n";
    file << "  \"coverage\": " << coverage() << ",\n";
    file << "  \"last_delta\": " << last_delta << ",\n";
    file << "  \"frozen\": " << (frozen ? "true" : "false") << ",\n";
    file << "  \"grid_size\": " << GRID_SIZE << ",\n";

    // Save sum array
    file << "  \"sum\": [";
    for (int i = 0; i < TOTAL; i++)
    {
        if (i > 0) file << ",";
        if (i % 12 == 0) file << "\n    ";
        file << sum[i];
    }
    file << "\n  ],\n";

    // Save count array
    file << "  \"count\": [";
    for (int i = 0; i < CELLS; i++)
    {
        if (i > 0) file << ",";
        if (i % 20 == 0) file << "\n    ";
        file << count[i];
    }
    file << "\n  ]\n";

    file << "}\n";

    std::cerr << "[CameraLut] Saved " << key() << " (" << sample_count << " samples, "
              << (coverage() * 100.0f) << "% coverage) to: " << path << "\n";
    return true;
}

bool CameraLut::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        // Not an error - cold start
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Simple JSON parsing (no dependencies)
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

    // Parse metadata
    camera_make = extractString("camera_make");
    camera_model = extractString("camera_model");
    creative_style = extractString("creative_style");
    dro = extractString("dro");
    sample_count = extractInt("sample_count");
    last_delta = extractFloat("last_delta");
    frozen = extractBool("frozen");

    int loaded_grid = extractInt("grid_size");
    if (loaded_grid != GRID_SIZE)
    {
        std::cerr << "[CameraLut] Grid size mismatch: " << loaded_grid << " vs " << GRID_SIZE << "\n";
        return false;
    }

    // Parse sum array
    size_t sum_pos = content.find("\"sum\": [");
    if (sum_pos != std::string::npos)
    {
        sum_pos = content.find("[", sum_pos);
        sum_pos++;
        for (int i = 0; i < TOTAL; i++)
        {
            while (sum_pos < content.size() && (content[sum_pos] == ' ' || content[sum_pos] == '\n' || content[sum_pos] == ','))
                sum_pos++;
            size_t end = sum_pos;
            while (end < content.size() && (std::isdigit(content[end]) || content[end] == '.' || content[end] == '-' || content[end] == 'e' || content[end] == 'E' || content[end] == '+'))
                end++;
            if (end > sum_pos)
                sum[i] = std::stod(content.substr(sum_pos, end - sum_pos));
            sum_pos = end;
        }
    }

    // Parse count array
    size_t count_pos = content.find("\"count\": [");
    if (count_pos != std::string::npos)
    {
        count_pos = content.find("[", count_pos);
        count_pos++;
        for (int i = 0; i < CELLS; i++)
        {
            while (count_pos < content.size() && (content[count_pos] == ' ' || content[count_pos] == '\n' || content[count_pos] == ','))
                count_pos++;
            size_t end = count_pos;
            while (end < content.size() && std::isdigit(content[end]))
                end++;
            if (end > count_pos)
                count[i] = std::stoi(content.substr(count_pos, end - count_pos));
            count_pos = end;
        }
    }

    estimated = sample_count > 0;

    std::cerr << "[CameraLut] Loaded " << key() << " (" << sample_count << " samples, "
              << (coverage() * 100.0f) << "% coverage"
              << (frozen ? ", FROZEN" : "") << ")\n";
    return true;
}

// ============================================================
// Format detection
// ============================================================

enum class Format {
    Unknown,
    SonyARW,
    // Future: CanonCR2, CanonCR3, NikonNEF, etc.
};

static Format detectFormat(pqtr::Sink& sink)
{
    if (sink.size() < 16) return Format::Unknown;

    // For now, assume all files are Sony ARW
    // TODO: Add format sniffing when we have multiple decoders
    return Format::SonyARW;
}

// ============================================================
// Sony loader
// ============================================================

static Result loadSony(pqtr::Sink& sink, const Options& opts)
{
    Result result;

    cv::UMat bayer;
    sony::Info sonyInfo;
    sony::RawMetadata meta;

    if (!sony::Decoder::prepare(sink, bayer, sonyInfo, meta))
        return result;

    // Convert raws::Options to sony::ProcessOptions
    sony::ProcessOptions sonyOpts;
    sonyOpts.undistort = opts.undistort;

    if (!sony::Decoder::process_linear(bayer, meta, result.data, sonyOpts))
        return result;

    // Data info: scene-linear metadata
    // Tree structure for UI: "group.key" format
    for (const auto& kv : sonyInfo)
        result.dataInfo[kv.first] = kv.second;

    result.dataInfo["decoder"] = "raws_sony_arw2";

    // Camera group
    result.dataInfo["camera.make"] = meta.camera_make;
    result.dataInfo["camera.model"] = meta.camera_model;
    result.dataInfo["camera.orientation"] = std::to_string(meta.orientation);

    // Image group
    result.dataInfo["image.width"] = std::to_string(meta.crop_width);
    result.dataInfo["image.height"] = std::to_string(meta.crop_height);
    result.dataInfo["image.black_level"] = std::to_string(meta.black_level);
    result.dataInfo["image.white_level"] = std::to_string(meta.white_level);

    // Lens group
    result.dataInfo["lens.model"] = meta.lens_model;
    std::ostringstream oss;
    oss << meta.focal_length;
    result.dataInfo["lens.focal_length"] = oss.str();
    oss.str(""); oss << meta.aperture;
    result.dataInfo["lens.aperture"] = oss.str();

    // Exposure group
    oss.str(""); oss << meta.iso;
    result.dataInfo["exposure.iso"] = oss.str();
    oss.str(""); oss << meta.shutter_speed;
    result.dataInfo["exposure.shutter"] = oss.str();

    // White balance group (normalized to green=1.0, already applied on Bayer)
    float wb_g = (meta.wb_rggb[1] + meta.wb_rggb[2]) / 2.0f;
    if (wb_g > 0) {
        oss.str(""); oss << (meta.wb_rggb[0] / wb_g);
        result.dataInfo["wb.r"] = oss.str();
        result.dataInfo["wb.g"] = "1.0";
        oss.str(""); oss << (meta.wb_rggb[3] / wb_g);
        result.dataInfo["wb.b"] = oss.str();
    }

    // Preview info: camera rendering settings (what produced the JPEG)
    result.preview = std::move(meta.preview);
    result.previewInfo["image.width"] = std::to_string(meta.preview_width);
    result.previewInfo["image.height"] = std::to_string(meta.preview_height);
    result.previewInfo["image.format"] = "srgb_8bit";

    // Style group (camera creative settings)
    result.previewInfo["style.creative"] = meta.creative_style;
    result.previewInfo["style.dro"] = meta.dro;
    result.previewInfo["style.contrast"] = std::to_string(meta.contrast);
    result.previewInfo["style.saturation"] = std::to_string(meta.saturation);
    result.previewInfo["style.sharpness"] = std::to_string(meta.sharpness);

    result.success = true;
    return result;
}

// ============================================================
// Public API
// ============================================================

Result load(pqtr::Sink& sink, const Options& opts)
{
    Format fmt = detectFormat(sink);

    switch (fmt) {
        case Format::SonyARW:
            return loadSony(sink, opts);

        default:
            return Result{};  // success = false
    }
}

// ============================================================
// Analyze: count new cells an image would fill
// ============================================================

int analyze(const pipe::View& flat, const pipe::View& target, const CameraLut& lut)
{
    if (flat.empty() || target.empty())
        return 0;

    constexpr int GRID = CameraLut::GRID_SIZE;

    try
    {
        // Convert flat to 8-bit for binning (same as tune)
        cv::UMat flat_8u;
        if (flat.type() == CV_32FC3)
        {
            cv::UMat clamped;
            cv::max(flat, 0.0f, clamped);
            cv::min(clamped, 1.0f, clamped);
            cv::UMat gamma;
            cv::pow(clamped, 1.0f/2.2f, gamma);
            gamma.convertTo(flat_8u, CV_8UC3, 255.0);
        }
        else
        {
            flat.convertTo(flat_8u, CV_8UC3);
        }

        cv::Mat flat_cpu;
        flat_8u.copyTo(flat_cpu);

        // Track which empty cells this image would fill
        std::vector<bool> would_fill(CameraLut::CELLS, false);
        float bin_size = 256.0f / GRID;

        for (int y = 0; y < flat_cpu.rows; y++)
        {
            const uchar* f_ptr = flat_cpu.ptr<uchar>(y);
            for (int x = 0; x < flat_cpu.cols; x++)
            {
                int idx = x * 3;
                int flat_b = f_ptr[idx + 0];
                int flat_g = f_ptr[idx + 1];
                int flat_r = f_ptr[idx + 2];

                int ri = std::min(GRID - 1, static_cast<int>(flat_r / bin_size));
                int gi = std::min(GRID - 1, static_cast<int>(flat_g / bin_size));
                int bi = std::min(GRID - 1, static_cast<int>(flat_b / bin_size));

                int cell_idx = (ri * GRID + gi) * GRID + bi;

                // Mark if this cell is currently empty in the LUT
                if (lut.count[cell_idx] == 0)
                    would_fill[cell_idx] = true;
            }
        }

        // Count new cells
        int new_cells = 0;
        for (int i = 0; i < CameraLut::CELLS; i++)
            if (would_fill[i]) new_cells++;

        return new_cells;
    }
    catch (...)
    {
        return 0;
    }
}

// ============================================================
// Tune: accumulate camera LUT from flat/jpeg pairs
// ============================================================

bool tune(const pipe::View& flat, const pipe::View& target, CameraLut& lut)
{
    if (flat.empty() || target.empty())
    {
        std::cerr << "[raws::tune] Error: Empty input\n";
        return false;
    }

    // Skip if already converged
    if (lut.frozen)
    {
        std::cerr << "[raws::tune] Profile frozen, skipping training\n";
        return true;
    }

    // Snapshot current state for delta tracking
    lut.snapshot();

    constexpr int GRID = CameraLut::GRID_SIZE;

    try
    {
        // Resize target to match flat if needed
        cv::UMat target_resized;
        if (flat.size() != target.size())
        {
            cv::resize(target, target_resized, flat.size());
        }
        else
        {
            target.copyTo(target_resized);
        }

        // Convert both to 8-bit BGR for binning
        cv::UMat flat_8u, target_8u;

        if (flat.type() == CV_32FC3)
        {
            cv::UMat clamped;
            cv::max(flat, 0.0f, clamped);
            cv::min(clamped, 1.0f, clamped);
            cv::UMat gamma;
            cv::pow(clamped, 1.0f/2.2f, gamma);
            gamma.convertTo(flat_8u, CV_8UC3, 255.0);
        }
        else
        {
            flat.convertTo(flat_8u, CV_8UC3);
        }

        if (target_resized.type() == CV_32FC3)
        {
            cv::UMat clamped;
            cv::max(target_resized, 0.0f, clamped);
            cv::min(clamped, 1.0f, clamped);
            cv::UMat gamma;
            cv::pow(clamped, 1.0f/2.2f, gamma);
            gamma.convertTo(target_8u, CV_8UC3, 255.0);
        }
        else
        {
            target_resized.convertTo(target_8u, CV_8UC3);
        }

        cv::Mat flat_cpu, target_cpu;
        flat_8u.copyTo(flat_cpu);
        target_8u.copyTo(target_cpu);

        float bin_size = 256.0f / GRID;
        long pixels_added = 0;

        for (int y = 0; y < flat_cpu.rows; y++)
        {
            const uchar* f_ptr = flat_cpu.ptr<uchar>(y);
            const uchar* t_ptr = target_cpu.ptr<uchar>(y);

            for (int x = 0; x < flat_cpu.cols; x++)
            {
                int idx = x * 3;
                // BGR order
                int flat_b = f_ptr[idx + 0];
                int flat_g = f_ptr[idx + 1];
                int flat_r = f_ptr[idx + 2];

                int tgt_b = t_ptr[idx + 0];
                int tgt_g = t_ptr[idx + 1];
                int tgt_r = t_ptr[idx + 2];

                // Quantize flat to grid cell
                int ri = std::min(GRID - 1, static_cast<int>(flat_r / bin_size));
                int gi = std::min(GRID - 1, static_cast<int>(flat_g / bin_size));
                int bi = std::min(GRID - 1, static_cast<int>(flat_b / bin_size));

                int cell_idx = (ri * GRID + gi) * GRID + bi;

                // Accumulate target RGB (normalized) into LUT
                lut.sum[cell_idx * 3 + 0] += tgt_r / 255.0;
                lut.sum[cell_idx * 3 + 1] += tgt_g / 255.0;
                lut.sum[cell_idx * 3 + 2] += tgt_b / 255.0;
                lut.count[cell_idx]++;
                pixels_added++;
            }
        }

        lut.estimated = true;
        lut.sample_count++;

        // Compute convergence delta
        lut.last_delta = lut.computeDelta();

        // Check for convergence (auto-freeze)
        bool just_converged = false;
        if (lut.sample_count >= 10 && lut.last_delta < 0.001f && lut.coverage() > 0.7f)
        {
            lut.frozen = true;
            just_converged = true;
        }

        std::cerr << "[raws::tune] Accumulated " << (pixels_added/1000) << "k pixels"
                  << ", coverage " << (lut.coverage() * 100.0f) << "%"
                  << ", delta " << (lut.last_delta * 100.0f) << "%"
                  << ", sample #" << lut.sample_count
                  << (just_converged ? " -> CONVERGED (frozen)" : "")
                  << "\n";

        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[raws::tune] Error: " << e.what() << "\n";
        return false;
    }
}

} // namespace raws

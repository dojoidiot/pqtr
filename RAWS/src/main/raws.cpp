// raws.cpp
// RAWS library implementation - decodes RAW files to scene-linear RGB
// Auto-detects format and dispatches to appropriate decoder (Sony, Canon, Nikon, etc.)

#include "raws.hpp"
#include "sony.h"
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <cmath>
#include <vector>

namespace raws {

// ============================================================
// Base curve estimation (from scene-linear data to JPEG preview)
// ============================================================

static void estimateBaseCurve(const cv::UMat& data, const cv::UMat& preview, float* curve)
{
    // Initialize all 3 channels to identity
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = i / 255.0f;

    if (data.empty() || preview.empty())
        return;

    // Resize preview to match data aspect ratio for comparison
    cv::Mat data_cpu, preview_cpu;
    data.copyTo(data_cpu);
    preview.copyTo(preview_cpu);

    // Resize data down to preview size for comparison
    cv::Mat data_small;
    cv::resize(data_cpu, data_small, preview_cpu.size(), 0, 0, cv::INTER_AREA);

    // Convert data to 8-bit gamma-encoded for comparison
    cv::Mat data_clamped;
    cv::max(data_small, 0.0f, data_clamped);
    cv::min(data_clamped, 1.0f, data_clamped);

    cv::Mat data_gamma;
    cv::pow(data_clamped, 1.0f / 2.2f, data_gamma);

    cv::Mat data_8u;
    data_gamma.convertTo(data_8u, CV_8UC3, 255.0);

    // Per-channel curve estimation (BGR order in OpenCV)
    // curve layout: [B0..B255, G0..G255, R0..R255] to match OpenCV BGR
    std::vector<double> sum[3];
    std::vector<double> count[3];
    for (int c = 0; c < 3; c++) {
        sum[c].resize(256, 0.0);
        count[c].resize(256, 0.0);
    }

    for (int y = 0; y < data_8u.rows; y++)
    {
        const uchar* d_ptr = data_8u.ptr<uchar>(y);
        const uchar* p_ptr = preview_cpu.ptr<uchar>(y);

        for (int x = 0; x < data_8u.cols; x++)
        {
            for (int c = 0; c < 3; c++)  // B, G, R
            {
                int bin = d_ptr[x * 3 + c];
                sum[c][bin] += p_ptr[x * 3 + c];
                count[c][bin] += 1.0;
            }
        }
    }

    // Compute curve values per channel
    for (int c = 0; c < 3; c++)
    {
        for (int i = 0; i < 256; i++)
        {
            if (count[c][i] > 0)
                curve[c * 256 + i] = static_cast<float>(sum[c][i] / count[c][i]) / 255.0f;
            else
                curve[c * 256 + i] = i / 255.0f;  // Identity fallback
        }

        // Ensure monotonicity per channel
        for (int i = 1; i < 256; i++)
        {
            if (curve[c * 256 + i] < curve[c * 256 + i - 1])
                curve[c * 256 + i] = curve[c * 256 + i - 1];
        }

        // Light smoothing per channel
        std::vector<float> smoothed(256);
        smoothed[0] = curve[c * 256 + 0];
        smoothed[255] = curve[c * 256 + 255];
        for (int i = 1; i < 255; i++)
            smoothed[i] = 0.25f * curve[c * 256 + i - 1] + 0.5f * curve[c * 256 + i] + 0.25f * curve[c * 256 + i + 1];
        for (int i = 0; i < 256; i++)
            curve[c * 256 + i] = smoothed[i];
    }
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
    // Format detection is done inside each decoder's prepare() call
    // For now, we just check minimum size and try Sony
    // Sony's prepare() will validate the TIFF header itself

    if (sink.size() < 16) return Format::Unknown;

    // For now, assume all files are Sony ARW
    // TODO: Add format sniffing when we have multiple decoders
    return Format::SonyARW;
}

// ============================================================
// Sony decoder
// ============================================================

static Result decodeSony(pqtr::Sink& sink)
{
    Result result;

    cv::UMat bayer;
    sony::Info sonyInfo;
    sony::RawMetadata meta;

    if (!sony::Decoder::prepare(sink, bayer, sonyInfo, meta))
        return result;

    if (!sony::Decoder::process_linear(bayer, meta, result.data))
        return result;

    // Data info: scene-linear metadata
    for (const auto& kv : sonyInfo)
        result.dataInfo[kv.first] = kv.second;

    result.dataInfo["decoder"] = "raws_sony_arw2";
    result.dataInfo["width"] = std::to_string(meta.crop_width);
    result.dataInfo["height"] = std::to_string(meta.crop_height);
    result.dataInfo["camera_make"] = meta.camera_make;
    result.dataInfo["camera_model"] = meta.camera_model;
    result.dataInfo["lens_model"] = meta.lens_model;

    std::ostringstream oss;
    oss << meta.iso; result.dataInfo["iso"] = oss.str();
    oss.str(""); oss << meta.shutter_speed; result.dataInfo["shutter_speed"] = oss.str();
    oss.str(""); oss << meta.aperture; result.dataInfo["aperture"] = oss.str();
    oss.str(""); oss << meta.focal_length; result.dataInfo["focal_length"] = oss.str();
    result.dataInfo["orientation"] = std::to_string(meta.orientation);

    // Preview info: what produced the camera look
    result.preview = std::move(meta.preview);
    result.previewInfo["width"] = std::to_string(meta.preview_width);
    result.previewInfo["height"] = std::to_string(meta.preview_height);
    result.previewInfo["format"] = "srgb_8bit";
    result.previewInfo["creative_style"] = meta.creative_style;
    result.previewInfo["dro"] = meta.dro;
    result.previewInfo["contrast"] = std::to_string(meta.contrast);
    result.previewInfo["saturation"] = std::to_string(meta.saturation);
    result.previewInfo["sharpness"] = std::to_string(meta.sharpness);

    // Estimate base curve from data→preview comparison
    estimateBaseCurve(result.data, result.preview, result.baseCurve);
    result.hasBaseCurve = true;

    result.success = true;
    return result;
}

// ============================================================
// Public API
// ============================================================

Result decode(pqtr::Sink& sink)
{
    Format fmt = detectFormat(sink);

    switch (fmt) {
        case Format::SonyARW:
            return decodeSony(sink);

        // Future decoders:
        // case Format::CanonCR2:
        //     return decodeCanon(sink);
        // case Format::NikonNEF:
        //     return decodeNikon(sink);

        default:
            return Result{};  // success = false
    }
}

} // namespace raws

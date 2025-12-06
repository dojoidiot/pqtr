// raws.cpp
// RAWS library implementation - decodes RAW files to scene-linear RGB
// Auto-detects format and dispatches to appropriate decoder (Sony, Canon, Nikon, etc.)
//
// RAWS is decode-only. Style estimation belongs in LABS.

#include "raws.hpp"
#include "sony.h"
#include <sstream>
#include <iostream>

namespace raws {

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
// Sony decoder
// ============================================================

static Result decodeSony(pqtr::Sink& sink, const Options& opts)
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

    // Camera baseline metadata (for HEAD to apply darktable-equivalent defaults)
    result.dataInfo["black_level"] = std::to_string(meta.black_level);
    result.dataInfo["white_level"] = std::to_string(meta.white_level);

    // WB coefficients normalized to green=1.0 (already applied by RAWS on Bayer)
    // These are provided for reference/debugging
    float wb_g = (meta.wb_rggb[1] + meta.wb_rggb[2]) / 2.0f;
    if (wb_g > 0) {
        oss.str(""); oss << (meta.wb_rggb[0] / wb_g); result.dataInfo["wb_r"] = oss.str();
        result.dataInfo["wb_g"] = "1.0";
        oss.str(""); oss << (meta.wb_rggb[3] / wb_g); result.dataInfo["wb_b"] = oss.str();
    }

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

    result.success = true;
    return result;
}

// ============================================================
// Public API
// ============================================================

Result decode(pqtr::Sink& sink, const Options& opts)
{
    Format fmt = detectFormat(sink);

    switch (fmt) {
        case Format::SonyARW:
            return decodeSony(sink, opts);

        default:
            return Result{};  // success = false
    }
}

} // namespace raws

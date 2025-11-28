// raws.cpp
// RAWS library implementation - decodes RAW files to scene-linear RGB
// Auto-detects format and dispatches to appropriate decoder (Sony, Canon, Nikon, etc.)

#include "raws.hpp"
#include "sony.h"
#include <sstream>

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

    // Data info: camera-native RGB metadata
    for (const auto& kv : sonyInfo)
        result.dataInfo[kv.first] = kv.second;

    result.dataInfo["decoder"] = "raws_sony_arw2";
    result.dataInfo["color_space"] = "camera_native";  // NEW: indicates no WB/matrix applied
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

    // Color science metadata (for LABS to apply)
    // WB: normalize so G=1.0
    float g_ref = meta.wb_rggb[1] > 0 ? static_cast<float>(meta.wb_rggb[1]) : 1024.0f;
    result.colorMeta.wb_r = static_cast<float>(meta.wb_rggb[0]) / g_ref;
    result.colorMeta.wb_g = 1.0f;
    result.colorMeta.wb_b = static_cast<float>(meta.wb_rggb[3]) / g_ref;  // Note: index 3 for B

    // Color matrix: camera RGB → sRGB
    result.colorMeta.color_matrix = meta.color_matrix;

    // Lens distortion
    result.colorMeta.has_distortion = meta.has_distortion_params;
    result.colorMeta.distortion_knot_count = meta.distortion_knot_count;
    if (meta.has_distortion_params) {
        for (int i = 0; i < meta.distortion_knot_count && i < 16; i++) {
            result.colorMeta.distortion_params[i] = meta.distortion_params[i];
        }
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

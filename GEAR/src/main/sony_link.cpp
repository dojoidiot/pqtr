// sony_link.cpp - Bridge between pipe::Link API and Sony decoder
//
// Uses pure (OpenCV-free) decoder for WASM compatibility
// Returns pipe::Data (Page = BayerBuffer*, Info = metadata)

#include "gear.hpp"
#include "part/sony_pure.h"
#include <cstring>

namespace gear {
namespace sony {

pipe::Data decode(const char* raw_data, size_t raw_size) {
    pipe::Data out;

    // Create Sink from raw buffer
    pqtr::Sink sink;
    char* copy = new char[raw_size];
    std::memcpy(copy, raw_data, raw_size);
    sink.push(copy, raw_size);

    // Decode using pure (OpenCV-free) Sony decoder
    ::sony::pure::Result result = ::sony::pure::decode(sink);

    if (!result.success) {
        out.info.text("error", result.error);
        return out;
    }

    const auto& meta = result.metadata;

    // Copy Bayer data to output buffer
    BayerBuffer* buf = new BayerBuffer();
    buf->width = meta.crop_width;
    buf->height = meta.crop_height;
    buf->black_level = meta.black_level;
    buf->white_level = meta.white_level;

    // Extract cropped region from full Bayer
    int crop_x = meta.crop_left;
    int crop_y = meta.crop_top;
    buf->data.resize(buf->width * buf->height);

    for (int y = 0; y < buf->height; y++) {
        const uint16_t* src = result.bayer.data.data() + (crop_y + y) * result.bayer.width + crop_x;
        uint16_t* dst = buf->data.data() + y * buf->width;
        std::memcpy(dst, src, buf->width * sizeof(uint16_t));
    }

    // Copy preview if available
    if (!result.preview.data.empty()) {
        buf->preview = std::move(result.preview.data);
        buf->preview_width = meta.preview_width;
        buf->preview_height = meta.preview_height;
    } else {
        buf->preview_width = 0;
        buf->preview_height = 0;
    }

    // Output Page = Bayer buffer
    out.page = buf;

    // Output Info = metadata (MUST fields per GEAR contract)
    out.info.text("decoder", "gear_sony_arw2_pure");
    out.info.text("make", meta.camera_make);
    out.info.text("model", meta.camera_model);

    out.info.dial("width", static_cast<float>(buf->width));
    out.info.dial("height", static_cast<float>(buf->height));
    out.info.dial("black_level", static_cast<float>(meta.black_level));
    out.info.dial("white_level", static_cast<float>(meta.white_level));
    out.info.dial("orientation", static_cast<float>(meta.orientation));

    out.info.dial("iso", meta.iso);
    out.info.dial("shutter", meta.shutter_speed);
    out.info.dial("aperture", meta.aperture);
    out.info.dial("focal_length", meta.focal_length);
    out.info.text("lens", meta.lens_model);

    // White balance (normalized to green=1.0)
    float wb_g = (meta.wb_rggb[1] + meta.wb_rggb[2]) / 2.0f;
    if (wb_g > 0) {
        out.info.dial("wb_r", meta.wb_rggb[0] / wb_g);
        out.info.dial("wb_g", 1.0f);
        out.info.dial("wb_b", meta.wb_rggb[3] / wb_g);
    }

    // Color matrix (3x3, row-major)
    out.info.data("color_matrix", meta.color_matrix, 9);

    // Bayer pattern
    out.info.dial("bayer_pattern", static_cast<float>(meta.bayer_pattern));

    // Crop params (for downstream links if needed)
    out.info.dial("crop_left", static_cast<float>(meta.crop_left));
    out.info.dial("crop_top", static_cast<float>(meta.crop_top));
    out.info.dial("crop_width", static_cast<float>(meta.crop_width));
    out.info.dial("crop_height", static_cast<float>(meta.crop_height));

    // Creative style info
    out.info.text("creative_style", meta.creative_style);
    out.info.text("dro", meta.dro);
    out.info.dial("contrast", static_cast<float>(meta.contrast));
    out.info.dial("saturation", static_cast<float>(meta.saturation));
    out.info.dial("sharpness", static_cast<float>(meta.sharpness));

    // Distortion params if available
    if (meta.has_distortion_params) {
        float dp[16];
        for (int i = 0; i < meta.distortion_knot_count; i++) {
            dp[i] = static_cast<float>(meta.distortion_params[i]);
        }
        out.info.data("distortion", dp, meta.distortion_knot_count);
    }

    // Preview dimensions in Info (actual data is in BayerBuffer)
    out.info.dial("preview_width", static_cast<float>(buf->preview_width));
    out.info.dial("preview_height", static_cast<float>(buf->preview_height));

    return out;
}

} // namespace sony
} // namespace gear

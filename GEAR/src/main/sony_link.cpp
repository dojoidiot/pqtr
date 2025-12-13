// sony_link.cpp - Bridge between new pipe::Link API and existing Sony decoder
//
// Wraps existing sony::Decoder to return pipe::Data (Page + Info)

#include "gear.hpp"
#include "part/sony.h"
#include <opencv2/imgproc.hpp>
#include <cstring>

namespace gear {
namespace sony {

// Buffer wrapper - holds decoded Bayer data
struct BayerBuffer {
    std::vector<uint16_t> data;
    int width;
    int height;
    int black_level;
    int white_level;
};

pipe::Data decode(const char* raw_data, size_t raw_size) {
    pipe::Data out;

    // Create Sink from raw buffer
    pqtr::Sink sink;
    char* copy = new char[raw_size];
    std::memcpy(copy, raw_data, raw_size);
    sink.push(copy, raw_size);

    // Decode using existing Sony decoder
    cv::UMat bayer;
    ::sony::Info sonyInfo;
    ::sony::RawMetadata meta;

    if (!::sony::Decoder::prepare(sink, bayer, sonyInfo, meta)) {
        out.info.text("error", "sony decoder failed");
        return out;
    }

    // Copy Bayer data to our buffer
    BayerBuffer* buf = new BayerBuffer();
    buf->width = meta.crop_width;
    buf->height = meta.crop_height;
    buf->black_level = meta.black_level;
    buf->white_level = meta.white_level;

    // Get Bayer data from UMat
    cv::Mat bayerCpu;
    bayer.copyTo(bayerCpu);

    // Store raw Bayer (cropped region)
    int crop_x = meta.crop_left;
    int crop_y = meta.crop_top;
    buf->data.resize(buf->width * buf->height);

    for (int y = 0; y < buf->height; y++) {
        const uint16_t* src = bayerCpu.ptr<uint16_t>(crop_y + y) + crop_x;
        uint16_t* dst = buf->data.data() + y * buf->width;
        std::memcpy(dst, src, buf->width * sizeof(uint16_t));
    }

    // Output Page = Bayer buffer
    out.page = buf;

    // Output Info = metadata
    out.info.text("decoder", "gear_sony_arw2");
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

    // Color matrix (3x3)
    float cm[9];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            cm[i * 3 + j] = meta.color_matrix(i, j);
        }
    }
    out.info.data("color_matrix", cm, 9);

    // Bayer pattern
    out.info.dial("bayer_pattern", static_cast<float>(meta.bayer_pattern));

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

    return out;
}

} // namespace sony
} // namespace gear

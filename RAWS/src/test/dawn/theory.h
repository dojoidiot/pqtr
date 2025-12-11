// theory.h - Reference implementations for RAWS shader testing
//
// Pure C++ implementations to validate WGSL shader correctness.

#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>

namespace theory {

//------------------------------------------------------------------------------
// Bayer Image (single channel)
//------------------------------------------------------------------------------
struct BayerImage {
    int width, height;
    std::vector<float> data;

    BayerImage(int w, int h) : width(w), height(h), data(w * h, 0.0f) {}

    float& at(int y, int x) { return data[y * width + x]; }
    float at(int y, int x) const { return data[y * width + x]; }
};

//------------------------------------------------------------------------------
// RGB Image (3 channels, BGR order)
//------------------------------------------------------------------------------
struct RGBImage {
    int width, height;
    std::vector<float> data;

    RGBImage(int w, int h) : width(w), height(h), data(w * h * 3, 0.0f) {}

    float& at(int y, int x, int c) { return data[(y * width + x) * 3 + c]; }
    float at(int y, int x, int c) const { return data[(y * width + x) * 3 + c]; }
};

//------------------------------------------------------------------------------
// Black Level Correction
//------------------------------------------------------------------------------
inline void blc_bayer(const std::vector<uint16_t>& input, BayerImage& output,
                      float black_level, float white_level) {
    float scale = 1.0f / (white_level - black_level);
    for (int i = 0; i < output.width * output.height; i++) {
        float val = (float(input[i]) - black_level) * scale;
        output.data[i] = std::max(0.0f, val);
    }
}

//------------------------------------------------------------------------------
// White Balance on Bayer
//------------------------------------------------------------------------------
inline void wb_bayer(const BayerImage& input, BayerImage& output,
                     float wb_r, float wb_b, int pattern) {
    // Pattern: 0=RGGB, 1=GRBG, 2=BGGR, 3=GBRG
    for (int y = 0; y < input.height; y++) {
        for (int x = 0; x < input.width; x++) {
            int px = x % 2;
            int py = y % 2;
            int pos = py * 2 + px;

            float gain = 1.0f;  // G gain

            if (pattern == 0) {  // RGGB
                if (pos == 0) gain = wb_r;
                else if (pos == 3) gain = wb_b;
            } else if (pattern == 1) {  // GRBG
                if (pos == 1) gain = wb_r;
                else if (pos == 2) gain = wb_b;
            } else if (pattern == 2) {  // BGGR
                if (pos == 0) gain = wb_b;
                else if (pos == 3) gain = wb_r;
            } else {  // GBRG
                if (pos == 1) gain = wb_b;
                else if (pos == 2) gain = wb_r;
            }

            output.at(y, x) = input.at(y, x) * gain;
        }
    }
}

//------------------------------------------------------------------------------
// Combined Prepare Bayer (BLC + WB)
//------------------------------------------------------------------------------
inline void prepare_bayer(const std::vector<uint16_t>& input, BayerImage& output,
                          float black_level, float white_level,
                          float wb_r, float wb_b, int pattern) {
    float scale = 1.0f / (white_level - black_level);

    for (int y = 0; y < output.height; y++) {
        for (int x = 0; x < output.width; x++) {
            int idx = y * output.width + x;

            // BLC
            float val = (float(input[idx]) - black_level) * scale;
            val = std::max(0.0f, val);

            // WB gain
            int px = x % 2;
            int py = y % 2;
            int pos = py * 2 + px;

            float gain = 1.0f;
            if (pattern == 0) {
                if (pos == 0) gain = wb_r;
                else if (pos == 3) gain = wb_b;
            } else if (pattern == 1) {
                if (pos == 1) gain = wb_r;
                else if (pos == 2) gain = wb_b;
            } else if (pattern == 2) {
                if (pos == 0) gain = wb_b;
                else if (pos == 3) gain = wb_r;
            } else {
                if (pos == 1) gain = wb_b;
                else if (pos == 2) gain = wb_r;
            }

            output.at(y, x) = val * gain;
        }
    }
}

//------------------------------------------------------------------------------
// Demosaic (Bilinear)
//------------------------------------------------------------------------------
inline void demosaic(const BayerImage& input, RGBImage& output, int pattern) {
    int w = input.width;
    int h = input.height;

    auto get = [&](int x, int y) -> float {
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        return input.at(y, x);
    };

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float c = get(x, y);
            float n = get(x, y - 1);
            float s = get(x, y + 1);
            float e = get(x + 1, y);
            float ww = get(x - 1, y);
            float ne = get(x + 1, y - 1);
            float nw = get(x - 1, y - 1);
            float se = get(x + 1, y + 1);
            float sw = get(x - 1, y + 1);

            int px = x % 2;
            int py = y % 2;
            int pos = py * 2 + px;

            float r = 0, g = 0, b = 0;

            if (pattern == 0) {  // RGGB
                if (pos == 0) {
                    r = c;
                    g = (n + s + e + ww) * 0.25f;
                    b = (ne + nw + se + sw) * 0.25f;
                } else if (pos == 1) {
                    r = (e + ww) * 0.5f;
                    g = c;
                    b = (n + s) * 0.5f;
                } else if (pos == 2) {
                    r = (n + s) * 0.5f;
                    g = c;
                    b = (e + ww) * 0.5f;
                } else {
                    r = (ne + nw + se + sw) * 0.25f;
                    g = (n + s + e + ww) * 0.25f;
                    b = c;
                }
            } else if (pattern == 1) {  // GRBG
                if (pos == 0) {
                    r = (e + ww) * 0.5f;
                    g = c;
                    b = (n + s) * 0.5f;
                } else if (pos == 1) {
                    r = c;
                    g = (n + s + e + ww) * 0.25f;
                    b = (ne + nw + se + sw) * 0.25f;
                } else if (pos == 2) {
                    r = (ne + nw + se + sw) * 0.25f;
                    g = (n + s + e + ww) * 0.25f;
                    b = c;
                } else {
                    r = (n + s) * 0.5f;
                    g = c;
                    b = (e + ww) * 0.5f;
                }
            } else if (pattern == 2) {  // BGGR
                if (pos == 0) {
                    r = (ne + nw + se + sw) * 0.25f;
                    g = (n + s + e + ww) * 0.25f;
                    b = c;
                } else if (pos == 1) {
                    r = (n + s) * 0.5f;
                    g = c;
                    b = (e + ww) * 0.5f;
                } else if (pos == 2) {
                    r = (e + ww) * 0.5f;
                    g = c;
                    b = (n + s) * 0.5f;
                } else {
                    r = c;
                    g = (n + s + e + ww) * 0.25f;
                    b = (ne + nw + se + sw) * 0.25f;
                }
            } else {  // GBRG
                if (pos == 0) {
                    r = (n + s) * 0.5f;
                    g = c;
                    b = (e + ww) * 0.5f;
                } else if (pos == 1) {
                    r = (ne + nw + se + sw) * 0.25f;
                    g = (n + s + e + ww) * 0.25f;
                    b = c;
                } else if (pos == 2) {
                    r = c;
                    g = (n + s + e + ww) * 0.25f;
                    b = (ne + nw + se + sw) * 0.25f;
                } else {
                    r = (e + ww) * 0.5f;
                    g = c;
                    b = (n + s) * 0.5f;
                }
            }

            // Output BGR
            output.at(y, x, 0) = b;
            output.at(y, x, 1) = g;
            output.at(y, x, 2) = r;
        }
    }
}

//------------------------------------------------------------------------------
// Color Matrix
//------------------------------------------------------------------------------
inline void color_matrix(const RGBImage& input, RGBImage& output, const float* m) {
    for (int y = 0; y < input.height; y++) {
        for (int x = 0; x < input.width; x++) {
            float b = input.at(y, x, 0);
            float g = input.at(y, x, 1);
            float r = input.at(y, x, 2);

            float out_r = m[0] * r + m[1] * g + m[2] * b;
            float out_g = m[3] * r + m[4] * g + m[5] * b;
            float out_b = m[6] * r + m[7] * g + m[8] * b;

            output.at(y, x, 0) = out_b;
            output.at(y, x, 1) = out_g;
            output.at(y, x, 2) = out_r;
        }
    }
}

} // namespace theory

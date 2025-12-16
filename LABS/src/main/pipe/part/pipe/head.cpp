// head.cpp - HEAD pipeline links
// Standard RAW processing: blc → wb → demosaic → cst → crop
// All parameters read from info (gear_* prefix)

#include "pipe.hpp"
#include "gear.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace pipe {

// ============================================================
// Buffer types for HEAD pipeline
// ============================================================

struct BayerF32 {
    float* data;
    int width;
    int height;
};

struct RgbF32 {
    float* data;  // RGB interleaved
    int width;
    int height;
};

// ============================================================
// BLC - Black Level Correction
// ============================================================
// Input: gear::sony::BayerBuffer* (raw sensor data)
// Output: BayerF32 (normalized [0,1+])
// Reads: gear_black_level, gear_white_level

class BlcLink : public Link {
public:
    Name name() const override { return "blc"; }
    Name type() const override { return "head"; }

    Data flow(Data in) override {
        auto* bayer = static_cast<gear::sony::BayerBuffer*>(in.page);
        if (!bayer || bayer->data.empty()) {
            in.info.text("error", "blc: no input data");
            return in;
        }

        float black = in.info.dial("gear_black_level");
        float white = in.info.dial("gear_white_level");
        if (white <= black) {
            black = static_cast<float>(bayer->black_level);
            white = static_cast<float>(bayer->white_level);
        }
        if (white <= black) {
            black = 512;
            white = 16383;
        }
        float scale = 1.0f / (white - black);

        int w = bayer->width;
        int h = bayer->height;
        size_t count = static_cast<size_t>(w) * h;

        auto* out = new BayerF32();
        out->data = new float[count];
        out->width = w;
        out->height = h;

        for (size_t i = 0; i < count; i++) {
            float v = (static_cast<float>(bayer->data[i]) - black) * scale;
            out->data[i] = std::max(0.0f, v);
        }

        // Clean up input
        delete bayer;

        in.page = out;
        return in;
    }
};

// ============================================================
// WB - White Balance (Bayer domain)
// ============================================================
// Input: BayerF32
// Output: BayerF32 (white-balanced)
// Reads: gear_wb_r, gear_wb_g, gear_wb_b, gear_bayer_pattern

class WbLink : public Link {
public:
    Name name() const override { return "wb"; }
    Name type() const override { return "head"; }

    Data flow(Data in) override {
        auto* bayer = static_cast<BayerF32*>(in.page);
        if (!bayer || !bayer->data) {
            in.info.text("error", "wb: no input data");
            return in;
        }

        float wb_r = in.info.test("gear_wb_r") ? in.info.dial("gear_wb_r") : 1.0f;
        float wb_g = in.info.test("gear_wb_g") ? in.info.dial("gear_wb_g") : 1.0f;
        float wb_b = in.info.test("gear_wb_b") ? in.info.dial("gear_wb_b") : 1.0f;
        int pattern = static_cast<int>(in.info.dial("gear_bayer_pattern"));
        if (pattern < 46 || pattern > 49) pattern = 46;

        // Build 2x2 gain pattern based on Bayer layout
        float gain[2][2];
        switch (pattern) {
            case 46: // RGGB
                gain[0][0] = wb_r; gain[0][1] = wb_g;
                gain[1][0] = wb_g; gain[1][1] = wb_b;
                break;
            case 47: // GRBG
                gain[0][0] = wb_g; gain[0][1] = wb_r;
                gain[1][0] = wb_b; gain[1][1] = wb_g;
                break;
            case 48: // BGGR
                gain[0][0] = wb_b; gain[0][1] = wb_g;
                gain[1][0] = wb_g; gain[1][1] = wb_r;
                break;
            case 49: // GBRG
                gain[0][0] = wb_g; gain[0][1] = wb_b;
                gain[1][0] = wb_r; gain[1][1] = wb_g;
                break;
        }

        int w = bayer->width;
        int h = bayer->height;

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                bayer->data[y * w + x] *= gain[y & 1][x & 1];
            }
        }

        return in;
    }
};

// ============================================================
// Demosaic - Bayer to RGB
// ============================================================
// Input: BayerF32
// Output: RgbF32
// Reads: gear_bayer_pattern
// Uses bilinear interpolation

class DemosaicLink : public Link {
public:
    Name name() const override { return "demosaic"; }
    Name type() const override { return "head"; }

    Data flow(Data in) override {
        auto* bayer = static_cast<BayerF32*>(in.page);
        if (!bayer || !bayer->data) {
            in.info.text("error", "demosaic: no input data");
            return in;
        }

        int pattern = static_cast<int>(in.info.dial("gear_bayer_pattern"));
        if (pattern < 46 || pattern > 49) pattern = 46;

        int w = bayer->width;
        int h = bayer->height;

        auto* rgb = new RgbF32();
        rgb->data = new float[static_cast<size_t>(w) * h * 3];
        rgb->width = w;
        rgb->height = h;

        // Determine pixel type offsets based on pattern
        // Pattern codes: 46=RGGB, 47=GRBG, 48=BGGR, 49=GBRG
        int r_dx = 0, r_dy = 0;
        int b_dx = 1, b_dy = 1;

        switch (pattern) {
            case 46: r_dx = 0; r_dy = 0; b_dx = 1; b_dy = 1; break; // RGGB
            case 47: r_dx = 1; r_dy = 0; b_dx = 0; b_dy = 1; break; // GRBG
            case 48: r_dx = 1; r_dy = 1; b_dx = 0; b_dy = 0; break; // BGGR
            case 49: r_dx = 0; r_dy = 1; b_dx = 1; b_dy = 0; break; // GBRG
        }

        auto get = [&](int x, int y) -> float {
            x = std::max(0, std::min(w - 1, x));
            y = std::max(0, std::min(h - 1, y));
            return bayer->data[y * w + x];
        };

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int px = x & 1;
                int py = y & 1;
                float r, g, b;

                // Determine what color this pixel is
                bool is_r = (px == r_dx && py == r_dy);
                bool is_b = (px == b_dx && py == b_dy);

                if (is_r) {
                    r = get(x, y);
                    g = (get(x-1, y) + get(x+1, y) + get(x, y-1) + get(x, y+1)) * 0.25f;
                    b = (get(x-1, y-1) + get(x+1, y-1) + get(x-1, y+1) + get(x+1, y+1)) * 0.25f;
                } else if (is_b) {
                    b = get(x, y);
                    g = (get(x-1, y) + get(x+1, y) + get(x, y-1) + get(x, y+1)) * 0.25f;
                    r = (get(x-1, y-1) + get(x+1, y-1) + get(x-1, y+1) + get(x+1, y+1)) * 0.25f;
                } else {
                    // Green pixel - need to figure out if neighbors are R/B or B/R
                    g = get(x, y);
                    if (py == r_dy) {
                        // R is on same row
                        r = (get(x-1, y) + get(x+1, y)) * 0.5f;
                        b = (get(x, y-1) + get(x, y+1)) * 0.5f;
                    } else {
                        // B is on same row
                        b = (get(x-1, y) + get(x+1, y)) * 0.5f;
                        r = (get(x, y-1) + get(x, y+1)) * 0.5f;
                    }
                }

                size_t idx = (static_cast<size_t>(y) * w + x) * 3;
                rgb->data[idx + 0] = r;
                rgb->data[idx + 1] = g;
                rgb->data[idx + 2] = b;
            }
        }

        // Clean up input
        delete[] bayer->data;
        delete bayer;

        in.page = rgb;
        return in;
    }
};

// ============================================================
// CST - Color Space Transform
// ============================================================
// Input: RgbF32 (camera RGB)
// Output: RgbF32 (sRGB linear)
// Reads: gear_color_matrix (9 floats, row-major 3x3)

class CstLink : public Link {
public:
    Name name() const override { return "cst"; }
    Name type() const override { return "head"; }

    Data flow(Data in) override {
        auto* rgb = static_cast<RgbF32*>(in.page);
        if (!rgb || !rgb->data) {
            in.info.text("error", "cst: no input data");
            return in;
        }

        // Get color matrix from info
        const float* mat = in.info.data("gear_color_matrix");
        size_t mat_size = in.info.size("gear_color_matrix");

        // Default identity matrix if not provided
        float m[9] = {1,0,0, 0,1,0, 0,0,1};
        if (mat && mat_size >= 9) {
            for (int i = 0; i < 9; i++) m[i] = mat[i];
        }

        int w = rgb->width;
        int h = rgb->height;
        size_t count = static_cast<size_t>(w) * h;

        for (size_t i = 0; i < count; i++) {
            float r = rgb->data[i * 3 + 0];
            float g = rgb->data[i * 3 + 1];
            float b = rgb->data[i * 3 + 2];

            rgb->data[i * 3 + 0] = m[0]*r + m[1]*g + m[2]*b;
            rgb->data[i * 3 + 1] = m[3]*r + m[4]*g + m[5]*b;
            rgb->data[i * 3 + 2] = m[6]*r + m[7]*g + m[8]*b;
        }

        return in;
    }
};

// ============================================================
// Crop - Active Area Crop
// ============================================================
// Input: RgbF32
// Output: RgbF32 (cropped)
// Reads: gear_crop_left, gear_crop_top, gear_crop_width, gear_crop_height

class CropLink : public Link {
public:
    Name name() const override { return "crop"; }
    Name type() const override { return "head"; }

    Data flow(Data in) override {
        auto* rgb = static_cast<RgbF32*>(in.page);
        if (!rgb || !rgb->data) {
            in.info.text("error", "crop: no input data");
            return in;
        }

        int src_w = rgb->width;
        int src_h = rgb->height;

        int left = static_cast<int>(in.info.dial("gear_crop_left"));
        int top = static_cast<int>(in.info.dial("gear_crop_top"));
        int crop_w = static_cast<int>(in.info.dial("gear_crop_width"));
        int crop_h = static_cast<int>(in.info.dial("gear_crop_height"));

        // Validate crop region
        if (crop_w <= 0 || crop_h <= 0 ||
            left < 0 || top < 0 ||
            left + crop_w > src_w || top + crop_h > src_h) {
            // No crop needed or invalid - pass through
            return in;
        }

        auto* out = new RgbF32();
        out->data = new float[static_cast<size_t>(crop_w) * crop_h * 3];
        out->width = crop_w;
        out->height = crop_h;

        for (int y = 0; y < crop_h; y++) {
            for (int x = 0; x < crop_w; x++) {
                size_t src_idx = (static_cast<size_t>(top + y) * src_w + (left + x)) * 3;
                size_t dst_idx = (static_cast<size_t>(y) * crop_w + x) * 3;
                out->data[dst_idx + 0] = rgb->data[src_idx + 0];
                out->data[dst_idx + 1] = rgb->data[src_idx + 1];
                out->data[dst_idx + 2] = rgb->data[src_idx + 2];
            }
        }

        // Update info with final dimensions
        in.info.dial("width", static_cast<float>(crop_w));
        in.info.dial("height", static_cast<float>(crop_h));

        // Clean up input
        delete[] rgb->data;
        delete rgb;

        in.page = out;
        return in;
    }
};

// ============================================================
// Factory functions
// ============================================================

Hold<Link> blc() { return Hold<Link>(new BlcLink()); }
Hold<Link> wb() { return Hold<Link>(new WbLink()); }
Hold<Link> demosaic() { return Hold<Link>(new DemosaicLink()); }
Hold<Link> cst() { return Hold<Link>(new CstLink()); }
Hold<Link> crop() { return Hold<Link>(new CropLink()); }

} // namespace pipe

// diff.cpp - Image comparison tool
//
// Compares two image files using delta-E and reports pass/fail.
// Used for verifying transpiled modules match darktable output.
//
// Usage: ./diff <reference.bin> <output.bin> [--bits|--bayer|--rgb|--lab|--display]
//
// PASS CRITERIA:
//   --bits:    exact binary match (memcmp)
//   --bayer:   float match (mean < 1e-6)
//   --rgb:     mean delta-E < 0.01
//   --lab:     mean delta-E < 0.01
//   --display: mean delta-E < 1.0

#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>

namespace pqtr {

// ============================================================================
// Types
// ============================================================================

enum Colorspace { BITS, BAYER, LINEAR_RGB, LAB, DISPLAY_SRGB };

const char* colorspace_name(Colorspace cs) {
    switch (cs) {
        case BITS:         return "BITS";
        case BAYER:        return "BAYER";
        case LINEAR_RGB:   return "LINEAR_RGB";
        case LAB:          return "LAB";
        case DISPLAY_SRGB: return "DISPLAY_SRGB";
        default:           return "UNKNOWN";
    }
}

struct DiffResult {
    int width = 0;
    int height = 0;
    size_t pixels = 0;
    double mean_de = 0.0;
    double max_de = 0.0;
    double pct_above_1 = 0.0;
    double pct_above_2 = 0.0;
};

// ============================================================================
// Colorspace helpers
// ============================================================================

// D50 white point
static constexpr float d50_inv[3] = { 1.0f/0.9642f, 1.0f, 1.0f/0.8249f };

// sRGB to XYZ matrix (D50 adapted)
static constexpr float srgb_to_xyz[3][3] = {
    { 0.4360747f, 0.2225045f, 0.0139322f },
    { 0.3850649f, 0.7168786f, 0.0971045f },
    { 0.1430804f, 0.0606169f, 0.7141733f }
};

static float srgb_to_linear(uint8_t v) {
    float f = v / 255.0f;
    if (f <= 0.04045f) return f / 12.92f;
    return std::pow((f + 0.055f) / 1.055f, 2.4f);
}

static float cbrt_5f(float f) {
    uint32_t* p = (uint32_t*)&f;
    *p = *p / 3 + 709921077;
    return f;
}

static float cbrta_halleyf(float a, float R) {
    const float a3 = a * a * a;
    return a * (a3 + R + R) / (a3 + a3 + R);
}

static float lab_f(float x) {
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? cbrta_halleyf(cbrt_5f(x), x) : (kappa * x + 16.0f) / 116.0f;
}

static void srgb_to_lab(uint8_t r, uint8_t g, uint8_t b, float Lab[3]) {
    float lin[3] = { srgb_to_linear(r), srgb_to_linear(g), srgb_to_linear(b) };

    float XYZ[3] = {
        srgb_to_xyz[0][0]*lin[0] + srgb_to_xyz[1][0]*lin[1] + srgb_to_xyz[2][0]*lin[2],
        srgb_to_xyz[0][1]*lin[0] + srgb_to_xyz[1][1]*lin[1] + srgb_to_xyz[2][1]*lin[2],
        srgb_to_xyz[0][2]*lin[0] + srgb_to_xyz[1][2]*lin[1] + srgb_to_xyz[2][2]*lin[2]
    };

    const float fx = lab_f(XYZ[0] * d50_inv[0]);
    const float fy = lab_f(XYZ[1] * d50_inv[1]);
    const float fz = lab_f(XYZ[2] * d50_inv[2]);

    Lab[0] = 116.0f * fy - 16.0f;
    Lab[1] = 500.0f * (fx - fy);
    Lab[2] = 200.0f * (fy - fz);
}

static float delta_e_76(const float Lab1[3], const float Lab2[3]) {
    float dL = Lab1[0] - Lab2[0];
    float da = Lab1[1] - Lab2[1];
    float db = Lab1[2] - Lab2[2];
    return std::sqrt(dL*dL + da*da + db*db);
}

// ============================================================================
// Comparison functions
// ============================================================================

static DiffResult diff_bayer(const float* buf1, const float* buf2, size_t count) {
    DiffResult result;
    result.pixels = count;

    double sum = 0.0, max_de = 0.0;
    size_t above_1 = 0, above_2 = 0;

    for (size_t i = 0; i < count; i++) {
        float de = std::abs(buf1[i] - buf2[i]);
        sum += de;
        if (de > max_de) max_de = de;
        if (de > 0.01f) above_1++;
        if (de > 0.02f) above_2++;
    }

    result.mean_de = sum / count;
    result.max_de = max_de;
    result.pct_above_1 = 100.0 * above_1 / count;
    result.pct_above_2 = 100.0 * above_2 / count;
    return result;
}

static DiffResult diff_rgb(const float* buf1, const float* buf2, size_t pixels) {
    DiffResult result;
    result.pixels = pixels;

    double sum = 0.0, max_de = 0.0;
    size_t above_1 = 0, above_2 = 0;

    for (size_t i = 0; i < pixels; i++) {
        size_t idx = i * 4;  // RGBA
        float XYZ1[3], XYZ2[3], Lab1[3], Lab2[3];

        for (int c = 0; c < 3; c++) {
            XYZ1[c] = srgb_to_xyz[0][c]*buf1[idx] +
                      srgb_to_xyz[1][c]*buf1[idx+1] +
                      srgb_to_xyz[2][c]*buf1[idx+2];
            XYZ2[c] = srgb_to_xyz[0][c]*buf2[idx] +
                      srgb_to_xyz[1][c]*buf2[idx+1] +
                      srgb_to_xyz[2][c]*buf2[idx+2];
        }

        float fx1 = lab_f(XYZ1[0]*d50_inv[0]);
        float fy1 = lab_f(XYZ1[1]*d50_inv[1]);
        float fz1 = lab_f(XYZ1[2]*d50_inv[2]);
        float fx2 = lab_f(XYZ2[0]*d50_inv[0]);
        float fy2 = lab_f(XYZ2[1]*d50_inv[1]);
        float fz2 = lab_f(XYZ2[2]*d50_inv[2]);

        Lab1[0] = 116.0f*fy1 - 16.0f; Lab1[1] = 500.0f*(fx1-fy1); Lab1[2] = 200.0f*(fy1-fz1);
        Lab2[0] = 116.0f*fy2 - 16.0f; Lab2[1] = 500.0f*(fx2-fy2); Lab2[2] = 200.0f*(fy2-fz2);

        float de = delta_e_76(Lab1, Lab2);
        sum += de;
        if (de > max_de) max_de = de;
        if (de > 1.0f) above_1++;
        if (de > 2.0f) above_2++;
    }

    result.mean_de = sum / pixels;
    result.max_de = max_de;
    result.pct_above_1 = 100.0 * above_1 / pixels;
    result.pct_above_2 = 100.0 * above_2 / pixels;
    return result;
}

static DiffResult diff_lab(const float* buf1, const float* buf2, size_t pixels) {
    DiffResult result;
    result.pixels = pixels;

    double sum = 0.0, max_de = 0.0;
    size_t above_1 = 0, above_2 = 0;

    for (size_t i = 0; i < pixels; i++) {
        size_t idx = i * 4;
        float Lab1[3] = { buf1[idx], buf1[idx+1], buf1[idx+2] };
        float Lab2[3] = { buf2[idx], buf2[idx+1], buf2[idx+2] };

        float de = delta_e_76(Lab1, Lab2);
        sum += de;
        if (de > max_de) max_de = de;
        if (de > 1.0f) above_1++;
        if (de > 2.0f) above_2++;
    }

    result.mean_de = sum / pixels;
    result.max_de = max_de;
    result.pct_above_1 = 100.0 * above_1 / pixels;
    result.pct_above_2 = 100.0 * above_2 / pixels;
    return result;
}

static DiffResult diff_display(const uint8_t* buf1, const uint8_t* buf2, size_t pixels) {
    DiffResult result;
    result.pixels = pixels;

    double sum = 0.0, max_de = 0.0;
    size_t above_1 = 0, above_2 = 0;

    for (size_t i = 0; i < pixels; i++) {
        size_t idx = i * 3;
        float Lab1[3], Lab2[3];
        srgb_to_lab(buf1[idx], buf1[idx+1], buf1[idx+2], Lab1);
        srgb_to_lab(buf2[idx], buf2[idx+1], buf2[idx+2], Lab2);

        float de = delta_e_76(Lab1, Lab2);
        sum += de;
        if (de > max_de) max_de = de;
        if (de > 1.0f) above_1++;
        if (de > 2.0f) above_2++;
    }

    result.mean_de = sum / pixels;
    result.max_de = max_de;
    result.pct_above_1 = 100.0 * above_1 / pixels;
    result.pct_above_2 = 100.0 * above_2 / pixels;
    return result;
}

// ============================================================================
// File I/O
// ============================================================================

// Detect PFM header and return offset to raw data (0 if not PFM)
static size_t pfm_header_size(const uint8_t* data, size_t size) {
    if (size < 10) return 0;

    // Check for "Pf\n" (grayscale) or "PF\n" (RGB)
    if (data[0] != 'P' || (data[1] != 'f' && data[1] != 'F') || data[2] != '\n')
        return 0;

    // Find end of width/height line
    size_t pos = 3;
    while (pos < size && data[pos] != '\n') pos++;
    if (pos >= size) return 0;
    pos++; // skip newline

    // Find end of scale line
    while (pos < size && data[pos] != '\n') pos++;
    if (pos >= size) return 0;
    pos++; // skip newline

    return pos;
}

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};

    size_t size = f.tellg();
    f.seekg(0);

    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);

    // Auto-detect and strip PFM header
    size_t offset = pfm_header_size(data.data(), data.size());
    if (offset > 0) {
        std::vector<uint8_t> raw(data.begin() + offset, data.end());
        return raw;
    }

    return data;
}

} // namespace pqtr

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    using namespace pqtr;

    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <reference.bin> <output.bin> [--bits|--bayer|--rgb|--lab|--display]\n", argv[0]);
        std::fprintf(stderr, "\nComparison modes:\n");
        std::fprintf(stderr, "  --bits    Exact binary match (memcmp) - for head output, raw data\n");
        std::fprintf(stderr, "  --bayer   Single-channel float32, exact match (1e-6)\n");
        std::fprintf(stderr, "  --rgb     4-channel float32 RGBA, delta-E < 0.01\n");
        std::fprintf(stderr, "  --lab     4-channel float32 Lab, delta-E < 0.01\n");
        std::fprintf(stderr, "  --display 3-channel uint8 RGB, delta-E < 1.0\n");
        return 1;
    }

    // Parse colorspace
    Colorspace cs = LINEAR_RGB;
    float threshold = 0.01f;

    for (int i = 3; i < argc; i++) {
        if (std::strcmp(argv[i], "--bits") == 0) {
            cs = BITS;
            threshold = 0.0f;
        } else if (std::strcmp(argv[i], "--bayer") == 0) {
            cs = BAYER;
            threshold = 1e-6f;
        } else if (std::strcmp(argv[i], "--rgb") == 0) {
            cs = LINEAR_RGB;
            threshold = 0.01f;
        } else if (std::strcmp(argv[i], "--lab") == 0) {
            cs = LAB;
            threshold = 0.01f;
        } else if (std::strcmp(argv[i], "--display") == 0) {
            cs = DISPLAY_SRGB;
            threshold = 1.0f;
        }
    }

    // Read files
    auto ref_data = read_file(argv[1]);
    auto out_data = read_file(argv[2]);

    if (ref_data.empty()) {
        std::fprintf(stderr, "Failed to read: %s\n", argv[1]);
        return 1;
    }
    if (out_data.empty()) {
        std::fprintf(stderr, "Failed to read: %s\n", argv[2]);
        return 1;
    }
    if (ref_data.size() != out_data.size()) {
        std::fprintf(stderr, "Size mismatch: %zu vs %zu bytes\n", ref_data.size(), out_data.size());
        return 1;
    }

    // Compare
    DiffResult result;

    switch (cs) {
        case BITS: {
            // Exact binary comparison
            int cmp = std::memcmp(ref_data.data(), out_data.data(), ref_data.size());
            result.pixels = ref_data.size();
            result.mean_de = (cmp == 0) ? 0.0 : 1.0;
            result.max_de = result.mean_de;
            break;
        }
        case BAYER: {
            size_t count = ref_data.size() / sizeof(float);
            result = diff_bayer(
                reinterpret_cast<const float*>(ref_data.data()),
                reinterpret_cast<const float*>(out_data.data()),
                count);
            break;
        }
        case LINEAR_RGB: {
            size_t pixels = ref_data.size() / (4 * sizeof(float));
            result = diff_rgb(
                reinterpret_cast<const float*>(ref_data.data()),
                reinterpret_cast<const float*>(out_data.data()),
                pixels);
            break;
        }
        case LAB: {
            size_t pixels = ref_data.size() / (4 * sizeof(float));
            result = diff_lab(
                reinterpret_cast<const float*>(ref_data.data()),
                reinterpret_cast<const float*>(out_data.data()),
                pixels);
            break;
        }
        case DISPLAY_SRGB: {
            size_t pixels = ref_data.size() / 3;
            result = diff_display(ref_data.data(), out_data.data(), pixels);
            break;
        }
    }

    // Report
    // For --bits mode, threshold=0 means exact match required (use <=)
    // For other modes, use < to allow some tolerance
    bool passed = (cs == BITS) ? (result.mean_de <= threshold)
                               : (result.mean_de < threshold);

    std::printf("=== Diff Report ===\n");
    std::printf("Colorspace: %s\n", colorspace_name(cs));
    std::printf("Pixels:     %zu\n", result.pixels);
    std::printf("Mean diff:  %.6f\n", result.mean_de);
    std::printf("Max diff:   %.6f\n", result.max_de);
    std::printf("Threshold:  %.6f\n", threshold);
    std::printf("Status:     %s\n", passed ? "PASS" : "FAIL");

    if (!passed) {
        std::printf("\nPixels above threshold:\n");
        std::printf("  >1: %.2f%%\n", result.pct_above_1);
        std::printf("  >2: %.2f%%\n", result.pct_above_2);
    }

    return passed ? 0 : 1;
}

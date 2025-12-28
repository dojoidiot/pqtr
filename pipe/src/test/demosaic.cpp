// src/test/demosaic.cpp - Test demosaic module
//
// Verifies: Bayer pattern → RGB interpolation
// Note: Exact match depends on algorithm (bilinear vs RCD etc)
//       This test checks basic sanity and correlation with rawpy LINEAR

#include "../../inc/pipe.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

static const char* RAW_FILE = "src/test/DSC00144.ARW";
static const char* REF_FILE = "src/test/ref/demosaic.bin";

static const float WB_R = 2.37890625f;
static const float WB_G = 1.0f;
static const float WB_B = 1.56640625f;

static std::vector<uint8_t> readFile(const char* path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    auto size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

int main()
{
    std::cout << "=== demosaic Test ===\n\n";

    // 1. Load RAW
    std::cout << "Loading " << RAW_FILE << "...\n";
    auto arw_data = readFile(RAW_FILE);
    if (arw_data.empty()) {
        std::cerr << "FAIL: Cannot read RAW file\n";
        return 1;
    }

    auto head = flow::makeHead();
    auto flow = head->decode(arw_data.data(), arw_data.size());
    if (!flow) {
        std::cerr << "FAIL: Cannot decode RAW file\n";
        return 1;
    }

    auto& root = flow->info().root();
    int width = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int height = static_cast<int>(root.leaf(flow::HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    std::cout << "  Size: " << width << " x " << height << "\n";

    // 2. Run pipeline up to demosaic
    std::cout << "\nRunning rawprepare + temperature + demosaic...\n";

    auto rawprepare = flow::makeRawprepare();
    rawprepare->process(*flow);

    auto temperature = flow::makeTemperature();
    temperature->setCoeffs(WB_R, WB_G, WB_B);
    temperature->process(*flow);

    auto demosaic = flow::makeDemosaic();
    demosaic->process(*flow);

    float* rgb = flow->rgb();

    // 3. Basic sanity checks
    std::cout << "\nVerifying RGB output...\n";

    float r_min = 1e10f, r_max = -1e10f;
    float g_min = 1e10f, g_max = -1e10f;
    float b_min = 1e10f, b_max = -1e10f;

    for (size_t i = 0; i < npixels; i++) {
        size_t idx = i * 4;
        float r = rgb[idx + 0];
        float g = rgb[idx + 1];
        float b = rgb[idx + 2];

        if (r < r_min) r_min = r;
        if (r > r_max) r_max = r;
        if (g < g_min) g_min = g;
        if (g > g_max) g_max = g;
        if (b < b_min) b_min = b;
        if (b > b_max) b_max = b;
    }

    std::cout << "  R range: [" << r_min << ", " << r_max << "]\n";
    std::cout << "  G range: [" << g_min << ", " << g_max << "]\n";
    std::cout << "  B range: [" << b_min << ", " << b_max << "]\n";

    // 4. Load reference and compare (if exists)
    std::ifstream ref_stream(REF_FILE, std::ios::binary);
    if (!ref_stream) {
        std::cout << "\nWARN: Reference file not found: " << REF_FILE << "\n";
        std::cout << "Generate with: python3 scripts/gen_ref.py demosaic\n";
        std::cout << "\nSanity check only - cannot verify against reference.\n";

        // At least check values are reasonable
        bool sane = (r_min >= -0.1f && r_max <= 10.0f &&
                     g_min >= -0.1f && g_max <= 10.0f &&
                     b_min >= -0.1f && b_max <= 10.0f);
        if (sane) {
            std::cout << "PASS: RGB values in reasonable range\n";
            return 0;
        } else {
            std::cerr << "FAIL: RGB values out of range\n";
            return 1;
        }
    }

    // Reference has shape (H, W, 3) as float32
    // rawpy crops to visible area (3936 wide), we have 3968
    // Compare common area only
    size_t ref_w = 3936;
    size_t ref_h = height;
    size_t ref_pixels = ref_w * ref_h;

    std::vector<float> ref(ref_pixels * 3);
    ref_stream.read(reinterpret_cast<char*>(ref.data()), ref_pixels * 3 * sizeof(float));

    std::cout << "\nComparing against reference (cropped to " << ref_w << " x " << ref_h << ")...\n";

    // Calculate correlation for each channel
    double sum_r_our = 0, sum_r_ref = 0, sum_r_prod = 0;
    double sum_g_our = 0, sum_g_ref = 0, sum_g_prod = 0;
    double sum_b_our = 0, sum_b_ref = 0, sum_b_prod = 0;
    double sum_r2_our = 0, sum_r2_ref = 0;
    double sum_g2_our = 0, sum_g2_ref = 0;
    double sum_b2_our = 0, sum_b2_ref = 0;

    for (size_t y = 0; y < ref_h; y++) {
        for (size_t x = 0; x < ref_w; x++) {
            size_t our_idx = (y * width + x) * 4;
            size_t ref_idx = (y * ref_w + x) * 3;

            float our_r = rgb[our_idx + 0];
            float our_g = rgb[our_idx + 1];
            float our_b = rgb[our_idx + 2];

            float ref_r = ref[ref_idx + 0];
            float ref_g = ref[ref_idx + 1];
            float ref_b = ref[ref_idx + 2];

            sum_r_our += our_r; sum_r_ref += ref_r; sum_r_prod += our_r * ref_r;
            sum_g_our += our_g; sum_g_ref += ref_g; sum_g_prod += our_g * ref_g;
            sum_b_our += our_b; sum_b_ref += ref_b; sum_b_prod += our_b * ref_b;

            sum_r2_our += our_r * our_r; sum_r2_ref += ref_r * ref_r;
            sum_g2_our += our_g * our_g; sum_g2_ref += ref_g * ref_g;
            sum_b2_our += our_b * our_b; sum_b2_ref += ref_b * ref_b;
        }
    }

    double n = ref_pixels;
    auto calc_corr = [n](double sum_x, double sum_y, double sum_xy, double sum_x2, double sum_y2) {
        double num = n * sum_xy - sum_x * sum_y;
        double den = std::sqrt((n * sum_x2 - sum_x * sum_x) * (n * sum_y2 - sum_y * sum_y));
        return den > 0 ? num / den : 0;
    };

    double corr_r = calc_corr(sum_r_our, sum_r_ref, sum_r_prod, sum_r2_our, sum_r2_ref);
    double corr_g = calc_corr(sum_g_our, sum_g_ref, sum_g_prod, sum_g2_our, sum_g2_ref);
    double corr_b = calc_corr(sum_b_our, sum_b_ref, sum_b_prod, sum_b2_our, sum_b2_ref);

    std::cout << "  Correlation R: " << corr_r << "\n";
    std::cout << "  Correlation G: " << corr_g << "\n";
    std::cout << "  Correlation B: " << corr_b << "\n";

    // Demosaic algorithms differ, so we expect high correlation but not exact match
    if (corr_r > 0.99 && corr_g > 0.99 && corr_b > 0.99) {
        std::cout << "\nPASS: demosaic correlates well with reference\n";
        return 0;
    } else if (corr_r > 0.95 && corr_g > 0.95 && corr_b > 0.95) {
        std::cout << "\nWARN: demosaic correlation acceptable but not ideal\n";
        return 0;
    } else {
        std::cerr << "\nFAIL: demosaic poorly correlated with reference\n";
        return 1;
    }
}

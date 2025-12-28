// src/test/temperature.cpp - Test temperature (WB) module
//
// Verifies: WB coefficients applied to RGGB bayer pattern
// Reference: LibRaw normalized + WB (pre-generated)

#include "../../inc/pipe.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

static const char* RAW_FILE = "src/test/DSC00144.ARW";
static const char* REF_FILE = "src/test/ref/temperature.bin";

// XMP WB coefficients
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
    std::cout << "=== temperature Test ===\n\n";

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
    std::cout << "  WB coeffs: [" << WB_R << ", " << WB_G << ", " << WB_B << "]\n";

    // 2. Run rawprepare then temperature
    std::cout << "\nRunning rawprepare + temperature...\n";
    auto rawprepare = flow::makeRawprepare();
    rawprepare->process(*flow);

    auto temperature = flow::makeTemperature();
    temperature->setCoeffs(WB_R, WB_G, WB_B);
    temperature->process(*flow);

    float* fdata = flow->fdata();

    // 3. Load reference
    std::ifstream ref_stream(REF_FILE, std::ios::binary);
    if (!ref_stream) {
        std::cerr << "\nWARN: Reference file not found: " << REF_FILE << "\n";
        std::cerr << "Generate with: python3 scripts/gen_ref.py temperature\n";
        return 1;
    }

    std::cout << "\nComparing against reference...\n";
    std::vector<float> ref(npixels);
    ref_stream.read(reinterpret_cast<char*>(ref.data()), npixels * sizeof(float));

    if (!ref_stream) {
        std::cerr << "FAIL: Cannot read reference file\n";
        return 1;
    }

    double max_diff = 0;
    double sum_diff = 0;
    int mismatches = 0;

    for (size_t i = 0; i < npixels; i++) {
        double diff = std::abs(fdata[i] - ref[i]);
        if (diff > max_diff) max_diff = diff;
        sum_diff += diff;
        if (diff > 1e-5) mismatches++;
    }

    double mean_diff = sum_diff / npixels;

    std::cout << "  Max diff: " << max_diff << "\n";
    std::cout << "  Mean diff: " << mean_diff << "\n";
    std::cout << "  Mismatches: " << mismatches << " / " << npixels << "\n";

    if (max_diff < 1e-4) {
        std::cout << "\nPASS: temperature matches reference\n";
        return 0;
    } else {
        std::cerr << "\nFAIL: temperature differs from reference\n";
        return 1;
    }
}

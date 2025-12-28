// src/test/rawprepare.cpp - Test rawprepare module
//
// Verifies: (raw - black) / (white - black) normalization
// Reference: LibRaw normalized output (pre-generated)

#include "../../inc/pipe.hpp"
#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

static const char* RAW_FILE = "src/test/DSC00144.ARW";
static const char* REF_FILE = "src/test/ref/rawprepare.bin";  // Pre-generated with rawpy

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
    std::cout << "=== rawprepare Test ===\n\n";

    // 1. Load RAW with our decoder
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
    int black = static_cast<int>(root.leaf(flow::BLACK).dial());
    int white = static_cast<int>(root.leaf(flow::WHITE).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    std::cout << "  Size: " << width << " x " << height << "\n";
    std::cout << "  Black: " << black << ", White: " << white << "\n";

    // 2. Run rawprepare
    std::cout << "\nRunning rawprepare...\n";
    auto rawprepare = flow::makeRawprepare();
    rawprepare->process(*flow);

    float* fdata = flow->fdata();

    // 3. Load reference (if exists)
    std::ifstream ref_stream(REF_FILE, std::ios::binary);
    if (!ref_stream) {
        std::cerr << "\nWARN: Reference file not found: " << REF_FILE << "\n";
        std::cerr << "Generate with: python3 scripts/gen_ref.py rawprepare\n";

        // Still verify math is correct
        std::cout << "\nVerifying normalization math...\n";
        uint16_t* raw = flow->data();
        int errors = 0;
        for (size_t i = 0; i < std::min(npixels, (size_t)1000); i++) {
            float expected = static_cast<float>(raw[i] - black) / (white - black);
            float actual = fdata[i];
            if (std::abs(actual - expected) > 1e-6f) {
                if (errors < 5) {
                    std::cerr << "  Mismatch at " << i << ": expected "
                              << expected << ", got " << actual << "\n";
                }
                errors++;
            }
        }
        if (errors == 0) {
            std::cout << "  PASS: Normalization formula correct\n";
        } else {
            std::cerr << "  FAIL: " << errors << " normalization errors\n";
            return 1;
        }
        return 0;
    }

    // 4. Compare against reference
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
        if (diff > 1e-6) mismatches++;
    }

    double mean_diff = sum_diff / npixels;

    std::cout << "  Max diff: " << max_diff << "\n";
    std::cout << "  Mean diff: " << mean_diff << "\n";
    std::cout << "  Mismatches: " << mismatches << " / " << npixels << "\n";

    if (max_diff < 1e-5) {
        std::cout << "\nPASS: rawprepare matches reference\n";
        return 0;
    } else {
        std::cerr << "\nFAIL: rawprepare differs from reference\n";
        return 1;
    }
}

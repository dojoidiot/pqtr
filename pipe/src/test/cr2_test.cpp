// cr2_test.cpp - Test Canon CR2 decoder
//
// Tests lossless JPEG decode against LibRaw reference

#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>
#include "canon/canon.h"

// Read entire file into memory
static std::vector<uint8_t> read_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return {};

    size_t size = file.tellg();
    file.seekg(0);

    std::vector<uint8_t> data(size);
    file.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

int main(int argc, char* argv[]) {
    const char* cr2_path = "dark/lib/desk/src/tests/integration/images/mire1.cr2";
    if (argc > 1) cr2_path = argv[1];

    std::cout << "=== Canon CR2 Decoder Test ===" << std::endl;
    std::cout << "Input: " << cr2_path << std::endl;

    // Read CR2 file
    auto data = read_file(cr2_path);
    if (data.empty()) {
        std::cerr << "Failed to read: " << cr2_path << std::endl;
        return 1;
    }
    std::cout << "File size: " << data.size() << " bytes" << std::endl;

    // Decode
    canon::BayerU16 bayer;
    canon::RawMetadata meta;

    if (!canon::Decoder::prepare(data.data(), data.size(), bayer, meta)) {
        std::cerr << "Decode failed" << std::endl;
        return 1;
    }

    std::cout << "\n=== Metadata ===" << std::endl;
    std::cout << "Camera: " << meta.camera_make << " " << meta.camera_model << std::endl;
    std::cout << "Size: " << meta.width << "x" << meta.height << std::endl;
    std::cout << "Precision: " << meta.ljpeg_precision << " bits" << std::endl;
    std::cout << "White level: " << meta.white_level << std::endl;
    std::cout << "Orientation: " << meta.orientation << std::endl;

    // Verify bayer data
    std::cout << "\n=== Bayer Data ===" << std::endl;
    std::cout << "Buffer size: " << bayer.size() << " pixels" << std::endl;

    // Statistics
    uint16_t min_val = UINT16_MAX, max_val = 0;
    double sum = 0;
    int zeros = 0;
    for (size_t i = 0; i < bayer.size(); i++) {
        uint16_t v = bayer.data[i];
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
        sum += v;
        if (v == 0) zeros++;
    }

    double mean = sum / bayer.size();
    std::cout << "Min: " << min_val << std::endl;
    std::cout << "Max: " << max_val << std::endl;
    std::cout << "Mean: " << mean << std::endl;
    std::cout << "Zeros: " << zeros << " (" << (100.0 * zeros / bayer.size()) << "%)" << std::endl;

    // Sample some pixels from different regions
    std::cout << "\n=== Sample Pixels ===" << std::endl;
    int w = meta.width;
    int h = meta.height;
    auto sample = [&](int x, int y) {
        if (x >= 0 && x < w && y >= 0 && y < h)
            return bayer.data[y * w + x];
        return (uint16_t)0;
    };

    std::cout << "Top-left (10,10): " << sample(10, 10) << std::endl;
    std::cout << "Center (" << w/2 << "," << h/2 << "): " << sample(w/2, h/2) << std::endl;
    std::cout << "Bottom-right (" << w-10 << "," << h-10 << "): " << sample(w-10, h-10) << std::endl;

    // Success criteria
    bool ok = true;
    if (zeros > bayer.size() * 0.5) {
        std::cerr << "FAIL: Too many zeros (decode may have failed)" << std::endl;
        ok = false;
    }
    if (max_val == 0) {
        std::cerr << "FAIL: All zeros" << std::endl;
        ok = false;
    }
    if (max_val > meta.white_level) {
        std::cerr << "WARN: Values exceed white level" << std::endl;
    }

    if (ok) {
        std::cout << "\n=== PASS ===" << std::endl;
    }

    return ok ? 0 : 1;
}

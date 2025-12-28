// bilat_test.cpp - Phase 4 bilat (local Laplacian) test
//
// Tests bilat module on Lab data from colorin

#include "../../inc/pipe.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <sys/stat.h>

static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

static bool write_file(const char* path, const void* data, size_t size) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(static_cast<const char*>(data), size);
    return f.good();
}

int main() {
    const char* arw_path = "src/test/DSC00144.ARW";
    const char* out_dir = "tmp/var/pipe";

    mkdir("tmp", 0755);
    mkdir("tmp/var", 0755);
    mkdir(out_dir, 0755);

    std::cout << "=== Phase 4: bilat Test ===\n\n";

    // Load ARW
    std::cout << "Loading " << arw_path << "...\n";
    auto arw_data = read_file(arw_path);
    if (arw_data.empty()) {
        std::cerr << "FAIL: Cannot read " << arw_path << "\n";
        return 1;
    }

    // Decode
    auto head = flow::makeHead();
    auto flow = head->decode(arw_data.data(), arw_data.size());
    if (!flow) {
        std::cerr << "FAIL: Head::decode() returned null\n";
        return 1;
    }

    auto& root = flow->info().root();
    int width = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int height = static_cast<int>(root.leaf(flow::HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;
    std::cout << "  Dimensions: " << width << " x " << height << "\n";

    // Pipeline up to colorin (to get Lab)
    std::cout << "\nRunning pipeline to colorin...\n";

    auto rawprepare = flow::makeRawprepare();
    rawprepare->process(*flow);

    auto temperature = flow::makeTemperature();
    temperature->setCoeffs(2.36328f, 1.0f, 1.57812f);  // from final.xmp
    temperature->process(*flow);

    auto highlights = flow::makeHighlights();
    highlights->setClip(1.0f);
    highlights->process(*flow);

    auto demosaic = flow::makeDemosaic();
    demosaic->process(*flow);

    auto exposure1 = flow::makeExposure();
    exposure1->setParams(0.7f, 0.0f);  // first exposure from final.xmp
    exposure1->process(*flow);

    auto colorin = flow::makeColorin();
    colorin->process(*flow);

    // Now we have Lab data
    float* lab_data = flow->rgb();

    // Check Lab range before bilat
    std::cout << "\nLab range BEFORE bilat:\n";
    float lab_min[3] = {1e10f, 1e10f, 1e10f};
    float lab_max[3] = {-1e10f, -1e10f, -1e10f};
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = lab_data[i * 4 + c];
            if (v < lab_min[c]) lab_min[c] = v;
            if (v > lab_max[c]) lab_max[c] = v;
        }
    }
    std::cout << "  L: [" << lab_min[0] << ", " << lab_max[0] << "]\n";
    std::cout << "  a: [" << lab_min[1] << ", " << lab_max[1] << "]\n";
    std::cout << "  b: [" << lab_min[2] << ", " << lab_max[2] << "]\n";

    // Save copy of L channel before bilat
    std::vector<float> L_before(npixels);
    for (size_t i = 0; i < npixels; i++) {
        L_before[i] = lab_data[i * 4];
    }

    // Run bilat with final.xmp params
    // sigma=0.5 (midtone), shadows=0.5, highlights=0.5, clarity=0.1
    std::cout << "\nRunning bilat (sigma=0.5, shadows=0.5, highlights=0.5, clarity=0.1)...\n";
    auto bilat = flow::makeBilat();
    bilat->setParams(0.5f, 0.5f, 0.5f, 0.1f);
    bilat->process(*flow);

    // Check Lab range after bilat
    std::cout << "\nLab range AFTER bilat:\n";
    for (int c = 0; c < 3; c++) {
        lab_min[c] = 1e10f;
        lab_max[c] = -1e10f;
    }
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = lab_data[i * 4 + c];
            if (v < lab_min[c]) lab_min[c] = v;
            if (v > lab_max[c]) lab_max[c] = v;
        }
    }
    std::cout << "  L: [" << lab_min[0] << ", " << lab_max[0] << "]\n";
    std::cout << "  a: [" << lab_min[1] << ", " << lab_max[1] << "]\n";
    std::cout << "  b: [" << lab_min[2] << ", " << lab_max[2] << "]\n";

    // Count changed L values
    int changed = 0;
    double total_diff = 0.0;
    for (size_t i = 0; i < npixels; i++) {
        float diff = std::abs(lab_data[i * 4] - L_before[i]);
        if (diff > 0.001f) {
            changed++;
            total_diff += diff;
        }
    }
    std::cout << "\nL channel changes:\n";
    std::cout << "  Pixels modified: " << changed << " (" << (100.0f * changed / npixels) << "%)\n";
    if (changed > 0) {
        std::cout << "  Mean L change: " << (total_diff / changed) << "\n";
    }

    // Continue pipeline to colorout + gamma
    std::cout << "\nContinuing pipeline...\n";
    auto colorout = flow::makeColorout();
    colorout->process(*flow);

    auto gamma = flow::makeGamma();
    gamma->process(*flow);

    // Save output
    float* rgb_data = flow->rgb();
    std::vector<uint8_t> rgb8(npixels * 3);
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb_data[i * 4 + c];
            v = std::max(0.0f, std::min(1.0f, v));
            rgb8[i * 3 + c] = static_cast<uint8_t>(v * 255.0f + 0.5f);
        }
    }

    auto png_data = flow::swap(rgb8.data(), width, height, flow::BIN, flow::PNG);
    std::string out_path = std::string(out_dir) + "/phase4-bilat.png";
    if (!png_data.empty() && write_file(out_path.c_str(), png_data.data(), png_data.size())) {
        std::cout << "  Saved: " << out_path << " (" << png_data.size() << " bytes)\n";
    }

    std::cout << "\n=== Phase 4 bilat Test Complete ===\n";
    return 0;
}

// cr2_pipeline.cpp - Quick CR2 full pipeline test
//
// Runs Canon CR2 through pipeline with neutral/default params
// to verify the pipeline works with Canon data

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
    const char* cr2_path = "dark/lib/desk/src/tests/integration/images/mire1.cr2";
    const char* out_dir = "tmp/var/pipe";

    mkdir("tmp", 0755);
    mkdir("tmp/var", 0755);
    mkdir(out_dir, 0755);

    std::cout << "=== CR2 Pipeline Test ===\n\n";

    // 1. Load and decode CR2
    std::cout << "Loading " << cr2_path << "...\n";
    auto data = read_file(cr2_path);
    if (data.empty()) {
        std::cerr << "FAIL: Cannot read file\n";
        return 1;
    }

    auto head = flow::makeHead();
    auto flow = head->decode(data.data(), data.size());
    if (!flow) {
        std::cerr << "FAIL: Decode failed\n";
        return 1;
    }

    auto& root = flow->info().root();
    int width = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int height = static_cast<int>(root.leaf(flow::HEIGHT).dial());
    size_t npixels = (size_t)width * height;

    std::cout << "  Size: " << width << "x" << height << "\n";
    std::cout << "  Black: " << root.leaf(flow::BLACK).dial() << "\n";
    std::cout << "  White: " << root.leaf(flow::WHITE).dial() << "\n";

    // 2. rawprepare - Black level subtraction
    std::cout << "\nrawprepare...\n";
    auto rawprepare = flow::makeRawprepare();
    rawprepare->process(*flow);

    // 3. temperature - Use as-shot WB from camera
    std::cout << "temperature (as-shot WB)...\n";
    auto& wb = root.next("wb");
    float wb_r = wb.leaf("r").dial();
    float wb_g = wb.leaf("g1").dial();
    float wb_b = wb.leaf("b").dial();
    // Normalize to green=1
    float coeff_r = wb_r / wb_g;
    float coeff_b = wb_b / wb_g;
    std::cout << "  WB coeffs: [" << coeff_r << ", 1.0, " << coeff_b << "]\n";

    auto temperature = flow::makeTemperature();
    temperature->setCoeffs(coeff_r, 1.0f, coeff_b);
    temperature->process(*flow);

    // 4. highlights
    std::cout << "highlights...\n";
    auto highlights = flow::makeHighlights();
    highlights->setClip(1.0f);
    highlights->process(*flow);

    // 5. demosaic
    std::cout << "demosaic...\n";
    auto demosaic = flow::makeDemosaic();
    demosaic->process(*flow);

    // 6. exposure (+0.5 EV default boost)
    std::cout << "exposure (+0.5 EV)...\n";
    auto exposure = flow::makeExposure();
    exposure->setParams(0.5f, 0.0f);
    exposure->process(*flow);

    // 7. colorin - Camera RGB → Lab
    std::cout << "colorin...\n";
    auto colorin = flow::makeColorin();
    colorin->process(*flow);

    // 8. swap Lab → RGB
    std::cout << "swap Lab→RGB...\n";
    flow::swapLabToRGB(*flow);

    // 9. filmicrgb - Tone mapping with neutral defaults
    std::cout << "filmicrgb (grey=18.45, black=-6, white=4)...\n";
    auto filmicrgb = flow::makeFilmicrgb();
    filmicrgb->setParams(18.45f, -6.0f, 4.0f, 1.0f, 0.01f, 2.5f);
    filmicrgb->process(*flow);

    // 10. swap RGB → Lab
    std::cout << "swap RGB→Lab...\n";
    flow::swapRGBToLab(*flow);

    // 11. colorout - Lab → sRGB
    std::cout << "colorout...\n";
    auto colorout = flow::makeColorout();
    colorout->process(*flow);

    // 12. gamma - sRGB transfer
    std::cout << "gamma...\n";
    auto gamma = flow::makeGamma();
    gamma->process(*flow);

    // Verify final range
    float* rgb = flow->rgb();
    float min_v = 1e10f, max_v = -1e10f;
    for (size_t i = 0; i < npixels * 4; i++) {
        if (rgb[i] < min_v) min_v = rgb[i];
        if (rgb[i] > max_v) max_v = rgb[i];
    }
    std::cout << "\nFinal range: [" << min_v << ", " << max_v << "]\n";

    // Save output
    std::cout << "\nSaving output...\n";
    std::vector<uint8_t> rgb8(npixels * 3);
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb[i * 4 + c];
            v = std::max(0.0f, std::min(1.0f, v));
            rgb8[i * 3 + c] = static_cast<uint8_t>(v * 255.0f + 0.5f);
        }
    }

    auto png = flow::swap(rgb8.data(), width, height, flow::BIN, flow::PNG);
    std::string out_path = std::string(out_dir) + "/cr2-pipeline.png";
    if (!png.empty() && write_file(out_path.c_str(), png.data(), png.size())) {
        std::cout << "  Saved: " << out_path << "\n";
    }

    std::cout << "\n=== CR2 Pipeline Complete ===\n";
    return 0;
}

// phase4.cpp - Full Phase 4 pipeline test with final.xmp
//
// Runs complete pipeline and compares to DT reference

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

// Calculate Pearson correlation between two RGB images
static void calculate_correlation(const uint8_t* img1, const uint8_t* img2,
                                   size_t npixels, double corr[3]) {
    for (int c = 0; c < 3; c++) {
        double sum1 = 0, sum2 = 0;
        for (size_t i = 0; i < npixels; i++) {
            sum1 += img1[i * 3 + c];
            sum2 += img2[i * 3 + c];
        }
        double mean1 = sum1 / npixels;
        double mean2 = sum2 / npixels;

        double num = 0, den1 = 0, den2 = 0;
        for (size_t i = 0; i < npixels; i++) {
            double d1 = img1[i * 3 + c] - mean1;
            double d2 = img2[i * 3 + c] - mean2;
            num += d1 * d2;
            den1 += d1 * d1;
            den2 += d2 * d2;
        }
        corr[c] = num / std::sqrt(den1 * den2);
    }
}

int main(int argc, char** argv) {
    const char* arw_path = "src/test/DSC00144.ARW";
    const char* ref_path = "tmp/var/pipe/phase4-dt.png";
    const char* out_dir = "tmp/var/pipe";

    // Parse XMP selection
    std::string xmp = "final";  // default
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--canon") xmp = "canon";
        if (std::string(argv[i]) == "--sony") xmp = "sony";
        if (std::string(argv[i]) == "--arw" && i + 1 < argc) arw_path = argv[++i];
    }

    if (xmp == "canon") {
        ref_path = "tmp/var/pipe/phase3-dt.png";
    }

    mkdir("tmp", 0755);
    mkdir("tmp/var", 0755);
    mkdir(out_dir, 0755);

    std::cout << "=== Phase 4: Pipeline Test (" << xmp << ".xmp) ===\n\n";

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

    std::cout << "\n--- Running Pipeline ---\n";

    // Common early stages
    std::cout << "  rawprepare...\n";
    auto rawprepare = flow::makeRawprepare();
    rawprepare->process(*flow);

    std::cout << "  temperature (r=2.36, g=1, b=1.58)...\n";
    auto temperature = flow::makeTemperature();
    temperature->setCoeffs(2.36328f, 1.0f, 1.57812f);
    temperature->process(*flow);

    std::cout << "  highlights...\n";
    auto highlights = flow::makeHighlights();
    highlights->setClip(1.0f);
    highlights->process(*flow);

    std::cout << "  demosaic...\n";
    auto demosaic = flow::makeDemosaic();
    demosaic->process(*flow);

    std::cout << "  exposure #1 (0.7 EV)...\n";
    auto exposure1 = flow::makeExposure();
    exposure1->setParams(0.7f, 0.0f);
    exposure1->process(*flow);

    std::cout << "  colorin...\n";
    auto colorin = flow::makeColorin();
    colorin->process(*flow);

    // Swap Lab → RGB for rest of processing
    std::cout << "  swap Lab→RGB...\n";
    flow::swapLabToRGB(*flow);

    // IOP 28.5: channelmixerrgb (identity matrix for these XMPs, but ready for optimization)
    std::cout << "  channelmixerrgb (identity)...\n";
    auto channelmixerrgb = flow::makeChannelmixerrgb();
    // Identity matrix - passthrough
    float red[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float green[4] = {0.0f, 1.0f, 0.0f, 0.0f};
    float blue[4] = {0.0f, 0.0f, 1.0f, 0.0f};
    channelmixerrgb->setMatrix(red, green, blue);
    channelmixerrgb->process(*flow);

    // IOP 41.5: colorbalancergb (saturation=0.2, vibrance=0.2 for non-sony XMPs)
    if (xmp != "sony") {
        std::cout << "  colorbalancergb (sat=0.2, vib=0.2)...\n";
        auto colorbalancergb = flow::makeColorbalancergb();
        // From XMP: saturation_global=0.2, vibrance=0.2
        colorbalancergb->setParams(0.0f, 0.2f, 0.2f, 0.0f, 0.1845f);
        colorbalancergb->process(*flow);
    }

    if (xmp == "final") {
        // IOP order: sigmoid (45.3) → exposure2 → filmicrgb (46.0) → bilat (54.0)
        std::cout << "  sigmoid (contrast=1.5)...\n";
        auto sigmoid = flow::makeSigmoid();
        sigmoid->setParams(1.5f, 100.0f, 0.0152f, 100.0f);
        sigmoid->process(*flow);

        // exposure #2 (between sigmoid and filmicrgb)
        std::cout << "  exposure #2 (1.1 EV)...\n";
        auto exposure2 = flow::makeExposure();
        exposure2->setParams(1.1f, 0.0f);
        exposure2->process(*flow);

        std::cout << "  filmicrgb...\n";
        auto filmicrgb = flow::makeFilmicrgb();
        filmicrgb->setParams(18.45f, -5.0f, 4.0f, 1.3f, 0.01f, 2.87537f);
        filmicrgb->process(*flow);

        // bilat in Lab (IOP 54.0, AFTER filmicrgb)
        std::cout << "  swap RGB→Lab (for bilat)...\n";
        flow::swapRGBToLab(*flow);
        std::cout << "  bilat (clarity=0.1)...\n";
        auto bilat = flow::makeBilat();
        bilat->setParams(0.5f, 0.5f, 0.5f, 0.1f);
        bilat->process(*flow);
        std::cout << "  swap Lab→RGB...\n";
        flow::swapLabToRGB(*flow);

    } else if (xmp == "sony") {
        // sony.xmp: just sigmoid (no filmicrgb, no bilat)
        std::cout << "  sigmoid (contrast=1.5)...\n";
        auto sigmoid = flow::makeSigmoid();
        sigmoid->setParams(1.5f, 100.0f, 0.0152f, 100.0f);
        sigmoid->process(*flow);

    } else {
        // canon.xmp: just filmicrgb, then exposure
        std::cout << "  filmicrgb...\n";
        auto filmicrgb = flow::makeFilmicrgb();
        filmicrgb->setParams(18.45f, -5.0f, 4.0f, 1.3f, 0.01f, 2.87537f);
        filmicrgb->process(*flow);

        std::cout << "  exposure #2 (1.2 EV)...\n";
        auto exposure2 = flow::makeExposure();
        exposure2->setParams(1.2f, 0.0f);
        exposure2->process(*flow);
    }

    // Final stages
    std::cout << "  swap RGB→Lab (for colorout)...\n";
    flow::swapRGBToLab(*flow);

    std::cout << "  colorout...\n";
    auto colorout = flow::makeColorout();
    colorout->process(*flow);

    std::cout << "  gamma...\n";
    auto gamma = flow::makeGamma();
    gamma->process(*flow);

    std::cout << "\n--- Pipeline Complete ---\n";

    // Convert to uint8
    float* rgb_data = flow->rgb();
    std::vector<uint8_t> our_rgb(npixels * 3);
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb_data[i * 4 + c];
            v = std::max(0.0f, std::min(1.0f, v));
            our_rgb[i * 3 + c] = static_cast<uint8_t>(v * 255.0f + 0.5f);
        }
    }

    // Save our output
    auto png_data = flow::swap(our_rgb.data(), width, height, flow::BIN, flow::PNG);
    std::string our_path = std::string(out_dir) + "/phase4-pipe.png";
    if (!png_data.empty() && write_file(our_path.c_str(), png_data.data(), png_data.size())) {
        std::cout << "\nSaved: " << our_path << "\n";
    }

    // Load DT reference and compare
    std::cout << "\n--- Loading DT Reference ---\n";
    auto ref_png = read_file(ref_path);
    if (ref_png.empty()) {
        std::cerr << "WARN: Cannot read " << ref_path << "\n";
        return 1;
    }

    auto ref_rgb_vec = flow::swap(ref_png.data(), ref_png.size(), 0, flow::PNG, flow::BIN);
    if (ref_rgb_vec.empty()) {
        std::cerr << "FAIL: Cannot decode reference PNG\n";
        return 1;
    }

    size_t ref_pixels = ref_rgb_vec.size() / 3;
    size_t compare_pixels = std::min(ref_pixels, npixels);

    // Calculate correlation
    std::cout << "\n--- Calculating Correlation ---\n";
    double corr[3];
    calculate_correlation(our_rgb.data(), ref_rgb_vec.data(), compare_pixels, corr);

    std::cout << "  R correlation: " << corr[0] << "\n";
    std::cout << "  G correlation: " << corr[1] << "\n";
    std::cout << "  B correlation: " << corr[2] << "\n";

    double avg_corr = (corr[0] + corr[1] + corr[2]) / 3.0;
    std::cout << "\n  Average correlation: " << avg_corr << "\n";

    // Pass/Fail
    std::cout << "\n=== Result ===\n";
    if (avg_corr >= 0.98) {
        std::cout << "PASS: Correlation " << avg_corr << " >= 0.98\n";
    } else if (avg_corr >= 0.95) {
        std::cout << "ACCEPTABLE: Correlation " << avg_corr << " >= 0.95\n";
    } else {
        std::cout << "NEEDS WORK: Correlation " << avg_corr << " < 0.95\n";
    }

    return 0;
}

// diff_test.cpp - Test delta-E diff functionality
//
// Usage: ./tmp/build/diff_test <image1.png> <image2.png> [diff_output.png]

#include "../../inc/pipe.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>

// Forward declarations from png.cpp
namespace flow {
    struct ImageResult {
        int width;
        int height;
        std::vector<uint8_t> rgb;
    };
    ImageResult decodePng(const uint8_t* data, size_t size);
    std::vector<uint8_t> encodePng(const uint8_t* rgb, int width, int height);
}

static std::vector<uint8_t> readFile(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

static bool writeFile(const char* path, const std::vector<uint8_t>& data)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::printf("Usage: %s <image1.png> <image2.png> [diff_output.png]\n", argv[0]);
        std::printf("\nCompares two PNG images using CIE Lab delta-E.\n");
        std::printf("Optionally writes a visual diff image.\n");
        return 1;
    }

    // Load images
    auto data1 = readFile(argv[1]);
    auto data2 = readFile(argv[2]);

    if (data1.empty()) {
        std::fprintf(stderr, "Failed to read: %s\n", argv[1]);
        return 1;
    }
    if (data2.empty()) {
        std::fprintf(stderr, "Failed to read: %s\n", argv[2]);
        return 1;
    }

    // Decode PNGs
    auto img1 = flow::decodePng(data1.data(), data1.size());
    auto img2 = flow::decodePng(data2.data(), data2.size());

    if (img1.rgb.empty()) {
        std::fprintf(stderr, "Failed to decode: %s\n", argv[1]);
        return 1;
    }
    if (img2.rgb.empty()) {
        std::fprintf(stderr, "Failed to decode: %s\n", argv[2]);
        return 1;
    }

    // Check dimensions match
    if (img1.width != img2.width || img1.height != img2.height) {
        std::fprintf(stderr, "Image dimensions don't match: %dx%d vs %dx%d\n",
                     img1.width, img1.height, img2.width, img2.height);
        return 1;
    }

    std::printf("Comparing images: %dx%d\n", img1.width, img1.height);
    std::printf("  %s\n", argv[1]);
    std::printf("  %s\n\n", argv[2]);

    // Compute diff
    auto result = flow::diff(img1.rgb.data(), img2.rgb.data(),
                             img1.width, img1.height, false);

    // Print stats
    flow::print_diff_stats(result);

    // Generate and save diff image if requested
    if (argc >= 4) {
        std::printf("\nGenerating diff image: %s\n", argv[3]);
        auto diff_rgb = flow::diff_image(img1.rgb.data(), img2.rgb.data(),
                                          img1.width, img1.height,
                                          flow::DiffMode::HEATMAP, 10.0f);
        auto diff_png = flow::encodePng(diff_rgb.data(), img1.width, img1.height);

        if (writeFile(argv[3], diff_png)) {
            std::printf("Saved: %s (%zu bytes)\n", argv[3], diff_png.size());
        } else {
            std::fprintf(stderr, "Failed to write: %s\n", argv[3]);
        }
    }

    return 0;
}

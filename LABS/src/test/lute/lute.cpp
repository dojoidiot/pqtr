// lute test - Camera profile learning via Flow::tune()
//
// Usage: ./lute [input.ARW]
//
// Tests the integrated learning:
//   1. f->tune(&device) runs HEAD + learns camera profile
//   2. Profile is automatically managed by camera key

#include "lute.hpp"
#include "flow.hpp"

#include <dawn/webgpu_cpp.h>

#include <fstream>
#include <iostream>
#include <cstdio>
#include <cmath>

// Dawn helper functions (defined in wgpu.cpp)
namespace dawn
{
    wgpu::Instance instance();
    wgpu::Adapter adapter(wgpu::Instance instance);
    wgpu::Device device(wgpu::Adapter adapter);
}

static std::vector<uint8_t> read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> data(size);
    fread(data.data(), 1, size, f);
    fclose(f);
    return data;
}

static std::string basename(const std::string &path)
{
    size_t slash = path.rfind('/');
    size_t dot = path.rfind('.');
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    size_t end = (dot == std::string::npos || dot < start) ? path.size() : dot;
    return path.substr(start, end - start);
}

// Linear to sRGB gamma
static uint8_t linear_to_srgb(float v)
{
    v = std::max(0.0f, std::min(1.0f, v));
    if (v <= 0.0031308f)
        v = v * 12.92f;
    else
        v = 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    return static_cast<uint8_t>(v * 255.0f + 0.5f);
}

int main(int argc, char **argv)
{
    const char *input = "src/test/flow/DSC00144.ARW";
    if (argc > 1)
        input = argv[1];

    std::cout << "=== LUTE Camera Profile Learning Test ===" << std::endl;
    std::cout << "Loading: " << input << std::endl;

    // Read raw file
    auto raw = read_file(input);
    if (raw.empty())
    {
        std::cerr << "Failed to read: " << input << std::endl;
        return 1;
    }

    std::cout << "Size: " << raw.size() << " bytes" << std::endl;

    // Load via flow
    std::string name = basename(input);
    auto bits = reinterpret_cast<uint16_t *>(raw.data());
    auto f = flow::make(name, bits, raw.size());

    auto &root = f->info().root();
    int w = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int h = static_cast<int>(root.leaf(flow::HEIGHT).dial());

    std::cout << "Dimensions: " << w << "x" << h << std::endl;

    // Check for embedded JPEG
    if (!f->view() || f->viewSize() == 0)
    {
        std::cerr << "No embedded JPEG - cannot tune" << std::endl;
        return 1;
    }
    std::cout << "Preview JPEG: " << f->viewSize() << " bytes" << std::endl;

    // =========================================================================
    // Initialize WebGPU
    // =========================================================================

    std::cout << "\nInitializing WebGPU..." << std::endl;

    wgpu::Instance inst = dawn::instance();
    wgpu::Adapter adapt = dawn::adapter(inst);
    wgpu::Device device = dawn::device(adapt);

    if (!device)
    {
        std::cerr << "Failed to create WebGPU device" << std::endl;
        return 1;
    }

    std::cout << "WebGPU ready" << std::endl;

    // =========================================================================
    // Step 1: Compute head.diff (HEAD vs JPEG, no LUT - before learning)
    // =========================================================================

    std::cout << "\n=== Computing head.diff (before learning) ===" << std::endl;

    flow::Task headDiffTask = f->tune(&device);
    std::cout << "HEAD diff " << headDiffTask.width() << "x" << headDiffTask.height() << "..." << std::endl;
    flow::Done headDiff = headDiffTask.diff();

    if (!headDiff.rgb.empty())
    {
        std::cout << "Head diff: " << headDiff.width << "x" << headDiff.height << std::endl;

        size_t dpixels = static_cast<size_t>(headDiff.width) * headDiff.height;
        std::vector<uint8_t> diff8(dpixels * 3);
        for (size_t i = 0; i < dpixels; i++)
        {
            diff8[i * 3 + 0] = linear_to_srgb(headDiff.rgb[i * 3 + 0]);
            diff8[i * 3 + 1] = linear_to_srgb(headDiff.rgb[i * 3 + 1]);
            diff8[i * 3 + 2] = linear_to_srgb(headDiff.rgb[i * 3 + 2]);
        }

        auto diffPng = flow::swap(diff8.data(), 0, headDiff.width, headDiff.height, flow::PNG);
        if (!diffPng.empty())
        {
            system("mkdir -p tmp/var/flow");
            std::string diffpath = "tmp/var/flow/" + name + ".head.diff.png";
            FILE *fp = fopen(diffpath.c_str(), "wb");
            if (fp)
            {
                fwrite(diffPng.data(), 1, diffPng.size(), fp);
                fclose(fp);
                std::cout << "Saved: " << diffpath << " (" << diffPng.size() << " bytes)" << std::endl;
            }
        }
    }

    // =========================================================================
    // Step 2: Learn camera profile
    // =========================================================================

    std::cout << "\n=== Learning camera profile ===" << std::endl;

    // Reload for learning
    raw = read_file(input);
    bits = reinterpret_cast<uint16_t *>(raw.data());
    f = flow::make(name, bits, raw.size());

    flow::Task learnTask = f->tune(&device);
    std::cout << "Processing " << learnTask.width() << "x" << learnTask.height() << "..." << std::endl;
    learnTask.post();
    flow::Done result = learnTask.done();

    std::cout << "Done: " << result.width << "x" << result.height << " scene-linear RGB" << std::endl;
    std::cout << "(Camera profile learned)" << std::endl;

    // =========================================================================
    // Step 3: Compute lute.diff (HEAD + LUT vs JPEG)
    // =========================================================================

    std::cout << "\n=== Computing lute.diff (after learning) ===" << std::endl;

    // Reload and run tune()->diff() which will apply the learned LUT
    raw = read_file(input);
    bits = reinterpret_cast<uint16_t *>(raw.data());
    f = flow::make(name, bits, raw.size());

    flow::Task luteDiffTask = f->tune(&device);
    std::cout << "LUTE diff " << luteDiffTask.width() << "x" << luteDiffTask.height() << "..." << std::endl;
    flow::Done luteDiff = luteDiffTask.diff();

    if (!luteDiff.rgb.empty())
    {
        std::cout << "Lute diff: " << luteDiff.width << "x" << luteDiff.height << std::endl;

        size_t dpixels = static_cast<size_t>(luteDiff.width) * luteDiff.height;
        std::vector<uint8_t> diff8(dpixels * 3);
        for (size_t i = 0; i < dpixels; i++)
        {
            diff8[i * 3 + 0] = linear_to_srgb(luteDiff.rgb[i * 3 + 0]);
            diff8[i * 3 + 1] = linear_to_srgb(luteDiff.rgb[i * 3 + 1]);
            diff8[i * 3 + 2] = linear_to_srgb(luteDiff.rgb[i * 3 + 2]);
        }

        auto diffPng = flow::swap(diff8.data(), 0, luteDiff.width, luteDiff.height, flow::PNG);
        if (!diffPng.empty())
        {
            std::string diffpath = "tmp/var/flow/" + name + ".lute.diff.png";
            FILE *fp = fopen(diffpath.c_str(), "wb");
            if (fp)
            {
                fwrite(diffPng.data(), 1, diffPng.size(), fp);
                fclose(fp);
                std::cout << "Saved: " << diffpath << " (" << diffPng.size() << " bytes)" << std::endl;
            }
        }
    }

    // =========================================================================
    // Save output image
    // =========================================================================

    std::cout << "\n=== Saving Output ===" << std::endl;

    // Convert to sRGB 8-bit
    size_t pixels = static_cast<size_t>(result.width) * result.height;
    std::vector<uint8_t> rgb8(pixels * 3);

    for (size_t i = 0; i < pixels; i++)
    {
        rgb8[i * 3 + 0] = linear_to_srgb(result.rgb[i * 3 + 0]);
        rgb8[i * 3 + 1] = linear_to_srgb(result.rgb[i * 3 + 1]);
        rgb8[i * 3 + 2] = linear_to_srgb(result.rgb[i * 3 + 2]);
    }

    // Encode PNG
    auto png = flow::swap(rgb8.data(), 0, result.width, result.height, flow::PNG);
    if (!png.empty())
    {
        system("mkdir -p tmp/var/flow");
        std::string pngpath = "tmp/var/flow/" + name + ".lute.png";
        FILE *fp = fopen(pngpath.c_str(), "wb");
        if (fp)
        {
            fwrite(png.data(), 1, png.size(), fp);
            fclose(fp);
            std::cout << "Saved: " << pngpath << " (" << png.size() << " bytes)" << std::endl;
        }
    }

    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}

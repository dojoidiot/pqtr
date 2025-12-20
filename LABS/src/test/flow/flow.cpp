// flow test - load RAW, process with GPU, save outputs
//
// Usage: ./flow [input.ARW]
// Output:
//   tmp/var/flow/{name}.flow.json  - metadata
//   tmp/var/flow/{name}.neg        - raw bayer
//   tmp/var/flow/{name}.jpg        - embedded preview
//   tmp/var/flow/{name}.head.png   - GPU-processed linear RGB

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

    std::cout << "Data: " << w << "x" << h << std::endl;

    // Output sidecar JSON
    std::string jsonpath = "tmp/var/flow/" + name + ".flow.json";
    std::ofstream out(jsonpath);
    out << f->info().json() << std::endl;
    out.close();
    std::cout << "Saved: " << jsonpath << std::endl;

    // Output neg (raw bayer data)
    std::string negpath = "tmp/var/flow/" + name + ".neg";
    FILE *neg = fopen(negpath.c_str(), "wb");
    if (neg)
    {
        size_t pixels = static_cast<size_t>(w) * static_cast<size_t>(h);
        fwrite(f->data(), sizeof(uint16_t), pixels, neg);
        fclose(neg);
        std::cout << "Saved: " << negpath << " (" << pixels * 2 << " bytes)" << std::endl;
    }

    // Output preview JPEG (raw bytes preserved from ARW)
    if (f->view() && f->viewSize() > 0)
    {
        std::string jpgpath = "tmp/var/flow/" + name + ".jpg";
        FILE *fp = fopen(jpgpath.c_str(), "wb");
        if (fp)
        {
            fwrite(f->view(), 1, f->viewSize(), fp);
            fclose(fp);
            std::cout << "Saved: " << jpgpath << " (" << f->viewSize() << " bytes)" << std::endl;
        }
    }

    // =========================================================================
    // GPU RAW processing
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

    // GPU processing (pipelines created internally, warp applied if distortion data present)
    flow::Task task = f->open(&device);

    std::cout << "Processing " << task.width() << "x" << task.height() << "..." << std::endl;

    task.post();
    flow::Done result = f->shut();

    std::cout << "Done: " << result.width << "x" << result.height << std::endl;

    // Convert linear RGB to sRGB 8-bit
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
        std::string pngpath = "tmp/var/flow/" + name + ".head.png";
        FILE *fp = fopen(pngpath.c_str(), "wb");
        if (fp)
        {
            fwrite(png.data(), 1, png.size(), fp);
            fclose(fp);
            std::cout << "Saved: " << pngpath << " (" << png.size() << " bytes)" << std::endl;
        }
    }

    return 0;
}

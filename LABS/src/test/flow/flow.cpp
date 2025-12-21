// flow test - HEAD + TONE + TUNE pipeline
//
// Pipeline:
//   1. HEAD - GPU RAW decode (BLC, WB, demosaic, CST, warp) -> scene-linear
//   2. TONE - Luminance histogram matching -> corrected luminance
//   3. TUNE - Color NN with histogram EMD loss -> final output
//
// Usage: ./flow [input.ARW]

#include "flow.hpp"
#include "tone.hpp"
#include "tune.hpp"
#include "lute.hpp"
#include "exposure.hpp"
#include "drum.hpp"

#include <dawn/webgpu_cpp.h>

#include <fstream>
#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

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

static std::string get_stem(const std::string &path)
{
    size_t slash = path.rfind('/');
    size_t start = (slash == std::string::npos) ? 0 : slash + 1;
    std::string filename = path.substr(start);
    size_t dot = filename.rfind('.');
    if (dot != std::string::npos)
        filename = filename.substr(0, dot);
    return filename;
}

// sRGB to linear conversion
static float srgb_to_linear(uint8_t v)
{
    float f = v / 255.0f;
    if (f <= 0.04045f)
        return f / 12.92f;
    return std::pow((f + 0.055f) / 1.055f, 2.4f);
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

// Save float RGB as PNG
static void save_png(const std::string &path, const float *rgb, int width, int height)
{
    size_t pixels = static_cast<size_t>(width) * height;
    std::vector<uint8_t> rgb8(pixels * 3);

    for (size_t i = 0; i < pixels; i++)
    {
        rgb8[i * 3 + 0] = linear_to_srgb(rgb[i * 3 + 0]);
        rgb8[i * 3 + 1] = linear_to_srgb(rgb[i * 3 + 1]);
        rgb8[i * 3 + 2] = linear_to_srgb(rgb[i * 3 + 2]);
    }

    auto png = flow::swap(rgb8.data(), 0, width, height, flow::PNG);
    if (!png.empty())
    {
        FILE *fp = fopen(path.c_str(), "wb");
        if (fp)
        {
            fwrite(png.data(), 1, png.size(), fp);
            fclose(fp);
            std::cout << "  " << path << " (" << png.size() / 1024 << " KB)" << std::endl;
        }
    }
}

// JPEG reference data
struct Reference
{
    std::vector<float> rgb;    // Linear RGB for comparison
    std::vector<uint8_t> rgb8; // sRGB uint8 for training
    int width = 0;
    int height = 0;
};

// Decode JPEG to linear RGB
static Reference decode_jpeg(const uint8_t *data, size_t size)
{
    Reference ref;

    // Parse JPEG header for dimensions
    size_t pos = 0;
    while (pos + 4 < size)
    {
        if (data[pos] != 0xFF) { pos++; continue; }
        uint8_t marker = data[pos + 1];
        if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC)
        {
            if (pos + 9 < size)
            {
                ref.height = (data[pos + 5] << 8) | data[pos + 6];
                ref.width = (data[pos + 7] << 8) | data[pos + 8];
                break;
            }
        }
        if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7))
            pos += 2;
        else if (pos + 4 < size)
            pos += 2 + ((data[pos + 2] << 8) | data[pos + 3]);
        else
            break;
    }

    if (ref.width <= 0 || ref.height <= 0)
        return ref;

    // Decode JPEG
    ref.rgb8 = flow::swap(const_cast<uint8_t *>(data), size, 0, 0, flow::BIN);
    if (ref.rgb8.empty())
        return ref;

    // Convert to linear RGB
    size_t pixels = static_cast<size_t>(ref.width) * ref.height;
    ref.rgb.resize(pixels * 3);
    for (size_t i = 0; i < pixels * 3; i++)
        ref.rgb[i] = srgb_to_linear(ref.rgb8[i]);

    return ref;
}

// Bilinear downsample for float RGB
static std::vector<float> downsample(const float *src, int src_w, int src_h, int dst_w, int dst_h)
{
    std::vector<float> dst(dst_w * dst_h * 3);
    float scale_x = static_cast<float>(src_w) / dst_w;
    float scale_y = static_cast<float>(src_h) / dst_h;

    for (int y = 0; y < dst_h; y++)
    {
        for (int x = 0; x < dst_w; x++)
        {
            float sx = (x + 0.5f) * scale_x - 0.5f;
            float sy = (y + 0.5f) * scale_y - 0.5f;
            int x0 = std::max(0, std::min(src_w - 1, static_cast<int>(sx)));
            int x1 = std::max(0, std::min(src_w - 1, x0 + 1));
            int y0 = std::max(0, std::min(src_h - 1, static_cast<int>(sy)));
            int y1 = std::max(0, std::min(src_h - 1, y0 + 1));
            float fx = sx - std::floor(sx);
            float fy = sy - std::floor(sy);

            size_t i00 = (y0 * src_w + x0) * 3;
            size_t i10 = (y0 * src_w + x1) * 3;
            size_t i01 = (y1 * src_w + x0) * 3;
            size_t i11 = (y1 * src_w + x1) * 3;
            size_t di = (y * dst_w + x) * 3;

            for (int c = 0; c < 3; c++)
            {
                float v0 = src[i00 + c] * (1 - fx) + src[i10 + c] * fx;
                float v1 = src[i01 + c] * (1 - fx) + src[i11 + c] * fx;
                dst[di + c] = v0 * (1 - fy) + v1 * fy;
            }
        }
    }
    return dst;
}

// Compute diff against reference and save visualization
static float compute_diff(const std::string &path, const float *stage, int sw, int sh,
                          const Reference &ref, const std::string &stage_name)
{
    auto ds = downsample(stage, sw, sh, ref.width, ref.height);

    size_t pixels = static_cast<size_t>(ref.width) * ref.height;
    std::vector<float> diff_rgb(pixels * 3);

    int brighter = 0;
    double sum_error = 0.0;
    float max_error = 0.0f;

    for (size_t i = 0; i < pixels; i++)
    {
        float sr = ds[i * 3 + 0], sg = ds[i * 3 + 1], sb = ds[i * 3 + 2];
        float rr = ref.rgb[i * 3 + 0], rg = ref.rgb[i * 3 + 1], rb = ref.rgb[i * 3 + 2];

        float s_lum = 0.299f * sr + 0.587f * sg + 0.114f * sb;
        float r_lum = 0.299f * rr + 0.587f * rg + 0.114f * rb;
        float lum_diff = s_lum - r_lum;

        if (lum_diff > 0) brighter++;
        float err = std::abs(lum_diff);
        sum_error += err;
        if (err > max_error) max_error = err;

        // False color: red = stage brighter, blue = ref brighter
        float scale = 3.0f;
        float r = 0.5f + lum_diff * scale;
        float g = 0.5f - std::abs(lum_diff) * scale * 0.5f;
        float b = 0.5f - lum_diff * scale;

        diff_rgb[i * 3 + 0] = std::max(0.0f, std::min(1.0f, r));
        diff_rgb[i * 3 + 1] = std::max(0.0f, std::min(1.0f, g));
        diff_rgb[i * 3 + 2] = std::max(0.0f, std::min(1.0f, b));
    }

    float pct_brighter = 100.0f * brighter / pixels;
    float mean_error = static_cast<float>(sum_error / pixels);

    std::cout << "  [" << stage_name << "] brighter=" << pct_brighter << "% mean=" << mean_error
              << " max=" << max_error << std::endl;

    save_png(path, diff_rgb.data(), ref.width, ref.height);

    return pct_brighter;
}

int main(int argc, char **argv)
{
    const char *input = "src/test/flow/DSC00144.ARW";
    if (argc > 1)
        input = argv[1];

    std::cout << "=== FLOW Pipeline Test (HEAD + TONE + TUNE) ===" << std::endl;
    std::cout << "Loading: " << input << std::endl;

    auto raw = read_file(input);
    if (raw.empty())
    {
        std::cerr << "Failed to read: " << input << std::endl;
        return 1;
    }

    std::cout << "Size: " << raw.size() / 1024 << " KB" << std::endl;

    std::string name = get_stem(input);
    auto bits = reinterpret_cast<uint16_t *>(raw.data());
    auto f = flow::make(name, bits, raw.size());

    auto &root = f->info().root();
    int w = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int h = static_cast<int>(root.leaf(flow::HEIGHT).dial());

    std::cout << "Dimensions: " << w << "x" << h << std::endl;

    // Decode reference JPEG
    Reference ref;
    if (f->view() && f->viewSize() > 0)
    {
        ref = decode_jpeg(f->view(), f->viewSize());
        std::cout << "Reference: " << ref.width << "x" << ref.height << std::endl;

        std::string jpgpath = "tmp/var/flow/" + name + ".0.ref.jpg";
        FILE *fp = fopen(jpgpath.c_str(), "wb");
        if (fp)
        {
            fwrite(f->view(), 1, f->viewSize(), fp);
            fclose(fp);
            std::cout << "  " << jpgpath << " (reference)" << std::endl;
        }
    }

    // Save metadata
    std::string jsonpath = "tmp/var/flow/" + name + ".flow.json";
    std::ofstream out(jsonpath);
    out << f->info().json() << std::endl;
    out.close();

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

    lute::CameraLut profile;

    std::cout << "WebGPU ready\n" << std::endl;

    std::string prefix = "tmp/var/flow/" + name;

    // =========================================================================
    // Stage 1: HEAD (GPU RAW processing)
    // =========================================================================
    std::cout << "=== Stage 1: HEAD ===" << std::endl;
    std::cout << "  BLC -> WB -> Demosaic -> CST -> Warp" << std::endl;

    flow::Task task = f->head(&device);
    task.post();
    flow::Done result = task.done();

    std::cout << "  Output: " << result.width << "x" << result.height << std::endl;

    std::vector<float> head_rgb = result.rgb;
    int sw = result.width;
    int sh = result.height;

    save_png(prefix + ".1.head.png", head_rgb.data(), sw, sh);
    if (!ref.rgb.empty())
        compute_diff(prefix + ".1.head.diff.png", head_rgb.data(), sw, sh, ref, "HEAD");

    // =========================================================================
    // Stage 2: EXPOSURE (Global Brightness Correction)
    // =========================================================================
    std::cout << "\n=== Stage 2: EXPOSURE ===" << std::endl;
    std::cout << "  Learning global exposure correction..." << std::endl;
    
    std::vector<float> exposure_rgb = head_rgb; // Copy for in-place modification
    if (!ref.rgb.empty())
    {
        exposure::Params exp_params = exposure::learn(exposure_rgb.data(), sw, sh, ref.rgb8.data(), ref.width, ref.height);
        std::cout << "  Exposure correction factor: " << exp_params.correction << std::endl;
        exposure::apply(exposure_rgb.data(), sw, sh, exp_params);
    }
    save_png(prefix + ".2.exposure.png", exposure_rgb.data(), sw, sh);
    if (!ref.rgb.empty())
        compute_diff(prefix + ".2.exposure.diff.png", exposure_rgb.data(), sw, sh, ref, "EXPOSURE");

    // =========================================================================
    // Stage 3: DRUM (Local Tone Mapping)
    // =========================================================================
    std::cout << "\n=== Stage 3: DRUM ===" << std::endl;
    std::cout << "  Applying local tone mapping (CLAHE)..." << std::endl;

    std::vector<float> drum_rgb = exposure_rgb; // Copy for in-place modification
    drum::Params drum_params;
    if (root.test("maker") && root.next("maker").test("d-range-optimizer")) {
        std::string dro_str = root.next("maker").leaf("d-range-optimizer").text();
        drum_params = drum::parse(dro_str);
        std::cout << "  DRO setting: '" << dro_str << "', clip_limit: " << drum_params.clip_limit << std::endl;
    } else {
        std::cout << "  DRO setting not found, using default." << std::endl;
    }
    drum::apply(drum_rgb.data(), sw, sh, drum_params);

    save_png(prefix + ".3.drum.png", drum_rgb.data(), sw, sh);
    if (!ref.rgb.empty())
        compute_diff(prefix + ".3.drum.diff.png", drum_rgb.data(), sw, sh, ref, "DRUM");

    // =========================================================================
    // Stage 4: TONE (Global Tone Curve)
    // =========================================================================
    std::cout << "\n=== Stage 4: TONE ===" << std::endl;
    std::cout << "  Learning global luminance curve..." << std::endl;

    std::vector<float> tone_rgb = drum_rgb;  // Copy for in-place modification
    if (!ref.rgb8.empty())
    {
        auto drum_ds = downsample(drum_rgb.data(), sw, sh, ref.width, ref.height);
        tone::learn(drum_ds.data(), ref.width, ref.height,
                    ref.rgb8.data(), ref.width, ref.height, profile);
        tone::apply(tone_rgb.data(), tone_rgb.data(), sw, sh, profile);
    }
    save_png(prefix + ".4.tone.png", tone_rgb.data(), sw, sh);
    if (!ref.rgb.empty())
        compute_diff(prefix + ".4.tone.diff.png", tone_rgb.data(), sw, sh, ref, "TONE");

    // =========================================================================
    // Stage 5: TUNE (Color Grading)
    // =========================================================================
    std::cout << "\n=== Stage 5: TUNE ===" << std::endl;
    std::cout << "  Learning 3D color lookup table..." << std::endl;

    std::vector<float> tune_rgb = tone_rgb; // Use buffer from previous stage
    if (!ref.rgb8.empty())
    {
        auto tone_ds = downsample(tone_rgb.data(), sw, sh, ref.width, ref.height);
        tune::learn(tone_ds.data(), ref.width, ref.height,
                    ref.rgb8.data(), ref.width, ref.height, profile);
        tune::apply(tune_rgb.data(), tune_rgb.data(), sw, sh, profile);
    }
    save_png(prefix + ".5.tune.png", tune_rgb.data(), sw, sh);
    if (!ref.rgb.empty())
        compute_diff(prefix + ".5.tune.diff.png", tune_rgb.data(), sw, sh, ref, "TUNE");

    // =========================================================================
    // Summary
    // =========================================================================
    std::cout << "\n=== Pipeline ===" << std::endl;
    std::cout << "  0.ref.jpg      - Camera JPEG (target)" << std::endl;
    std::cout << "  1.head.png     - scene-linear (GPU RAW decode)" << std::endl;
    std::cout << "  2.exposure.png - global exposure matched" << std::endl;
    std::cout << "  3.drum.png     - local tone mapped (CLAHE)" << std::endl;
    std::cout << "  4.tone.png     - global tone curve applied" << std::endl;
    std::cout << "  5.tune.png     - 3D color LUT applied (final)" << std::endl;
    std::cout << "\n  *.diff.png  - Error vs reference" << std::endl;

    std::cout << "\n=== Done ===" << std::endl;

    return 0;
}

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
#include "loud.hpp"
#include "drum.hpp"

#include <dawn/webgpu_cpp.h>

#include <fstream>
#include <iostream>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

// Dawn helper functions (defined in wgpu.cpp)
namespace dawn
{
    wgpu::Instance instance();
    wgpu::Adapter adapter(wgpu::Instance instance);
    wgpu::Device device(wgpu::Adapter adapter);
}

namespace {

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

// Helper to calculate luminance from linear RGB
static float linear_rgb_to_luminance(float r, float g, float b)
{
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
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


// Histogram and CDF helpers for tone matching
static std::vector<int> get_luminance_histogram(const std::vector<float>& lum_values, int bins)
{
    std::vector<int> histogram(bins, 0);
    for (float lum : lum_values)
    {
        int bin = std::min(bins - 1, std::max(0, static_cast<int>(lum * (bins - 1) + 0.5f)));
        histogram[bin]++;
    }
    return histogram;
}

static std::vector<float> get_cdf(const std::vector<int>& histogram, size_t num_pixels)
{
    std::vector<float> cdf(histogram.size());
    long cumulative_sum = 0;
    for (size_t i = 0; i < histogram.size(); ++i)
    {
        cumulative_sum += histogram[i];
        cdf[i] = static_cast<float>(cumulative_sum) / num_pixels;
    }
    return cdf;
}

// Maps a luminance value from a source CDF to a target CDF (histogram matching)
static float map_luminance_to_target_cdf(float lum_val, int bins, const std::vector<float>& source_cdf, const std::vector<float>& target_cdf)
{
    int bin = std::min(bins - 1, std::max(0, static_cast<int>(lum_val * (bins - 1) + 0.5f)));
    float src_cdf_val = source_cdf[bin];

    // Find the corresponding luminance value in the target CDF
    // This is effectively target_cdf_inverse(src_cdf_val)
    for (int i = 0; i < bins; ++i)
    {
        if (target_cdf[i] >= src_cdf_val)
        {
            // Simple linear interpolation between bins to get a smoother map
            if (i == 0) return 0.0f;
            float prev_cdf = target_cdf[i-1];
            float current_cdf = target_cdf[i];
            float t = (src_cdf_val - prev_cdf) / (current_cdf - prev_cdf);
            return ((float)(i-1) + t) / (bins - 1);
        }
    }
    return 1.0f; // Should not happen for well-formed CDFs, but for safety.
}

// Helper to create a tone-matched reference RGB buffer
// This function assumes original_ref_rgb8_data is a uint8 sRGB image.
// It will generate a float RGB buffer where the luminance histogram matches that of pipeline_tone_rgb.
static std::vector<float> create_tone_matched_reference(
    const float* pipeline_tone_rgb, int pipeline_w, int pipeline_h, // Output of TONE stage
    const uint8_t* original_ref_rgb8_data, int ref_w, int ref_h)      // Original reference JPEG (uint8 sRGB)
{
    // 0. Ensure original_ref_rgb8_data is converted to linear float first
    std::vector<float> original_ref_rgb_linear(ref_w * ref_h * 3);
    for (size_t i = 0; i < (size_t)ref_w * ref_h; ++i) {
        original_ref_rgb_linear[i*3+0] = srgb_to_linear(original_ref_rgb8_data[i*3+0]);
        original_ref_rgb_linear[i*3+1] = srgb_to_linear(original_ref_rgb8_data[i*3+1]);
        original_ref_rgb_linear[i*3+2] = srgb_to_linear(original_ref_rgb8_data[i*3+2]);
    }

    // 1. Extract luminance values for pipeline_tone_rgb (downsampled to ref_w, ref_h)
    // The input pipeline_tone_rgb is already downsampled if it's from `tone_ds`
    std::vector<float> tone_lum_values(ref_w * ref_h);
    const float* current_tone_rgb_ds = downsample(pipeline_tone_rgb, pipeline_w, pipeline_h, ref_w, ref_h).data();
    for (size_t i = 0; i < (size_t)ref_w * ref_h; ++i) {
        tone_lum_values[i] = linear_rgb_to_luminance(
            current_tone_rgb_ds[i*3+0], current_tone_rgb_ds[i*3+1], current_tone_rgb_ds[i*3+2]);
    }

    // 2. Extract luminance values for original_ref_rgb_linear
    std::vector<float> ref_lum_values(ref_w * ref_h);
    for (size_t i = 0; i < (size_t)ref_w * ref_h; ++i) {
        ref_lum_values[i] = linear_rgb_to_luminance(
            original_ref_rgb_linear[i*3+0], original_ref_rgb_linear[i*3+1], original_ref_rgb_linear[i*3+2]);
    }

    // 3. Compute CDF for tone_lum_values (target CDF)
    int bins = 256; // Use 256 bins for luminance histograms
    std::vector<int> tone_hist = get_luminance_histogram(tone_lum_values, bins);
    std::vector<float> tone_cdf = get_cdf(tone_hist, tone_lum_values.size());

    // 4. Compute CDF for ref_lum_values (source CDF)
    std::vector<int> ref_hist = get_luminance_histogram(ref_lum_values, bins);
    std::vector<float> ref_cdf = get_cdf(ref_hist, ref_lum_values.size());
    
    // 5. Create the tone-matched reference RGB buffer
    std::vector<float> tone_matched_ref_rgb(ref_w * ref_h * 3);
    for (size_t i = 0; i < (size_t)ref_w * ref_h; ++i) {
        float original_ref_lum = ref_lum_values[i];
        
        // Map original_ref_lum to new luminance using histogram matching
        float new_ref_lum = map_luminance_to_target_cdf(original_ref_lum, bins, ref_cdf, tone_cdf);

        // Calculate ratio to preserve color
        float ratio = (original_ref_lum > 1e-6f) ? (new_ref_lum / original_ref_lum) : 1.0f;

        // Apply ratio to original color channels
        tone_matched_ref_rgb[i*3+0] = original_ref_rgb_linear[i*3+0] * ratio;
        tone_matched_ref_rgb[i*3+1] = original_ref_rgb_linear[i*3+1] * ratio;
        tone_matched_ref_rgb[i*3+2] = original_ref_rgb_linear[i*3+2] * ratio;
    }

    return tone_matched_ref_rgb;
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

} // namespace


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
    // Stage 2: LOUD (Global Loudness Correction)
    // =========================================================================
    std::cout << "\n=== Stage 2: LOUD ===" << std::endl;
    std::cout << "  Learning global loudness correction..." << std::endl;
    
    std::vector<float> loud_rgb = head_rgb; // Copy for in-place modification
    if (!ref.rgb.empty())
    {
        loud::Params loud_params = loud::learn(loud_rgb.data(), sw, sh, ref.rgb8.data(), ref.width, ref.height);
        std::cout << "  Loudness correction factor: " << loud_params.correction << std::endl;
        loud::apply(loud_rgb.data(), sw, sh, loud_params);
    }
    save_png(prefix + ".2.loud.png", loud_rgb.data(), sw, sh);
    if (!ref.rgb.empty())
        compute_diff(prefix + ".2.loud.diff.png", loud_rgb.data(), sw, sh, ref, "LOUD");

    // =========================================================================
    // Stage 3: DRUM (Local Tone Mapping)
    // =========================================================================
    std::cout << "\n=== Stage 3: DRUM ===" << std::endl;
    std::cout << "  Applying local tone mapping (CLAHE)..." << std::endl;

    std::vector<float> drum_rgb = loud_rgb; // Input from LOUD stage
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

    std::vector<float> tone_rgb = drum_rgb;  // Input from DRUM stage
    if (!ref.rgb8.empty())
    {
        auto drum_ds = downsample(drum_rgb.data(), sw, sh, ref.width, ref.height); // Downsample DRUM output
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

    std::vector<float> tune_rgb = tone_rgb; // Input from TONE stage
    if (!ref.rgb8.empty())
    {
        // Our pipeline's tone-corrected output (downsampled for learning)
        auto tone_ds = downsample(tone_rgb.data(), sw, sh, ref.width, ref.height);
        
        // Create a tone-matched reference from the original JPEG for orthogonal color learning
        std::vector<float> tone_matched_ref_ds = create_tone_matched_reference(
            tone_rgb.data(), sw, sh, ref.rgb8.data(), ref.width, ref.height);

        tune::learn(tone_ds.data(), ref.width, ref.height,
                    tone_matched_ref_ds.data(), ref.width, ref.height, profile); // Use tone_matched_ref_ds as target
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
    std::cout << "  2.loud.png     - global loudness matched" << std::endl;
    std::cout << "  3.drum.png     - local tone mapped (CLAHE)" << std::endl;
    std::cout << "  4.tone.png     - global tone curve applied" << std::endl;
    std::cout << "  5.tune.png     - 3D color LUT applied (final)" << std::endl;
    std::cout << "\n  *.diff.png  - Error vs reference" << std::endl;

    std::cout << "\n=== Done ===" << std::endl;

    return 0;
}

// test_dawn.cpp - DAWN shader tests
//
// Compares WGSL compute shader output against theory.h reference implementations.

#include "pipe.hpp"
#include "../theory.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

// Shader directory
const char* SHADER_DIR = "src/main/dawn/";

// Load shader from file
std::string load_shader(const char* name)
{
    std::string path = std::string(SHADER_DIR) + name + ".wgsl";
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Failed to load shader: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Compare theory::Image to float vector
double max_diff(const theory::Image& a, const std::vector<float>& b)
{
    if (a.data.size() != b.size()) return 999.0;
    double max_d = 0;
    for (size_t i = 0; i < a.data.size(); i++)
    {
        double d = std::abs(a.data[i] - b[i]);
        if (d > max_d) max_d = d;
    }
    return max_d;
}

// Create test input image (gradient pattern)
theory::Image create_test_image(int w, int h)
{
    theory::Image img(w, h);
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            float u = float(x) / (w - 1);
            float v = float(y) / (h - 1);
            img.at(y, x, 0) = u;         // B
            img.at(y, x, 1) = v;         // G
            img.at(y, x, 2) = (u + v) / 2; // R
        }
    }
    return img;
}

} // anon

//------------------------------------------------------------------------------
// Test: Exposure
//------------------------------------------------------------------------------
bool test_dawn_exposure()
{
    const int W = 64, H = 64;
    const float dial = 0.7f;

    // Create test input
    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::exposure(input, expected, dial);

    // Run DAWN
    dawn_pipe::Pipe pipe;
    if (!pipe.init())
    {
        std::cerr << "exposure: DAWN init failed\n";
        return false;
    }

    std::string shader = load_shader("exposure");
    if (shader.empty() || !pipe.load(shader.c_str()))
    {
        std::cerr << "exposure: shader load failed\n";
        return false;
    }

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width;
        uint32_t height;
        float dial;
        float _pad;
    } uniforms = {W, H, dial, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));

    if (!pipe.dispatch())
    {
        std::cerr << "exposure: dispatch failed\n";
        return false;
    }

    std::vector<float> result = pipe.get_output();

    double diff = max_diff(expected, result);
    bool pass = diff < 1e-5;

    std::cout << std::left << std::setw(18) << "exposure" << " dawn: ";
    if (pass)
        std::cout << "PASS\n";
    else
        std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: White Balance
//------------------------------------------------------------------------------
bool test_dawn_white_balance()
{
    const int W = 64, H = 64;
    const float temp_dial = 0.3f;
    const float tint_dial = 0.6f;

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::white_balance(input, expected, temp_dial, tint_dial);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("white_balance");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float temp_dial, tint_dial;
    } uniforms = {W, H, temp_dial, tint_dial};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 1e-4;

    std::cout << std::left << std::setw(18) << "white_balance" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Split Tone
//------------------------------------------------------------------------------
bool test_dawn_split_tone()
{
    const int W = 64, H = 64;

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::split_tone(input, expected, 0.6f, 0.4f, 0.3f, 0.7f);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("split_tone");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float shadow_temp, shadow_tint;
        float highlight_temp, highlight_tint;
        float _pad0, _pad1;
    } uniforms = {W, H, 0.6f, 0.4f, 0.3f, 0.7f, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 1e-5;

    std::cout << std::left << std::setw(18) << "split_tone" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Baseline
//------------------------------------------------------------------------------
bool test_dawn_baseline()
{
    const int W = 64, H = 64;
    const float ev = 0.7f;
    const float clip = 0.95f;

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::baseline(input, expected, ev, clip);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("baseline");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float ev, clip_threshold;
    } uniforms = {W, H, ev, clip};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 1e-5;

    std::cout << std::left << std::setw(18) << "baseline" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Sigmoid
//------------------------------------------------------------------------------
bool test_dawn_sigmoid()
{
    const int W = 64, H = 64;
    const float contrast = 1.5f;
    const float skewness = 0.0f;
    const float white_target = 1.0f;
    const float black_target = 0.0f;

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::sigmoid(input, expected, contrast, skewness, white_target, black_target);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("sigmoid");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float contrast, skewness, white_target, black_target;
        float _pad0, _pad1;
    } uniforms = {W, H, contrast, skewness, white_target, black_target, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 0.02;  // Sigmoid has complex math, allow higher tolerance

    std::cout << std::left << std::setw(18) << "sigmoid" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Tone Map
//------------------------------------------------------------------------------
bool test_dawn_tone_map()
{
    const int W = 64, H = 64;

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::tone_map(input, expected, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("tone_map");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float contrast, highlights, shadows, toe_pivot, shoulder_pivot;
        float white_point, black_point;
        float _pad0, _pad1, _pad2;
    } uniforms = {W, H, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 0.01;

    std::cout << std::left << std::setw(18) << "tone_map" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Color Matrix
//------------------------------------------------------------------------------
bool test_dawn_color_matrix()
{
    const int W = 64, H = 64;

    // Identity-ish matrix with slight tint
    float matrix[9] = {1.1f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 0.9f};

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::color_matrix(input, expected, matrix);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("color_matrix");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float _pad0, _pad1;
        float m00, m01, m02, _p0;
        float m10, m11, m12, _p1;
        float m20, m21, m22, _p2;
    } uniforms = {W, H, 0, 0,
                  matrix[0], matrix[1], matrix[2], 0,
                  matrix[3], matrix[4], matrix[5], 0,
                  matrix[6], matrix[7], matrix[8], 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 1e-5;

    std::cout << std::left << std::setw(18) << "color_matrix" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Global Color
//------------------------------------------------------------------------------
bool test_dawn_global_color()
{
    const int W = 64, H = 64;

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::global_color(input, expected, 0.6f, 0.6f, 0.5f);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("global_color");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float vibrance, saturation, density;
        float _pad0, _pad1, _pad2;
    } uniforms = {W, H, 0.6f, 0.6f, 0.5f, 0, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 0.05;  // Lab conversion has precision differences

    std::cout << std::left << std::setw(18) << "global_color" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Detail
//------------------------------------------------------------------------------
bool test_dawn_detail()
{
    const int W = 64, H = 64;

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::detail(input, expected, 0.5f, 0.0f, 0.0f, 0.0f);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("detail");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float sharpen_amount, sharpen_radius, denoise_amount, denoise_radius;
        float _pad0, _pad1;
    } uniforms = {W, H, 0.5f, 0.0f, 0.0f, 0.0f, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 0.01;  // Edge handling differs slightly

    std::cout << std::left << std::setw(18) << "detail" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Selective Color
//------------------------------------------------------------------------------
bool test_dawn_selective_color()
{
    const int W = 64, H = 64;

    // Neutral dials (all 0.5)
    float hue[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float sat[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    float lum[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    // Boost red hue slightly
    sat[0] = 0.7f;

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::selective_color(input, expected, hue, sat, lum);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("selective_color");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float _pad0, _pad1;
        float hue0, hue1, hue2, hue3, hue4, hue5, hue6, hue7;
        float sat0, sat1, sat2, sat3, sat4, sat5, sat6, sat7;
        float lum0, lum1, lum2, lum3, lum4, lum5, lum6, lum7;
    } uniforms = {W, H, 0, 0,
                  hue[0], hue[1], hue[2], hue[3], hue[4], hue[5], hue[6], hue[7],
                  sat[0], sat[1], sat[2], sat[3], sat[4], sat[5], sat[6], sat[7],
                  lum[0], lum[1], lum[2], lum[3], lum[4], lum[5], lum[6], lum[7]};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 0.02;  // HLS conversion has precision differences

    std::cout << std::left << std::setw(18) << "selective_color" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Local Tone
//------------------------------------------------------------------------------
bool test_dawn_local_tone()
{
    const int W = 64, H = 64;
    const float strength = 0.5f;
    const float delta = 0.1f;
    const float window_scale = 5.0f / 64.0f;  // ~5 pixel window

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::local_tone(input, expected, strength, delta, window_scale);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("local_tone");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float strength, delta, window_scale;
        float _pad0, _pad1, _pad2;
    } uniforms = {W, H, strength, delta, window_scale, 0, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));

    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 0.05;  // Different window sizes may cause larger differences

    std::cout << std::left << std::setw(18) << "local_tone" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Geometric
//------------------------------------------------------------------------------
bool test_dawn_geometric()
{
    const int W = 64, H = 64;

    // Neutral settings (no transform)
    const float crop_top = 0.0f;
    const float crop_right = 0.0f;
    const float crop_bottom = 0.0f;
    const float crop_left = 0.0f;
    const float zoom_dial = 0.5f;  // 1x zoom
    const float tilt_dial = 0.5f;  // 0 rotation

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::geometric(input, expected, crop_top, crop_right, crop_bottom, crop_left, zoom_dial, tilt_dial);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("geometric");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float crop_top, crop_right, crop_bottom, crop_left;
        float zoom_dial, tilt_dial;
    } uniforms = {W, H, crop_top, crop_right, crop_bottom, crop_left, zoom_dial, tilt_dial};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));

    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 0.01;  // Bilinear interpolation differences

    std::cout << std::left << std::setw(18) << "geometric" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: HSV LUT
//------------------------------------------------------------------------------
bool test_dawn_hsv_lut()
{
    const int W = 64, H = 64;
    const int H_BINS = 12, S_BINS = 4;

    // Create identity LUT (all zeros = no change)
    float lut[H_BINS * S_BINS * 3];
    std::memset(lut, 0, sizeof(lut));

    // Add small adjustments to test
    for (int h = 0; h < H_BINS; h++)
    {
        for (int s = 0; s < S_BINS; s++)
        {
            int idx = (h * S_BINS + s) * 3;
            lut[idx + 0] = 5.0f;   // Small hue shift
            lut[idx + 1] = 0.1f;   // Small saturation boost
            lut[idx + 2] = 0.0f;   // No value change
        }
    }

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::hsv_lut(input, expected, lut, H_BINS, S_BINS);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("hsv_lut");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        uint32_t h_bins, s_bins;
    } uniforms = {W, H, H_BINS, S_BINS};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    pipe.set_data(lut, sizeof(lut));

    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 0.02;  // HSV conversion has precision differences

    std::cout << std::left << std::setw(18) << "hsv_lut" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: LUT Curve
//------------------------------------------------------------------------------
bool test_dawn_lut_curve()
{
    const int W = 64, H = 64;
    const int LUT_SIZE = 17;  // Small LUT to expand

    // Create simple contrast curve LUT
    float small_lut[LUT_SIZE * 3];
    for (int c = 0; c < 3; c++)
    {
        for (int i = 0; i < LUT_SIZE; i++)
        {
            float x = float(i) / (LUT_SIZE - 1);
            // S-curve
            small_lut[c * LUT_SIZE + i] = 0.5f + 0.5f * std::tanh(2.0f * (x - 0.5f));
        }
    }

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::lut_curve(input, expected, small_lut, LUT_SIZE);

    // Expand LUT to 768 elements for shader
    float expanded_lut[768];
    float step = 1.0f / (LUT_SIZE - 1);
    for (int c = 0; c < 3; c++)
    {
        for (int i = 0; i < 256; i++)
        {
            float val = i / 255.0f;
            float pos = val / step;
            int idx0 = std::min(int(pos), LUT_SIZE - 1);
            int idx1 = std::min(idx0 + 1, LUT_SIZE - 1);
            float frac = pos - idx0;
            expanded_lut[c * 256 + i] = small_lut[c * LUT_SIZE + idx0] +
                frac * (small_lut[c * LUT_SIZE + idx1] - small_lut[c * LUT_SIZE + idx0]);
        }
    }

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("lut_curve");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float _pad0, _pad1;
    } uniforms = {W, H, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    pipe.set_data(expanded_lut, sizeof(expanded_lut));

    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 1e-4;

    std::cout << std::left << std::setw(18) << "lut_curve" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Base Curve
//------------------------------------------------------------------------------
bool test_dawn_base_curve()
{
    const int W = 64, H = 64;

    // Create S-curve for contrast (768 elements: 256 per channel)
    float curve[768];
    for (int c = 0; c < 3; c++)
    {
        for (int i = 0; i < 256; i++)
        {
            float x = float(i) / 255.0f;
            // Mild S-curve
            float y = 0.5f + 0.5f * std::tanh(2.0f * (x - 0.5f));
            curve[c * 256 + i] = y;
        }
    }

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::base_curve(input, expected, curve);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("base_curve");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float _pad0, _pad1;
    } uniforms = {W, H, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    pipe.set_data(curve, sizeof(curve));

    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 1e-4;

    std::cout << std::left << std::setw(18) << "base_curve" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Poly Color
//------------------------------------------------------------------------------
bool test_dawn_poly_color()
{
    const int W = 64, H = 64;

    // Identity-ish polynomial: out = input (coeff[1]=1 for each channel)
    // But add slight cross-channel mixing for realistic test
    float coeffs[30] = {
        // Red: out_r = 0.1 + 0.9*r + 0.05*g
        0.1f, 0.9f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        // Green: out_g = 0.05 + 0.95*g
        0.05f, 0.0f, 0.95f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        // Blue: out_b = 0.0 + 0.9*b + 0.1*r²
        0.0f, 0.0f, 0.0f, 0.9f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };

    theory::Image input = create_test_image(W, H);
    theory::Image expected(W, H);
    theory::poly_color(input, expected, coeffs);

    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("poly_color");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_input(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float _pad0, _pad1;
        // Red coefficients (10) + 2 padding
        float cr0, cr1, cr2, cr3, cr4, cr5, cr6, cr7, cr8, cr9;
        float _padr0, _padr1;
        // Green coefficients (10) + 2 padding
        float cg0, cg1, cg2, cg3, cg4, cg5, cg6, cg7, cg8, cg9;
        float _padg0, _padg1;
        // Blue coefficients (10) + 2 padding
        float cb0, cb1, cb2, cb3, cb4, cb5, cb6, cb7, cb8, cb9;
        float _padb0, _padb1;
    } uniforms = {W, H, 0, 0,
                  coeffs[0], coeffs[1], coeffs[2], coeffs[3], coeffs[4],
                  coeffs[5], coeffs[6], coeffs[7], coeffs[8], coeffs[9], 0, 0,
                  coeffs[10], coeffs[11], coeffs[12], coeffs[13], coeffs[14],
                  coeffs[15], coeffs[16], coeffs[17], coeffs[18], coeffs[19], 0, 0,
                  coeffs[20], coeffs[21], coeffs[22], coeffs[23], coeffs[24],
                  coeffs[25], coeffs[26], coeffs[27], coeffs[28], coeffs[29], 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff(expected, result);
    bool pass = diff < 1e-4;

    std::cout << std::left << std::setw(18) << "poly_color" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------
int main()
{
    std::cout << "VIBE DAWN Shader Tests\n";
    std::cout << "======================\n\n";

    int passed = 0, failed = 0;

    if (test_dawn_exposure()) passed++; else failed++;
    if (test_dawn_white_balance()) passed++; else failed++;
    if (test_dawn_split_tone()) passed++; else failed++;
    if (test_dawn_baseline()) passed++; else failed++;
    if (test_dawn_sigmoid()) passed++; else failed++;
    if (test_dawn_tone_map()) passed++; else failed++;
    if (test_dawn_color_matrix()) passed++; else failed++;
    if (test_dawn_global_color()) passed++; else failed++;
    if (test_dawn_detail()) passed++; else failed++;
    if (test_dawn_selective_color()) passed++; else failed++;
    if (test_dawn_local_tone()) passed++; else failed++;
    if (test_dawn_geometric()) passed++; else failed++;
    if (test_dawn_hsv_lut()) passed++; else failed++;
    if (test_dawn_lut_curve()) passed++; else failed++;
    if (test_dawn_base_curve()) passed++; else failed++;
    if (test_dawn_poly_color()) passed++; else failed++;

    std::cout << "\n";
    std::cout << "Passed: " << passed << "/" << (passed + failed) << "\n";

    return failed > 0 ? 1 : 0;
}

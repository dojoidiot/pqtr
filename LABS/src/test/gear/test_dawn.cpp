// test_dawn.cpp - RAWS DAWN shader tests
//
// Compares WGSL compute shader output against theory.h reference implementations.

#include "pipe.hpp"
#include "theory.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstring>

namespace {

const char* SHADER_DIR = "src/main/dawn/";

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

// Compare Bayer images (single channel)
double max_diff_bayer(const theory::BayerImage& a, const std::vector<float>& b)
{
    if (a.data.size() != b.size()) return 999.0;
    double max_d = 0;
    for (size_t i = 0; i < a.data.size(); i++) {
        double d = std::abs(a.data[i] - b[i]);
        if (d > max_d) max_d = d;
    }
    return max_d;
}

// Compare RGB images (3 channels)
double max_diff_rgb(const theory::RGBImage& a, const std::vector<float>& b)
{
    if (a.data.size() != b.size()) return 999.0;
    double max_d = 0;
    for (size_t i = 0; i < a.data.size(); i++) {
        double d = std::abs(a.data[i] - b[i]);
        if (d > max_d) max_d = d;
    }
    return max_d;
}

// Create test Bayer pattern (gradient with pattern structure)
std::vector<uint16_t> create_test_bayer_u16(int w, int h, uint16_t black, uint16_t white)
{
    std::vector<uint16_t> data(w * h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float u = float(x) / (w - 1);
            float v = float(y) / (h - 1);
            float val = (u + v) * 0.5f;
            data[y * w + x] = uint16_t(black + val * (white - black));
        }
    }
    return data;
}

// Create test Bayer f32 with color variation
theory::BayerImage create_test_bayer_f32(int w, int h, int pattern)
{
    theory::BayerImage img(w, h);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float u = float(x) / (w - 1);
            float v = float(y) / (h - 1);
            int px = x % 2;
            int py = y % 2;
            int pos = py * 2 + px;

            // Create different values for R/G/B positions
            float val;
            if (pattern == 0) {  // RGGB
                if (pos == 0) val = 0.3f + 0.4f * u;      // R
                else if (pos == 3) val = 0.2f + 0.4f * v; // B
                else val = 0.4f + 0.2f * (u + v);         // G
            } else if (pattern == 1) {  // GRBG
                if (pos == 1) val = 0.3f + 0.4f * u;      // R
                else if (pos == 2) val = 0.2f + 0.4f * v; // B
                else val = 0.4f + 0.2f * (u + v);         // G
            } else if (pattern == 2) {  // BGGR
                if (pos == 3) val = 0.3f + 0.4f * u;      // R
                else if (pos == 0) val = 0.2f + 0.4f * v; // B
                else val = 0.4f + 0.2f * (u + v);         // G
            } else {  // GBRG
                if (pos == 2) val = 0.3f + 0.4f * u;      // R
                else if (pos == 1) val = 0.2f + 0.4f * v; // B
                else val = 0.4f + 0.2f * (u + v);         // G
            }
            img.at(y, x) = val;
        }
    }
    return img;
}

// Pack u16 array into u32 pairs for GPU
std::vector<uint32_t> pack_u16_to_u32(const std::vector<uint16_t>& data)
{
    size_t packed_size = (data.size() + 1) / 2;
    std::vector<uint32_t> packed(packed_size, 0);
    for (size_t i = 0; i < data.size(); i++) {
        if (i % 2 == 0) {
            packed[i / 2] = data[i];
        } else {
            packed[i / 2] |= (uint32_t(data[i]) << 16);
        }
    }
    return packed;
}

} // anon

//------------------------------------------------------------------------------
// Test: BLC Bayer (u16 -> f32)
//------------------------------------------------------------------------------
bool test_dawn_blc_bayer()
{
    const int W = 64, H = 64;
    const float BLACK = 512.0f;
    const float WHITE = 16383.0f;

    // Create test input
    auto input_u16 = create_test_bayer_u16(W, H, uint16_t(BLACK), uint16_t(WHITE));

    // Reference
    theory::BayerImage expected(W, H);
    theory::blc_bayer(input_u16, expected, BLACK, WHITE);

    // Pack for GPU
    auto input_packed = pack_u16_to_u32(input_u16);

    // Run DAWN
    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("blc_bayer");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_output_channels(1);
    pipe.set_input_raw(input_packed.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float black_level, white_level;
    } uniforms = {W, H, BLACK, WHITE};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff_bayer(expected, result);
    bool pass = diff < 1e-4;

    std::cout << std::left << std::setw(18) << "blc_bayer" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(4) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Prepare Bayer (BLC + WB combined)
//------------------------------------------------------------------------------
bool test_dawn_prepare_bayer()
{
    const int W = 64, H = 64;
    const float BLACK = 512.0f;
    const float WHITE = 16383.0f;
    const float WB_R = 2.0f;
    const float WB_B = 1.5f;
    const int PATTERN = 0;  // RGGB

    // Create test input
    auto input_u16 = create_test_bayer_u16(W, H, uint16_t(BLACK), uint16_t(WHITE));

    // Reference
    theory::BayerImage expected(W, H);
    theory::prepare_bayer(input_u16, expected, BLACK, WHITE, WB_R, WB_B, PATTERN);

    // Pack for GPU
    auto input_packed = pack_u16_to_u32(input_u16);

    // Run DAWN
    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("prepare_bayer");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_output_channels(1);  // Single-channel output
    pipe.set_input_raw(input_packed.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float black_level, white_level;
        float wb_r, wb_b;
        uint32_t pattern;
        float _pad;
    } uniforms = {W, H, BLACK, WHITE, WB_R, WB_B, PATTERN, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff_bayer(expected, result);
    bool pass = diff < 1e-4;

    std::cout << std::left << std::setw(18) << "prepare_bayer" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(4) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: WB Bayer (f32 -> f32)
//------------------------------------------------------------------------------
bool test_dawn_wb_bayer()
{
    const int W = 64, H = 64;
    const float WB_R = 2.0f;
    const float WB_B = 1.5f;
    const int PATTERN = 0;  // RGGB

    // Create test input
    theory::BayerImage input = create_test_bayer_f32(W, H, PATTERN);
    theory::BayerImage expected(W, H);
    theory::wb_bayer(input, expected, WB_R, WB_B, PATTERN);

    // Run DAWN
    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("wb_bayer");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_output_channels(1);
    pipe.set_input_bayer(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        float wb_r, wb_b;
        uint32_t pattern;
        float _pad0, _pad1, _pad2;
    } uniforms = {W, H, WB_R, WB_B, PATTERN, 0, 0, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff_bayer(expected, result);
    bool pass = diff < 1e-5;

    std::cout << std::left << std::setw(18) << "wb_bayer" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(4) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Demosaic (Bayer f32 -> RGB f32)
//------------------------------------------------------------------------------
bool test_dawn_demosaic()
{
    const int W = 64, H = 64;
    const int PATTERN = 0;  // RGGB

    // Create test Bayer with color variation
    theory::BayerImage input = create_test_bayer_f32(W, H, PATTERN);

    theory::RGBImage expected(W, H);
    theory::demosaic(input, expected, PATTERN);

    // Run DAWN
    dawn_pipe::Pipe pipe;
    std::string shader = load_shader("demosaic");
    if (!pipe.init() || shader.empty() || !pipe.load(shader.c_str()))
        return false;

    pipe.set_output_channels(3);
    pipe.set_input_bayer(input.data.data(), W, H);

    struct Uniforms {
        uint32_t width, height;
        uint32_t pattern;
        float _pad;
    } uniforms = {W, H, PATTERN, 0};

    pipe.set_uniforms(&uniforms, sizeof(uniforms));
    if (!pipe.dispatch()) return false;

    std::vector<float> result = pipe.get_output();
    double diff = max_diff_rgb(expected, result);
    bool pass = diff < 1e-4;

    std::cout << std::left << std::setw(18) << "demosaic" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(4) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Test: Color Matrix (RGB -> RGB)
//------------------------------------------------------------------------------
bool test_dawn_color_matrix()
{
    const int W = 64, H = 64;

    // Create test RGB image
    theory::RGBImage input(W, H);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float u = float(x) / (W - 1);
            float v = float(y) / (H - 1);
            input.at(y, x, 0) = u;           // B
            input.at(y, x, 1) = v;           // G
            input.at(y, x, 2) = (u + v) / 2; // R
        }
    }

    // Test matrix: slight color cast
    float matrix[9] = {
        1.1f, 0.0f, 0.0f,   // R row
        0.0f, 1.0f, 0.0f,   // G row
        0.0f, 0.0f, 0.9f    // B row
    };

    theory::RGBImage expected(W, H);
    theory::color_matrix(input, expected, matrix);

    // Run DAWN
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
    double diff = max_diff_rgb(expected, result);
    bool pass = diff < 1e-5;

    std::cout << std::left << std::setw(18) << "color_matrix" << " dawn: ";
    if (pass) std::cout << "PASS\n";
    else std::cout << "FAIL (max=" << std::scientific << std::setprecision(4) << diff << ")\n";

    return pass;
}

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------
int main()
{
    std::cout << "RAWS DAWN Shader Tests\n";
    std::cout << "======================\n\n";

    int passed = 0, failed = 0;

    if (test_dawn_blc_bayer()) passed++; else failed++;
    if (test_dawn_prepare_bayer()) passed++; else failed++;
    if (test_dawn_wb_bayer()) passed++; else failed++;
    if (test_dawn_demosaic()) passed++; else failed++;
    if (test_dawn_color_matrix()) passed++; else failed++;

    std::cout << "\nPassed: " << passed << "/" << (passed + failed) << "\n";

    return failed > 0 ? 1 : 0;
}

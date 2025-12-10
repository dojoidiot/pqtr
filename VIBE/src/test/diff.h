// diff.h - VIBE Test
// Comparison utilities and test declarations

#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <iomanip>
#include <cmath>

// VIBE mods
#include "../main/mods/mods.h"

// LABS mods (pipe namespace) - for CV comparison
namespace pipe { namespace mods {
    bool exposure(const cv::UMat& in, cv::UMat& out, float dial);
    bool white_balance(const cv::UMat& in, cv::UMat& out, float temp, float tint);
    bool tone_map(const cv::UMat& in, cv::UMat& out, float contrast, float highlights,
                  float shadows, float toe_pivot, float shoulder_pivot,
                  float white_point, float black_point);
    bool global_color(const cv::UMat& in, cv::UMat& out, float vibrance, float saturation, float density);
    bool geometric(const cv::UMat& in, cv::UMat& out, float crop_top, float crop_right,
                   float crop_bottom, float crop_left, float zoom, float tilt);
    bool selective_color(const cv::UMat& in, cv::UMat& out,
                         const float hue[8], const float sat[8], const float lum[8]);
    bool split_tone(const cv::UMat& in, cv::UMat& out, float sh_temp, float sh_tint,
                    float hi_temp, float hi_tint);
    bool detail(const cv::UMat& in, cv::UMat& out, float sharpen_amt, float sharpen_rad,
                float denoise_luma, float denoise_chroma);
    bool baseline(const cv::UMat& in, cv::UMat& out, float ev, float clip);
    bool sigmoid(const cv::UMat& in, cv::UMat& out, float contrast, float skew, float white, float black);
    bool base_curve(const cv::UMat& in, cv::UMat& out, const float* curve);
    bool color_matrix(const cv::UMat& in, cv::UMat& out, const cv::Matx33f& matrix);
    bool lut_curve(const cv::UMat& in, cv::UMat& out, const float* lut, int size);
    bool lut3d_apply(const cv::UMat& in, cv::UMat& out, const float* lut, int grid_size);
    bool hsv_lut_apply(const cv::UMat& in, cv::UMat& out, const float* lut);
    bool poly_color(const cv::UMat& in, cv::UMat& out, const float* coeffs);
    bool local_tone(const cv::UMat& in, cv::UMat& out, float strength, float delta, float window_scale);
}}

static const char* GOLD_DIR = "src/test/gold/";

// Comparison result
struct CompareResult
{
    double max_diff;
    double avg_diff;
    bool pass;
};

// Load gold input image
inline cv::UMat load_input()
{
    std::string path = std::string(GOLD_DIR) + "input.png";
    cv::Mat img16 = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (img16.empty())
    {
        std::cerr << "Failed to load: " << path << "\n";
        return cv::UMat();
    }
    cv::Mat img32;
    img16.convertTo(img32, CV_32FC3, 1.0 / 65535.0);
    cv::UMat result;
    img32.copyTo(result);
    return result;
}

// Load gold reference for a module
inline cv::UMat load_gold(const char* name)
{
    std::string path = std::string(GOLD_DIR) + name + ".png";
    cv::Mat img16 = cv::imread(path, cv::IMREAD_UNCHANGED);
    if (img16.empty())
    {
        std::cerr << "Failed to load: " << path << "\n";
        return cv::UMat();
    }
    cv::Mat img32;
    img16.convertTo(img32, CV_32FC3, 1.0 / 65535.0);
    cv::UMat result;
    img32.copyTo(result);
    return result;
}

// Compare two images (raw, no clamping)
inline CompareResult compare(const cv::UMat& a, const cv::UMat& b)
{
    CompareResult r = {0, 0, false};

    if (a.empty() || b.empty() || a.size() != b.size())
    {
        r.max_diff = 999.0;
        return r;
    }

    cv::Mat ma, mb;
    a.copyTo(ma);
    b.copyTo(mb);

    double sum_diff = 0, max_diff = 0;
    int count = 0;

    for (int y = 0; y < ma.rows; y++)
    {
        const float* pa = ma.ptr<float>(y);
        const float* pb = mb.ptr<float>(y);
        for (int x = 0; x < ma.cols * 3; x++)
        {
            double diff = std::abs(pa[x] - pb[x]);
            sum_diff += diff;
            if (diff > max_diff) max_diff = diff;
            count++;
        }
    }

    r.max_diff = max_diff;
    r.avg_diff = count > 0 ? sum_diff / count : 0;
    // Allow 8-bit quantization error (1/255 ≈ 4e-3) plus margin
    r.pass = (max_diff < 5e-2);
    return r;
}

// Compare with clamping (for gold comparison where values are clamped to 0-1)
inline CompareResult compare_clamped(const cv::UMat& a, const cv::UMat& b)
{
    cv::UMat ac, bc;
    cv::max(a, 0.0f, ac);
    cv::min(ac, 1.0f, ac);
    cv::max(b, 0.0f, bc);
    cv::min(bc, 1.0f, bc);
    return compare(ac, bc);
}

// Print result for gold comparison
inline bool print_gold(const char* name, const CompareResult& r)
{
    std::cout << std::left << std::setw(18) << name << " gold: ";
    if (r.pass)
    {
        std::cout << "PASS\n";
        return true;
    }
    std::cout << "FAIL (max=" << std::scientific << std::setprecision(2) << r.max_diff << ")\n";
    return false;
}

// Print result for CV comparison
inline bool print_cv(const char* name, const CompareResult& r)
{
    std::cout << std::left << std::setw(18) << name << " cv:   ";
    if (r.max_diff < 1e-6)
    {
        std::cout << "IDENTICAL\n";
        return true;
    }
    std::cout << "DIFF (max=" << std::scientific << std::setprecision(2) << r.max_diff << ")\n";
    return false;
}

// Test declarations
bool test_exposure();
bool test_white_balance();
bool test_tone_map();
bool test_global_color();
bool test_geometric();
bool test_selective_color();
bool test_split_tone();
bool test_detail();
bool test_baseline();
bool test_sigmoid();
bool test_base_curve();
bool test_color_matrix();
bool test_lut_curve();
bool test_lut3d();
bool test_hsv_lut();
bool test_poly_color();
bool test_local_tone();

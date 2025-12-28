// diff.cpp - Delta-E image difference calculation
//
// Similar to ImageMagick's compare, but using CIE Lab delta-E
// for perceptually accurate color difference measurement.
//
// Delta-E values:
//   <1: Not perceptible by human eyes
//   1-2: Perceptible through close observation
//   2-10: Perceptible at a glance
//   11-49: Colors more similar than opposite
//   100: Exact opposite colors

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace flow
{

// ============================================================================
// Colorspace helpers (from colorspace.cpp, minimal version for diff)
// ============================================================================

// D50 white point (standard for Lab)
static constexpr float d50_inv[3] = { 1.0f/0.9642f, 1.0f, 1.0f/0.8249f };

// sRGB to XYZ matrix (D50 adapted) - from DT
static constexpr float srgb_to_xyz[3][3] = {
    { 0.4360747f, 0.2225045f, 0.0139322f },
    { 0.3850649f, 0.7168786f, 0.0971045f },
    { 0.1430804f, 0.0606169f, 0.7141733f }
};

// sRGB gamma decode (uint8 -> linear float)
static inline float srgb_to_linear(uint8_t v)
{
    float f = v / 255.0f;
    if (f <= 0.04045f)
        return f / 12.92f;
    return std::pow((f + 0.055f) / 1.055f, 2.4f);
}

// Fast cube root approximation (from DT)
static inline float cbrt_5f(float f)
{
    uint32_t* p = (uint32_t*)&f;
    *p = *p / 3 + 709921077;
    return f;
}

// Halley's method refinement (from DT)
static inline float cbrta_halleyf(float a, float R)
{
    const float a3 = a * a * a;
    const float b = a * (a3 + R + R) / (a3 + a3 + R);
    return b;
}

// Lab f function - from DT colorspaces_inline_conversions.h:160-165
static inline float lab_f(float x)
{
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? cbrta_halleyf(cbrt_5f(x), x) : (kappa * x + 16.0f) / 116.0f;
}

// sRGB uint8 → Lab
static inline void srgb_to_lab(uint8_t r, uint8_t g, uint8_t b, float Lab[3])
{
    // sRGB → linear
    float lin[3] = {
        srgb_to_linear(r),
        srgb_to_linear(g),
        srgb_to_linear(b)
    };

    // linear RGB → XYZ
    float XYZ[3] = {
        srgb_to_xyz[0][0] * lin[0] + srgb_to_xyz[1][0] * lin[1] + srgb_to_xyz[2][0] * lin[2],
        srgb_to_xyz[0][1] * lin[0] + srgb_to_xyz[1][1] * lin[1] + srgb_to_xyz[2][1] * lin[2],
        srgb_to_xyz[0][2] * lin[0] + srgb_to_xyz[1][2] * lin[1] + srgb_to_xyz[2][2] * lin[2]
    };

    // XYZ → Lab
    const float fx = lab_f(XYZ[0] * d50_inv[0]);
    const float fy = lab_f(XYZ[1] * d50_inv[1]);
    const float fz = lab_f(XYZ[2] * d50_inv[2]);

    Lab[0] = 116.0f * fy - 16.0f;
    Lab[1] = 500.0f * (fx - fy);
    Lab[2] = 200.0f * (fy - fz);
}

// ============================================================================
// Delta-E calculation (CIE76 - Euclidean distance in Lab)
// ============================================================================

static inline float delta_e_76(const float Lab1[3], const float Lab2[3])
{
    float dL = Lab1[0] - Lab2[0];
    float da = Lab1[1] - Lab2[1];
    float db = Lab1[2] - Lab2[2];
    return std::sqrt(dL * dL + da * da + db * db);
}

// ============================================================================
// diff() - Compare two sRGB images and compute delta-E metrics
// ============================================================================

DiffResult diff(const uint8_t* img1, const uint8_t* img2, int width, int height, bool compute_map)
{
    DiffResult result = {};
    result.width = width;
    result.height = height;

    if (!img1 || !img2 || width <= 0 || height <= 0)
        return result;

    size_t npixels = static_cast<size_t>(width) * height;

    // Stats accumulators
    double sum_de = 0.0;
    double max_de = 0.0;
    size_t count_above_1 = 0;
    size_t count_above_2 = 0;

    // Correlation accumulators (for each channel)
    double sum_r1 = 0, sum_r2 = 0, sum_r1r2 = 0, sum_r1sq = 0, sum_r2sq = 0;
    double sum_g1 = 0, sum_g2 = 0, sum_g1g2 = 0, sum_g1sq = 0, sum_g2sq = 0;
    double sum_b1 = 0, sum_b2 = 0, sum_b1b2 = 0, sum_b1sq = 0, sum_b2sq = 0;

    if (compute_map)
        result.diff_map.resize(npixels);

    for (size_t i = 0; i < npixels; i++)
    {
        size_t idx = i * 3;

        uint8_t r1 = img1[idx + 0], g1 = img1[idx + 1], b1 = img1[idx + 2];
        uint8_t r2 = img2[idx + 0], g2 = img2[idx + 1], b2 = img2[idx + 2];

        // Convert to Lab and compute delta-E
        float Lab1[3], Lab2[3];
        srgb_to_lab(r1, g1, b1, Lab1);
        srgb_to_lab(r2, g2, b2, Lab2);

        float de = delta_e_76(Lab1, Lab2);

        sum_de += de;
        if (de > max_de) max_de = de;
        if (de > 1.0f) count_above_1++;
        if (de > 2.0f) count_above_2++;

        if (compute_map)
            result.diff_map[i] = de;

        // Correlation stats (in linear sRGB space for consistency with pipe tests)
        double lr1 = srgb_to_linear(r1), lg1 = srgb_to_linear(g1), lb1 = srgb_to_linear(b1);
        double lr2 = srgb_to_linear(r2), lg2 = srgb_to_linear(g2), lb2 = srgb_to_linear(b2);

        sum_r1 += lr1; sum_r2 += lr2; sum_r1r2 += lr1 * lr2;
        sum_r1sq += lr1 * lr1; sum_r2sq += lr2 * lr2;
        sum_g1 += lg1; sum_g2 += lg2; sum_g1g2 += lg1 * lg2;
        sum_g1sq += lg1 * lg1; sum_g2sq += lg2 * lg2;
        sum_b1 += lb1; sum_b2 += lb2; sum_b1b2 += lb1 * lb2;
        sum_b1sq += lb1 * lb1; sum_b2sq += lb2 * lb2;
    }

    double n = static_cast<double>(npixels);
    result.mean_de = sum_de / n;
    result.max_de = max_de;
    result.pct_above_1 = 100.0 * count_above_1 / n;
    result.pct_above_2 = 100.0 * count_above_2 / n;

    // Compute Pearson correlation per channel, average
    auto pearson = [n](double sx, double sy, double sxy, double sxsq, double sysq) -> double {
        double num = n * sxy - sx * sy;
        double den = std::sqrt((n * sxsq - sx * sx) * (n * sysq - sy * sy));
        return (den > 1e-10) ? num / den : 1.0;
    };

    double corr_r = pearson(sum_r1, sum_r2, sum_r1r2, sum_r1sq, sum_r2sq);
    double corr_g = pearson(sum_g1, sum_g2, sum_g1g2, sum_g1sq, sum_g2sq);
    double corr_b = pearson(sum_b1, sum_b2, sum_b1b2, sum_b1sq, sum_b2sq);
    result.correlation = (corr_r + corr_g + corr_b) / 3.0;

    return result;
}

// ============================================================================
// diff_image() - Generate a visual diff image (like ImageMagick compare)
// ============================================================================
std::vector<uint8_t> diff_image(const uint8_t* img1, const uint8_t* img2,
                                 int width, int height, DiffMode mode, float scale)
{
    if (!img1 || !img2 || width <= 0 || height <= 0)
        return {};

    size_t npixels = static_cast<size_t>(width) * height;
    std::vector<uint8_t> output(npixels * 3);

    for (size_t i = 0; i < npixels; i++)
    {
        size_t idx = i * 3;

        uint8_t r1 = img1[idx + 0], g1 = img1[idx + 1], b1 = img1[idx + 2];
        uint8_t r2 = img2[idx + 0], g2 = img2[idx + 1], b2 = img2[idx + 2];

        float Lab1[3], Lab2[3];
        srgb_to_lab(r1, g1, b1, Lab1);
        srgb_to_lab(r2, g2, b2, Lab2);

        float de = delta_e_76(Lab1, Lab2);
        float normalized = std::min(1.0f, de / scale);

        uint8_t r, g, b;

        switch (mode)
        {
        case DiffMode::GRAYSCALE:
            r = g = b = static_cast<uint8_t>(normalized * 255);
            break;

        case DiffMode::HEATMAP:
            // Blue → Cyan → Green → Yellow → Red
            if (normalized < 0.25f)
            {
                float t = normalized / 0.25f;
                r = 0;
                g = static_cast<uint8_t>(t * 255);
                b = 255;
            }
            else if (normalized < 0.5f)
            {
                float t = (normalized - 0.25f) / 0.25f;
                r = 0;
                g = 255;
                b = static_cast<uint8_t>((1 - t) * 255);
            }
            else if (normalized < 0.75f)
            {
                float t = (normalized - 0.5f) / 0.25f;
                r = static_cast<uint8_t>(t * 255);
                g = 255;
                b = 0;
            }
            else
            {
                float t = (normalized - 0.75f) / 0.25f;
                r = 255;
                g = static_cast<uint8_t>((1 - t) * 255);
                b = 0;
            }
            break;

        case DiffMode::HIGHLIGHT:
        default:
            // Show original image, but highlight differences in red
            if (de > 2.0f)
            {
                // Red highlight (blend with underlying)
                r = static_cast<uint8_t>(std::min(255, r1 / 2 + 180));
                g = g1 / 3;
                b = b1 / 3;
            }
            else if (de > 1.0f)
            {
                // Yellow-ish for just noticeable
                r = static_cast<uint8_t>(std::min(255, r1 / 2 + 128));
                g = static_cast<uint8_t>(std::min(255, g1 / 2 + 100));
                b = b1 / 2;
            }
            else
            {
                // Grayscale for matching pixels
                uint8_t gray = static_cast<uint8_t>((r1 + g1 + b1) / 3);
                r = g = b = gray;
            }
            break;
        }

        output[idx + 0] = r;
        output[idx + 1] = g;
        output[idx + 2] = b;
    }

    return output;
}

// ============================================================================
// print_diff_stats() - Print diff statistics to stdout
// ============================================================================

void print_diff_stats(const DiffResult& result)
{
    std::printf("Delta-E Statistics:\n");
    std::printf("  Mean:          %.3f\n", result.mean_de);
    std::printf("  Max:           %.3f\n", result.max_de);
    std::printf("  >1 (JND):      %.2f%%\n", result.pct_above_1);
    std::printf("  >2 (visible):  %.2f%%\n", result.pct_above_2);
    std::printf("  Correlation:   %.4f\n", result.correlation);
}

// ============================================================================
// diff_float() - Compare float32 buffers (non-visual intermediate steps)
// ============================================================================

DiffResult diff_float(const float* buf1, const float* buf2, int width, int height,
                      Colorspace cs, bool compute_map)
{
    DiffResult result = {};
    result.width = width;
    result.height = height;

    if (!buf1 || !buf2 || width <= 0 || height <= 0)
        return result;

    size_t npixels = static_cast<size_t>(width) * height;

    double sum_de = 0.0;
    double max_de = 0.0;
    size_t count_above_1 = 0;
    size_t count_above_2 = 0;

    // Correlation accumulators (channel 0 only for simplicity)
    double sum_a = 0, sum_b = 0, sum_ab = 0, sum_asq = 0, sum_bsq = 0;

    if (compute_map)
        result.diff_map.resize(npixels);

    for (size_t i = 0; i < npixels; i++)
    {
        size_t idx = i * 4;  // RGBX or LabX format

        float de;

        if (cs == LAB)
        {
            // Direct delta-E in Lab space
            float Lab1[3] = { buf1[idx + 0], buf1[idx + 1], buf1[idx + 2] };
            float Lab2[3] = { buf2[idx + 0], buf2[idx + 1], buf2[idx + 2] };
            de = delta_e_76(Lab1, Lab2);
        }
        else if (cs == LINEAR_RGB)
        {
            // Convert linear RGB to Lab, then delta-E
            float rgb1[3] = { buf1[idx + 0], buf1[idx + 1], buf1[idx + 2] };
            float rgb2[3] = { buf2[idx + 0], buf2[idx + 1], buf2[idx + 2] };

            // Linear RGB → XYZ → Lab
            float XYZ1[3], XYZ2[3], Lab1[3], Lab2[3];

            XYZ1[0] = srgb_to_xyz[0][0] * rgb1[0] + srgb_to_xyz[1][0] * rgb1[1] + srgb_to_xyz[2][0] * rgb1[2];
            XYZ1[1] = srgb_to_xyz[0][1] * rgb1[0] + srgb_to_xyz[1][1] * rgb1[1] + srgb_to_xyz[2][1] * rgb1[2];
            XYZ1[2] = srgb_to_xyz[0][2] * rgb1[0] + srgb_to_xyz[1][2] * rgb1[1] + srgb_to_xyz[2][2] * rgb1[2];

            XYZ2[0] = srgb_to_xyz[0][0] * rgb2[0] + srgb_to_xyz[1][0] * rgb2[1] + srgb_to_xyz[2][0] * rgb2[2];
            XYZ2[1] = srgb_to_xyz[0][1] * rgb2[0] + srgb_to_xyz[1][1] * rgb2[1] + srgb_to_xyz[2][1] * rgb2[2];
            XYZ2[2] = srgb_to_xyz[0][2] * rgb2[0] + srgb_to_xyz[1][2] * rgb2[1] + srgb_to_xyz[2][2] * rgb2[2];

            float fx1 = lab_f(XYZ1[0] * d50_inv[0]), fy1 = lab_f(XYZ1[1] * d50_inv[1]), fz1 = lab_f(XYZ1[2] * d50_inv[2]);
            float fx2 = lab_f(XYZ2[0] * d50_inv[0]), fy2 = lab_f(XYZ2[1] * d50_inv[1]), fz2 = lab_f(XYZ2[2] * d50_inv[2]);

            Lab1[0] = 116.0f * fy1 - 16.0f; Lab1[1] = 500.0f * (fx1 - fy1); Lab1[2] = 200.0f * (fy1 - fz1);
            Lab2[0] = 116.0f * fy2 - 16.0f; Lab2[1] = 500.0f * (fx2 - fy2); Lab2[2] = 200.0f * (fy2 - fz2);

            de = delta_e_76(Lab1, Lab2);
        }
        else if (cs == BAYER)
        {
            // Single channel comparison - just absolute difference scaled to delta-E range
            // Bayer is single value per pixel, stored in first channel
            float v1 = buf1[idx];
            float v2 = buf2[idx];
            de = std::abs(v1 - v2) * 100.0f;  // Scale to roughly match delta-E range
        }
        else
        {
            // Fallback: RMSE-style
            float dr = buf1[idx + 0] - buf2[idx + 0];
            float dg = buf1[idx + 1] - buf2[idx + 1];
            float db = buf1[idx + 2] - buf2[idx + 2];
            de = std::sqrt(dr * dr + dg * dg + db * db) * 50.0f;
        }

        sum_de += de;
        if (de > max_de) max_de = de;
        if (de > 1.0f) count_above_1++;
        if (de > 2.0f) count_above_2++;

        if (compute_map)
            result.diff_map[i] = de;

        // Correlation on first channel
        double a = buf1[idx], b = buf2[idx];
        sum_a += a; sum_b += b; sum_ab += a * b;
        sum_asq += a * a; sum_bsq += b * b;
    }

    double n = static_cast<double>(npixels);
    result.mean_de = sum_de / n;
    result.max_de = max_de;
    result.pct_above_1 = 100.0 * count_above_1 / n;
    result.pct_above_2 = 100.0 * count_above_2 / n;

    // Pearson correlation
    double num = n * sum_ab - sum_a * sum_b;
    double den = std::sqrt((n * sum_asq - sum_a * sum_a) * (n * sum_bsq - sum_b * sum_b));
    result.correlation = (den > 1e-10) ? num / den : 1.0;

    return result;
}

} // namespace flow

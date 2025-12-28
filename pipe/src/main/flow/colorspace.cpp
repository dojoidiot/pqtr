// colorspace.cpp - Lab ↔ RGB colorspace conversions
//
// CLEAN COPY from darktable common/colorspaces_inline_conversions.h
// These are the "swap" functions for colorspace transitions in the pipeline.

#include "../../../inc/pipe.hpp"
#include <cmath>

namespace flow
{

// ============================================================================
// Constants - CLEAN COPY from DT colorspaces_inline_conversions.h:168-169
// ============================================================================

// D50 white point (standard for Lab)
static constexpr float d50[3] = { 0.9642f, 1.0f, 0.8249f };
static constexpr float d50_inv[3] = { 1.0f/0.9642f, 1.0f, 1.0f/0.8249f };

// XYZ to sRGB matrix (D50 adapted)
// CLEAN COPY from DT colorspaces_inline_conversions.h:497-500
static constexpr float xyz_to_srgb[3][3] = {
    {  3.1338561f, -0.9787684f,  0.0719453f },
    { -1.6168667f,  1.9161415f, -0.2289914f },
    { -0.4906146f,  0.0334540f,  1.4052427f }
};

// sRGB to XYZ matrix (D50 adapted)
// CLEAN COPY from DT colorspaces_inline_conversions.h:492-495
static constexpr float srgb_to_xyz[3][3] = {
    { 0.4360747f, 0.2225045f, 0.0139322f },
    { 0.3850649f, 0.7168786f, 0.0971045f },
    { 0.1430804f, 0.0606169f, 0.7141733f }
};

// ============================================================================
// Lab helper functions - CLEAN COPY from DT
// ============================================================================

// CLEAN COPY from DT colorspaces_inline_conversions.h:201-207
static inline float lab_f_inv(const float x)
{
    const float epsilon = 0.20689655172413796f; // cbrtf(216.0f/24389.0f)
    const float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? x * x * x : (116.0f * x - 16.0f) / kappa;
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

// CLEAN COPY from DT colorspaces_inline_conversions.h:160-165
static inline float lab_f(float x)
{
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? cbrta_halleyf(cbrt_5f(x), x) : (kappa * x + 16.0f) / 116.0f;
}

// ============================================================================
// dt_Lab_to_XYZ - CLEAN COPY from DT colorspaces_inline_conversions.h:211-225
// ============================================================================

static inline void dt_Lab_to_XYZ(const float Lab[3], float XYZ[3])
{
    // Simplified from DT (without SIMD optimizations)
    // f[0] = (L + 16) / 116
    // f[1] = a / 500 + f[0]
    // f[2] = f[0] - b / 200
    const float fy = (Lab[0] + 16.0f) / 116.0f;
    const float fx = Lab[1] / 500.0f + fy;
    const float fz = fy - Lab[2] / 200.0f;

    XYZ[0] = d50[0] * lab_f_inv(fx);
    XYZ[1] = d50[1] * lab_f_inv(fy);
    XYZ[2] = d50[2] * lab_f_inv(fz);
}

// ============================================================================
// dt_XYZ_to_Lab - CLEAN COPY from DT colorspaces_inline_conversions.h:172-199
// ============================================================================

static inline void dt_XYZ_to_Lab(const float XYZ[3], float Lab[3])
{
    const float fx = lab_f(XYZ[0] * d50_inv[0]);
    const float fy = lab_f(XYZ[1] * d50_inv[1]);
    const float fz = lab_f(XYZ[2] * d50_inv[2]);

    Lab[0] = 116.0f * fy - 16.0f;
    Lab[1] = 500.0f * (fx - fy);
    Lab[2] = 200.0f * (fy - fz);
}

// ============================================================================
// dt_XYZ_to_Rec709_D50 - CLEAN COPY from DT colorspaces_inline_conversions.h:529-533
// XYZ D50 → linear sRGB
// ============================================================================

static inline void dt_XYZ_to_Rec709_D50(const float XYZ[3], float sRGB[3])
{
    // Matrix multiplication: sRGB = xyz_to_srgb * XYZ
    // Note: DT uses transposed matrix, we use row-major
    sRGB[0] = xyz_to_srgb[0][0] * XYZ[0] + xyz_to_srgb[1][0] * XYZ[1] + xyz_to_srgb[2][0] * XYZ[2];
    sRGB[1] = xyz_to_srgb[0][1] * XYZ[0] + xyz_to_srgb[1][1] * XYZ[1] + xyz_to_srgb[2][1] * XYZ[2];
    sRGB[2] = xyz_to_srgb[0][2] * XYZ[0] + xyz_to_srgb[1][2] * XYZ[1] + xyz_to_srgb[2][2] * XYZ[2];
}

// ============================================================================
// dt_linearRGB_to_XYZ - CLEAN COPY from DT colorspaces_inline_conversions.h:502-505
// linear sRGB → XYZ D50
// ============================================================================

static inline void dt_linearRGB_to_XYZ(const float sRGB[3], float XYZ[3])
{
    // Matrix multiplication: XYZ = srgb_to_xyz * sRGB
    XYZ[0] = srgb_to_xyz[0][0] * sRGB[0] + srgb_to_xyz[1][0] * sRGB[1] + srgb_to_xyz[2][0] * sRGB[2];
    XYZ[1] = srgb_to_xyz[0][1] * sRGB[0] + srgb_to_xyz[1][1] * sRGB[1] + srgb_to_xyz[2][1] * sRGB[2];
    XYZ[2] = srgb_to_xyz[0][2] * sRGB[0] + srgb_to_xyz[1][2] * sRGB[1] + srgb_to_xyz[2][2] * sRGB[2];
}

// ============================================================================
// Public swap functions for pipeline
// ============================================================================

void swapLabToRGB(Flow& flow)
{
    auto& root = flow.info().root();
    int width = static_cast<int>(root.leaf(WIDTH).dial());
    int height = static_cast<int>(root.leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    float* data = flow.rgb();

    for (size_t i = 0; i < npixels; i++)
    {
        size_t idx = i * 4;
        float Lab[3] = { data[idx + 0], data[idx + 1], data[idx + 2] };
        float XYZ[3], RGB[3];

        dt_Lab_to_XYZ(Lab, XYZ);
        dt_XYZ_to_Rec709_D50(XYZ, RGB);

        data[idx + 0] = RGB[0];
        data[idx + 1] = RGB[1];
        data[idx + 2] = RGB[2];
    }
}

void swapRGBToLab(Flow& flow)
{
    auto& root = flow.info().root();
    int width = static_cast<int>(root.leaf(WIDTH).dial());
    int height = static_cast<int>(root.leaf(HEIGHT).dial());
    size_t npixels = static_cast<size_t>(width) * height;

    float* data = flow.rgb();

    for (size_t i = 0; i < npixels; i++)
    {
        size_t idx = i * 4;
        float RGB[3] = { data[idx + 0], data[idx + 1], data[idx + 2] };
        float XYZ[3], Lab[3];

        dt_linearRGB_to_XYZ(RGB, XYZ);
        dt_XYZ_to_Lab(XYZ, Lab);

        data[idx + 0] = Lab[0];
        data[idx + 1] = Lab[1];
        data[idx + 2] = Lab[2];
    }
}

} // namespace flow

// channelmixer.cpp - CAT16 chromatic adaptation
//
// CLEAN COPY from darktable src/common/chromatic_adaptation.h
// Uses CAT16 (CIECAM16) matrices and adaptation formula.
//
// Per Rule 7 (IOP order): channelmixer runs AFTER colorin (28.5 > 28.0)
// Per Rule 8: Pipeline is Lab after colorin; this module converts Lab ↔ XYZ internally

#include "../../../inc/pipe.hpp"
#include <cmath>

namespace flow
{

// =============================================================================
// Constants from DT common/math.h line 31
// =============================================================================
static constexpr float NORM_MIN = 1.52587890625e-05f; // FLT_EPSILON * 2^10

// =============================================================================
// D50 white point (from colorin/colorout)
// =============================================================================
static constexpr float D50_X = 0.9642f;
static constexpr float D50_Y = 1.0000f;
static constexpr float D50_Z = 0.8249f;

// =============================================================================
// Lab ↔ XYZ conversion (CLEAN COPY from DT colorspaces_inline_conversions.h)
// =============================================================================

// Lab f^-1() function - line 202-207
static inline float lab_f_inv(float x)
{
    constexpr float epsilon = 0.20689655172413796f; // cbrtf(216.0f/24389.0f)
    constexpr float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? x * x * x : (116.0f * x - 16.0f) / kappa;
}

// Fast cube root - line 144-149
static inline float cbrt_5f(float f)
{
    uint32_t* p = (uint32_t*)&f;
    *p = *p / 3 + 709921077;
    return f;
}

// Halley refinement - line 151-157
static inline float cbrta_halleyf(float a, float R)
{
    const float a3 = a * a * a;
    return a * (a3 + R + R) / (a3 + a3 + R);
}

// Lab f() function - line 160-165
static inline float lab_f(float x)
{
    constexpr float epsilon = 216.0f / 24389.0f;
    constexpr float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? cbrta_halleyf(cbrt_5f(x), x) : (kappa * x + 16.0f) / 116.0f;
}

// Lab → XYZ D50 (from line 211-225)
static inline void Lab_to_XYZ(const float Lab[3], float XYZ[3])
{
    float fy = (Lab[0] + 16.0f) / 116.0f;
    float fx = Lab[1] / 500.0f + fy;
    float fz = fy - Lab[2] / 200.0f;

    XYZ[0] = D50_X * lab_f_inv(fx);
    XYZ[1] = D50_Y * lab_f_inv(fy);
    XYZ[2] = D50_Z * lab_f_inv(fz);
}

// XYZ D50 → Lab (from line 172-199)
static inline void XYZ_to_Lab(const float XYZ[3], float Lab[3])
{
    float fx = lab_f(XYZ[0] / D50_X);
    float fy = lab_f(XYZ[1] / D50_Y);
    float fz = lab_f(XYZ[2] / D50_Z);

    Lab[0] = 116.0f * fy - 16.0f;
    Lab[1] = 500.0f * (fx - fy);
    Lab[2] = 200.0f * (fy - fz);
}

// =============================================================================
// CAT16 matrices from DT chromatic_adaptation.h lines 92-108
// =============================================================================

// XYZ to CAT16 LMS (CIECAM16)
static constexpr float XYZ_to_CAT16_LMS[9] = {
     0.401288f,  0.650173f, -0.051461f,
    -0.250268f,  1.204414f,  0.045854f,
    -0.002079f,  0.048952f,  0.953127f
};

// CAT16 LMS to XYZ
static constexpr float CAT16_LMS_to_XYZ[9] = {
     1.862068f, -1.011255f,  0.149187f,
     0.38752f,   0.621447f, -0.008974f,
    -0.015841f, -0.034123f,  1.049964f
};

// D50 white point in CAT16 LMS (from chromatic_adaptation.h line 323)
static constexpr float D50_CAT16_LMS[3] = { 0.994535f, 1.000997f, 0.833036f };

// =============================================================================
// Helper functions - CLEAN COPY from DT
// =============================================================================

// Convert xy chromaticity to XYZ (Y=1) - from DT illuminant_xy_to_XYZ
static inline void illuminant_xy_to_XYZ(float x, float y, float XYZ[3])
{
    XYZ[0] = x / y;
    XYZ[1] = 1.0f;
    XYZ[2] = (1.0f - x - y) / y;
}

// Matrix-vector multiply
static inline void mat3_mul_vec(const float M[9], const float in[3], float out[3])
{
    out[0] = M[0]*in[0] + M[1]*in[1] + M[2]*in[2];
    out[1] = M[3]*in[0] + M[4]*in[1] + M[5]*in[2];
    out[2] = M[6]*in[0] + M[7]*in[1] + M[8]*in[2];
}

// =============================================================================
// Vector scaling - CLEAN COPY from DT common/math.h lines 333-351
// =============================================================================

// Divide vector by scaling factor (normalize by luminance)
static inline void downscale_vector(float vector[3], const float scaling)
{
    // check that scaling is positive (NaN produces FALSE)
    const int valid = (scaling > NORM_MIN);
    for (int c = 0; c < 3; c++)
        vector[c] = (valid) ? vector[c] / (scaling + NORM_MIN) : vector[c] / NORM_MIN;
}

// Multiply vector by scaling factor (restore luminance)
static inline void upscale_vector(float vector[3], const float scaling)
{
    // check that scaling is positive (NaN produces FALSE)
    const int valid = (scaling > NORM_MIN);
    for (int c = 0; c < 3; c++)
        vector[c] = (valid) ? vector[c] * (scaling + NORM_MIN) : vector[c] * NORM_MIN;
}

// =============================================================================
// CAT16 adaptation - CLEAN COPY from chromatic_adaptation.h lines 313-336
// =============================================================================

// CAT16 chromatic adaptation from origin illuminant to D50
// Full adaptation (D=1.0, full=TRUE)
static inline void CAT16_adapt_D50(const float lms_in[3],
                                   const float origin_illuminant[3],
                                   float lms_out[3])
{
    // From DT line 327: lms_out[c] = lms_in[c] * D50[c] / origin_illuminant[c]
    lms_out[0] = lms_in[0] * D50_CAT16_LMS[0] / origin_illuminant[0];
    lms_out[1] = lms_in[1] * D50_CAT16_LMS[1] / origin_illuminant[1];
    lms_out[2] = lms_in[2] * D50_CAT16_LMS[2] / origin_illuminant[2];
}

// =============================================================================
// Implementation
// =============================================================================

class ChannelmixerImpl : public Channelmixer
{
    float illuminant_LMS_[3] = {1.0f, 1.0f, 1.0f};
    bool enabled_ = false;

public:
    std::string name() const override { return "channelmixer"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setParams(float x, float y, float temp) override
    {
        (void)temp; // Temperature is informational, we use x,y directly

        // Convert illuminant xy to XYZ (DT: illuminant_xy_to_XYZ)
        float illuminant_XYZ[3];
        illuminant_xy_to_XYZ(x, y, illuminant_XYZ);

        // Convert illuminant XYZ to CAT16 LMS (DT: convert_any_XYZ_to_LMS)
        mat3_mul_vec(XYZ_to_CAT16_LMS, illuminant_XYZ, illuminant_LMS_);

        enabled_ = true;
    }

    void process(Flow& flow) override
    {
        if (!enabled_) return;

        auto& root = flow.info().root();

        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        float* data = flow.rgb();

        // Process each pixel - CLEAN COPY from DT chromatic_adaptation.h lines 453-460
        // case DT_ADAPTATION_CAT16:
        // {
        //   convert_XYZ_to_CAT16_LMS(in, temp_two);
        //   downscale_vector(temp_two, Y);
        //   CAT16_adapt_D50(temp_two, illuminant, 1.0f, TRUE, temp_one);
        //   upscale_vector(temp_one, Y);
        //   convert_CAT16_LMS_to_XYZ(temp_one, out);
        // }
        for (size_t i = 0; i < npixels; i++)
        {
            size_t idx = i * 4;

            // Input is Lab (after colorin)
            float Lab[3] = {data[idx+0], data[idx+1], data[idx+2]};

            // Lab → XYZ D50
            float XYZ[3];
            Lab_to_XYZ(Lab, XYZ);

            // Save Y for scaling (DT line 431: const float Y = in[1])
            const float Y = XYZ[1];

            // convert_XYZ_to_CAT16_LMS(in, temp_two)
            float LMS[3];
            mat3_mul_vec(XYZ_to_CAT16_LMS, XYZ, LMS);

            // downscale_vector(temp_two, Y)
            downscale_vector(LMS, Y);

            // CAT16_adapt_D50(temp_two, illuminant, 1.0f, TRUE, temp_one)
            float adapted_LMS[3];
            CAT16_adapt_D50(LMS, illuminant_LMS_, adapted_LMS);

            // upscale_vector(temp_one, Y)
            upscale_vector(adapted_LMS, Y);

            // convert_CAT16_LMS_to_XYZ(temp_one, out)
            float adapted_XYZ[3];
            mat3_mul_vec(CAT16_LMS_to_XYZ, adapted_LMS, adapted_XYZ);

            // XYZ D50 → Lab
            float out_Lab[3];
            XYZ_to_Lab(adapted_XYZ, out_Lab);

            data[idx+0] = out_Lab[0];
            data[idx+1] = out_Lab[1];
            data[idx+2] = out_Lab[2];
        }
    }
};

std::unique_ptr<Channelmixer> makeChannelmixer()
{
    return std::make_unique<ChannelmixerImpl>();
}

} // namespace flow

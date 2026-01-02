/*
    channelmixerrgb - EXACT COPY of darktable channelmixerrgb

    Source: dark/lib/desk/src/iop/channelmixerrgb.c
            dark/lib/desk/src/common/chromatic_adaptation.h
            dark/lib/desk/src/common/colorspaces_inline_conversions.h

    Input: float32 RGB (4 channels, pipeline RGB)
    Output: float32 RGB (4 channels, pipeline RGB)

    Process:
    1. Convert RGB to LMS (Bradford)
    2. Chromatic adaptation to D50
    3. Apply MIX matrix, convert to XYZ
    4. Gamut mapping
    5. Convert back to LMS, apply saturation/lightness
    6. Convert to XYZ, then back to RGB
*/

#include <math.h>
#include <string.h>
#include <stdint.h>
#include <float.h>

#include "types.h"

/* ============================================================================
   Type definitions (use shared types.h)
   ============================================================================ */

#ifndef for_four_channels
#define for_four_channels(c) for(int c = 0; c < 4; c++)
#endif
#ifndef for_three_channels
#define for_three_channels(c) for(int c = 0; c < 3; c++)
#endif
#define DT_FMA(a, b, c) ((a) * (b) + (c))

#define NORM_MIN 1.52587890625e-05f  /* from colorspaces_inline_conversions.h */
#define INVERSE_SQRT_3 0.5773502691896258f

typedef enum {
    DT_ADAPTATION_LINEAR_BRADFORD = 0,
    DT_ADAPTATION_CAT16 = 1,
    DT_ADAPTATION_FULL_BRADFORD = 2,
    DT_ADAPTATION_XYZ = 3,
    DT_ADAPTATION_RGB = 4,
    DT_ADAPTATION_LAST
} dt_adaptation_t;

typedef enum {
    CHANNELMIXERRGB_V_1 = 0,
    CHANNELMIXERRGB_V_2 = 1,
    CHANNELMIXERRGB_V_3 = 2,
} dt_iop_channelmixer_rgb_version_t;

/* ============================================================================
   From chromatic_adaptation.h - Bradford LMS matrices (lines 41-57)
   ============================================================================ */

static const dt_colormatrix_t XYZ_to_Bradford_LMS = {
    {  0.8951f,  0.2664f, -0.1614f, 0.f },
    { -0.7502f,  1.7135f,  0.0367f, 0.f },
    {  0.0389f, -0.0685f,  1.0296f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t XYZ_to_Bradford_LMS_trans = {
    {  0.8951f, -0.7502f,  0.0389f, 0.f },
    {  0.2664f,  1.7135f, -0.0685f, 0.f },
    { -0.1614f,  0.0367f,  1.0296f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t Bradford_LMS_to_XYZ = {
    {  0.9870f, -0.1471f,  0.1600f, 0.f },
    {  0.4323f,  0.5184f,  0.0493f, 0.f },
    { -0.0085f,  0.0400f,  0.9685f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t Bradford_LMS_to_XYZ_trans = {
    {  0.9870f,  0.4323f, -0.0085f, 0.f },
    { -0.1471f,  0.5184f,  0.0400f, 0.f },
    {  0.1600f,  0.0493f,  0.9685f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

/* ============================================================================
   From chromatic_adaptation.h - CAT16 LMS matrices (lines 92-108)
   ============================================================================ */

static const dt_colormatrix_t XYZ_to_CAT16_LMS = {
    {  0.401288f, 0.650173f, -0.051461f, 0.f },
    { -0.250268f, 1.204414f,  0.045854f, 0.f },
    { -0.002079f, 0.048952f,  0.953127f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t XYZ_to_CAT16_LMS_trans = {
    {  0.401288f, -0.250268f, -0.002079f, 0.f },
    {  0.650173f,  1.204414f,  0.048952f, 0.f },
    { -0.051461f,  0.045854f,  0.953127f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t CAT16_LMS_to_XYZ = {
    {  1.862068f, -1.011255f,  0.149187f, 0.f },
    {  0.38752f ,  0.621447f, -0.008974f, 0.f },
    { -0.015841f, -0.034123f,  1.049964f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t CAT16_LMS_to_XYZ_trans = {
    {  1.862068f,  0.38752f , -0.015841f, 0.f },
    { -1.011255f,  0.621447f, -0.034123f, 0.f },
    {  0.149187f, -0.008974f,  1.049964f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

/* ============================================================================
   From colorspaces_inline_conversions.h - D50xyY (line 51)
   ============================================================================ */

static const struct { float x, y, Y; } D50xyY = { 0.34567f, 0.35850f, 1.0f };

/* ============================================================================
   From colorspaces_inline_conversions.h - matrix operations
   ============================================================================ */

#ifndef DT_APPLY_TRANSPOSED_DEFINED
#define DT_APPLY_TRANSPOSED_DEFINED
static inline void dt_apply_transposed_color_matrix(const dt_aligned_pixel_t in,
                                                     const dt_colormatrix_t matrix,
                                                     dt_aligned_pixel_t out)
{
    for(int i = 0; i < 3; i++)
        out[i] = matrix[0][i] * in[0] + matrix[1][i] * in[1] + matrix[2][i] * in[2];
    out[3] = 0.0f;
}
#endif

#ifndef DT_COLORMATRIX_MUL_DEFINED
#define DT_COLORMATRIX_MUL_DEFINED
static inline void dt_colormatrix_mul(dt_colormatrix_t dst,
                                       const dt_colormatrix_t m1,
                                       const dt_colormatrix_t m2)
{
    for(int k = 0; k < 3; k++) {
        for(int i = 0; i < 3; i++) {
            float sum = 0.0f;
            for(int j = 0; j < 3; j++)
                sum += m1[k][j] * m2[j][i];
            dst[k][i] = sum;
        }
        dst[k][3] = 0.0f;
    }
    for(int i = 0; i < 4; i++) dst[3][i] = 0.0f;
}
#endif

#ifndef DT_COLORMATRIX_TRANSPOSE_DEFINED
#define DT_COLORMATRIX_TRANSPOSE_DEFINED
static inline void dt_colormatrix_transpose(dt_colormatrix_t dst,
                                             const dt_colormatrix_t src)
{
    for(int i = 0; i < 3; i++)
        for(int j = 0; j < 3; j++)
            dst[i][j] = src[j][i];
    for(int i = 0; i < 4; i++) dst[i][3] = dst[3][i] = 0.0f;
}
#endif

static inline void dt_colormatrix_copy(dt_colormatrix_t dst, const dt_colormatrix_t src)
{
    memcpy(dst, src, sizeof(dt_colormatrix_t));
}

/* ============================================================================
   From chromatic_adaptation.h - Bradford conversion helpers (lines 60-86)
   ============================================================================ */

static inline void convert_XYZ_to_bradford_LMS(const dt_aligned_pixel_t XYZ, dt_aligned_pixel_t LMS)
{
    dt_apply_transposed_color_matrix(XYZ, XYZ_to_Bradford_LMS_trans, LMS);
}

static inline void make_RGB_to_Bradford_LMS(const dt_colormatrix_t rgb, dt_colormatrix_t lms)
{
    dt_colormatrix_mul(lms, XYZ_to_Bradford_LMS, rgb);
}

static inline void convert_bradford_LMS_to_XYZ(const dt_aligned_pixel_t LMS, dt_aligned_pixel_t XYZ)
{
    dt_apply_transposed_color_matrix(LMS, Bradford_LMS_to_XYZ_trans, XYZ);
}

static inline void make_Bradford_LMS_to_XYZ(const dt_colormatrix_t lms, dt_colormatrix_t xyz)
{
    dt_colormatrix_mul(xyz, Bradford_LMS_to_XYZ, lms);
}

/* ============================================================================
   From chromatic_adaptation.h - CAT16 conversion helpers (lines 110-137)
   ============================================================================ */

static inline void convert_XYZ_to_CAT16_LMS(const dt_aligned_pixel_t XYZ, dt_aligned_pixel_t LMS)
{
    dt_apply_transposed_color_matrix(XYZ, XYZ_to_CAT16_LMS_trans, LMS);
}

static inline void make_RGB_to_CAT16_LMS(const dt_colormatrix_t rgb, dt_colormatrix_t lms)
{
    dt_colormatrix_mul(lms, XYZ_to_CAT16_LMS, rgb);
}

static inline void convert_CAT16_LMS_to_XYZ(const dt_aligned_pixel_t LMS, dt_aligned_pixel_t XYZ)
{
    dt_apply_transposed_color_matrix(LMS, CAT16_LMS_to_XYZ_trans, XYZ);
}

static inline void make_CAT16_LMS_to_XYZ(const dt_colormatrix_t lms, dt_colormatrix_t xyz)
{
    dt_colormatrix_mul(xyz, CAT16_LMS_to_XYZ, lms);
}

/* ============================================================================
   From chromatic_adaptation.h - convert_any_LMS_to_XYZ (lines 141-171)
   ============================================================================ */

static inline void convert_any_LMS_to_XYZ(const dt_aligned_pixel_t LMS, dt_aligned_pixel_t XYZ,
                                          const dt_adaptation_t kind)
{
    switch(kind)
    {
        case DT_ADAPTATION_FULL_BRADFORD:
        case DT_ADAPTATION_LINEAR_BRADFORD:
            convert_bradford_LMS_to_XYZ(LMS, XYZ);
            break;
        case DT_ADAPTATION_CAT16:
            convert_CAT16_LMS_to_XYZ(LMS, XYZ);
            break;
        case DT_ADAPTATION_XYZ:
        case DT_ADAPTATION_RGB:
        case DT_ADAPTATION_LAST:
        default:
            XYZ[0] = LMS[0];
            XYZ[1] = LMS[1];
            XYZ[2] = LMS[2];
            break;
    }
}

static inline void convert_any_XYZ_to_LMS(const dt_aligned_pixel_t XYZ, dt_aligned_pixel_t LMS,
                                          dt_adaptation_t kind)
{
    switch(kind)
    {
        case DT_ADAPTATION_FULL_BRADFORD:
        case DT_ADAPTATION_LINEAR_BRADFORD:
            convert_XYZ_to_bradford_LMS(XYZ, LMS);
            break;
        case DT_ADAPTATION_CAT16:
            convert_XYZ_to_CAT16_LMS(XYZ, LMS);
            break;
        case DT_ADAPTATION_XYZ:
        case DT_ADAPTATION_RGB:
        case DT_ADAPTATION_LAST:
        default:
            LMS[0] = XYZ[0];
            LMS[1] = XYZ[1];
            LMS[2] = XYZ[2];
            break;
    }
}

/* ============================================================================
   From chromatic_adaptation.h - bradford_adapt_D50 (lines 255-280)
   ============================================================================ */

static inline void bradford_adapt_D50(const dt_aligned_pixel_t lms_in,
                                      const dt_aligned_pixel_t origin_illuminant,
                                      const float p, const int full,
                                      dt_aligned_pixel_t lms_out)
{
    const dt_aligned_pixel_t D50 = { 0.996078f, 1.020646f, 0.818155f, 0.f };

    dt_aligned_pixel_t temp = { lms_in[0] / origin_illuminant[0],
                                lms_in[1] / origin_illuminant[1],
                                lms_in[2] / origin_illuminant[2],
                                0.f };

    if(full) temp[2] = (temp[2] > 0.f) ? powf(temp[2], p) : temp[2];

    lms_out[0] = D50[0] * temp[0];
    lms_out[1] = D50[1] * temp[1];
    lms_out[2] = D50[2] * temp[2];
}

/* ============================================================================
   From chromatic_adaptation.h - CAT16_adapt_D50 (lines 312-337)
   ============================================================================ */

static inline void CAT16_adapt_D50(const dt_aligned_pixel_t lms_in,
                                    const dt_aligned_pixel_t origin_illuminant,
                                    const float D, const int full,
                                    dt_aligned_pixel_t lms_out)
{
    const dt_aligned_pixel_t D50 = { 0.994535f, 1.000997f, 0.833036f, 0.f };

    if(full)
    {
        lms_out[0] = lms_in[0] * D50[0] / origin_illuminant[0];
        lms_out[1] = lms_in[1] * D50[1] / origin_illuminant[1];
        lms_out[2] = lms_in[2] * D50[2] / origin_illuminant[2];
    }
    else
    {
        lms_out[0] = lms_in[0] * (D * D50[0] / origin_illuminant[0] + 1.f - D);
        lms_out[1] = lms_in[1] * (D * D50[1] / origin_illuminant[1] + 1.f - D);
        lms_out[2] = lms_in[2] * (D * D50[2] / origin_illuminant[2] + 1.f - D);
    }
}

/* ============================================================================
   From colorspaces_inline_conversions.h - xyY/uvY conversions
   ============================================================================ */

static inline void dt_xyY_to_uvY(const dt_aligned_pixel_t xyY, dt_aligned_pixel_t uvY)
{
    const float div = -2.0f * xyY[0] + 12.0f * xyY[1] + 3.0f;
    uvY[0] = 4.0f * xyY[0] / div;
    uvY[1] = 9.0f * xyY[1] / div;
    uvY[2] = xyY[2];
    uvY[3] = 0.f;
}

static inline void dt_uvY_to_xyY(const dt_aligned_pixel_t uvY, dt_aligned_pixel_t xyY)
{
    const float div = 6.0f * uvY[0] - 16.0f * uvY[1] + 12.0f;
    xyY[0] = 9.0f * uvY[0] / div;
    xyY[1] = 4.0f * uvY[1] / div;
    xyY[2] = uvY[2];
    xyY[3] = 0.f;
}

#ifndef DT_XYY_TO_XYZ_DEFINED
#define DT_XYY_TO_XYZ_DEFINED
static inline void dt_xyY_to_XYZ(const dt_aligned_pixel_t xyY, dt_aligned_pixel_t XYZ)
{
    XYZ[0] = xyY[2] * xyY[0] / xyY[1];
    XYZ[1] = xyY[2];
    XYZ[2] = xyY[2] * (1.0f - xyY[0] - xyY[1]) / xyY[1];
    XYZ[3] = 0.f;
}
#endif

/* ============================================================================
   From common/math.h - sqf, euclidean_norm, scalar_product
   (sqf is in types.h)
   ============================================================================ */

static inline float euclidean_norm(const dt_aligned_pixel_t x)
{
    return sqrtf(sqf(x[0]) + sqf(x[1]) + sqf(x[2]));
}

#ifndef SCALAR_PRODUCT_DEFINED
#define SCALAR_PRODUCT_DEFINED
static inline float scalar_product(const dt_aligned_pixel_t x, const dt_aligned_pixel_t y)
{
    return x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
}
#endif

/* ============================================================================
   From channelmixerrgb.c - _gamut_mapping (lines 648-703)
   ============================================================================ */

static inline void _gamut_mapping(const dt_aligned_pixel_t input,
                                  const float compression,
                                  const int clip,
                                  dt_aligned_pixel_t output)
{
    const float sum = input[0] + input[1] + input[2];
    const float Y = input[1];

    dt_aligned_pixel_t xyY = { sum > 0.0f ? input[0] / sum : D50xyY.x,
                               sum > 0.0f ? input[1] / sum : D50xyY.y,
                               Y,
                               0.0f };

    dt_aligned_pixel_t uvY;
    dt_xyY_to_uvY(xyY, uvY);

    const float D50[2] = { 0.20915914598542354f, 0.488075320769787f };
    const float delta[2] = { D50[0] - uvY[0], D50[1] - uvY[1] };
    const float Delta = Y * (sqf(delta[0]) + sqf(delta[1]));

    const float correction = (compression == 0.0f) ? 0.f : powf(Delta, compression);
    for(size_t c = 0; c < 2; c++)
    {
        const float tmp = DT_FMA(correction, delta[c], uvY[c]);
        uvY[c] = (uvY[c] > D50[c]) ? fmaxf(tmp, D50[c]) : fminf(tmp, D50[c]);
    }

    dt_uvY_to_xyY(uvY, xyY);

    if(clip) for(size_t c = 0; c < 2; c++) xyY[c] = fmaxf(xyY[c], 0.0f);

    xyY[1] = fmaxf(xyY[1], NORM_MIN);

    const float scale = xyY[0] + xyY[1];
    const int sanitize = (scale >= 1.f);
    for(size_t c = 0; c < 2; c++) xyY[c] = (sanitize) ? xyY[c] / scale : xyY[c];

    dt_xyY_to_XYZ(xyY, output);
}

/* ============================================================================
   From channelmixerrgb.c - _luma_chroma (lines 706-769)
   ============================================================================ */

static inline void _luma_chroma(const dt_aligned_pixel_t input,
                                const dt_aligned_pixel_t saturation,
                                const dt_aligned_pixel_t lightness,
                                dt_aligned_pixel_t output,
                                const dt_iop_channelmixer_rgb_version_t version)
{
    float norm = euclidean_norm(input);
    const float avg = fmaxf((input[0] + input[1] + input[2]) / 3.0f, NORM_MIN);

    if(norm > 0.f && avg > 0.f)
    {
        const float mix = scalar_product(input, lightness);

        if(version == CHANNELMIXERRGB_V_3) norm *= INVERSE_SQRT_3;

        for_three_channels(c)
            output[c] = input[c] / norm;

        float coeff_ratio = 0.f;

        if(version == CHANNELMIXERRGB_V_1)
        {
            for_three_channels(c)
                coeff_ratio += sqf(1.0f - output[c]) * saturation[c];
        }
        else
            coeff_ratio = scalar_product(output, saturation) / 3.f;

        for_three_channels(c)
        {
            const float min_ratio = (output[c] < 0.0f) ? output[c] : 0.0f;
            const float output_inverse = 1.0f - output[c];
            output[c] = fmaxf(DT_FMA(output_inverse, coeff_ratio, output[c]), min_ratio);
        }

        if(version == CHANNELMIXERRGB_V_3) norm /= euclidean_norm(output) * INVERSE_SQRT_3;

        norm *= fmaxf(1.f + mix / avg, 0.f);
        for_three_channels(c)
            output[c] *= norm;
    }
    else
    {
        for_three_channels(c)
            output[c] = input[c];
    }
}

/* ============================================================================
   From channelmixerrgb.c - dt_vector_max_nan, dt_vector_clipneg_nan, copy_pixel
   ============================================================================ */

static inline void dt_vector_max_nan(dt_aligned_pixel_t out, const float *in,
                                      const dt_aligned_pixel_t min_value)
{
    for_four_channels(c)
        out[c] = (isnan(in[c]) || in[c] < min_value[c]) ? min_value[c] : in[c];
}

static inline void dt_vector_clipneg_nan(dt_aligned_pixel_t out)
{
    for_four_channels(c)
        out[c] = (isnan(out[c]) || out[c] < 0.0f) ? 0.0f : out[c];
}

static inline void copy_pixel(dt_aligned_pixel_t out, const dt_aligned_pixel_t in)
{
    for_four_channels(c) out[c] = in[c];
}

/* ============================================================================
   From channelmixerrgb.c - _loop_switch (lines 771-985)
   LINEAR_BRADFORD path only
   ============================================================================ */

static inline void _loop_switch(const float *const in,
                                float *const out,
                                const size_t width,
                                const size_t height,
                                const size_t ch,
                                const dt_colormatrix_t XYZ_to_RGB,
                                const dt_colormatrix_t RGB_to_XYZ,
                                const dt_colormatrix_t MIX,
                                const dt_aligned_pixel_t illuminant,
                                const dt_aligned_pixel_t saturation,
                                const dt_aligned_pixel_t lightness,
                                const dt_aligned_pixel_t grey,
                                const float p,
                                const float gamut,
                                const int clip,
                                const int apply_grey,
                                const dt_adaptation_t kind,
                                const dt_iop_channelmixer_rgb_version_t version)
{
    dt_colormatrix_t RGB_to_LMS = { { 0.0f } };
    dt_colormatrix_t MIX_to_XYZ = { { 0.0f } };

    switch (kind)
    {
        case DT_ADAPTATION_FULL_BRADFORD:
        case DT_ADAPTATION_LINEAR_BRADFORD:
            make_RGB_to_Bradford_LMS(RGB_to_XYZ, RGB_to_LMS);
            make_Bradford_LMS_to_XYZ(MIX, MIX_to_XYZ);
            break;
        case DT_ADAPTATION_CAT16:
            make_RGB_to_CAT16_LMS(RGB_to_XYZ, RGB_to_LMS);
            make_CAT16_LMS_to_XYZ(MIX, MIX_to_XYZ);
            break;
        case DT_ADAPTATION_XYZ:
            dt_colormatrix_copy(RGB_to_LMS, RGB_to_XYZ);
            dt_colormatrix_copy(MIX_to_XYZ, MIX);
            break;
        case DT_ADAPTATION_RGB:
        case DT_ADAPTATION_LAST:
        default:
            dt_colormatrix_mul(MIX_to_XYZ, RGB_to_XYZ, MIX);
            break;
    }

    const float minval = clip ? 0.0f : -FLT_MAX;
    const dt_aligned_pixel_t min_value = { minval, minval, minval, minval };

    dt_colormatrix_t RGB_to_XYZ_trans;
    dt_colormatrix_transpose(RGB_to_XYZ_trans, RGB_to_XYZ);
    dt_colormatrix_t RGB_to_LMS_trans;
    dt_colormatrix_transpose(RGB_to_LMS_trans, RGB_to_LMS);
    dt_colormatrix_t MIX_to_XYZ_trans;
    dt_colormatrix_transpose(MIX_to_XYZ_trans, MIX_to_XYZ);
    dt_colormatrix_t XYZ_to_RGB_trans;
    dt_colormatrix_transpose(XYZ_to_RGB_trans, XYZ_to_RGB);

    for(size_t k = 0; k < height * width * 4; k += 4)
    {
        dt_aligned_pixel_t temp_one;
        dt_aligned_pixel_t temp_two;

        dt_vector_max_nan(temp_two, &in[k], min_value);

        switch(kind)
        {
            case DT_ADAPTATION_LINEAR_BRADFORD:
            {
                dt_apply_transposed_color_matrix(temp_two, RGB_to_LMS_trans, temp_one);
                bradford_adapt_D50(temp_one, illuminant, p, 0, temp_two);
                break;
            }
            case DT_ADAPTATION_FULL_BRADFORD:
            {
                dt_apply_transposed_color_matrix(temp_two, RGB_to_XYZ_trans, temp_one);
                const float Y = temp_one[1];
                convert_XYZ_to_bradford_LMS(temp_one, temp_two);
                temp_two[0] /= Y; temp_two[1] /= Y; temp_two[2] /= Y;
                bradford_adapt_D50(temp_two, illuminant, p, 1, temp_one);
                temp_one[0] *= Y; temp_one[1] *= Y; temp_one[2] *= Y;
                copy_pixel(temp_two, temp_one);
                break;
            }
            case DT_ADAPTATION_CAT16:
            {
                dt_apply_transposed_color_matrix(temp_two, RGB_to_LMS_trans, temp_one);
                CAT16_adapt_D50(temp_one, illuminant, 1.0f, 1, temp_two);
                break;
            }
            case DT_ADAPTATION_XYZ:
            {
                dt_apply_transposed_color_matrix(temp_two, RGB_to_XYZ_trans, temp_one);
                const dt_aligned_pixel_t D50 = { 0.9642119944211994f, 1.0f, 0.8251882845188288f, 0.f };
                temp_two[0] = temp_one[0] * D50[0] / illuminant[0];
                temp_two[1] = temp_one[1] * D50[1] / illuminant[1];
                temp_two[2] = temp_one[2] * D50[2] / illuminant[2];
                break;
            }
            case DT_ADAPTATION_RGB:
            case DT_ADAPTATION_LAST:
            default:
            {
                for_four_channels(c) temp_one[c] = 0.0f;
            }
        }

        dt_apply_transposed_color_matrix(temp_two, MIX_to_XYZ_trans, temp_one);

        if(clip) dt_vector_clipneg_nan(temp_one);
        _gamut_mapping(temp_one, gamut, clip, temp_two);

        switch(kind)
        {
            case DT_ADAPTATION_FULL_BRADFORD:
            case DT_ADAPTATION_LINEAR_BRADFORD:
            case DT_ADAPTATION_CAT16:
            case DT_ADAPTATION_XYZ:
                convert_any_XYZ_to_LMS(temp_two, temp_one, kind);
                break;
            case DT_ADAPTATION_RGB:
            case DT_ADAPTATION_LAST:
            default:
                dt_apply_transposed_color_matrix(temp_two, XYZ_to_RGB_trans, temp_one);
                break;
        }

        if(clip) dt_vector_clipneg_nan(temp_one);

        _luma_chroma(temp_one, saturation, lightness, temp_two, version);

        if(clip) dt_vector_clipneg_nan(temp_two);

        if(apply_grey)
        {
            const float grey_mix = fmaxf(scalar_product(temp_two, grey), 0.0f);
            temp_two[0] = temp_two[1] = temp_two[2] = grey_mix;
        }
        else
        {
            switch(kind)
            {
                case DT_ADAPTATION_FULL_BRADFORD:
                case DT_ADAPTATION_LINEAR_BRADFORD:
                case DT_ADAPTATION_CAT16:
                case DT_ADAPTATION_XYZ:
                    convert_any_LMS_to_XYZ(temp_two, temp_one, kind);
                    break;
                case DT_ADAPTATION_RGB:
                case DT_ADAPTATION_LAST:
                default:
                    dt_apply_transposed_color_matrix(temp_two, RGB_to_XYZ_trans, temp_one);
                    break;
            }

            if(clip) dt_vector_clipneg_nan(temp_one);

            dt_apply_transposed_color_matrix(temp_one, XYZ_to_RGB_trans, temp_two);

            if(clip) dt_vector_clipneg_nan(temp_two);
        }

        temp_two[3] = in[k + 3];
        out[k+0] = temp_two[0];
        out[k+1] = temp_two[1];
        out[k+2] = temp_two[2];
        out[k+3] = temp_two[3];
    }
}

/* ============================================================================
   Module data structure
   ============================================================================ */

typedef struct {
    dt_adaptation_t adaptation;
    dt_aligned_pixel_t illuminant;
    dt_colormatrix_t MIX;
    dt_aligned_pixel_t saturation;
    dt_aligned_pixel_t lightness;
    dt_aligned_pixel_t grey;
    float p;
    float gamut;
    int clip;
    int apply_grey;
    dt_iop_channelmixer_rgb_version_t version;
} ChannelMixerRGBData;

/* ============================================================================
   Process function
   ============================================================================ */

void channelmixerrgb_process(const float *in, float *out,
                              int width, int height,
                              const dt_colormatrix_t RGB_to_XYZ,
                              const dt_colormatrix_t XYZ_to_RGB,
                              const ChannelMixerRGBData *d)
{
    _loop_switch(in, out, width, height, 4,
                 XYZ_to_RGB, RGB_to_XYZ, d->MIX,
                 d->illuminant, d->saturation, d->lightness, d->grey,
                 d->p, d->gamut, d->clip, d->apply_grey,
                 d->adaptation, d->version);
}

/* ============================================================================
   Reset to defaults (from commit_params with default values)
   ============================================================================ */

void channelmixerrgb_reset(ChannelMixerRGBData *d)
{
    d->adaptation = DT_ADAPTATION_LINEAR_BRADFORD;

    /* D65 illuminant in Bradford LMS */
    d->illuminant[0] = 0.941238f;
    d->illuminant[1] = 1.040633f;
    d->illuminant[2] = 1.088791f;
    d->illuminant[3] = 0.0f;

    /* Identity MIX matrix */
    memset(d->MIX, 0, sizeof(d->MIX));
    d->MIX[0][0] = 1.0f;
    d->MIX[1][1] = 1.0f;
    d->MIX[2][2] = 1.0f;

    for(int i = 0; i < 4; i++) {
        d->saturation[i] = 0.0f;
        d->lightness[i] = 0.0f;
        d->grey[i] = 0.0f;
    }

    d->p = 1.0f;
    d->gamut = 1.0f;
    d->clip = 1;
    d->apply_grey = 0;
    d->version = CHANNELMIXERRGB_V_3;
}

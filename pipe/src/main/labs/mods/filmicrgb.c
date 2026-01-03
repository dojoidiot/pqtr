/*
 * filmicrgb module - copied from darktable
 * Uses filmic_v5 path (version=4 in DT enum)
 *
 * Runtime data dumped from DT for phase2.xmp
 */

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
   Inlined types (GPU shader compatible)
   ============================================================================ */

typedef float dt_colormatrix_t[4][4];
typedef float dt_aligned_pixel_t[4];

#ifndef SQF_DEFINED
#define SQF_DEFINED
static inline float sqf(const float x) { return x * x; }
#endif

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38f
#endif

/* ========== Local types for filmic (uses 3x4 matrices internally) ========== */
typedef float filmic_matrix_t[3][4];

/* ========== Constants ========== */
#define M_PI_F 3.14159265358979323846f
#define CIE_Y_1931_to_CIE_Y_2006(x) (1.05785528f * (x))

/* Filmlight RGB <-> LMS matrices (transposed) */
static const filmic_matrix_t filmlightRGB_D65_to_LMS_D65_trans
    = { { 0.95f, 0.05f, 0.00f, 0.f },
        { 0.38f, 0.62f, 0.00f, 0.f },
        { 0.00f, 0.03f, 0.97f, 0.f } };

static const filmic_matrix_t LMS_D65_to_filmlightRGB_D65_trans
    = { {  1.08771930f, -0.0877193f,          0.f, 0.f },
        { -0.66666667f,  1.66666667f,         0.f, 0.f },
        {  0.02061856f, -0.05154639f, 1.03092784f, 0.f } };

/* D65 white point in Yrg space */
static const float D65_r = 0.21902143f;
static const float D65_g = 0.54371398f;

/* ============================================================================
   Rec2020 work profile matrices (for scene -> display conversion)
   From DT colorspaces.c Rec2020 primaries
   ============================================================================ */

static const dt_colormatrix_t FILMIC_INPUT_MATRIX_TRANS = {
    { 0.406808585f, 0.067756809f, 0.022140555f, 0.f },
    { 0.617819786f, 0.748962402f, -0.015321350f, 0.f },
    { 0.045817729f, 0.100109629f, 0.587274075f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t FILMIC_OUTPUT_MATRIX = {
    { 2.837817192f, -2.337296247f, 0.177027255f, 0.f },
    { -0.241587654f, 1.529518247f, -0.241881117f, 0.f },
    { -0.113289982f, 0.128020823f, 1.689797878f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t FILMIC_OUTPUT_MATRIX_TRANS = {
    { 2.837817192f, -0.241587654f, -0.113289982f, 0.f },
    { -2.337296247f, 1.529518247f, 0.128020823f, 0.f },
    { 0.177027255f, -0.241881117f, 1.689797878f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

/* Export profile matrices (sRGB target) */
static const dt_colormatrix_t FILMIC_EXPORT_INPUT_MATRIX_TRANS = {
    { 0.298672199f, 0.095901854f, 0.022459989f, 0.f },
    { 0.706104636f, 0.719828308f, 0.044898711f, 0.f },
    { 0.065669231f, 0.101098664f, 0.526734650f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t FILMIC_EXPORT_OUTPUT_MATRIX = {
    { 4.862406731f, -4.789227962f, 0.313011587f, 0.f },
    { -0.626189709f, 2.022818327f, -0.310180575f, 0.f },
    { -0.153957039f, 0.031788439f, 1.911581993f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

static const dt_colormatrix_t FILMIC_EXPORT_OUTPUT_MATRIX_TRANS = {
    { 4.862406731f, -0.626189709f, -0.153957039f, 0.f },
    { -4.789227962f, 2.022818327f, 0.031788439f, 0.f },
    { 0.313011587f, -0.310180575f, 1.911581993f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

/* ========== Spline type enum ========== */
typedef enum {
    DT_FILMIC_CURVE_POLY_4 = 0,
    DT_FILMIC_CURVE_POLY_3 = 1,
    DT_FILMIC_CURVE_RATIONAL = 2
} dt_iop_filmicrgb_curve_type_t;

/* ========== Spline data ========== */
typedef struct {
    dt_aligned_pixel_t M1, M2, M3, M4, M5;
    float latitude_min, latitude_max;
    float y[5];
    float x[5];
    dt_iop_filmicrgb_curve_type_t type[2];
} dt_iop_filmic_rgb_spline_t;

/* ========== Module data ========== */
typedef struct {
    float grey_source;
    float black_source;
    float white_source;
    float dynamic_range;
    float normalize;
    float output_power;
    float contrast;
    float saturation;
    float sigma_toe, sigma_shoulder;
    dt_iop_filmic_rgb_spline_t spline;
} FilmicRGBData;

/* ========== Helper macros ========== */
#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

/* sqf and FLT_MAX are in types.h */
static inline float clamp_simd(float x) { return CLAMP(x, 0.0f, 1.0f); }

#ifndef DT_FAST_HYPOTF_DEFINED
#define DT_FAST_HYPOTF_DEFINED
static inline float dt_fast_hypotf(float a, float b)
{
    return sqrtf(a * a + b * b);
}
#endif

static inline float max3f(const dt_aligned_pixel_t p)
{
    return MAX(MAX(p[0], p[1]), p[2]);
}

/* ========== Matrix operations ========== */
#ifndef DT_APPLY_TRANSPOSED_DEFINED
#define DT_APPLY_TRANSPOSED_DEFINED
static inline void dt_apply_transposed_color_matrix(const dt_aligned_pixel_t in,
                                                     const dt_colormatrix_t matrix_trans,
                                                     dt_aligned_pixel_t out)
{
    out[0] = matrix_trans[0][0] * in[0] + matrix_trans[1][0] * in[1] + matrix_trans[2][0] * in[2];
    out[1] = matrix_trans[0][1] * in[0] + matrix_trans[1][1] * in[1] + matrix_trans[2][1] * in[2];
    out[2] = matrix_trans[0][2] * in[0] + matrix_trans[1][2] * in[1] + matrix_trans[2][2] * in[2];
    out[3] = 0.f;
}
#endif

/* ========== Filmlight RGB <-> LMS ========== */
#ifndef GRADING_RGB_LMS_DEFINED
#define GRADING_RGB_LMS_DEFINED
static inline void gradingRGB_to_LMS(const dt_aligned_pixel_t RGB, dt_aligned_pixel_t LMS)
{
    dt_apply_transposed_color_matrix(RGB, filmlightRGB_D65_to_LMS_D65_trans, LMS);
}

static inline void LMS_to_gradingRGB(const dt_aligned_pixel_t LMS, dt_aligned_pixel_t RGB)
{
    dt_apply_transposed_color_matrix(LMS, LMS_D65_to_filmlightRGB_D65_trans, RGB);
}
#endif

/* ========== LMS <-> Yrg ========== */
#ifndef LMS_YRG_DEFINED
#define LMS_YRG_DEFINED
static inline void LMS_to_Yrg(const dt_aligned_pixel_t LMS, dt_aligned_pixel_t Yrg)
{
    const float Y = 0.68990272f * LMS[0] + 0.34832189f * LMS[1];
    const float a = LMS[0] + LMS[1] + LMS[2];
    dt_aligned_pixel_t lms = { 0.f };
    for (int c = 0; c < 4; c++) lms[c] = (a == 0.f) ? 0.f : LMS[c] / a;

    dt_aligned_pixel_t rgb = { 0.f };
    LMS_to_gradingRGB(lms, rgb);

    Yrg[0] = Y;
    Yrg[1] = rgb[0];
    Yrg[2] = rgb[1];
}

static inline void Yrg_to_LMS(const dt_aligned_pixel_t Yrg, dt_aligned_pixel_t LMS)
{
    const float Y = Yrg[0];
    const float r = Yrg[1];
    const float g = Yrg[2];
    const float b = 1.f - r - g;
    const dt_aligned_pixel_t rgb = { r, g, b, 0.f };

    dt_aligned_pixel_t lms = { 0.f };
    gradingRGB_to_LMS(rgb, lms);

    const float denom = (0.68990272f * lms[0] + 0.34832189f * lms[1]);
    const float a = (denom == 0.f) ? 0.f : Y / denom;
    for (int c = 0; c < 4; c++) LMS[c] = lms[c] * a;
}
#endif

/* ========== Yrg <-> Ych ========== */
#ifndef YRG_YCH_DEFINED
#define YRG_YCH_DEFINED
static inline void Yrg_to_Ych(const dt_aligned_pixel_t Yrg, dt_aligned_pixel_t Ych)
{
    const float Y = Yrg[0];
    const float r = Yrg[1] - D65_r;
    const float g = Yrg[2] - D65_g;
    const float c = dt_fast_hypotf(g, r);
    const float cos_h = c != 0.f ? r / c : 1.f;
    const float sin_h = c != 0.f ? g / c : 0.f;
    Ych[0] = Y;
    Ych[1] = c;
    Ych[2] = cos_h;
    Ych[3] = sin_h;
}

static inline void Ych_to_Yrg(const dt_aligned_pixel_t Ych, dt_aligned_pixel_t Yrg)
{
    const float Y = Ych[0];
    const float c = Ych[1];
    const float cos_h = Ych[2];
    const float sin_h = Ych[3];
    const float r = c * cos_h + D65_r;
    const float g = c * sin_h + D65_g;
    Yrg[0] = Y;
    Yrg[1] = r;
    Yrg[2] = g;
}
#endif

/* ========== RGB <-> Ych ========== */
static inline void RGB_to_Ych(const dt_aligned_pixel_t in,
                               const dt_colormatrix_t matrix_trans,
                               dt_aligned_pixel_t out)
{
    dt_aligned_pixel_t LMS = { 0.f };
    dt_aligned_pixel_t Yrg = { 0.f };
    dt_apply_transposed_color_matrix(in, matrix_trans, LMS);
    LMS_to_Yrg(LMS, Yrg);
    Yrg_to_Ych(Yrg, out);
}

static inline void Ych_to_RGB(const dt_aligned_pixel_t in,
                               const dt_colormatrix_t matrix_trans,
                               dt_aligned_pixel_t out)
{
    dt_aligned_pixel_t LMS = { 0.f };
    dt_aligned_pixel_t Yrg = { 0.f };
    Ych_to_Yrg(in, Yrg);
    Yrg_to_LMS(Yrg, LMS);
    dt_apply_transposed_color_matrix(LMS, matrix_trans, out);
}

/* ========== Gamut checking ========== */
#ifndef GAMUT_CHECK_YRG_DEFINED
#define GAMUT_CHECK_YRG_DEFINED
static inline void gamut_check_Yrg(dt_aligned_pixel_t Ych)
{
    dt_aligned_pixel_t Yrg = { 0.f };
    Ych_to_Yrg(Ych, Yrg);

    float max_c = Ych[1];
    const float cos_h = Ych[2];
    const float sin_h = Ych[3];

    if(Yrg[1] < 0.f) max_c = fminf(-D65_r / cos_h, max_c);
    if(Yrg[2] < 0.f) max_c = fminf(-D65_g / sin_h, max_c);
    if(Yrg[1] + Yrg[2] > 1.f) max_c = fminf((1.f - D65_r - D65_g) / (cos_h + sin_h), max_c);

    Ych[1] = max_c;
}
#endif

static inline float _clip_chroma_white_raw(const float coeffs[3], const float target_white, const float Y,
                                           const float cos_h, const float sin_h)
{
    const float denominator_Y_coeff = coeffs[0] * (0.979381443298969f * cos_h + 0.391752577319588f * sin_h)
                                      + coeffs[1] * (0.0206185567010309f * cos_h + 0.608247422680412f * sin_h)
                                      - coeffs[2] * (cos_h + sin_h);
    const float denominator_target_term = target_white * (0.68285981628866f * cos_h + 0.482137060515464f * sin_h);

    if(denominator_Y_coeff == 0.f) return FLT_MAX;
    const float Y_asymptote = denominator_target_term / denominator_Y_coeff;
    if(Y <= Y_asymptote) return FLT_MAX;

    const float denominator = Y * denominator_Y_coeff - denominator_target_term;
    const float numerator = -0.427506877216495f
                            * (Y * (coeffs[0] + 0.856492345150334f * coeffs[1] + 0.554995960637719f * coeffs[2])
                               - 0.988237752433297f * target_white);
    return numerator / denominator;
}

static inline float _clip_chroma_white(const float coeffs[3], const float target_white, const float Y,
                                       const float cos_h, const float sin_h)
{
    const float eps = 1e-3f;
    const float max_Y = CIE_Y_1931_to_CIE_Y_2006(target_white);
    const float delta_Y = MAX(max_Y - Y, 0.f);
    float max_chroma;
    if(delta_Y < eps)
        max_chroma = delta_Y / (eps * max_Y) * _clip_chroma_white_raw(coeffs, target_white, (1.f - eps) * max_Y, cos_h, sin_h);
    else
        max_chroma = _clip_chroma_white_raw(coeffs, target_white, Y, cos_h, sin_h);
    return max_chroma >= 0.f ? max_chroma : FLT_MAX;
}

static inline float _clip_chroma_black(const float coeffs[3], const float cos_h, const float sin_h)
{
    const float denominator = coeffs[0] * (0.979381443298969f * cos_h + 0.391752577319588f * sin_h)
                              + coeffs[1] * (0.0206185567010309f * cos_h + 0.608247422680412f * sin_h)
                              - coeffs[2] * (cos_h + sin_h);
    if(denominator == 0.f) return FLT_MAX;
    const float numerator = -0.427506877216495f * (coeffs[0] + 0.856492345150334f * coeffs[1] + 0.554995960637719f * coeffs[2]);
    const float max_chroma = numerator / denominator;
    return max_chroma >= 0.f ? max_chroma : FLT_MAX;
}

static inline float Ych_max_chroma_without_negatives(const dt_colormatrix_t matrix_out,
                                                      const float cos_h, const float sin_h)
{
    const float chroma_R_black = _clip_chroma_black(matrix_out[0], cos_h, sin_h);
    const float chroma_G_black = _clip_chroma_black(matrix_out[1], cos_h, sin_h);
    const float chroma_B_black = _clip_chroma_black(matrix_out[2], cos_h, sin_h);
    return MIN(MIN(chroma_R_black, chroma_G_black), chroma_B_black);
}

static inline float Ych_max_chroma(const dt_colormatrix_t matrix_out, const float target_white, const float Y,
                                   const float cos_h, const float sin_h)
{
    const float chroma_R_white = _clip_chroma_white(matrix_out[0], target_white, Y, cos_h, sin_h);
    const float chroma_G_white = _clip_chroma_white(matrix_out[1], target_white, Y, cos_h, sin_h);
    const float chroma_B_white = _clip_chroma_white(matrix_out[2], target_white, Y, cos_h, sin_h);
    const float max_chroma_white = MIN(MIN(chroma_R_white, chroma_G_white), chroma_B_white);
    const float max_chroma_black = Ych_max_chroma_without_negatives(matrix_out, cos_h, sin_h);
    return MIN(max_chroma_black, max_chroma_white);
}

static inline void gamut_check_RGB(const dt_colormatrix_t matrix_in_trans,
                                   const dt_colormatrix_t matrix_out,
                                   const dt_colormatrix_t matrix_out_trans,
                                   const float display_black,
                                   const float display_white,
                                   const dt_aligned_pixel_t Ych_in,
                                   dt_aligned_pixel_t RGB_out)
{
    dt_aligned_pixel_t RGB_brightened = { 0.f };
    Ych_to_RGB(Ych_in, matrix_out_trans, RGB_brightened);
    const float min_pix = MIN(MIN(RGB_brightened[0], RGB_brightened[1]), RGB_brightened[2]);
    const float black_offset = MAX(-min_pix, 0.f);
    for (int c = 0; c < 4; c++) RGB_brightened[c] += black_offset;
    dt_aligned_pixel_t Ych_brightened = { 0.f };
    RGB_to_Ych(RGB_brightened, matrix_in_trans, Ych_brightened);

    const float Y = CLAMP((Ych_in[0] + Ych_brightened[0]) / 2.f,
                          CIE_Y_1931_to_CIE_Y_2006(display_black),
                          CIE_Y_1931_to_CIE_Y_2006(display_white));

    const float cos_h = Ych_in[2];
    const float sin_h = Ych_in[3];
    const float new_chroma = MIN(Ych_in[1], Ych_max_chroma(matrix_out, display_white, Y, cos_h, sin_h));

    const dt_aligned_pixel_t Ych = { Y, new_chroma, cos_h, sin_h };
    Ych_to_RGB(Ych, matrix_out_trans, RGB_out);

    for (int c = 0; c < 4; c++) RGB_out[c] = CLAMP(RGB_out[c], 0.f, display_white);
}

/* ========== Gamut mapping ========== */
static inline void gamut_mapping(dt_aligned_pixel_t Ych_final,
                                  dt_aligned_pixel_t Ych_original,
                                  dt_aligned_pixel_t pix_out,
                                  const dt_colormatrix_t input_matrix_trans,
                                  const dt_colormatrix_t output_matrix,
                                  const dt_colormatrix_t output_matrix_trans,
                                  const dt_colormatrix_t export_input_matrix_trans,
                                  const dt_colormatrix_t export_output_matrix,
                                  const dt_colormatrix_t export_output_matrix_trans,
                                  const float display_black,
                                  const float display_white,
                                  const float saturation,
                                  const int use_output_profile)
{
    /* Force final hue to original */
    Ych_final[2] = Ych_original[2];
    Ych_final[3] = Ych_original[3];

    /* Clip luminance */
    Ych_final[0] = CLAMP(Ych_final[0],
                         CIE_Y_1931_to_CIE_Y_2006(display_black),
                         CIE_Y_1931_to_CIE_Y_2006(display_white));

    /* filmic_desaturate_v4 - but saturation=0 in V5, so skip */
    /* Note: for v5, saturation is passed as 0, so delta_chroma=0, no change */

    gamut_check_Yrg(Ych_final);

    if(!use_output_profile)
    {
        gamut_check_RGB(input_matrix_trans, output_matrix, output_matrix_trans,
                        display_black, display_white, Ych_final, pix_out);
    }
    else
    {
        gamut_check_RGB(export_input_matrix_trans, export_output_matrix, export_output_matrix_trans,
                        display_black, display_white, Ych_final, pix_out);

        dt_aligned_pixel_t LMS = { 0.f };
        dt_apply_transposed_color_matrix(pix_out, export_input_matrix_trans, LMS);
        dt_apply_transposed_color_matrix(LMS, output_matrix_trans, pix_out);
    }
}

/* ========== Filmic core functions ========== */
static inline float exp_tonemapping_v2(const float x, const float grey, const float black,
                                        const float dynamic_range)
{
    return grey * exp2f(dynamic_range * x + black);
}

static inline float log_tonemapping_v2_1ch(const float x, const float grey, const float black,
                                            const float dynamic_range)
{
    return clamp_simd((log2f(x / grey) - black) / dynamic_range);
}

static inline void log_tonemapping_v2(dt_aligned_pixel_t mapped, const dt_aligned_pixel_t x,
                                       const float grey, const float black, const float dynamic_range)
{
    for (int c = 0; c < 4; c++) {
        float scaled = x[c] / grey;
        float log_val = log2f(scaled);
        mapped[c] = CLAMP((log_val - black) / dynamic_range, 0.f, 1.f);
    }
}

static inline float filmic_spline(const float x, const dt_aligned_pixel_t M1, const dt_aligned_pixel_t M2,
                                   const dt_aligned_pixel_t M3, const dt_aligned_pixel_t M4,
                                   const dt_aligned_pixel_t M5, const float latitude_min,
                                   const float latitude_max, const dt_iop_filmicrgb_curve_type_t type[2])
{
    float result;

    if(x < latitude_min)
    {
        if(type[0] == DT_FILMIC_CURVE_POLY_4)
            result = M1[0] + x * (M2[0] + x * (M3[0] + x * (M4[0] + x * M5[0])));
        else if(type[0] == DT_FILMIC_CURVE_POLY_3)
            result = M1[0] + x * (M2[0] + x * (M3[0] + x * M4[0]));
        else
        {
            const float xi = latitude_min - x;
            const float rat = xi * (xi * M2[0] + 1.f);
            result = M4[0] - M1[0] * rat / (rat + M3[0]);
        }
    }
    else if(x > latitude_max)
    {
        if(type[1] == DT_FILMIC_CURVE_POLY_4)
            result = M1[1] + x * (M2[1] + x * (M3[1] + x * (M4[1] + x * M5[1])));
        else if(type[1] == DT_FILMIC_CURVE_POLY_3)
            result = M1[1] + x * (M2[1] + x * (M3[1] + x * M4[1]));
        else
        {
            const float xi = x - latitude_max;
            const float rat = xi * (xi * M2[1] + 1.f);
            result = M4[1] + M1[1] * rat / (rat + M3[1]);
        }
    }
    else
    {
        result = M1[2] + x * M2[2];
    }

    return result;
}

static inline void RGB_tone_mapping_v4(const dt_aligned_pixel_t pix_in,
                                        dt_aligned_pixel_t pix_out,
                                        const FilmicRGBData *data,
                                        const float display_black,
                                        const float display_white)
{
    dt_aligned_pixel_t mapped;
    log_tonemapping_v2(mapped, pix_in, data->grey_source, data->black_source, data->dynamic_range);
    for(int c = 0; c < 3; c++)
    {
        mapped[c] = filmic_spline(mapped[c], data->spline.M1, data->spline.M2, data->spline.M3,
                                  data->spline.M4, data->spline.M5, data->spline.latitude_min,
                                  data->spline.latitude_max, data->spline.type);
    }
    for(int c = 0; c < 4; c++)
        mapped[c] = CLAMP(mapped[c], 0.0f, display_white);
    for(int c = 0; c < 4; c++)
        pix_out[c] = powf(mapped[c], data->output_power);
}

static inline void norm_tone_mapping_v4(const dt_aligned_pixel_t pix_in,
                                         dt_aligned_pixel_t pix_out,
                                         const FilmicRGBData *data,
                                         const float norm_min, const float norm_max,
                                         const float display_black, const float display_white)
{
    /* get_pixel_norm for MAX_RGB variant */
    float norm = CLAMP(max3f(pix_in), norm_min, norm_max);

    /* Save the ratios */
    dt_aligned_pixel_t ratios = { 0.0f };
    for(int c = 0; c < 4; c++) ratios[c] = pix_in[c] / norm;

    /* Log tone-mapping */
    norm = log_tonemapping_v2_1ch(norm, data->grey_source, data->black_source, data->dynamic_range);

    /* Filmic S curve + output power */
    norm = powf(CLAMP(filmic_spline(norm, data->spline.M1, data->spline.M2, data->spline.M3,
                                    data->spline.M4, data->spline.M5, data->spline.latitude_min,
                                    data->spline.latitude_max, data->spline.type),
                      display_black, display_white),
                data->output_power);

    /* Restore RGB */
    for(int c = 0; c < 4; c++) pix_out[c] = ratios[c] * norm;
}

/* ========== Main process function (filmic_v5 path) ========== */
void filmicrgb_process(const float *in, float *out, int width, int height,
                        const FilmicRGBData *data,
                        const dt_colormatrix_t input_matrix_trans,
                        const dt_colormatrix_t output_matrix,
                        const dt_colormatrix_t output_matrix_trans,
                        const dt_colormatrix_t export_input_matrix_trans,
                        const dt_colormatrix_t export_output_matrix,
                        const dt_colormatrix_t export_output_matrix_trans,
                        const float display_black, const float display_white,
                        const int use_output_profile)
{
    const float norm_min = exp_tonemapping_v2(0.f, data->grey_source, data->black_source, data->dynamic_range);
    const float norm_max = exp_tonemapping_v2(1.f, data->grey_source, data->black_source, data->dynamic_range);

    for(size_t k = 0; k < (size_t)height * width * 4; k += 4)
    {
        const float *pix_in = in + k;

        dt_aligned_pixel_t max_rgb = { 0.f };
        dt_aligned_pixel_t naive_rgb = { 0.f };

        RGB_tone_mapping_v4(pix_in, naive_rgb, data, display_black, display_white);
        norm_tone_mapping_v4(pix_in, max_rgb, data, norm_min, norm_max, display_black, display_white);

        /* Mix max RGB with naive RGB (saturation=0 for v5) */
        dt_aligned_pixel_t pix_out;
        for(int c = 0; c < 4; c++)
            pix_out[c] = 0.5f * naive_rgb[c] + 0.5f * max_rgb[c];


        /* Save Ych in Kirk/Filmlight Yrg */
        dt_aligned_pixel_t Ych_original = { 0.f };
        RGB_to_Ych(pix_in, input_matrix_trans, Ych_original);

        /* Get final Ych in Kirk/Filmlight Yrg */
        dt_aligned_pixel_t Ych_final = { 0.f };
        RGB_to_Ych(pix_out, input_matrix_trans, Ych_final);

        Ych_final[1] = fminf(Ych_original[1], Ych_final[1]);


        gamut_mapping(Ych_final, Ych_original, pix_out, input_matrix_trans, output_matrix, output_matrix_trans,
                      export_input_matrix_trans, export_output_matrix, export_output_matrix_trans,
                      display_black, display_white, 0.0f, use_output_profile);

        for(int c = 0; c < 4; c++) out[k + c] = pix_out[c];
    }
}

/* ========== Default/reset function ========== */
void filmicrgb_reset(FilmicRGBData *data)
{
    /* Runtime data dumped from DT for phase2.xmp:
     * version=4 (DT_FILMIC_COLORSCIENCE_V5)
     * preserve_color=3 (POWER_NORM) - but v5 uses MAX_RGB variant
     * spline_version=2
     */
    data->grey_source = 0.184499994f;
    data->black_source = -5.000000000f;
    data->white_source = 0.000000000f;
    data->dynamic_range = 8.199999809f;
    data->normalize = 9.436863899f;
    data->output_power = 3.416451693f;
    data->contrast = 1.499999762f;
    data->saturation = 0.000000000f;
    data->sigma_toe = 0.041306622f;
    data->sigma_shoulder = 0.016918922f;

    /* Spline data */
    data->spline.x[0] = 0.000000000f;
    data->spline.x[1] = 0.609720886f;
    data->spline.x[2] = 0.609756112f;
    data->spline.x[3] = 0.609781742f;
    data->spline.x[4] = 1.000000000f;

    data->spline.y[0] = 0.076246955f;
    data->spline.y[1] = 0.609703720f;
    data->spline.y[2] = 0.609756112f;
    data->spline.y[3] = 0.609794259f;
    data->spline.y[4] = 1.000000000f;

    data->spline.latitude_min = 0.609720886f;
    data->spline.latitude_max = 0.609781742f;
    data->spline.type[0] = DT_FILMIC_CURVE_POLY_4;
    data->spline.type[1] = DT_FILMIC_CURVE_POLY_4;

    data->spline.M1[0] = 0.076246955f;
    data->spline.M1[1] = 0.331994981f;
    data->spline.M1[2] = -0.297136068f;
    data->spline.M1[3] = 0.000000000f;

    data->spline.M2[0] = 0.000000000f;
    data->spline.M2[1] = -1.511357784f;
    data->spline.M2[2] = 1.487303138f;
    data->spline.M2[3] = 0.000000000f;

    data->spline.M3[0] = 1.291752219f;
    data->spline.M3[1] = 4.600980282f;
    data->spline.M3[2] = 0.000000000f;
    data->spline.M3[3] = 0.000000000f;

    data->spline.M4[0] = 1.175917983f;
    data->spline.M4[1] = -1.995867610f;
    data->spline.M4[2] = 0.000000000f;
    data->spline.M4[3] = 0.000000000f;

    data->spline.M5[0] = -1.543424726f;
    data->spline.M5[1] = -0.425750077f;
    data->spline.M5[2] = 0.000000000f;
    data->spline.M5[3] = 0.000000000f;
}

/* ============================================================================
   Helper: return FilmicRGBData with defaults (V5 tone mapping)
   ============================================================================ */

static inline FilmicRGBData filmicrgb_defaults(void)
{
    FilmicRGBData d;
    filmicrgb_reset(&d);
    return d;
}

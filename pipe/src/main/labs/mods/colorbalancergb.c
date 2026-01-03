/*
 * colorbalancergb.c - Color balance RGB module
 * Copied from darktable src/iop/colorbalancergb.c
 * All dependencies inlined. Each module is independent.
 */

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <string.h>
#include <float.h>

#define M_PI_F 3.14159265358979324f
#define LUT_ELEM 512  /* Must match DT's value for bitmask to work */
#define DT_UCS_L_STAR_RANGE 2.098883786377f
#define DT_UCS_L_STAR_UPPER_LIMIT 2.09885f

typedef float dt_aligned_pixel_t[4];
typedef float dt_colormatrix_t[4][4];

/* ============ Math helpers ============ */

#ifndef SQF_DEFINED
#define SQF_DEFINED
static inline float sqf(float x) { return x * x; }
#endif

#ifndef DT_FAST_HYPOTF_DEFINED
#define DT_FAST_HYPOTF_DEFINED
static inline float dt_fast_hypotf(float x, float y)
{
    return sqrtf(x * x + y * y);
}
#endif

#ifndef SCALAR_PRODUCT_DEFINED
#define SCALAR_PRODUCT_DEFINED
static inline float scalar_product(const float a[4], const float b[4])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
#endif

#ifndef CLAMP
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#endif
#ifndef CLAMPF
#define CLAMPF(x, low, high) CLAMP(x, low, high)
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

/* ============ Matrix operations ============ */

#ifndef DT_COLORMATRIX_MUL_DEFINED
#define DT_COLORMATRIX_MUL_DEFINED
static inline void dt_colormatrix_mul(dt_colormatrix_t dst, const dt_colormatrix_t m1, const dt_colormatrix_t m2)
{
    for(int k = 0; k < 3; k++)
    {
        for(int i = 0; i < 3; i++)
        {
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
static inline void dt_colormatrix_transpose(dt_colormatrix_t dst, const dt_colormatrix_t src)
{
    for(int i = 0; i < 3; i++)
        for(int j = 0; j < 3; j++)
            dst[i][j] = src[j][i];
    for(int i = 0; i < 4; i++) dst[i][3] = dst[3][i] = 0.0f;
}
#endif

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

static inline void dot_product(const dt_aligned_pixel_t in,
                               const dt_colormatrix_t matrix,
                               dt_aligned_pixel_t out)
{
    for(int i = 0; i < 3; i++)
        out[i] = matrix[i][0] * in[0] + matrix[i][1] * in[1] + matrix[i][2] * in[2];
    out[3] = 0.0f;
}

/* ============ Color matrices ============ */

static const dt_colormatrix_t XYZ_D50_to_D65_CAT16
    = { { 9.89466254e-01f, -4.00304626e-02f, 4.40530317e-02f, 0.f },
        { -5.40518733e-03f, 1.00666069e+00f, -1.75551955e-03f, 0.f },
        { -4.03920992e-04f, 1.50768030e-02f, 1.30210211e+00f, 0.f },
        { 0.f, 0.f, 0.f, 0.f } };

static const dt_colormatrix_t XYZ_D65_to_D50_CAT16
    = { { 1.01085433e+00f, 4.07086103e-02f, -3.41445825e-02f, 0.f },
        { 5.42814201e-03f, 9.93581926e-01f, 1.15592039e-03f, 0.f },
        { 2.50722468e-04f, -1.14918759e-02f, 7.67964947e-01f, 0.f },
        { 0.f, 0.f, 0.f, 0.f } };

/* XYZ D65 to CIE 2006 LMS D65 */
static const dt_colormatrix_t XYZ_D65_to_LMS_2006_D65
    = { {  0.257085f,  0.859943f, -0.031061f, 0.f },
        { -0.394427f,  1.175800f,  0.106423f, 0.f },
        {  0.064856f, -0.076250f,  0.559067f, 0.f },
        { 0.f, 0.f, 0.f, 0.f } };

static const dt_colormatrix_t LMS_2006_D65_to_XYZ_D65
    = { {  1.80794659f, -1.29971660f,  0.34785879f, 0.f },
        {  0.61783960f,  0.39595453f, -0.04104687f, 0.f },
        { -0.12546960f,  0.20478038f,  1.74274183f, 0.f },
        { 0.f, 0.f, 0.f, 0.f } };

/* Filmlight grading RGB matrices (4x4 versions for colorbalancergb) */
static const dt_colormatrix_t filmlightRGB_D65_to_LMS_D65_4x4
    = { { 0.95f, 0.38f, 0.00f, 0.f },
        { 0.05f, 0.62f, 0.03f, 0.f },
        { 0.00f, 0.00f, 0.97f, 0.f },
        { 0.f, 0.f, 0.f, 0.f } };

static const dt_colormatrix_t filmlightRGB_D65_to_LMS_D65_trans_4x4
    = { { 0.95f, 0.05f, 0.00f, 0.f },
        { 0.38f, 0.62f, 0.00f, 0.f },
        { 0.00f, 0.03f, 0.97f, 0.f },
        { 0.f, 0.f, 0.f, 0.f } };

static const dt_colormatrix_t LMS_D65_to_filmlightRGB_D65_4x4
    = { {  1.0877193f, -0.66666667f,  0.02061856f, 0.f },
        { -0.0877193f,  1.66666667f, -0.05154639f, 0.f },
        {  0.0f,        0.0f,         1.03092784f, 0.f },
        { 0.f, 0.f, 0.f, 0.f } };

static const dt_colormatrix_t LMS_D65_to_filmlightRGB_D65_trans_4x4
    = { {  1.0877193f, -0.0877193f,  0.0f, 0.f },
        { -0.66666667f, 1.66666667f, 0.0f, 0.f },
        {  0.02061856f, -0.05154639f, 1.03092784f, 0.f },
        { 0.f, 0.f, 0.f, 0.f } };

/* ============ Color space conversions ============ */

#ifndef GRADING_RGB_LMS_DEFINED
#define GRADING_RGB_LMS_DEFINED
static inline void gradingRGB_to_LMS(const dt_aligned_pixel_t RGB, dt_aligned_pixel_t LMS)
{
    dt_apply_transposed_color_matrix(RGB, filmlightRGB_D65_to_LMS_D65_trans_4x4, LMS);
}

static inline void LMS_to_gradingRGB(const dt_aligned_pixel_t LMS, dt_aligned_pixel_t RGB)
{
    dt_apply_transposed_color_matrix(LMS, LMS_D65_to_filmlightRGB_D65_trans_4x4, RGB);
}
#endif

#ifndef LMS_YRG_DEFINED
#define LMS_YRG_DEFINED
static inline void LMS_to_Yrg(const dt_aligned_pixel_t LMS, dt_aligned_pixel_t Yrg)
{
    const float Y = 0.68990272f * LMS[0] + 0.34832189f * LMS[1];
    const float a = LMS[0] + LMS[1] + LMS[2];
    dt_aligned_pixel_t lms = { 0.f, 0.f, 0.f, 0.f };
    for(int c = 0; c < 3; c++) lms[c] = (a == 0.f) ? 0.f : LMS[c] / a;
    dt_aligned_pixel_t rgb = { 0.f, 0.f, 0.f, 0.f };
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
    dt_aligned_pixel_t lms = { 0.f, 0.f, 0.f, 0.f };
    gradingRGB_to_LMS(rgb, lms);
    const float denom = (0.68990272f * lms[0] + 0.34832189f * lms[1]);
    const float a = (denom == 0.f) ? 0.f : Y / denom;
    for(int c = 0; c < 3; c++) LMS[c] = lms[c] * a;
    LMS[3] = 0.f;
}

static inline void Yrg_to_Ych(const dt_aligned_pixel_t Yrg, dt_aligned_pixel_t Ych)
{
    const float Y = Yrg[0];
    const float r = Yrg[1] - 0.21902143f;
    const float g = Yrg[2] - 0.54371398f;
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
    const float r = c * cos_h + 0.21902143f;
    const float g = c * sin_h + 0.54371398f;
    Yrg[0] = Y;
    Yrg[1] = r;
    Yrg[2] = g;
}
#endif /* LMS_YRG_DEFINED */

static inline void make_Ych(const float Y, const float c, const float h, dt_aligned_pixel_t Ych)
{
    Ych[0] = Y;
    Ych[1] = c;
    Ych[2] = cosf(h);
    Ych[3] = sinf(h);
}

static inline void Ych_to_gradingRGB(const dt_aligned_pixel_t Ych, dt_aligned_pixel_t RGB)
{
    dt_aligned_pixel_t Yrg = { 0.f };
    dt_aligned_pixel_t LMS = { 0.f };
    Ych_to_Yrg(Ych, Yrg);
    Yrg_to_LMS(Yrg, LMS);
    LMS_to_gradingRGB(LMS, RGB);
}

static inline void LMS_to_XYZ(const dt_aligned_pixel_t LMS, dt_aligned_pixel_t XYZ)
{
    dot_product(LMS, LMS_2006_D65_to_XYZ_D65, XYZ);
}

/* ============ Gamut checking ============ */

#ifndef GAMUT_CHECK_YRG_DEFINED
#define GAMUT_CHECK_YRG_DEFINED
static inline void gamut_check_Yrg(dt_aligned_pixel_t Ych)
{
    /* Check if the color fits in Yrg and LMS cone space
       clip chroma at constant hue and luminance otherwise */

    /* Do a test conversion to Yrg */
    dt_aligned_pixel_t Yrg = { 0.f };
    Ych_to_Yrg(Ych, Yrg);

    /* Gamut-clip chroma in Yrg at constant hue and luminance
       e.g. find the max chroma value that fits in gamut at the current hue */
    const float D65_r = 0.21902143f;
    const float D65_g = 0.54371398f;

    float max_c = Ych[1];
    const float cos_h = Ych[2];
    const float sin_h = Ych[3];

    if(Yrg[1] < 0.f)
    {
        max_c = fminf(-D65_r / cos_h, max_c);
    }
    if(Yrg[2] < 0.f)
    {
        max_c = fminf(-D65_g / sin_h, max_c);
    }
    if(Yrg[1] + Yrg[2] > 1.f)
    {
        max_c = fminf((1.f - D65_r - D65_g) / (cos_h + sin_h), max_c);
    }

    /* Overwrite chroma with the sanitized value */
    Ych[1] = max_c;
}
#endif /* GAMUT_CHECK_YRG_DEFINED */

/* ============ XYZ/xyY conversions ============ */

static inline void dt_D65_XYZ_to_xyY(const dt_aligned_pixel_t XYZ, dt_aligned_pixel_t xyY)
{
    const float sum = XYZ[0] + XYZ[1] + XYZ[2];
    xyY[0] = (sum > 0.f) ? XYZ[0] / sum : 0.31272660439158345f; /* D65 x */
    xyY[1] = (sum > 0.f) ? XYZ[1] / sum : 0.32902315240275790f; /* D65 y */
    xyY[2] = XYZ[1];
}

#ifndef DT_XYY_TO_XYZ_DEFINED
#define DT_XYY_TO_XYZ_DEFINED
static inline void dt_xyY_to_XYZ(const dt_aligned_pixel_t xyY, dt_aligned_pixel_t XYZ)
{
    const float x = xyY[0];
    const float y = MAX(xyY[1], 1e-6f);
    const float Y = xyY[2];
    XYZ[0] = Y * x / y;
    XYZ[1] = Y;
    XYZ[2] = Y * (1.f - x - y) / y;
}
#endif

/* ============ darktable UCS 2022 ============ */

static inline float Y_to_dt_UCS_L_star(const float Y)
{
    const float Y_hat = powf(Y, 0.631651345306265f);
    return DT_UCS_L_STAR_RANGE * Y_hat / (Y_hat + 1.12426773749357f);
}

static inline float dt_UCS_L_star_to_Y(const float L_star)
{
    return powf((1.12426773749357f * L_star / (DT_UCS_L_STAR_RANGE - L_star)), 1.5831518565279648f);
}

static inline void xyY_to_dt_UCS_UV(const dt_aligned_pixel_t xyY, float UV_star_prime[2])
{
    const float x_factors[4] = { -0.783941002840055f,  0.745273540913283f, 0.318707282433486f, 0.f };
    const float y_factors[4] = {  0.277512987809202f, -0.205375866083878f, 2.16743692732158f,  0.f };
    const float offsets[4]   = {  0.153836578598858f, -0.165478376301988f, 0.291320554395942f, 0.f };

    float UVD[3] = { 0.f };
    for(int c = 0; c < 3; c++)
        UVD[c] = x_factors[c] * xyY[0] + y_factors[c] * xyY[1] + offsets[c];

    const float div = (UVD[2] >= 0.0f) ? MAX(FLT_MIN, UVD[2]) : MIN(-FLT_MIN, UVD[2]);
    UVD[0] /= div;
    UVD[1] /= div;

    float UV_star[2] = { 0.f };
    const float factors[2]     = { 1.39656225667f, 1.4513954287f };
    const float half_values[2] = { 1.49217352929f, 1.52488637914f };
    for(int c = 0; c < 2; c++)
        UV_star[c] = factors[c] * UVD[c] / (fabsf(UVD[c]) + half_values[c]);

    UV_star_prime[0] = -1.124983854323892f * UV_star[0] - 0.980483721769325f * UV_star[1];
    UV_star_prime[1] =  1.86323315098672f  * UV_star[0] + 1.971853092390862f * UV_star[1];
}

static inline void dt_UCS_LUV_to_JCH(const float L_star, const float L_white, const float UV_star_prime[2], dt_aligned_pixel_t JCH)
{
    const float M2 = UV_star_prime[0] * UV_star_prime[0] + UV_star_prime[1] * UV_star_prime[1];
    JCH[0] = L_star / L_white;
    JCH[1] = 15.932993652962535f * powf(L_star, 0.6523997524738018f) * powf(M2, 0.6007557017508491f) / L_white;
    JCH[2] = atan2f(UV_star_prime[1], UV_star_prime[0]);
}

static inline void xyY_to_dt_UCS_JCH(const dt_aligned_pixel_t xyY, const float L_white, dt_aligned_pixel_t JCH)
{
    float UV_star_prime[2];
    xyY_to_dt_UCS_UV(xyY, UV_star_prime);
    dt_UCS_LUV_to_JCH(Y_to_dt_UCS_L_star(xyY[2]), L_white, UV_star_prime, JCH);
}

static inline void dt_UCS_JCH_to_xyY(const dt_aligned_pixel_t JCH, const float L_white, dt_aligned_pixel_t xyY)
{
    const float L_star = CLAMPF(JCH[0] * L_white, 0.f, DT_UCS_L_STAR_UPPER_LIMIT);
    const float M = L_star != 0.f
        ? powf(JCH[1] * L_white / (15.932993652962535f * powf(L_star, 0.6523997524738018f)), 0.8322850678616855f)
        : 0.f;

    const float U_star_prime = M * cosf(JCH[2]);
    const float V_star_prime = M * sinf(JCH[2]);

    const float UV_star[2] = { -5.037522385190711f * U_star_prime - 2.504856328185843f * V_star_prime,
                                4.760029407436461f * U_star_prime + 2.874012963239247f * V_star_prime };

    float UV[2] = { 0.f };
    const float factors[2]     = { 1.39656225667f, 1.4513954287f };
    const float half_values[2] = { 1.49217352929f, 1.52488637914f };
    for(int c = 0; c < 2; c++)
        UV[c] = -half_values[c] * UV_star[c] / (fabsf(UV_star[c]) - factors[c]);

    const float U_factors[4] = {  0.167171472114775f,   -0.150959086409163f,    0.940254742367256f,  0.f };
    const float V_factors[4] = {  0.141299802443708f,   -0.155185060382272f,    1.000000000000000f,  0.f };
    const float offsets[4]   = { -0.00801531300850582f, -0.00843312433578007f, -0.0256325967652889f, 0.f };

    float xyD[4] = { 0.f };
    for(int c = 0; c < 3; c++)
        xyD[c] = U_factors[c] * UV[0] + V_factors[c] * UV[1] + offsets[c];

    const float div = (xyD[2] >= 0.0f) ? MAX(FLT_MIN, xyD[2]) : MIN(-FLT_MIN, xyD[2]);
    xyY[0] = xyD[0] / div;
    xyY[1] = xyD[1] / div;
    xyY[2] = dt_UCS_L_star_to_Y(L_star);
}

static inline void dt_UCS_JCH_to_HCB(const dt_aligned_pixel_t JCH, dt_aligned_pixel_t HCB)
{
    HCB[2] = JCH[0] * (powf(JCH[1], 1.33654221029386f) + 1.f);
    HCB[1] = JCH[1];
    HCB[0] = JCH[2];
}

static inline void dt_UCS_HCB_to_JCH(const dt_aligned_pixel_t HCB, dt_aligned_pixel_t JCH)
{
    JCH[2] = HCB[0];
    JCH[1] = HCB[1];
    JCH[0] = HCB[2] / (powf(HCB[1], 1.33654221029386f) + 1.f);
}

static inline void dt_UCS_JCH_to_HSB(const dt_aligned_pixel_t JCH, dt_aligned_pixel_t HSB)
{
    HSB[2] = JCH[0] * (powf(JCH[1], 1.33654221029386f) + 1.f);
    HSB[1] = (HSB[2] > 0.f) ? JCH[1] / HSB[2] : 0.f;
    HSB[0] = JCH[2];
}

static inline void dt_UCS_HSB_to_JCH(const dt_aligned_pixel_t HSB, dt_aligned_pixel_t JCH)
{
    JCH[2] = HSB[0];
    JCH[1] = HSB[1] * HSB[2];
    JCH[0] = HSB[2] / (powf(JCH[1], 1.33654221029386f) + 1.f);
}

/* ============ D65 white point ============ */

static const float D65_x = 0.31272660439158345f;
static const float D65_y = 0.32902315240275790f;

/* ============ Gamut LUT builder ============ */

static inline float Delta_H(const float h_1, const float h_2)
{
    float diff = h_1 - h_2;
    diff += (diff < -M_PI_F) ? 2.f * M_PI_F : 0.f;
    diff -= (diff > M_PI_F) ? 2.f * M_PI_F : 0.f;
    return diff;
}

static void build_gamut_LUT(const dt_colormatrix_t matrix_in, float gamut_LUT[LUT_ELEM])
{
    /* Build input matrix: RGB -> XYZ D65 */
    dt_colormatrix_t temp_matrix;
    dt_colormatrix_t input_matrix;
    dt_colormatrix_mul(temp_matrix, XYZ_D50_to_D65_CAT16, matrix_in);

    /* init LUT */
    for(size_t k = 0; k < LUT_ELEM; k++) gamut_LUT[k] = 0.f;
    float sampler[LUT_ELEM];
    for(size_t k = 0; k < LUT_ELEM; k++) sampler[k] = 0.f;

    dt_aligned_pixel_t D65_xyY = { D65_x, D65_y, 1.f, 0.f };

    /* Compute RGB primaries in xyY */
    dt_aligned_pixel_t RGB_red   = { 1.f, 0.f, 0.f, 0.f };
    dt_aligned_pixel_t RGB_green = { 0.f, 1.f, 0.f, 0.f };
    dt_aligned_pixel_t RGB_blue  = { 0.f, 0.f, 1.f, 0.f };

    dt_aligned_pixel_t XYZ_red, XYZ_green, XYZ_blue;
    dot_product(RGB_red, temp_matrix, XYZ_red);
    dot_product(RGB_green, temp_matrix, XYZ_green);
    dot_product(RGB_blue, temp_matrix, XYZ_blue);

    dt_aligned_pixel_t xyY_red, xyY_green, xyY_blue;
    dt_D65_XYZ_to_xyY(XYZ_red, xyY_red);
    dt_D65_XYZ_to_xyY(XYZ_green, xyY_green);
    dt_D65_XYZ_to_xyY(XYZ_blue, xyY_blue);

    const float h_red   = atan2f(xyY_red[1] - D65_xyY[1], xyY_red[0] - D65_xyY[0]);
    const float h_green = atan2f(xyY_green[1] - D65_xyY[1], xyY_green[0] - D65_xyY[0]);
    const float h_blue  = atan2f(xyY_blue[1] - D65_xyY[1], xyY_blue[0] - D65_xyY[0]);

    /* March gamut boundary */
    for(int i = 0; i < 50 * LUT_ELEM; i++)
    {
        const float angle = -M_PI_F + ((float)i) / (float)(50 * LUT_ELEM) * 2.f * M_PI_F;
        const float tan_angle = tanf(angle);

        const float t_1 = Delta_H(angle, h_blue)  / Delta_H(h_red, h_blue);
        const float t_2 = Delta_H(angle, h_red)   / Delta_H(h_green, h_red);
        const float t_3 = Delta_H(angle, h_green) / Delta_H(h_blue, h_green);

        float x_t = 0;
        float y_t = 0;

        if(t_1 >= 0.f && t_1 <= 1.f)
        {
            const float t = (D65_xyY[1] - xyY_blue[1] + tan_angle * (xyY_blue[0] - D65_xyY[0]))
                      / (xyY_red[1] - xyY_blue[1] + tan_angle * (xyY_blue[0] - xyY_red[0]));
            x_t = xyY_blue[0] + t * (xyY_red[0] - xyY_blue[0]);
            y_t = xyY_blue[1] + t * (xyY_red[1] - xyY_blue[1]);
        }
        else if(t_2 >= 0.f && t_2 <= 1.f)
        {
            const float t = (D65_xyY[1] - xyY_red[1] + tan_angle * (xyY_red[0] - D65_xyY[0]))
                      / (xyY_green[1] - xyY_red[1] + tan_angle * (xyY_red[0] - xyY_green[0]));
            x_t = xyY_red[0] + t * (xyY_green[0] - xyY_red[0]);
            y_t = xyY_red[1] + t * (xyY_green[1] - xyY_red[1]);
        }
        else if(t_3 >= 0.f && t_3 <= 1.f)
        {
            const float t = (D65_xyY[1] - xyY_green[1] + tan_angle * (xyY_green[0] - D65_xyY[0]))
                          / (xyY_blue[1] - xyY_green[1] + tan_angle * (xyY_green[0] - xyY_blue[0]));
            x_t = xyY_green[0] + t * (xyY_blue[0] - xyY_green[0]);
            y_t = xyY_green[1] + t * (xyY_blue[1] - xyY_green[1]);
        }

        dt_aligned_pixel_t xyY = { x_t, y_t, 1.f, 0.f };
        float UV_star_prime[2];
        xyY_to_dt_UCS_UV(xyY, UV_star_prime);

        const float hue = atan2f(UV_star_prime[1], UV_star_prime[0]);
        int index = (int)roundf((float)(LUT_ELEM - 1) * (hue + M_PI_F) / (2.f * M_PI_F));
        index += (index < 0) ? LUT_ELEM : 0;
        index -= (index >= LUT_ELEM) ? LUT_ELEM : 0;
        gamut_LUT[index] += UV_star_prime[0] * UV_star_prime[0] + UV_star_prime[1] * UV_star_prime[1];
        sampler[index] += 1.0f;
    }
    for(size_t k = 0; k < LUT_ELEM; k++)
        gamut_LUT[k] = gamut_LUT[k] / fmaxf(1.0f, sampler[k]);
}

/* ============ Gamut LUT lookup ============ */

static inline float lookup_gamut(const float gamut_lut[LUT_ELEM], const float hue)
{
    const float x_test = (float)LUT_ELEM * (hue + M_PI_F) / (2.f * M_PI_F);
    const float x_prev = floorf(x_test);
    const float x_next = ceilf(x_test);
    const int xi = (int)x_prev & (LUT_ELEM - 1);
    const int xii = (int)x_next & (LUT_ELEM - 1);
    const float y_prev = gamut_lut[xi];
    return y_prev + ((xi != xii) ? (x_test - x_prev) * (gamut_lut[xii] - y_prev) : 0.0f);
}

static inline float soft_clip(const float x, const float soft_threshold, const float hard_threshold)
{
    const float norm = hard_threshold - soft_threshold;
    return (x > soft_threshold) ? soft_threshold + (1.f - expf(-(x - soft_threshold) / norm)) * norm : x;
}

/* ============ Opacity masks ============ */

static inline void opacity_masks(const float x,
                                 const float shadows_weight, const float highlights_weight,
                                 const float midtones_weight, const float mask_grey_fulcrum,
                                 dt_aligned_pixel_t output, dt_aligned_pixel_t output_comp)
{
    const float x_offset = (x - mask_grey_fulcrum);
    const float x_offset_norm = x_offset / mask_grey_fulcrum;
    const float alpha = 1.f / (1.f + expf(x_offset_norm * shadows_weight));
    const float beta = 1.f / (1.f + expf(-x_offset_norm * highlights_weight));
    const float alpha_comp = 1.f - alpha;
    const float beta_comp = 1.f - beta;
    const float gamma = expf(-sqf(x_offset) * midtones_weight / 4.f) * sqf(alpha_comp) * sqf(beta_comp) * 8.f;
    const float gamma_comp = 1.f - gamma;

    output[0] = alpha;
    output[1] = gamma;
    output[2] = beta;
    output[3] = 0.f;

    if(output_comp)
    {
        output_comp[0] = alpha_comp;
        output_comp[1] = gamma_comp;
        output_comp[2] = beta_comp;
        output_comp[3] = 0.f;
    }
}

/* ============ Runtime data ============ */

typedef struct {
    float global[4];
    float shadows[4];
    float highlights[4];
    float midtones[4];
    float midtones_Y;
    float chroma_global, chroma[4], vibrance, contrast;
    float saturation_global, saturation[4];
    float brilliance_global, brilliance[4];
    float hue_angle;
    float shadows_weight, highlights_weight, midtones_weight, mask_grey_fulcrum;
    float white_fulcrum, grey_fulcrum;
    float gamut_LUT[LUT_ELEM];
    float max_chroma;
    int saturation_formula;
} ColorBalanceRGBData;

/* ============ Process function ============ */

void colorbalancergb_process(const float *in, float *out,
                             int width, int height,
                             const dt_colormatrix_t input_matrix,
                             const dt_colormatrix_t output_matrix,
                             const ColorBalanceRGBData *d)
{
    dt_colormatrix_t input_matrix_trans;
    dt_colormatrix_t output_matrix_trans;
    dt_colormatrix_transpose(input_matrix_trans, input_matrix);
    dt_colormatrix_transpose(output_matrix_trans, output_matrix);

    const float L_white = Y_to_dt_UCS_L_star(d->white_fulcrum);

    const float hue_rotation_matrix[2][2] = {
        { cosf(d->hue_angle), -sinf(d->hue_angle) },
        { sinf(d->hue_angle),  cosf(d->hue_angle) },
    };

    const size_t npixels = (size_t)height * width;


    for(size_t k = 0; k < 4 * npixels; k += 4)
    {
        /* clip pipeline RGB */
        dt_aligned_pixel_t RGB;
        RGB[0] = MAX(in[k+0], 0.f);
        RGB[1] = MAX(in[k+1], 0.f);
        RGB[2] = MAX(in[k+2], 0.f);
        RGB[3] = 0.f;

        /* go to CIE 2006 LMS D65 */
        dt_aligned_pixel_t LMS;
        dt_apply_transposed_color_matrix(RGB, input_matrix_trans, LMS);

        /* go to Filmlight Yrg */
        dt_aligned_pixel_t Yrg = { 0.f };
        LMS_to_Yrg(LMS, Yrg);

        /* go to Ych */
        dt_aligned_pixel_t Ych = { 0.f };
        Yrg_to_Ych(Yrg, Ych);

        /* Sanitize input : no negative luminance */
        Ych[0] = MAX(Ych[0], 0.f);

        /* Opacities for luma masks */
        dt_aligned_pixel_t opacities;
        dt_aligned_pixel_t opacities_comp;
        opacity_masks(powf(Ych[0], 0.4101205819200422f),
                      d->shadows_weight, d->highlights_weight, d->midtones_weight,
                      d->mask_grey_fulcrum, opacities, opacities_comp);

        /* Hue shift */
        const float cos_h = Ych[2];
        const float sin_h = Ych[3];
        Ych[2] = hue_rotation_matrix[0][0] * cos_h + hue_rotation_matrix[0][1] * sin_h;
        Ych[3] = hue_rotation_matrix[1][0] * cos_h + hue_rotation_matrix[1][1] * sin_h;

        /* Linear chroma */
        const float chroma_boost = d->chroma_global + scalar_product(opacities, d->chroma);
        const float vibrance = d->vibrance * (1.0f - powf(Ych[1], fabsf(d->vibrance)));
        const float chroma_factor = MAX(1.f + chroma_boost + vibrance, 0.f);
        Ych[1] *= chroma_factor;

        /* clip chroma at constant hue and Y if needed */
        gamut_check_Yrg(Ych);

        /* go to Yrg for real */
        Ych_to_Yrg(Ych, Yrg);

        /* Go to LMS */
        Yrg_to_LMS(Yrg, LMS);

        /* Go to Filmlight RGB */
        LMS_to_gradingRGB(LMS, RGB);

        /* Color balance */
        for(int c = 0; c < 4; c++)
            RGB[c] += d->global[c];

        for(int c = 0; c < 4; c++)
            RGB[c] *= opacities_comp[2] * (opacities_comp[0] + opacities[0] * d->shadows[c]) + opacities[2] * d->highlights[c];

        float sign[4];
        for(int c = 0; c < 4; c++)
            sign[c] = (RGB[c] < 0.f) ? -1.f : 1.f;

        float abs_RGB[4];
        for(int c = 0; c < 4; c++)
            abs_RGB[c] = fabsf(RGB[c]);

        float scaled_RGB[4];
        for(int c = 0; c < 4; c++)
            scaled_RGB[c] = abs_RGB[c] / d->white_fulcrum;

        for(int c = 0; c < 4; c++)
            RGB[c] = powf(scaled_RGB[c], d->midtones[c]) * sign[c] * d->white_fulcrum;

        /* for the non-linear ops we need to go in Yrg again */
        gradingRGB_to_LMS(RGB, LMS);
        LMS_to_Yrg(LMS, Yrg);

        /* Y midtones power (gamma) */
        Yrg[0] = powf(MAX(Yrg[0] / d->white_fulcrum, 0.f), d->midtones_Y) * d->white_fulcrum;

        /* Y fulcrumed contrast */
        Yrg[0] = d->grey_fulcrum * powf(Yrg[0] / d->grey_fulcrum, d->contrast);

        Yrg_to_LMS(Yrg, LMS);
        dt_aligned_pixel_t XYZ_D65 = { 0.f };
        LMS_to_XYZ(LMS, XYZ_D65);

        /* Perceptual color adjustments - DTUCS path (saturation_formula == 1) */
        if(d->saturation_formula == 0)
        {
            /* JzAzBz path - not implemented for now */
        }
        else
        {
            /* DTUCS path */
            dt_aligned_pixel_t xyY, JCH, HCB;
            dt_D65_XYZ_to_xyY(XYZ_D65, xyY);
            xyY_to_dt_UCS_JCH(xyY, L_white, JCH);
            dt_UCS_JCH_to_HCB(JCH, HCB);

            const float radius = dt_fast_hypotf(HCB[1], HCB[2]);
            const float sin_T = (radius > 0.f) ? HCB[1] / radius : 0.f;
            const float cos_T = (radius > 0.f) ? HCB[2] / radius : 0.f;
            const float M_rot_inv[2][2] = { { cos_T,  sin_T }, { -sin_T, cos_T } };

            const float P = MAX(FLT_MIN, HCB[1]);
            const float W = sin_T * HCB[1] + cos_T * HCB[2];

            float a = MAX(1.f + d->saturation_global + scalar_product(opacities, d->saturation), 0.f);
            const float b = MAX(1.f + d->brilliance_global + scalar_product(opacities, d->brilliance), 0.f);

            const float max_a = dt_fast_hypotf(P, W) / P;
            a = soft_clip(a, 0.5f * max_a, max_a);

            const float P_prime = (a - 1.f) * P;
            const float W_prime = sqrtf(sqf(P) * (1.f - sqf(a)) + sqf(W)) * b;

            HCB[1] = MAX(M_rot_inv[0][0] * P_prime + M_rot_inv[0][1] * W_prime, 0.f);
            HCB[2] = MAX(M_rot_inv[1][0] * P_prime + M_rot_inv[1][1] * W_prime, 0.f);

            dt_UCS_HCB_to_JCH(HCB, JCH);

            /* Gamut mapping */
            const float max_colorfulness = lookup_gamut(d->gamut_LUT, JCH[2]);
            const float max_chroma = (15.932993652962535f * powf(JCH[0] * L_white, 0.6523997524738018f)
                                      * powf(max_colorfulness, 0.6007557017508491f) / L_white);
            const dt_aligned_pixel_t JCH_gamut_boundary = { JCH[0], max_chroma, JCH[2], 0.f };
            dt_aligned_pixel_t HSB_gamut_boundary;
            dt_UCS_JCH_to_HSB(JCH_gamut_boundary, HSB_gamut_boundary);

            /* Clip saturation at constant brightness */
            dt_aligned_pixel_t HSB = { HCB[0], (HCB[2] > 0.f) ? HCB[1] / HCB[2] : 0.f, HCB[2], 0.f };
            HSB[1] = soft_clip(HSB[1], 0.8f * HSB_gamut_boundary[1], HSB_gamut_boundary[1]);

            dt_UCS_HSB_to_JCH(HSB, JCH);
            dt_UCS_JCH_to_xyY(JCH, L_white, xyY);
            dt_xyY_to_XYZ(xyY, XYZ_D65);
        }

        /* Project back to D50 pipeline RGB */
        dt_aligned_pixel_t pix_out;
        dt_apply_transposed_color_matrix(XYZ_D65, output_matrix_trans, pix_out);

        /* clip negatives */
        pix_out[0] = MAX(pix_out[0], 0.f);
        pix_out[1] = MAX(pix_out[1], 0.f);
        pix_out[2] = MAX(pix_out[2], 0.f);

        out[k+0] = pix_out[0];
        out[k+1] = pix_out[1];
        out[k+2] = pix_out[2];
        out[k+3] = in[k+3];
    }
}

/* Reset - values from DT phase2.xmp dump */
void colorbalancergb_reset(ColorBalanceRGBData *d)
{
    memset(d, 0, sizeof(*d));

    /* DT runtime values from phase2.xmp dump */
    d->global[0] = -0.000000119f;
    d->global[1] = 0.0f;
    d->global[2] = 0.0f;
    d->global[3] = 0.0f;

    d->shadows[0] = 0.999999881f;
    d->shadows[1] = 1.0f;
    d->shadows[2] = 1.0f;
    d->shadows[3] = 1.0f;

    d->highlights[0] = 0.999999881f;
    d->highlights[1] = 1.0f;
    d->highlights[2] = 1.0f;
    d->highlights[3] = 1.0f;

    d->midtones[0] = 1.000000119f;
    d->midtones[1] = 1.0f;
    d->midtones[2] = 1.0f;
    d->midtones[3] = 1.0f;

    for (int c = 0; c < 4; c++) {
        d->chroma[c] = 0.0f;
        d->saturation[c] = 0.0f;
        d->brilliance[c] = 0.0f;
    }

    d->midtones_Y = 1.0f;
    d->contrast = 1.0f;

    d->chroma_global = 0.0f;
    d->vibrance = 0.0f;           /* DT default */
    d->saturation_global = 0.0f;  /* DT default */
    d->brilliance_global = 0.0f;

    d->hue_angle = 0.0f;

    /* DT computed weights */
    d->shadows_weight = 4.0f;
    d->highlights_weight = 4.0f;
    d->midtones_weight = 8.0f;
    d->mask_grey_fulcrum = 0.5f;

    d->white_fulcrum = 1.0f;
    d->grey_fulcrum = 0.184499994f;

    /* Identity gamut LUT */
    for (int i = 0; i < LUT_ELEM; i++) {
        d->gamut_LUT[i] = 1.0f;
    }

    d->max_chroma = 128.0f;
    d->saturation_formula = 1;  /* from DT dump */
}

/* ============================================================================
   Helper: return ColorBalanceRGBData with defaults (neutral grading)
   ============================================================================ */

static inline ColorBalanceRGBData colorbalancergb_defaults(void)
{
    ColorBalanceRGBData d;
    colorbalancergb_reset(&d);
    return d;
}

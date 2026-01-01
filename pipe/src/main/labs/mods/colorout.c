/*
    colorout - EXACT COPY of darktable colorout fastpath

    Source: dark/lib/desk/src/iop/colorout.c
            dark/lib/desk/src/common/colorspaces_inline_conversions.h

    Input: float32 Lab (4 channels from colorin)
    Output: float32 sRGB (4 channels, gamma-encoded)

    Process:
    1. Convert Lab to XYZ using D50 white point
    2. Apply color matrix XYZ -> linear RGB
    3. Apply sRGB transfer function (gamma)
*/

#include <math.h>
#include <stdint.h>
#include "../pipe_state.h"

/* ============================================================================
   From colorout.c - dt_iop_colorout_params_t (lines 63-68)
   ============================================================================ */

typedef struct {
    int type;              /* DT_COLORSPACE_SRGB = default for export */
    char filename[512];
    int intent;            /* DT_INTENT_PERCEPTUAL = 0 */
} ColoroutParams;

/* ============================================================================
   From colorspaces_inline_conversions.h - D50 constants (line 168)
   ============================================================================ */

static const float d50[4] = { 0.9642f, 1.0f, 0.8249f, 0.0f };

/* ============================================================================
   XYZ D50 -> sRGB matrix (Bradford adapted from D65)
   Used by colorout cmatrix path
   ============================================================================ */

static const float XYZ_D50_to_sRGB[4][4] = {
    { 3.1338561f, -1.6168667f, -0.4906146f, 0.f },
    { -0.9787684f, 1.9161415f, 0.0334540f, 0.f },
    { 0.0719453f, -0.2289914f, 1.4052427f, 0.f },
    { 0.f, 0.f, 0.f, 0.f }
};

/* ============================================================================
   XYZ D65 -> sRGB matrix (standard IEC 61966-2-1)
   For direct XYZ -> sRGB without Bradford adaptation
   ============================================================================ */

static const float XYZ_D65_to_sRGB[3][3] = {
    {  3.2404542f, -1.5371385f, -0.4985314f },
    { -0.9692660f,  1.8760108f,  0.0415560f },
    {  0.0556434f, -0.2040259f,  1.0572252f }
};

/* ============================================================================
   From colorspaces_inline_conversions.h - lab_f_inv (lines 202-207)
   Inverse of lab_f - converts from f(x) back to x
   ============================================================================ */

static inline float lab_f_inv(const float x)
{
    const float epsilon = 0.20689655172413796f; /* cbrtf(216.0f/24389.0f) */
    const float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? x * x * x : (116.0f * x - 16.0f) / kappa;
}

/* ============================================================================
   From colorspaces_inline_conversions.h - dt_Lab_to_XYZ (lines 211-225)
   Uses D50 white point
   ============================================================================ */

static inline void dt_Lab_to_XYZ(const float Lab[4], float XYZ[4])
{
    /* f = { a/500 + (L+16)/116, (L+16)/116, -b/200 + (L+16)/116, alpha } */
    float f[4];
    f[1] = (Lab[0] + 16.0f) / 116.0f;  /* (L+16)/116 */
    f[0] = Lab[1] / 500.0f + f[1];      /* a/500 + f[1] */
    f[2] = f[1] - Lab[2] / 200.0f;      /* f[1] - b/200 */
    f[3] = Lab[3];

    /* Apply inverse and scale by D50 */
    for (int c = 0; c < 4; c++)
        XYZ[c] = d50[c] * lab_f_inv(f[c]);
}

/* ============================================================================
   From colorspaces_inline_conversions.h - sRGB transfer function (line 556)
   linear RGB -> gamma-encoded sRGB
   ============================================================================ */

/* ============================================================================
   From imageop_math.h - dt_iop_estimate_exp (lines 98-128)
   Estimates exponential form f(x) = a*x^g from sample points
   ============================================================================ */

static inline void dt_iop_estimate_exp(const float *const x, const float *const y,
                                        const int num, float *coeff)
{
    const float x0 = x[num - 1], y0 = y[num - 1];

    float g = 0.0f;
    int cnt = 0;
    for (int k = 0; k < num - 1; k++)
    {
        const float yy = y[k] / y0, xx = x[k] / x0;
        if (yy > 0.0f && xx > 0.0f)
        {
            const float gg = logf(y[k] / y0) / logf(x[k] / x0);
            g += gg;
            cnt++;
        }
    }
    if (cnt)
        g *= 1.0f / cnt;
    else
        g = 1.0f;
    coeff[0] = 1.0f / x0;
    coeff[1] = y0;
    coeff[2] = g;
}

/* ============================================================================
   From imageop_math.h - dt_iop_eval_exp (lines 133-136)
   Evaluates the exponential fit for x >= 1.0
   ============================================================================ */

static inline float dt_iop_eval_exp(const float *const coeff, const float x)
{
    return coeff[1] * powf(x * coeff[0], coeff[2]);
}

/* ============================================================================
   sRGB transfer function with exponential extension for x >= 1.0
   Matches DT's LUT + unbounded_coeffs approach
   ============================================================================ */

static inline float srgb_gamma(const float x)
{
    /* sRGB transfer function:
       if x <= 0.0031308: 12.92 * x
       else: 1.055 * x^(1/2.4) - 0.055 */
    if (x <= 0.0031308f)
        return 12.92f * x;
    else
        return 1.055f * powf(x, 1.0f / 2.4f) - 0.055f;
}

/* Precomputed sRGB extension coefficients
   Computed the same way as DT: sample at {0.7, 0.8, 0.9, 1.0}, fit exponential */
static float srgb_unbounded_coeffs[3] = { 0.0f, 0.0f, 0.0f };
static int srgb_coeffs_initialized = 0;

static void init_srgb_unbounded_coeffs(void)
{
    if (srgb_coeffs_initialized) return;

    const float x[4] = { 0.7f, 0.8f, 0.9f, 1.0f };
    const float y[4] = { srgb_gamma(0.7f), srgb_gamma(0.8f),
                         srgb_gamma(0.9f), srgb_gamma(1.0f) };
    dt_iop_estimate_exp(x, y, 4, srgb_unbounded_coeffs);
    srgb_coeffs_initialized = 1;
}

static inline float linear_to_sRGB(const float linear)
{
    /* Clamp negative values like DT's _lerp_lut does with MAX(v,0.0f) */
    const float v = (linear < 0.0f) ? 0.0f : linear;

    /* For x < 1.0: use sRGB transfer function
       For x >= 1.0: use exponential extension like DT */
    if (v < 1.0f)
        return srgb_gamma(v);
    else
        return dt_iop_eval_exp(srgb_unbounded_coeffs, v);
}

/* ============================================================================
   From colorout.c - apply transposed color matrix (local version)
   ============================================================================ */

static inline void colorout_apply_matrix(const float in[4],
                                          const float row0[4],
                                          const float row1[4],
                                          const float row2[4],
                                          float out[4])
{
    for (int r = 0; r < 4; r++)
        out[r] = row0[r] * in[0] + row1[r] * in[1] + row2[r] * in[2];
}

/* ============================================================================
   From colorout.c - _transform_cmatrix_linear (lines 406-432)
   ============================================================================ */

static void _transform_cmatrix_srgb(float *const out,
                                     const float *const in,
                                     size_t npixels,
                                     const float cmatrix[4][4])
{
    /* Transpose for row-wise access */
    const float cmatrix_row0[4] = { cmatrix[0][0], cmatrix[1][0], cmatrix[2][0], 0.0f };
    const float cmatrix_row1[4] = { cmatrix[0][1], cmatrix[1][1], cmatrix[2][1], 0.0f };
    const float cmatrix_row2[4] = { cmatrix[0][2], cmatrix[1][2], cmatrix[2][2], 0.0f };

    for (size_t k = 0; k < npixels; k++)
    {
        float XYZ[4];
        dt_Lab_to_XYZ(in + 4*k, XYZ);

        float rgb[4];
        for (int r = 0; r < 4; r++)
            rgb[r] = cmatrix_row0[r] * XYZ[0] + cmatrix_row1[r] * XYZ[1] + cmatrix_row2[r] * XYZ[2];

        /* Apply sRGB gamma transfer function */
        out[4*k+0] = linear_to_sRGB(rgb[0]);
        out[4*k+1] = linear_to_sRGB(rgb[1]);
        out[4*k+2] = linear_to_sRGB(rgb[2]);
        out[4*k+3] = rgb[3];
    }
}

/* ============================================================================
   colorout_process - Main entry point

   Simplified version for fastpath only (cmatrix path, no LUT)
   ============================================================================ */

void colorout_process(
    const float* in,
    float* out,
    const PipeState* state,
    const float cmatrix[4][4])
{
    /* Initialize sRGB extension coefficients (one-time) */
    init_srgb_unbounded_coeffs();

    const size_t npixels = (size_t)state->width * state->height;
    _transform_cmatrix_srgb(out, in, npixels, cmatrix);
}

/* ============================================================================
   Reset to default values
   ============================================================================ */

void colorout_reset(ColoroutParams* p)
{
    p->type = 1;           /* DT_COLORSPACE_SRGB */
    p->filename[0] = '\0';
    p->intent = 0;         /* DT_INTENT_PERCEPTUAL */
}

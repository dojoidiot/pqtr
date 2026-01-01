/*
    colorin - EXACT COPY of darktable colorin fastpath

    Source: dark/lib/desk/src/iop/colorin.c
            dark/lib/desk/src/common/colorspaces_inline_conversions.h

    Input: float32 RGB (4 channels from demosaic)
    Output: float32 Lab (4 channels, L*a*b* + alpha)

    Process:
    1. Apply correction coefficients (D65/as_shot if late_correction)
    2. Apply color matrix RGB -> XYZ
    3. Convert XYZ to Lab using D50 white point
*/

#include <math.h>
#include <stdint.h>
#include "../pipe_state.h"

/* ============================================================================
   From colorin.c - dt_iop_colorin_params_t (lines 71-81)
   ============================================================================ */

typedef struct {
    int type;              /* DT_COLORSPACE_ENHANCED_MATRIX = default */
    char filename[512];
    int intent;            /* DT_INTENT_PERCEPTUAL = 0 */
    int normalize;         /* DT_NORMALIZE_OFF = 0 */
    int blue_mapping;      /* FALSE */
    int type_work;         /* DT_COLORSPACE_LIN_REC2020 */
    char filename_work[512];
} ColorinParams;

/* ============================================================================
   From colorspaces_inline_conversions.h - D50 constants (lines 168-169)
   ============================================================================ */

static const float d50_inv[4] = { 1.0f/0.9642f, 1.0f, 1.0f/0.8249f, 0.0f };

/* ============================================================================
   From colorspaces_inline_conversions.h - cbrt functions (lines 143-165)

   Fast cube root approximation using bit manipulation + Halley iteration
   ============================================================================ */

static inline float cbrt_5f(float f)
{
    uint32_t * const p = (uint32_t *)&f;
    *p = *p / 3 + 709921077;
    return f;
}

static inline float cbrta_halleyf(const float a, const float R)
{
    const float a3 = a * a * a;
    const float b = a * (a3 + R + R) / (a3 + a3 + R);
    return b;
}

static inline float lab_f(const float x)
{
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? cbrta_halleyf(cbrt_5f(x), x) : (kappa * x + 16.0f) / 116.0f;
}

/* ============================================================================
   From colorspaces_inline_conversions.h - dt_apply_color_matrix_by_row (lines 131-141)
   ============================================================================ */

static inline void dt_apply_color_matrix_by_row(const float in[4],
                                                const float matrix_row0[4],
                                                const float matrix_row1[4],
                                                const float matrix_row2[4],
                                                float out[4])
{
    for (int r = 0; r < 4; r++)
        out[r] = matrix_row0[r] * in[0] + matrix_row1[r] * in[1] + matrix_row2[r] * in[2];
}

/* ============================================================================
   From colorspaces_inline_conversions.h - dt_XYZ_to_Lab (lines 172-199)
   ============================================================================ */

static inline void dt_XYZ_to_Lab(const float XYZ[4], float Lab[4])
{
    float f[4];
    for (int i = 0; i < 4; i++)
        f[i] = lab_f(XYZ[i] * d50_inv[i]);

    /* Lab[0] = 116.0f * f[1] - 16.0f;
       Lab[1] = 500.0f * (f[0] - f[1]);
       Lab[2] = -200.0f * (f[2] - f[1]); */
    static const float coeff[4] = { 116.0f, 500.0f, -200.0f, 0.0f };
    static const float offset[4] = { 16.0f, 0.0f, 0.0f, 0.0f };

    const float tmp1[4] = { f[1], f[0], f[2], f[3] };
    const float tmp2[4] = { 0.0f, f[1], f[1], 0.0f };

    for (int c = 0; c < 4; c++)
        Lab[c] = (coeff[c] * (tmp1[c] - tmp2[c])) - offset[c];
}

/* ============================================================================
   From colorspaces_inline_conversions.h - dt_RGB_to_Lab (lines 588-597)
   ============================================================================ */

static inline void dt_RGB_to_Lab(const float rgb[4],
                                 const float cmatrix_row0[4],
                                 const float cmatrix_row1[4],
                                 const float cmatrix_row2[4],
                                 float Lab[4])
{
    float XYZ[4];
    dt_apply_color_matrix_by_row(rgb, cmatrix_row0, cmatrix_row1, cmatrix_row2, XYZ);
    dt_XYZ_to_Lab(XYZ, Lab);
}

/* ============================================================================
   From colorin.c - _cmatrix_fastpath_simple (lines 827-855)
   ============================================================================ */

static void _cmatrix_fastpath_simple(float *const out,
                                     const float *const in,
                                     size_t npixels,
                                     const float cmatrix[4][4],
                                     const float corr[4])
{
    const float cmatrix_row0[4] = { cmatrix[0][0], cmatrix[1][0], cmatrix[2][0], 0.0f };
    const float cmatrix_row1[4] = { cmatrix[0][1], cmatrix[1][1], cmatrix[2][1], 0.0f };
    const float cmatrix_row2[4] = { cmatrix[0][2], cmatrix[1][2], cmatrix[2][2], 0.0f };

    for (size_t k = 0; k < npixels; k++)
    {
        float cam[4] = {in[4*k] * corr[0], in[4*k+1] * corr[1], in[4*k+2] * corr[2], 1.0f};
        float res[4];
        dt_RGB_to_Lab(cam, cmatrix_row0, cmatrix_row1, cmatrix_row2, res);
        out[4*k+0] = res[0];
        out[4*k+1] = res[1];
        out[4*k+2] = res[2];
        out[4*k+3] = res[3];
    }
}

/* ============================================================================
   colorin_process - Main entry point

   Simplified version for fastpath only (no blue_mapping, no nonlinearlut)
   ============================================================================ */

void colorin_process(
    const float* in,
    float* out,
    const PipeState* state,
    const float cmatrix[4][4])
{
    const size_t npixels = (size_t)state->width * state->height;

    /* Compute correction coefficients:
       corrected = late_correction && type != LAB
       corr = D65coeffs / as_shot if corrected, else 1.0 */
    float corr[4];
    if (state->chroma.late_correction)
    {
        corr[0] = (float)(state->chroma.D65coeffs[0] / state->chroma.as_shot[0]);
        corr[1] = (float)(state->chroma.D65coeffs[1] / state->chroma.as_shot[1]);
        corr[2] = (float)(state->chroma.D65coeffs[2] / state->chroma.as_shot[2]);
        corr[3] = (state->chroma.as_shot[3] != 0.0) ?
                  (float)(state->chroma.D65coeffs[3] / state->chroma.as_shot[3]) : 0.0f;
    }
    else
    {
        corr[0] = corr[1] = corr[2] = corr[3] = 1.0f;
    }

    _cmatrix_fastpath_simple(out, in, npixels, cmatrix, corr);
}

/* ============================================================================
   Reset to default values
   ============================================================================ */

void colorin_reset(ColorinParams* p)
{
    p->type = 6;           /* DT_COLORSPACE_ENHANCED_MATRIX */
    p->filename[0] = '\0';
    p->intent = 0;         /* DT_INTENT_PERCEPTUAL */
    p->normalize = 0;      /* DT_NORMALIZE_OFF */
    p->blue_mapping = 0;
    p->type_work = 10;     /* DT_COLORSPACE_LIN_REC2020 */
    p->filename_work[0] = '\0';
}

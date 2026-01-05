/*
    pipe_prepare - compute derived values for pipeline

    Raw data (adobe_XYZ_to_CAM, as_shot, etc.) is populated by head decoder.
    This module computes derived values like D65coeffs.

    Source: dark/lib/desk/src/common/colorspaces.c lines 2320-2399
*/

#include <math.h>
#include <string.h>
#include "pipe_state.h"

/* ============================================================================
   From dttypes.h - colormatrix validity
   ============================================================================ */

static inline int dt_is_valid_colormatrix(float matrix)
{
    return isfinite(matrix);
}

/* ============================================================================
   From colorspaces.c lines 2320-2399: dt_colorspaces_conversion_matrices_rgb

   Computes D65 WB multipliers from camera color matrix.
   ============================================================================ */

static int compute_mul_from_matrix(
    const float adobe_XYZ_to_CAM[4][3],
    const float *embedded_matrix,
    double mul[4])
{
    float XYZ_to_CAM[4][3];

    /* Use embedded matrix if valid, otherwise adobe_XYZ_to_CAM */
    if (embedded_matrix == NULL || !dt_is_valid_colormatrix(embedded_matrix[0]))
    {
        for (int k = 0; k < 4; k++)
            for (int i = 0; i < 3; i++)
                XYZ_to_CAM[k][i] = adobe_XYZ_to_CAM[k][i];
    }
    else
    {
        XYZ_to_CAM[0][0] = embedded_matrix[0];
        XYZ_to_CAM[0][1] = embedded_matrix[1];
        XYZ_to_CAM[0][2] = embedded_matrix[2];
        XYZ_to_CAM[1][0] = embedded_matrix[3];
        XYZ_to_CAM[1][1] = embedded_matrix[4];
        XYZ_to_CAM[1][2] = embedded_matrix[5];
        XYZ_to_CAM[2][0] = embedded_matrix[6];
        XYZ_to_CAM[2][1] = embedded_matrix[7];
        XYZ_to_CAM[2][2] = embedded_matrix[8];
        for (int i = 0; i < 3; i++)
            XYZ_to_CAM[3][i] = 0.0f;
    }

    if (!dt_is_valid_colormatrix(XYZ_to_CAM[0][0]))
        return 0;

    /* sRGB D65 RGB_to_XYZ */
    static const double RGB_to_XYZ[3][3] = {
        { 0.412453, 0.357580, 0.180423 },
        { 0.212671, 0.715160, 0.072169 },
        { 0.019334, 0.119193, 0.950227 },
    };

    /* RGB_to_CAM = XYZ_to_CAM * RGB_to_XYZ */
    double RGB_to_CAM[4][3];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 3; j++)
        {
            RGB_to_CAM[i][j] = 0.0;
            for (int k = 0; k < 3; k++)
                RGB_to_CAM[i][j] += XYZ_to_CAM[i][k] * RGB_to_XYZ[k][j];
        }

    /* Normalize rows, compute mul = 1/row_sum */
    for (int i = 0; i < 4; i++)
    {
        double num = 0.0;
        for (int j = 0; j < 3; j++)
            num += RGB_to_CAM[i][j];
        if (num != 0.0)
            mul[i] = 1.0 / num;
        else
            mul[i] = 0.0;
    }

    return 1;
}

/* ============================================================================
   pipe_prepare - compute D65coeffs from color matrix

   Call after head decoder has populated:
   - state->adobe_XYZ_to_CAM
   - state->d65_color_matrix
   - state->chroma.as_shot
   ============================================================================ */

void pipe_prepare(PipeState *state)
{
    double mul[4];

    if (compute_mul_from_matrix(
            state->adobe_XYZ_to_CAM,
            state->d65_color_matrix,
            mul))
    {
        /* Normalize by green */
        state->chroma.D65coeffs[0] = mul[0] / mul[1];
        state->chroma.D65coeffs[1] = 1.0;
        state->chroma.D65coeffs[2] = mul[2] / mul[1];
        state->chroma.D65coeffs[3] = (mul[3] != 0.0) ? mul[3] / mul[1] : 0.0;
    }
    else
    {
        /* No valid matrix - use identity */
        state->chroma.D65coeffs[0] = 1.0;
        state->chroma.D65coeffs[1] = 1.0;
        state->chroma.D65coeffs[2] = 1.0;
        state->chroma.D65coeffs[3] = 1.0;
    }
}

/* ============================================================================
   pipe_state_init_sony - initialize PipeState with exact DT values

   This is the COPY approach: dump all runtime values from DT and use them
   directly. No computation, no tracing - just copy the exact state.

   Dumped from: darktable-cli src/test/raws/sony.ARW
   ============================================================================ */

void pipe_state_init_sony(PipeState *state)
{
    memset(state, 0, sizeof(*state));

    /* From PQTR_PIPESTATE dump */
    state->width = 6048;
    state->height = 4024;
    state->filters = 2492765332;  /* 0x94949494 = RGGB */

    /* temperature output */
    state->temperature.enabled = 1;
    state->temperature.coeffs[0] = 2.511718750f;
    state->temperature.coeffs[1] = 1.000000000f;
    state->temperature.coeffs[2] = 1.457031250f;
    state->temperature.coeffs[3] = 0.000000000f;

    /* chroma state */
    state->chroma.late_correction = 1;
    state->chroma.as_shot[0] = 2.511718750;
    state->chroma.as_shot[1] = 1.000000000;
    state->chroma.as_shot[2] = 1.457031250;
    state->chroma.as_shot[3] = 0.000000000;
    state->chroma.D65coeffs[0] = 2.671514144;
    state->chroma.D65coeffs[1] = 1.000000000;
    state->chroma.D65coeffs[2] = 1.347009702;
    state->chroma.D65coeffs[3] = INFINITY;  /* DT has inf here */
}

/*
 * types.h - Shared types for all modules
 *
 * Resolves conflicts between modules that define the same types differently.
 */

#ifndef LABS_MODS_TYPES_H
#define LABS_MODS_TYPES_H

#include <stdint.h>
#include <math.h>

/* ==========================================================================
   Color matrix type - 4x4 for maximum compatibility
   ========================================================================== */

typedef float dt_colormatrix_t[4][4];
typedef float dt_aligned_pixel_t[4];

/* ==========================================================================
   Common math functions
   ========================================================================== */

#ifndef LABS_SQF_DEFINED
#define LABS_SQF_DEFINED
static inline float sqf(const float x) { return x * x; }
#endif

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38f
#endif

#ifndef FLT_MIN
#define FLT_MIN 1.175494351e-38f
#endif

/* ==========================================================================
   Matrix operations
   ========================================================================== */

#ifndef LABS_MATRIX_OPS_DEFINED
#define LABS_MATRIX_OPS_DEFINED

static inline void dt_apply_transposed_color_matrix_4x4(
    const dt_aligned_pixel_t in,
    const dt_colormatrix_t matrix,
    dt_aligned_pixel_t out)
{
    for (int r = 0; r < 4; r++)
        out[r] = matrix[0][r] * in[0] + matrix[1][r] * in[1] + matrix[2][r] * in[2];
}

static inline void dt_apply_color_matrix_4x4(
    const dt_aligned_pixel_t in,
    const dt_colormatrix_t matrix,
    dt_aligned_pixel_t out)
{
    for (int r = 0; r < 3; r++)
        out[r] = matrix[r][0] * in[0] + matrix[r][1] * in[1] + matrix[r][2] * in[2];
    out[3] = 0.0f;
}

#endif /* LABS_MATRIX_OPS_DEFINED */

/* ==========================================================================
   Channel iteration macros
   ========================================================================== */

#ifndef for_each_channel
#define for_each_channel(c) for (int c = 0; c < 4; c++)
#endif

#ifndef for_three_channels
#define for_three_channels(c) for (int c = 0; c < 3; c++)
#endif

#ifndef for_four_channels
#define for_four_channels(c) for (int c = 0; c < 4; c++)
#endif

#endif /* LABS_MODS_TYPES_H */

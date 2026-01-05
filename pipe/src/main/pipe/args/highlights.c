/*
    highlights - EXACT COPY of darktable highlights OPPOSED mode

    Source files:
    - dark/lib/desk/src/iop/highlights.c (params, clip_magics)
    - dark/lib/desk/src/iop/hlreconstruct/opposed.c (_process_opposed, _mask_dilated, _raw_to_cmap)
    - dark/lib/desk/src/iop/hlreconstruct/segbased.c (_calc_refavg, HL_POWERF)

    Input: float32 bayer mosaic (from temperature)
    Output: float32 bayer mosaic (highlight reconstructed)
*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include "../pipe_state.h"

/* ============================================================================
   Inlined from common.h - FC (Filter Color) for Bayer index
   ============================================================================ */

#ifndef FC_DEFINED
#define FC_DEFINED
static inline int FC(const size_t row, const size_t col, const uint32_t filters)
{
    return (filters >> (((row << 1 & 14) + (col & 1)) << 1)) & 3;
}
#endif

/* ============================================================================
   From common/dttypes.h (guarded to avoid redefinition)
   ============================================================================ */

#ifndef for_three_channels
#define for_three_channels(c) for (int c = 0; c < 3; c++)
#endif
#ifndef for_each_channel
#define for_each_channel(c) for (int c = 0; c < 4; c++)
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* ============================================================================
   From highlights.c lines 57-65 - dt_iop_highlights_mode_t
   ============================================================================ */

typedef enum {
    DT_IOP_HIGHLIGHTS_CLIP = 0,
    DT_IOP_HIGHLIGHTS_LCH = 1,
    DT_IOP_HIGHLIGHTS_INPAINT = 2,
    DT_IOP_HIGHLIGHTS_LAPLACIAN = 3,
    DT_IOP_HIGHLIGHTS_SEGMENTS = 4,
    DT_IOP_HIGHLIGHTS_OPPOSED = 5,
} HighlightsMode;

/* ============================================================================
   From highlights.c lines 104-122 - dt_iop_highlights_params_t
   ============================================================================ */

typedef struct {
    HighlightsMode mode;
    float blendL;
    float blendC;
    float strength;
    float clip;
    float noise_level;
    int iterations;
    int scales;
    float candidating;
    float combine;
    int recovery;
    float solid_color;
} HighlightsParams;

typedef HighlightsParams HighlightsData;

/* ============================================================================
   From highlights.c line 55 - clip magic values
   ============================================================================ */

static float highlights_clip_magics[6] = { 1.0f, 1.0f, 0.987f, 0.995f, 0.987f, 0.987f };

/* ============================================================================
   From segbased.c line 91 - HL_POWERF
   ============================================================================ */

#define HL_POWERF 3.0f

/* FC() is in common.h */

/* ============================================================================
   From segbased.c lines 188-224 - _calc_refavg
   ============================================================================ */

static inline float _calc_refavg(const float *in,
                                 const uint32_t filters,
                                 const int row,
                                 const int col,
                                 const int width,
                                 const int height,
                                 const float *correction,
                                 const int linear)
{
    const int color = FC(row, col, filters);
    float mean[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float cnt[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    const int dymin = MAX(0, row - 1);
    const int dxmin = MAX(0, col - 1);
    const int dymax = MIN(height - 1, row + 2);
    const int dxmax = MIN(width - 1, col + 2);

    for (int dy = dymin; dy < dymax; dy++)
    {
        for (int dx = dxmin; dx < dxmax; dx++)
        {
            const float val = fmaxf(0.0f, in[(size_t)dy * width + dx]);
            const int c = FC(dy, dx, filters);
            mean[c] += val;
            cnt[c] += 1.0f;
        }
    }

    for_each_channel(c)
        mean[c] = (cnt[c] > 0.0f) ? powf((correction[c] * mean[c]) / cnt[c], 1.0f / HL_POWERF) : 0.0f;

    const float croot_refavg[4] = {
        0.5f * (mean[1] + mean[2]),
        0.5f * (mean[0] + mean[2]),
        0.5f * (mean[0] + mean[1]),
        0.0f
    };

    return (linear) ? powf(croot_refavg[color], HL_POWERF) : croot_refavg[color];
}

/* ============================================================================
   From opposed.c lines 59-62 - _raw_to_cmap
   ============================================================================ */

static inline size_t _raw_to_cmap(const size_t width, const size_t row, const size_t col)
{
    return (row / 3) * width + (col / 3);
}

/* ============================================================================
   From opposed.c lines 64-81 - _mask_dilated
   ============================================================================ */

static inline char _mask_dilated(const char *in, const size_t w1)
{
    if (in[0])
        return 1;

    if (in[-w1-1] | in[-w1] | in[-w1+1] | in[-1] | in[1] | in[w1-1] | in[w1] | in[w1+1])
        return 1;

    const size_t w2 = 2 * w1;
    const size_t w3 = 3 * w1;
    return (in[-w3-2] | in[-w3-1] | in[-w3]   | in[-w3+1] | in[-w3+2] |
            in[-w2-3] | in[-w2-2] | in[-w2-1] | in[-w2]   | in[-w2+1] | in[-w2+2] | in[-w2+3] |
            in[-w1-3] | in[-w1-2] | in[-w1+2] | in[-w1+3] |
            in[-3]    | in[-2]    | in[2]     | in[3]     |
            in[w1-3]  | in[w1-2]  | in[w1+2]  | in[w1+3]  |
            in[w2-3]  | in[w2-2]  | in[w2-1]  | in[w2]    | in[w2+1]  | in[w2+2]  | in[w2+3] |
            in[w3-2]  | in[w3-1]  | in[w3]    | in[w3+1]  | in[w3+2]) ? 1 : 0;
}

/* ============================================================================
   From opposed.c lines 219-410 - _process_opposed (Bayer only, no xtrans)
   Simplified: removed OpenCL, removed hash caching, removed roi offset handling
   ============================================================================ */

void highlights_process_opposed(
    const float *input,
    float *output,
    const PipeState *state,
    const HighlightsData *d)
{
    const uint32_t filters = state->filters;
    const int width = state->width;
    const int height = state->height;

    const float clipval = highlights_clip_magics[DT_IOP_HIGHLIGHTS_OPPOSED] * d->clip;

    /* From opposed.c lines 233-238: icoeffs from dsc.temperature */
    const int wbon = state->temperature.enabled;
    const float icoeffs[4] = {
        wbon ? state->temperature.coeffs[0] : 1.0f,
        wbon ? state->temperature.coeffs[1] : 1.0f,
        wbon ? state->temperature.coeffs[2] : 1.0f,
        1.0f
    };
    const float clips[4] = {
        clipval * icoeffs[0],
        clipval * icoeffs[1],
        clipval * icoeffs[2],
        clipval
    };

    /* From opposed.c lines 240-245: correction from chroma */
    const int late = state->chroma.late_correction;
    const float correction[4] = {
        late ? (float)(state->chroma.D65coeffs[0] / state->chroma.as_shot[0]) : 1.0f,
        late ? (float)(state->chroma.D65coeffs[1] / state->chroma.as_shot[1]) : 1.0f,
        late ? (float)(state->chroma.D65coeffs[2] / state->chroma.as_shot[2]) : 1.0f,
        1.0f
    };

    /* From opposed.c lines 247-249 */
    const size_t mwidth = width / 3;
    const size_t mheight = height / 3;
    const size_t msize = (((mwidth + 1) * (mheight + 1) + 15) & ~15);

    float chrominance[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    /* From opposed.c lines 266-357: mask processing */
    char *mask = (char *)calloc(6 * msize, sizeof(char));
    if (mask)
    {
        int anyclipped = 0;

        /* From opposed.c lines 270-293: Pass 1 - mark clipped areas */
        for (int mrow = 1; mrow < (int)mheight - 1; mrow++)
        {
            for (int mcol = 1; mcol < (int)mwidth - 1; mcol++)
            {
                char mbuff[4] = { 0, 0, 0, 0 };  /* R, G1, B, G2 */
                const size_t grp = 3 * (mrow * width + mcol);
                for (int y = -1; y < 2; y++)
                {
                    for (int x = -1; x < 2; x++)
                    {
                        const size_t idx = grp + y * width + x;
                        const int color = FC(mrow + y, mcol + x, filters);
                        const int clipped = input[idx] >= clips[color];
                        mbuff[color] += clipped ? 1 : 0;
                    }
                }
                /* Combine G1 (color 1) and G2 (color 3) into green channel */
                mbuff[1] = mbuff[1] | mbuff[3];
                for_three_channels(c)
                {
                    mask[c * msize + mrow * mwidth + mcol] = mbuff[c] ? 1 : 0;
                    anyclipped |= mbuff[c] ? 1 : 0;
                }
            }
        }

        float sums[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float cnts[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (anyclipped)
        {
            /* From opposed.c lines 305-315: Pass 2 - dilate mask */
            for (size_t row = 3; row < mheight - 3; row++)
            {
                for (size_t col = 3; col < mwidth - 3; col++)
                {
                    const size_t mx = row * mwidth + col;
                    mask[3 * msize + mx] = _mask_dilated(mask + mx, mwidth);
                    mask[4 * msize + mx] = _mask_dilated(mask + msize + mx, mwidth);
                    mask[5 * msize + mx] = _mask_dilated(mask + 2 * msize + mx, mwidth);
                }
            }

            /* From opposed.c line 317 */
            const float lo_clips[4] = { 0.2f * clips[0], 0.2f * clips[1], 0.2f * clips[2], 1.0f };

            /* From opposed.c lines 319-336: Pass 3 - calculate chrominance */
            for (size_t row = 3; row < (size_t)height - 3; row++)
            {
                for (size_t col = 3; col < (size_t)width - 3; col++)
                {
                    const size_t idx = row * width + col;
                    const int color = FC(row, col, filters);
                    const float inval = input[idx];

                    if ((inval < clips[color]) && (inval > lo_clips[color])
                        && (mask[(color + 3) * msize + _raw_to_cmap(mwidth, row, col)]))
                    {
                        sums[color] += inval - _calc_refavg(input, filters, row, col, width, height, correction, 1);
                        cnts[color] += 1.0f;
                    }
                }
            }

            /* From opposed.c lines 337-338 */
            for_three_channels(c)
                chrominance[c] = (cnts[c] > 100.0f) ? sums[c] / cnts[c] : 0.0f;
        }

        free(mask);
    }

    /* From opposed.c lines 381-408: Pass 4 - apply reconstruction */
    for (size_t row = 0; row < (size_t)height; row++)
    {
        for (size_t col = 0; col < (size_t)width; col++)
        {
            const size_t idx = row * width + col;
            const int color = FC(row, col, filters);
            float oval = fmaxf(0.0f, input[idx]);

            if (oval >= clips[color])
            {
                const float ref = _calc_refavg(input, filters, row, col, width, height, correction, 1);
                oval = fmaxf(oval, ref + chrominance[color]);
            }

            output[idx] = oval;
        }
    }
}

/* ============================================================================
   Main process function - dispatches to appropriate mode
   ============================================================================ */

void highlights_process(
    const float *in,
    float *out,
    const PipeState *state,
    const HighlightsData *d)
{
    /* Only OPPOSED mode implemented for now */
    if (d->mode == DT_IOP_HIGHLIGHTS_OPPOSED)
    {
        highlights_process_opposed(in, out, state, d);
    }
    else
    {
        /* Passthrough for other modes */
        memcpy(out, in, state->width * state->height * sizeof(float));
    }
}

/* ============================================================================
   Reset to values from XMP
   ============================================================================ */

void highlights_reset(HighlightsParams *p,
                      HighlightsMode mode,
                      float clip,
                      float strength)
{
    p->mode = mode;
    p->blendL = 1.0f;
    p->blendC = 0.0f;
    p->strength = strength;
    p->clip = clip;
    p->noise_level = 0.0f;
    p->iterations = 30;
    p->scales = 6;
    p->candidating = 0.4f;
    p->combine = 2.0f;
    p->recovery = 0;
    p->solid_color = 0.0f;
}

/* ============================================================================
   Helper: return HighlightsData with defaults (OPPOSED mode)
   ============================================================================ */

static inline HighlightsData highlights_defaults(void)
{
    HighlightsData d;
    d.mode = DT_IOP_HIGHLIGHTS_OPPOSED;
    d.blendL = 1.0f;
    d.blendC = 0.0f;
    d.strength = 1.0f;
    d.clip = 1.0f;
    d.noise_level = 0.0f;
    d.iterations = 30;
    d.scales = 6;
    d.candidating = 0.4f;
    d.combine = 2.0f;
    d.recovery = 0;
    d.solid_color = 0.0f;
    return d;
}

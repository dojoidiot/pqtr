/*
    demosaic - EXACT COPY of darktable RCD demosaic algorithm

    Source: dark/lib/desk/src/iop/demosaicing/rcd.c

    Input: float32 bayer mosaic (from highlights)
    Output: float32 RGBA (4 channels per pixel)
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include "pipe_state.h"

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
   From demosaic.c - dt_iop_demosaic_params_t (lines 148-162)
   ============================================================================ */

typedef struct {
    int green_eq;           /* 0 = disabled */
    float median_thrs;      /* 0.0 */
    int color_smoothing;    /* 0 = disabled */
    int demosaicing_method; /* 5 = RCD */
    int lmmse_refine;       /* 1 */
    float dual_thrs;        /* 0.2 */
    float cs_radius;        /* 0.0 */
    float cs_thrs;          /* 0.40 */
    float cs_boost;         /* 0.0 */
    int cs_iter;            /* 8 */
    float cs_center;        /* 0.0 */
    int cs_enabled;         /* FALSE */
} DemosaicParams;

/* FC() is in common.h */

/* ============================================================================
   From common/math.h - helper functions (lines 119-130)
   ============================================================================ */

static inline float sqrf(const float a)
{
    return a * a;
}

/* taken from rt code: calculate a * b + (1 - a) * c */
static inline float interpolatef(const float a,
                                 const float b,
                                 const float c)
{
    return a * (b - c) + c;
}

/* ============================================================================
   From rcd.c - constants (lines 68-77)
   ============================================================================ */

#define DT_RCD_TILESIZE 112
#define RCD_BORDER 9          /* avoid tile-overlap errors */
#define RCD_MARGIN 7          /* for the outermost tiles we can have a smaller outer border */
#define RCD_TILEVALID (DT_RCD_TILESIZE - 2 * RCD_BORDER)
#define w1 DT_RCD_TILESIZE
#define w2 (2 * DT_RCD_TILESIZE)
#define w3 (3 * DT_RCD_TILESIZE)
#define w4 (4 * DT_RCD_TILESIZE)

#define eps 1e-5f              /* Tolerance to avoid dividing by zero */
#define epssq 1e-10f

/* ============================================================================
   From rcd.c - _safe_in (lines 80-83)
   ============================================================================ */

static inline float _safe_in(float a, float scale)
{
    return fmaxf(0.0f, a) * scale;
}

/* ============================================================================
   Memory allocation helpers (simplified versions of DT functions)
   ============================================================================ */

#ifndef DT_ALLOC_ALIGN_FLOAT_DEFINED
#define DT_ALLOC_ALIGN_FLOAT_DEFINED
static inline float* dt_alloc_align_float(size_t count)
{
    return (float*)aligned_alloc(64, count * sizeof(float));
}

static inline float* dt_calloc_align_float(size_t count)
{
    float* ptr = dt_alloc_align_float(count);
    if (ptr) memset(ptr, 0, count * sizeof(float));
    return ptr;
}

static inline void dt_free_align(void* ptr)
{
    free(ptr);
}
#endif

/* ============================================================================
   From rcd.c - rcd_ppg_border (lines 86-268)

   This is basically ppg adopted to only write data to RCD_MARGIN
   ============================================================================ */

static void rcd_ppg_border(float *const out,
                           const float *const in,
                           const int width,
                           const int height,
                           const uint32_t filters,
                           const int margin)
{
    const int border = margin + 3;
    /* write approximated 3-pixel border region to out */
    float sum[8];
    for(int j = 0; j < height; j++)
    {
        for(int i = 0; i < width; i++)
        {
            if(i == 3 && j >= 3 && j < height - 3) i = width - 3;
            if(i == width) break;
            memset(sum, 0, sizeof(float) * 8);
            for(int y = j - 1; y != j + 2; y++)
            {
                for(int x = i - 1; x != i + 2; x++)
                {
                    if((y >= 0) && (x >= 0) && (y < height) && (x < width))
                    {
                        const int f = FC(y, x, filters);
                        sum[f] += fmaxf(0.0f, in[(size_t)y * width + x]);
                        sum[f + 4]++;
                    }
                }
            }
            const int f = FC(j, i, filters);
            for(int c = 0; c < 3; c++)
            {
                if(c != f && sum[c + 4] > 0.0f)
                    out[4 * ((size_t)j * width + i) + c] = sum[c] / sum[c + 4];
                else
                    out[4 * ((size_t)j * width + i) + c] = fmaxf(0.0f, in[(size_t)j * width + i]);
            }
        }
    }

    for(int j = 3; j < height - 3; j++)
    {
        float *buf = out + (size_t)4 * width * j + 4 * 3;
        const float *buf_in = in + (size_t)width * j + 3;
        for(int i = 3; i < width - 3; i++)
        {
            if(i == border && j >= border && j < height - border)
            {
                i = width - border;
                buf = out + (size_t)4 * width * j + 4 * i;
                buf_in = in + (size_t)width * j + i;
            }
            if(i == width) break;

            const int c = FC(j, i, filters);
            float color[4];
            const float pc = fmaxf(0.0f, buf_in[0]);
            if(c == 0 || c == 2)
            {
                color[c] = pc;
                const float pym  = fmaxf(0.0f, buf_in[-width * 1]);
                const float pym2 = fmaxf(0.0f, buf_in[-width * 2]);
                const float pym3 = fmaxf(0.0f, buf_in[-width * 3]);
                const float pyM  = fmaxf(0.0f, buf_in[+width * 1]);
                const float pyM2 = fmaxf(0.0f, buf_in[+width * 2]);
                const float pyM3 = fmaxf(0.0f, buf_in[+width * 3]);
                const float pxm  = fmaxf(0.0f, buf_in[-1]);
                const float pxm2 = fmaxf(0.0f, buf_in[-2]);
                const float pxm3 = fmaxf(0.0f, buf_in[-3]);
                const float pxM  = fmaxf(0.0f, buf_in[+1]);
                const float pxM2 = fmaxf(0.0f, buf_in[+2]);
                const float pxM3 = fmaxf(0.0f, buf_in[+3]);

                const float guessx = (pxm + pc + pxM) * 2.0f - pxM2 - pxm2;
                const float diffx = (fabsf(pxm2 - pc) + fabsf(pxM2 - pc) + fabsf(pxm - pxM)) * 3.0f
                                    + (fabsf(pxM3 - pxM) + fabsf(pxm3 - pxm)) * 2.0f;
                const float guessy = (pym + pc + pyM) * 2.0f - pyM2 - pym2;
                const float diffy = (fabsf(pym2 - pc) + fabsf(pyM2 - pc) + fabsf(pym - pyM)) * 3.0f
                                    + (fabsf(pyM3 - pyM) + fabsf(pym3 - pym)) * 2.0f;
                if(diffx > diffy)
                {
                    /* use guessy */
                    const float m = fminf(pym, pyM);
                    const float M = fmaxf(pym, pyM);
                    color[1] = fmaxf(fminf(guessy * .25f, M), m);
                }
                else
                {
                    const float m = fminf(pxm, pxM);
                    const float M = fmaxf(pxm, pxM);
                    color[1] = fmaxf(fminf(guessx * .25f, M), m);
                }
            }
            else
                color[1] = pc;

            color[3] = 0.0f;
            for(size_t k = 0; k < 4; k++)
                buf[k] = color[k];
            buf += 4;
            buf_in++;
        }
    }

    /* for all pixels: interpolate colors into float array */
    for(int j = 1; j < height - 1; j++)
    {
        float *buf = out + (size_t)4 * width * j + 4;
        for(int i = 1; i < width - 1; i++)
        {
            if(i == margin && j >= margin && j < height - margin)
            {
                i = width - margin;
                buf = out + (size_t)4 * (width * j + i);
            }
            const int c = FC(j, i, filters);
            float color[4] = { buf[0], buf[1], buf[2], buf[3] };
            const int linesize = 4 * width;
            /* fill all four pixels with correctly interpolated stuff: r/b for green1/2 */
            /* b for r and r for b */
            if(c == 1 || c == 3) /* c & 1 */
            {
                /* calculate red and blue for green pixels: */
                /* need 4-nbhood: */
                const float *nt = buf - linesize;
                const float *nb = buf + linesize;
                const float *nl = buf - 4;
                const float *nr = buf + 4;
                if(FC(j, i + 1, filters) == 0) /* red nb in same row */
                {
                    color[2] = (nt[2] + nb[2] + 2.0f * color[1] - nt[1] - nb[1]) * .5f;
                    color[0] = (nl[0] + nr[0] + 2.0f * color[1] - nl[1] - nr[1]) * .5f;
                }
                else
                {
                    /* blue nb */
                    color[0] = (nt[0] + nb[0] + 2.0f * color[1] - nt[1] - nb[1]) * .5f;
                    color[2] = (nl[2] + nr[2] + 2.0f * color[1] - nl[1] - nr[1]) * .5f;
                }
            }
            else
            {
                /* get 4-star-nbhood: */
                const float *ntl = buf - 4 - linesize;
                const float *ntr = buf + 4 - linesize;
                const float *nbl = buf - 4 + linesize;
                const float *nbr = buf + 4 + linesize;

                if(c == 0)
                {
                    /* red pixel, fill blue: */
                    const float diff1  = fabsf(ntl[2] - nbr[2]) + fabsf(ntl[1] - color[1]) + fabsf(nbr[1] - color[1]);
                    const float guess1 = ntl[2] + nbr[2] + 2.0f * color[1] - ntl[1] - nbr[1];
                    const float diff2  = fabsf(ntr[2] - nbl[2]) + fabsf(ntr[1] - color[1]) + fabsf(nbl[1] - color[1]);
                    const float guess2 = ntr[2] + nbl[2] + 2.0f * color[1] - ntr[1] - nbl[1];
                    if(diff1 > diff2)
                        color[2] = guess2 * .5f;
                    else if(diff1 < diff2)
                        color[2] = guess1 * .5f;
                    else
                        color[2] = (guess1 + guess2) * .25f;
                }
                else /* c == 2, blue pixel, fill red: */
                {
                    const float diff1  = fabsf(ntl[0] - nbr[0]) + fabsf(ntl[1] - color[1]) + fabsf(nbr[1] - color[1]);
                    const float guess1 = ntl[0] + nbr[0] + 2.0f * color[1] - ntl[1] - nbr[1];
                    const float diff2  = fabsf(ntr[0] - nbl[0]) + fabsf(ntr[1] - color[1]) + fabsf(nbl[1] - color[1]);
                    const float guess2 = ntr[0] + nbl[0] + 2.0f * color[1] - ntr[1] - nbl[1];
                    if(diff1 > diff2)
                        color[0] = guess2 * .5f;
                    else if(diff1 < diff2)
                        color[0] = guess1 * .5f;
                    else
                        color[0] = (guess1 + guess2) * .25f;
                }
            }
            for(size_t k = 0; k < 4; k++)
                buf[k] = color[k];
            buf += 4;
        }
    }
}

/* ============================================================================
   From rcd.c - rcd_demosaic (lines 303-582)

   Main RCD demosaic algorithm
   ============================================================================ */

static void rcd_demosaic(float *const out,
                         const float *const in,
                         const int width,
                         const int height,
                         const uint32_t filters,
                         const float scaler)
{
    if(width < 2*RCD_BORDER || height < 2*RCD_BORDER)
    {
        rcd_ppg_border(out, in, width, height, filters, RCD_BORDER);
        return;
    }

    rcd_ppg_border(out, in, width, height, filters, RCD_MARGIN);

    const float revscaler = 1.0f / scaler;

    const int num_vertical = 1 + (height - 2 * RCD_BORDER -1) / RCD_TILEVALID;
    const int num_horizontal = 1 + (width - 2 * RCD_BORDER -1) / RCD_TILEVALID;

    /* Note: OpenMP parallelization removed for simplicity - single threaded */
    {
        /* ensure that border elements which are read but never actually set below are zeroed out so use calloc */
        float *const VH_Dir = dt_calloc_align_float((size_t) DT_RCD_TILESIZE * DT_RCD_TILESIZE);
        float *const PQ_Dir = dt_alloc_align_float((size_t) DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);
        float *const cfa =    dt_alloc_align_float((size_t) DT_RCD_TILESIZE * DT_RCD_TILESIZE);
        float *const P_CDiff_Hpf = dt_alloc_align_float((size_t) DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);
        float *const Q_CDiff_Hpf = dt_alloc_align_float((size_t) DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);

        float (*const rgb)[DT_RCD_TILESIZE * DT_RCD_TILESIZE] = (float (*)[DT_RCD_TILESIZE * DT_RCD_TILESIZE])dt_alloc_align_float((size_t)3 * DT_RCD_TILESIZE * DT_RCD_TILESIZE);

        /* No overlapping use so re-use same buffer */
        float *const lpf = PQ_Dir;

        for(int tile_vertical = 0; tile_vertical < num_vertical; tile_vertical++)
        {
            for(int tile_horizontal = 0; tile_horizontal < num_horizontal; tile_horizontal++)
            {
                const int rowStart = tile_vertical * RCD_TILEVALID;
                const int rowEnd = (rowStart + DT_RCD_TILESIZE < height) ? rowStart + DT_RCD_TILESIZE : height;

                const int colStart = tile_horizontal * RCD_TILEVALID;
                const int colEnd = (colStart + DT_RCD_TILESIZE < width) ? colStart + DT_RCD_TILESIZE : width;

                const int tileRows = (rowEnd - rowStart < DT_RCD_TILESIZE) ? rowEnd - rowStart : DT_RCD_TILESIZE;
                const int tileCols = (colEnd - colStart < DT_RCD_TILESIZE) ? colEnd - colStart : DT_RCD_TILESIZE;

                if(rowStart + DT_RCD_TILESIZE > height || colStart + DT_RCD_TILESIZE > width)
                {
                    /* VH_Dir is only filled for(4,4)..(height-4,width-4), but the refinement code reads (3,3)...(h-3,w-3), */
                    /* so we need to ensure that the border is zeroed for partial tiles to get consistent results */
                    memset(VH_Dir, 0, sizeof(float) * DT_RCD_TILESIZE * DT_RCD_TILESIZE);
                    memset(rgb, 0, sizeof(float) * 3 * DT_RCD_TILESIZE * DT_RCD_TILESIZE);
                }

                /* Step 0: fill data and make sure data are not negative. */
                for(int row = rowStart; row < rowEnd; row++)
                {
                    const int c0 = FC(row, colStart, filters);
                    const int c1 = FC(row, colStart + 1, filters);
                    for(int col = colStart, indx = (row - rowStart) * DT_RCD_TILESIZE, in_indx = row * width + colStart; col < colEnd; col++, indx++, in_indx++)
                    {
                        cfa[indx] = rgb[c0][indx] = rgb[c1][indx] = _safe_in(in[in_indx], revscaler);
                    }
                }

                /* STEP 1: Find vertical and horizontal interpolation directions */
                float bufferV[3][DT_RCD_TILESIZE - 8];
                /* Step 1.1: Calculate the square of the vertical and horizontal color difference high pass filter */
                for(int row = 3; row < (tileRows - 3 < 5 ? tileRows - 3 : 5); row++ )
                {
                    for(int col = 4, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++ )
                    {
                        bufferV[row - 3][col - 4] = sqrf((cfa[indx - w3] - cfa[indx - w1] - cfa[indx + w1] + cfa[indx + w3]) - 3.0f * (cfa[indx - w2] + cfa[indx + w2]) + 6.0f * cfa[indx]);
                    }
                }

                /* Step 1.2: Obtain the vertical and horizontal directional discrimination strength */
                float bufferH[DT_RCD_TILESIZE];
                /* We start with V0, V1 and V2 pointing to row -1, row and row +1 */
                float* V0 = bufferV[0];
                float* V1 = bufferV[1];
                float* V2 = bufferV[2];
                for(int row = 4; row < tileRows - 4; row++ )
                {
                    for(int col = 3, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 3; col++, indx++)
                    {
                        bufferH[col - 3] = sqrf((cfa[indx -  3] - cfa[indx -  1] - cfa[indx +  1] + cfa[indx +  3]) - 3.0f * (cfa[indx -  2] + cfa[indx +  2]) + 6.0f * cfa[indx]);
                    }
                    for(int col = 4, indx = (row + 1) * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++)
                    {
                        V2[col - 4] = sqrf((cfa[indx - w3] - cfa[indx - w1] - cfa[indx + w1] + cfa[indx + w3]) - 3.0f * (cfa[indx - w2] + cfa[indx + w2]) + 6.0f * cfa[indx]);
                    }
                    for(int col = 4, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++ )
                    {
                        const float V_Stat = fmaxf(epssq,      V0[col - 4] +      V1[col - 4] +      V2[col - 4]);
                        const float H_Stat = fmaxf(epssq, bufferH[col - 4] + bufferH[col - 3] + bufferH[col - 2]);
                        VH_Dir[indx] = V_Stat / ( V_Stat + H_Stat );
                    }
                    /* rolling the line pointers */
                    float* tmp = V0; V0 = V1; V1 = V2; V2 = tmp;
                }

                /* STEP 2: Calculate the low pass filter */
                /* Step 2.1: Low pass filter incorporating green, red and blue local samples from the raw data */
                for(int row = 2; row < tileRows - 2; row++)
                {
                    for(int col = 2 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, lp_indx = indx / 2; col < tileCols - 2; col += 2, indx +=2, lp_indx++)
                    {
                        lpf[lp_indx] = cfa[indx]
                                    + 0.5f * (cfa[indx - w1]     + cfa[indx + w1] +     cfa[indx - 1] +      cfa[indx + 1])
                                   + 0.25f * (cfa[indx - w1 - 1] + cfa[indx - w1 + 1] + cfa[indx + w1 - 1] + cfa[indx + w1 + 1]);
                    }
                }

                /* STEP 3: Populate the green channel */
                /* Step 3.1: Populate the green channel at blue and red CFA positions */
                for(int row = 4; row < tileRows - 4; row++)
                {
                    for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, lpindx = indx / 2; col < tileCols - 4; col += 2, indx += 2, lpindx++)
                    {
                        const float cfai = cfa[indx];

                        /* Cardinal gradients */
                        const float N_Grad = eps + fabsf(cfa[indx - w1] - cfa[indx + w1]) + fabsf(cfai - cfa[indx - w2]) + fabsf(cfa[indx - w1] - cfa[indx - w3]) + fabsf(cfa[indx - w2] - cfa[indx - w4]);
                        const float S_Grad = eps + fabsf(cfa[indx - w1] - cfa[indx + w1]) + fabsf(cfai - cfa[indx + w2]) + fabsf(cfa[indx + w1] - cfa[indx + w3]) + fabsf(cfa[indx + w2] - cfa[indx + w4]);
                        const float W_Grad = eps + fabsf(cfa[indx -  1] - cfa[indx +  1]) + fabsf(cfai - cfa[indx -  2]) + fabsf(cfa[indx -  1] - cfa[indx -  3]) + fabsf(cfa[indx -  2] - cfa[indx -  4]);
                        const float E_Grad = eps + fabsf(cfa[indx -  1] - cfa[indx +  1]) + fabsf(cfai - cfa[indx +  2]) + fabsf(cfa[indx +  1] - cfa[indx +  3]) + fabsf(cfa[indx +  2] - cfa[indx +  4]);

                        /* Cardinal pixel estimations */
                        const float lpfi = lpf[lpindx];
                        const float N_Est = cfa[indx - w1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx - w1]);
                        const float S_Est = cfa[indx + w1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx + w1]);
                        const float W_Est = cfa[indx -  1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx -  1]);
                        const float E_Est = cfa[indx +  1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx +  1]);

                        /* Vertical and horizontal estimations */
                        const float V_Est = (S_Grad * N_Est + N_Grad * S_Est) / (N_Grad + S_Grad);
                        const float H_Est = (W_Grad * E_Est + E_Grad * W_Est) / (E_Grad + W_Grad);

                        /* G@B and G@R interpolation */
                        /* Refined vertical and horizontal local discrimination */
                        const float VH_Central_Value = VH_Dir[indx];
                        const float VH_Neighbourhood_Value = 0.25f * (VH_Dir[indx - w1 - 1] + VH_Dir[indx - w1 + 1] + VH_Dir[indx + w1 - 1] + VH_Dir[indx + w1 + 1]);
                        const float VH_Disc = (fabsf(0.5f - VH_Central_Value) < fabsf(0.5f - VH_Neighbourhood_Value)) ? VH_Neighbourhood_Value : VH_Central_Value;

                        rgb[1][indx] = interpolatef(VH_Disc, H_Est, V_Est);
                    }
                }

                /* STEP 4: Populate the red and blue channels */

                /* Step 4.0: Calculate the square of the P/Q diagonals color difference high pass filter */
                for(int row = 3; row < tileRows - 3; row++)
                {
                    for(int col = 3, indx = row * DT_RCD_TILESIZE + col, indx2 = indx / 2; col < tileCols - 3; col+=2, indx+=2, indx2++)
                    {
                        P_CDiff_Hpf[indx2] = sqrf((cfa[indx - w3 - 3] - cfa[indx - w1 - 1] - cfa[indx + w1 + 1] + cfa[indx + w3 + 3]) - 3.0f * (cfa[indx - w2 - 2] + cfa[indx + w2 + 2]) + 6.0f * cfa[indx]);
                        Q_CDiff_Hpf[indx2] = sqrf((cfa[indx - w3 + 3] - cfa[indx - w1 + 1] - cfa[indx + w1 - 1] + cfa[indx + w3 - 3]) - 3.0f * (cfa[indx - w2 + 2] + cfa[indx + w2 - 2]) + 6.0f * cfa[indx]);
                    }
                }
                /* Step 4.1: Obtain the P/Q diagonals directional discrimination strength */
                for(int row = 4; row < tileRows - 4; row++)
                {
                    for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, indx2 = indx / 2, indx3 = (indx - w1 - 1) / 2, indx4 = (indx + w1 - 1) / 2; col < tileCols - 4; col += 2, indx += 2, indx2++, indx3++, indx4++ )
                    {
                        const float P_Stat = fmaxf(epssq, P_CDiff_Hpf[indx3]     + P_CDiff_Hpf[indx2] + P_CDiff_Hpf[indx4 + 1]);
                        const float Q_Stat = fmaxf(epssq, Q_CDiff_Hpf[indx3 + 1] + Q_CDiff_Hpf[indx2] + Q_CDiff_Hpf[indx4]);
                        PQ_Dir[indx2] = P_Stat / (P_Stat + Q_Stat);
                    }
                }

                /* Step 4.2: Populate the red and blue channels at blue and red CFA positions */
                for(int row = 4; row < tileRows - 4; row++)
                {
                    for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, c = 2 - FC(row, col, filters), pqindx = indx / 2, pqindx2 = (indx - w1 - 1) / 2, pqindx3 = (indx + w1 - 1) / 2; col < tileCols - 4; col += 2, indx += 2, pqindx++, pqindx2++, pqindx3++)
                    {
                        /* Refined P/Q diagonal local discrimination */
                        const float PQ_Central_Value   = PQ_Dir[pqindx];
                        const float PQ_Neighbourhood_Value = 0.25f * (PQ_Dir[pqindx2] + PQ_Dir[pqindx2 + 1] + PQ_Dir[pqindx3] + PQ_Dir[pqindx3 + 1]);

                        const float PQ_Disc = (fabsf(0.5f - PQ_Central_Value) < fabsf(0.5f - PQ_Neighbourhood_Value)) ? PQ_Neighbourhood_Value : PQ_Central_Value;

                        /* Diagonal gradients */
                        const float NW_Grad = eps + fabsf(rgb[c][indx - w1 - 1] - rgb[c][indx + w1 + 1]) + fabsf(rgb[c][indx - w1 - 1] - rgb[c][indx - w3 - 3]) + fabsf(rgb[1][indx] - rgb[1][indx - w2 - 2]);
                        const float NE_Grad = eps + fabsf(rgb[c][indx - w1 + 1] - rgb[c][indx + w1 - 1]) + fabsf(rgb[c][indx - w1 + 1] - rgb[c][indx - w3 + 3]) + fabsf(rgb[1][indx] - rgb[1][indx - w2 + 2]);
                        const float SW_Grad = eps + fabsf(rgb[c][indx - w1 + 1] - rgb[c][indx + w1 - 1]) + fabsf(rgb[c][indx + w1 - 1] - rgb[c][indx + w3 - 3]) + fabsf(rgb[1][indx] - rgb[1][indx + w2 - 2]);
                        const float SE_Grad = eps + fabsf(rgb[c][indx - w1 - 1] - rgb[c][indx + w1 + 1]) + fabsf(rgb[c][indx + w1 + 1] - rgb[c][indx + w3 + 3]) + fabsf(rgb[1][indx] - rgb[1][indx + w2 + 2]);

                        /* Diagonal colour differences */
                        const float NW_Est = rgb[c][indx - w1 - 1] - rgb[1][indx - w1 - 1];
                        const float NE_Est = rgb[c][indx - w1 + 1] - rgb[1][indx - w1 + 1];
                        const float SW_Est = rgb[c][indx + w1 - 1] - rgb[1][indx + w1 - 1];
                        const float SE_Est = rgb[c][indx + w1 + 1] - rgb[1][indx + w1 + 1];

                        /* P/Q estimations */
                        const float P_Est = (NW_Grad * SE_Est + SE_Grad * NW_Est) / (NW_Grad + SE_Grad);
                        const float Q_Est = (NE_Grad * SW_Est + SW_Grad * NE_Est) / (NE_Grad + SW_Grad);

                        /* R@B and B@R interpolation */
                        rgb[c][indx] = rgb[1][indx] + interpolatef(PQ_Disc, Q_Est, P_Est);
                    }
                }

                /* Step 4.3: Populate the red and blue channels at green CFA positions */
                for(int row = 4; row < tileRows - 4; row++)
                {
                    for(int col = 4 + (FC(row, 1, filters) & 1), indx = row * DT_RCD_TILESIZE + col; col < tileCols - 4; col += 2, indx +=2)
                    {
                        /* Refined vertical and horizontal local discrimination */
                        const float VH_Central_Value = VH_Dir[indx];
                        const float VH_Neighbourhood_Value = 0.25f * (VH_Dir[indx - w1 - 1] + VH_Dir[indx - w1 + 1] + VH_Dir[indx + w1 - 1] + VH_Dir[indx + w1 + 1]);
                        const float VH_Disc = (fabsf(0.5f - VH_Central_Value) < fabsf(0.5f - VH_Neighbourhood_Value) ) ? VH_Neighbourhood_Value : VH_Central_Value;
                        const float rgb1 = rgb[1][indx];
                        const float N1 = eps + fabsf(rgb1 - rgb[1][indx - w2]);
                        const float S1 = eps + fabsf(rgb1 - rgb[1][indx + w2]);
                        const float W1 = eps + fabsf(rgb1 - rgb[1][indx -  2]);
                        const float E1 = eps + fabsf(rgb1 - rgb[1][indx +  2]);

                        const float rgb1mw1 = rgb[1][indx - w1];
                        const float rgb1pw1 = rgb[1][indx + w1];
                        const float rgb1m1 =  rgb[1][indx - 1];
                        const float rgb1p1 =  rgb[1][indx + 1];

                        for(int c = 0; c <= 2; c += 2)
                        {
                            const float SNabs = fabsf(rgb[c][indx - w1] - rgb[c][indx + w1]);
                            const float EWabs = fabsf(rgb[c][indx -  1] - rgb[c][indx +  1]);

                            /* Cardinal gradients */
                            const float N_Grad = N1 + SNabs + fabsf(rgb[c][indx - w1] - rgb[c][indx - w3]);
                            const float S_Grad = S1 + SNabs + fabsf(rgb[c][indx + w1] - rgb[c][indx + w3]);
                            const float W_Grad = W1 + EWabs + fabsf(rgb[c][indx -  1] - rgb[c][indx -  3]);
                            const float E_Grad = E1 + EWabs + fabsf(rgb[c][indx +  1] - rgb[c][indx +  3]);

                            /* Cardinal colour differences */
                            const float N_Est = rgb[c][indx - w1] - rgb1mw1;
                            const float S_Est = rgb[c][indx + w1] - rgb1pw1;
                            const float W_Est = rgb[c][indx -  1] - rgb1m1;
                            const float E_Est = rgb[c][indx +  1] - rgb1p1;

                            /* Vertical and horizontal estimations */
                            const float V_Est = (N_Grad * S_Est + S_Grad * N_Est) / (N_Grad + S_Grad);
                            const float H_Est = (E_Grad * W_Est + W_Grad * E_Est) / (E_Grad + W_Grad);

                            /* R@G and B@G interpolation */
                            rgb[c][indx] = rgb1 + interpolatef(VH_Disc, H_Est, V_Est);
                        }
                    }
                }

                /* For the outermost tiles in all directions we can use a smaller border margin */
                const int first_vertical =   rowStart + ((tile_vertical == 0) ? RCD_MARGIN : RCD_BORDER);
                const int last_vertical =    rowEnd   - ((tile_vertical == num_vertical - 1)     ? RCD_MARGIN : RCD_BORDER);
                const int first_horizontal = colStart + ((tile_horizontal == 0) ? RCD_MARGIN : RCD_BORDER);
                const int last_horizontal =  colEnd   - ((tile_horizontal == num_horizontal - 1) ? RCD_MARGIN : RCD_BORDER);
                for(int row = first_vertical; row < last_vertical; row++)
                {
                    for(int col = first_horizontal, idx = (row - rowStart) * DT_RCD_TILESIZE + col - colStart, o_idx = (row * width + col) * 4; col < last_horizontal; col++, o_idx += 4, idx++)
                    {
                        out[o_idx]   = scaler * fmaxf(0.0f, rgb[0][idx]);
                        out[o_idx+1] = scaler * fmaxf(0.0f, rgb[1][idx]);
                        out[o_idx+2] = scaler * fmaxf(0.0f, rgb[2][idx]);
                        out[o_idx+3] = 0.0f;
                    }
                }
            }
        }
        dt_free_align(cfa);
        dt_free_align(rgb);
        dt_free_align(VH_Dir);
        dt_free_align(PQ_Dir);
        dt_free_align(P_CDiff_Hpf);
        dt_free_align(Q_CDiff_Hpf);
    }
}

/* ============================================================================
   demosaic_process - Main entry point

   Simplified version of DT's process() for RCD method only
   ============================================================================ */

void demosaic_process(
    const float* in,
    float* out,
    const PipeState* state,
    const DemosaicParams* p)
{
    const int width = state->width;
    const int height = state->height;
    const uint32_t filters = state->filters;

    /* scaler = dt_iop_get_processed_maximum(piece) = fmaxf(1.0f, max3f(processed_maximum))
       After rawprepare normalization, this should be 1.0 */
    const float scaler = 1.0f;

    rcd_demosaic(out, in, width, height, filters, scaler);
}

/* ============================================================================
   Reset to default values from DT (demosaic.c lines 148-162)
   ============================================================================ */

void demosaic_reset(DemosaicParams* p)
{
    p->green_eq = 0;           /* DT_IOP_GREEN_EQ_NO */
    p->median_thrs = 0.0f;
    p->color_smoothing = 0;    /* DT_DEMOSAIC_SMOOTH_OFF */
    p->demosaicing_method = 5; /* DT_IOP_DEMOSAIC_RCD */
    p->lmmse_refine = 1;       /* DT_LMMSE_REFINE_1 */
    p->dual_thrs = 0.2f;
    p->cs_radius = 0.0f;
    p->cs_thrs = 0.40f;
    p->cs_boost = 0.0f;
    p->cs_iter = 8;
    p->cs_center = 0.0f;
    p->cs_enabled = 0;         /* FALSE */
}

/* ============================================================================
   Helper: return DemosaicParams with defaults (RCD method)
   ============================================================================ */

static inline DemosaicParams demosaic_defaults(void)
{
    DemosaicParams p;
    p.green_eq = 0;
    p.median_thrs = 0.0f;
    p.color_smoothing = 0;
    p.demosaicing_method = 5;  /* RCD */
    p.lmmse_refine = 1;
    p.dual_thrs = 0.2f;
    p.cs_radius = 0.0f;
    p.cs_thrs = 0.40f;
    p.cs_boost = 0.0f;
    p.cs_iter = 8;
    p.cs_center = 0.0f;
    p.cs_enabled = 0;
    return p;
}

#undef DT_RCD_TILESIZE
#undef RCD_BORDER
#undef RCD_MARGIN
#undef RCD_TILEVALID
#undef w1
#undef w2
#undef w3
#undef w4
#undef eps
#undef epssq

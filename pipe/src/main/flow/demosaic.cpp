// demosaic.cpp - RCD Demosaicing
//
// CLEAN COPY from darktable: dark/lib/desk/src/iop/demosaicing/rcd.c
// Original: Luis Sanz Rodríguez (luis.sanz.rodriguez@gmail.com)
// Tiling: Ingo Weyrich (heckflosse67@gmx.de)
// Optimizations: Hanno Schwalm (hanno@schwalm-bremen.de)

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <vector>

namespace flow
{

// ============================================================================
// DT Macro translations
// ============================================================================

#define DT_RCD_TILESIZE 112
#define RCD_BORDER 9
#define RCD_MARGIN 7
#define RCD_TILEVALID (DT_RCD_TILESIZE - 2 * RCD_BORDER)
#define w1 DT_RCD_TILESIZE
#define w2 (2 * DT_RCD_TILESIZE)
#define w3 (3 * DT_RCD_TILESIZE)
#define w4 (4 * DT_RCD_TILESIZE)

#define eps 1e-5f
#define epssq 1e-10f

// FC: get bayer color at (row, col) for RGGB pattern
// filters = 0x94949494 for RGGB
static inline int FC(int row, int col, uint32_t filters)
{
    return (filters >> (((row << 1 & 14) | (col & 1)) << 1) & 3);
}

// Square function
static inline float sqrf(float x) { return x * x; }

// Linear interpolation: a * (1-t) + b * t, rewritten as a + t*(b-a)
static inline float interpolatef(float t, float a, float b)
{
    return a + t * (b - a);
}

// Safe input: clamp negative to zero, scale
static inline float _safe_in(float a, float scale)
{
    return std::fmax(0.0f, a) * scale;
}

// ============================================================================
// PPG Border interpolation (for tile edges and small images)
// Copied from rcd.c: rcd_ppg_border()
// ============================================================================

static void rcd_ppg_border(float *const out,
                           const float *const in,
                           const int width,
                           const int height,
                           const uint32_t filters,
                           const int margin)
{
    const int border = margin + 3;

    // Write approximated 3-pixel border region
    float sum[8];
    for(int j = 0; j < height; j++)
    {
        for(int i = 0; i < width; i++)
        {
            if(i == 3 && j >= 3 && j < height - 3) i = width - 3;
            if(i == width) break;
            std::memset(sum, 0, sizeof(float) * 8);
            for(int y = j - 1; y != j + 2; y++)
            {
                for(int x = i - 1; x != i + 2; x++)
                {
                    if((y >= 0) && (x >= 0) && (y < height) && (x < width))
                    {
                        const int f = FC(y, x, filters);
                        sum[f] += std::fmax(0.0f, in[(size_t)y * width + x]);
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
                    out[4 * ((size_t)j * width + i) + c] = std::fmax(0.0f, in[(size_t)j * width + i]);
            }
        }
    }

    // Green channel interpolation for border region
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
            const float pc = std::fmax(0.0f, buf_in[0]);
            if(c == 0 || c == 2)
            {
                color[c] = pc;
                const float pym  = std::fmax(0.0f, buf_in[-width * 1]);
                const float pym2 = std::fmax(0.0f, buf_in[-width * 2]);
                const float pym3 = std::fmax(0.0f, buf_in[-width * 3]);
                const float pyM  = std::fmax(0.0f, buf_in[+width * 1]);
                const float pyM2 = std::fmax(0.0f, buf_in[+width * 2]);
                const float pyM3 = std::fmax(0.0f, buf_in[+width * 3]);
                const float pxm  = std::fmax(0.0f, buf_in[-1]);
                const float pxm2 = std::fmax(0.0f, buf_in[-2]);
                const float pxm3 = std::fmax(0.0f, buf_in[-3]);
                const float pxM  = std::fmax(0.0f, buf_in[+1]);
                const float pxM2 = std::fmax(0.0f, buf_in[+2]);
                const float pxM3 = std::fmax(0.0f, buf_in[+3]);

                const float guessx = (pxm + pc + pxM) * 2.0f - pxM2 - pxm2;
                const float diffx = (std::fabs(pxm2 - pc) + std::fabs(pxM2 - pc) + std::fabs(pxm - pxM)) * 3.0f
                                    + (std::fabs(pxM3 - pxM) + std::fabs(pxm3 - pxm)) * 2.0f;
                const float guessy = (pym + pc + pyM) * 2.0f - pyM2 - pym2;
                const float diffy = (std::fabs(pym2 - pc) + std::fabs(pyM2 - pc) + std::fabs(pym - pyM)) * 3.0f
                                    + (std::fabs(pyM3 - pyM) + std::fabs(pym3 - pym)) * 2.0f;
                if(diffx > diffy)
                {
                    const float m = std::fmin(pym, pyM);
                    const float M = std::fmax(pym, pyM);
                    color[1] = std::fmax(std::fmin(guessy * .25f, M), m);
                }
                else
                {
                    const float m = std::fmin(pxm, pxM);
                    const float M = std::fmax(pxm, pxM);
                    color[1] = std::fmax(std::fmin(guessx * .25f, M), m);
                }
            }
            else
                color[1] = pc;

            color[3] = 0.0f;
            for(int k = 0; k < 4; k++)
                buf[k] = color[k];
            buf += 4;
            buf_in++;
        }
    }

    // R/B interpolation for border region
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

            if(c == 1 || c == 3) // green pixel
            {
                const float *nt = buf - linesize;
                const float *nb = buf + linesize;
                const float *nl = buf - 4;
                const float *nr = buf + 4;
                if(FC(j, i + 1, filters) == 0) // red neighbor in same row
                {
                    color[2] = (nt[2] + nb[2] + 2.0f * color[1] - nt[1] - nb[1]) * .5f;
                    color[0] = (nl[0] + nr[0] + 2.0f * color[1] - nl[1] - nr[1]) * .5f;
                }
                else
                {
                    color[0] = (nt[0] + nb[0] + 2.0f * color[1] - nt[1] - nb[1]) * .5f;
                    color[2] = (nl[2] + nr[2] + 2.0f * color[1] - nl[1] - nr[1]) * .5f;
                }
            }
            else
            {
                const float *ntl = buf - 4 - linesize;
                const float *ntr = buf + 4 - linesize;
                const float *nbl = buf - 4 + linesize;
                const float *nbr = buf + 4 + linesize;

                if(c == 0) // red pixel, fill blue
                {
                    const float diff1  = std::fabs(ntl[2] - nbr[2]) + std::fabs(ntl[1] - color[1]) + std::fabs(nbr[1] - color[1]);
                    const float guess1 = ntl[2] + nbr[2] + 2.0f * color[1] - ntl[1] - nbr[1];
                    const float diff2  = std::fabs(ntr[2] - nbl[2]) + std::fabs(ntr[1] - color[1]) + std::fabs(nbl[1] - color[1]);
                    const float guess2 = ntr[2] + nbl[2] + 2.0f * color[1] - ntr[1] - nbl[1];
                    if(diff1 > diff2)
                        color[2] = guess2 * .5f;
                    else if(diff1 < diff2)
                        color[2] = guess1 * .5f;
                    else
                        color[2] = (guess1 + guess2) * .25f;
                }
                else // blue pixel, fill red
                {
                    const float diff1  = std::fabs(ntl[0] - nbr[0]) + std::fabs(ntl[1] - color[1]) + std::fabs(nbr[1] - color[1]);
                    const float guess1 = ntl[0] + nbr[0] + 2.0f * color[1] - ntl[1] - nbr[1];
                    const float diff2  = std::fabs(ntr[0] - nbl[0]) + std::fabs(ntr[1] - color[1]) + std::fabs(nbl[1] - color[1]);
                    const float guess2 = ntr[0] + nbl[0] + 2.0f * color[1] - ntr[1] - nbl[1];
                    if(diff1 > diff2)
                        color[0] = guess2 * .5f;
                    else if(diff1 < diff2)
                        color[0] = guess1 * .5f;
                    else
                        color[0] = (guess1 + guess2) * .25f;
                }
            }
            for(int k = 0; k < 4; k++)
                buf[k] = color[k];
            buf += 4;
        }
    }
}

// ============================================================================
// RCD Demosaic - main algorithm
// Copied from rcd.c: rcd_demosaic()
// ============================================================================

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

    const int num_vertical = 1 + (height - 2 * RCD_BORDER - 1) / RCD_TILEVALID;
    const int num_horizontal = 1 + (width - 2 * RCD_BORDER - 1) / RCD_TILEVALID;

    // Allocate tile buffers (no OpenMP, single-threaded for now)
    std::vector<float> VH_Dir_buf((size_t)DT_RCD_TILESIZE * DT_RCD_TILESIZE, 0.0f);
    std::vector<float> PQ_Dir_buf((size_t)DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);
    std::vector<float> cfa_buf((size_t)DT_RCD_TILESIZE * DT_RCD_TILESIZE);
    std::vector<float> P_CDiff_Hpf_buf((size_t)DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);
    std::vector<float> Q_CDiff_Hpf_buf((size_t)DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);
    std::vector<float> rgb_buf((size_t)3 * DT_RCD_TILESIZE * DT_RCD_TILESIZE);

    float* VH_Dir = VH_Dir_buf.data();
    float* PQ_Dir = PQ_Dir_buf.data();
    float* cfa = cfa_buf.data();
    float* P_CDiff_Hpf = P_CDiff_Hpf_buf.data();
    float* Q_CDiff_Hpf = Q_CDiff_Hpf_buf.data();
    float (*rgb)[DT_RCD_TILESIZE * DT_RCD_TILESIZE] = (float (*)[DT_RCD_TILESIZE * DT_RCD_TILESIZE])rgb_buf.data();

    // No overlapping use so re-use same buffer
    float* lpf = PQ_Dir;

    for(int tile_vertical = 0; tile_vertical < num_vertical; tile_vertical++)
    {
        for(int tile_horizontal = 0; tile_horizontal < num_horizontal; tile_horizontal++)
        {
            const int rowStart = tile_vertical * RCD_TILEVALID;
            const int rowEnd = std::min(rowStart + DT_RCD_TILESIZE, height);

            const int colStart = tile_horizontal * RCD_TILEVALID;
            const int colEnd = std::min(colStart + DT_RCD_TILESIZE, width);

            const int tileRows = std::min(rowEnd - rowStart, DT_RCD_TILESIZE);
            const int tileCols = std::min(colEnd - colStart, DT_RCD_TILESIZE);

            if(rowStart + DT_RCD_TILESIZE > height || colStart + DT_RCD_TILESIZE > width)
            {
                std::memset(VH_Dir, 0, sizeof(float) * DT_RCD_TILESIZE * DT_RCD_TILESIZE);
                std::memset(rgb, 0, sizeof(float) * 3 * DT_RCD_TILESIZE * DT_RCD_TILESIZE);
            }

            // Step 0: fill data and make sure data are not negative
            for(int row = rowStart; row < rowEnd; row++)
            {
                const int c0 = FC(row, colStart, filters);
                const int c1 = FC(row, colStart + 1, filters);
                for(int col = colStart, indx = (row - rowStart) * DT_RCD_TILESIZE, in_indx = row * width + colStart;
                    col < colEnd; col++, indx++, in_indx++)
                {
                    cfa[indx] = rgb[c0][indx] = rgb[c1][indx] = _safe_in(in[in_indx], revscaler);
                }
            }

            // STEP 1: Find vertical and horizontal interpolation directions
            float bufferV[3][DT_RCD_TILESIZE - 8];

            // Step 1.1: Calculate the square of the vertical and horizontal color difference high pass filter
            for(int row = 3; row < std::min(tileRows - 3, 5); row++)
            {
                for(int col = 4, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++)
                {
                    bufferV[row - 3][col - 4] = sqrf((cfa[indx - w3] - cfa[indx - w1] - cfa[indx + w1] + cfa[indx + w3])
                                                     - 3.0f * (cfa[indx - w2] + cfa[indx + w2]) + 6.0f * cfa[indx]);
                }
            }

            // Step 1.2: Obtain the vertical and horizontal directional discrimination strength
            float bufferH[DT_RCD_TILESIZE];
            float* V0 = bufferV[0];
            float* V1 = bufferV[1];
            float* V2 = bufferV[2];

            for(int row = 4; row < tileRows - 4; row++)
            {
                for(int col = 3, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 3; col++, indx++)
                {
                    bufferH[col - 3] = sqrf((cfa[indx - 3] - cfa[indx - 1] - cfa[indx + 1] + cfa[indx + 3])
                                            - 3.0f * (cfa[indx - 2] + cfa[indx + 2]) + 6.0f * cfa[indx]);
                }
                for(int col = 4, indx = (row + 1) * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++)
                {
                    V2[col - 4] = sqrf((cfa[indx - w3] - cfa[indx - w1] - cfa[indx + w1] + cfa[indx + w3])
                                       - 3.0f * (cfa[indx - w2] + cfa[indx + w2]) + 6.0f * cfa[indx]);
                }
                for(int col = 4, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++)
                {
                    const float V_Stat = std::fmax(epssq, V0[col - 4] + V1[col - 4] + V2[col - 4]);
                    const float H_Stat = std::fmax(epssq, bufferH[col - 4] + bufferH[col - 3] + bufferH[col - 2]);
                    VH_Dir[indx] = V_Stat / (V_Stat + H_Stat);
                }
                // Rolling the line pointers
                float* tmp = V0; V0 = V1; V1 = V2; V2 = tmp;
            }

            // STEP 2: Calculate the low pass filter
            // Step 2.1: Low pass filter incorporating green, red and blue local samples
            for(int row = 2; row < tileRows - 2; row++)
            {
                for(int col = 2 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, lp_indx = indx / 2;
                    col < tileCols - 2; col += 2, indx += 2, lp_indx++)
                {
                    lpf[lp_indx] = cfa[indx]
                                 + 0.5f * (cfa[indx - w1] + cfa[indx + w1] + cfa[indx - 1] + cfa[indx + 1])
                                 + 0.25f * (cfa[indx - w1 - 1] + cfa[indx - w1 + 1] + cfa[indx + w1 - 1] + cfa[indx + w1 + 1]);
                }
            }

            // STEP 3: Populate the green channel
            // Step 3.1: Populate the green channel at blue and red CFA positions
            for(int row = 4; row < tileRows - 4; row++)
            {
                for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, lpindx = indx / 2;
                    col < tileCols - 4; col += 2, indx += 2, lpindx++)
                {
                    const float cfai = cfa[indx];

                    // Cardinal gradients
                    const float N_Grad = eps + std::fabs(cfa[indx - w1] - cfa[indx + w1]) + std::fabs(cfai - cfa[indx - w2])
                                       + std::fabs(cfa[indx - w1] - cfa[indx - w3]) + std::fabs(cfa[indx - w2] - cfa[indx - w4]);
                    const float S_Grad = eps + std::fabs(cfa[indx - w1] - cfa[indx + w1]) + std::fabs(cfai - cfa[indx + w2])
                                       + std::fabs(cfa[indx + w1] - cfa[indx + w3]) + std::fabs(cfa[indx + w2] - cfa[indx + w4]);
                    const float W_Grad = eps + std::fabs(cfa[indx - 1] - cfa[indx + 1]) + std::fabs(cfai - cfa[indx - 2])
                                       + std::fabs(cfa[indx - 1] - cfa[indx - 3]) + std::fabs(cfa[indx - 2] - cfa[indx - 4]);
                    const float E_Grad = eps + std::fabs(cfa[indx - 1] - cfa[indx + 1]) + std::fabs(cfai - cfa[indx + 2])
                                       + std::fabs(cfa[indx + 1] - cfa[indx + 3]) + std::fabs(cfa[indx + 2] - cfa[indx + 4]);

                    // Cardinal pixel estimations
                    const float lpfi = lpf[lpindx];
                    const float N_Est = cfa[indx - w1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx - w1]);
                    const float S_Est = cfa[indx + w1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx + w1]);
                    const float W_Est = cfa[indx - 1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx - 1]);
                    const float E_Est = cfa[indx + 1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx + 1]);

                    // Vertical and horizontal estimations
                    const float V_Est = (S_Grad * N_Est + N_Grad * S_Est) / (N_Grad + S_Grad);
                    const float H_Est = (W_Grad * E_Est + E_Grad * W_Est) / (E_Grad + W_Grad);

                    // G@B and G@R interpolation
                    const float VH_Central_Value = VH_Dir[indx];
                    const float VH_Neighbourhood_Value = 0.25f * (VH_Dir[indx - w1 - 1] + VH_Dir[indx - w1 + 1]
                                                                + VH_Dir[indx + w1 - 1] + VH_Dir[indx + w1 + 1]);
                    const float VH_Disc = (std::fabs(0.5f - VH_Central_Value) < std::fabs(0.5f - VH_Neighbourhood_Value))
                                         ? VH_Neighbourhood_Value : VH_Central_Value;

                    rgb[1][indx] = interpolatef(VH_Disc, H_Est, V_Est);
                }
            }

            // STEP 4: Populate the red and blue channels

            // Step 4.0: Calculate the square of the P/Q diagonals color difference high pass filter
            for(int row = 3; row < tileRows - 3; row++)
            {
                for(int col = 3, indx = row * DT_RCD_TILESIZE + col, indx2 = indx / 2;
                    col < tileCols - 3; col += 2, indx += 2, indx2++)
                {
                    P_CDiff_Hpf[indx2] = sqrf((cfa[indx - w3 - 3] - cfa[indx - w1 - 1] - cfa[indx + w1 + 1] + cfa[indx + w3 + 3])
                                             - 3.0f * (cfa[indx - w2 - 2] + cfa[indx + w2 + 2]) + 6.0f * cfa[indx]);
                    Q_CDiff_Hpf[indx2] = sqrf((cfa[indx - w3 + 3] - cfa[indx - w1 + 1] - cfa[indx + w1 - 1] + cfa[indx + w3 - 3])
                                             - 3.0f * (cfa[indx - w2 + 2] + cfa[indx + w2 - 2]) + 6.0f * cfa[indx]);
                }
            }

            // Step 4.1: Obtain the P/Q diagonals directional discrimination strength
            for(int row = 4; row < tileRows - 4; row++)
            {
                for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col,
                    indx2 = indx / 2, indx3 = (indx - w1 - 1) / 2, indx4 = (indx + w1 - 1) / 2;
                    col < tileCols - 4; col += 2, indx += 2, indx2++, indx3++, indx4++)
                {
                    const float P_Stat = std::fmax(epssq, P_CDiff_Hpf[indx3] + P_CDiff_Hpf[indx2] + P_CDiff_Hpf[indx4 + 1]);
                    const float Q_Stat = std::fmax(epssq, Q_CDiff_Hpf[indx3 + 1] + Q_CDiff_Hpf[indx2] + Q_CDiff_Hpf[indx4]);
                    PQ_Dir[indx2] = P_Stat / (P_Stat + Q_Stat);
                }
            }

            // Step 4.2: Populate the red and blue channels at blue and red CFA positions
            for(int row = 4; row < tileRows - 4; row++)
            {
                for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col,
                    c = 2 - FC(row, col, filters), pqindx = indx / 2,
                    pqindx2 = (indx - w1 - 1) / 2, pqindx3 = (indx + w1 - 1) / 2;
                    col < tileCols - 4; col += 2, indx += 2, pqindx++, pqindx2++, pqindx3++)
                {
                    // Refined P/Q diagonal local discrimination
                    const float PQ_Central_Value = PQ_Dir[pqindx];
                    const float PQ_Neighbourhood_Value = 0.25f * (PQ_Dir[pqindx2] + PQ_Dir[pqindx2 + 1]
                                                                + PQ_Dir[pqindx3] + PQ_Dir[pqindx3 + 1]);
                    const float PQ_Disc = (std::fabs(0.5f - PQ_Central_Value) < std::fabs(0.5f - PQ_Neighbourhood_Value))
                                         ? PQ_Neighbourhood_Value : PQ_Central_Value;

                    // Diagonal gradients
                    const float NW_Grad = eps + std::fabs(rgb[c][indx - w1 - 1] - rgb[c][indx + w1 + 1])
                                        + std::fabs(rgb[c][indx - w1 - 1] - rgb[c][indx - w3 - 3])
                                        + std::fabs(rgb[1][indx] - rgb[1][indx - w2 - 2]);
                    const float NE_Grad = eps + std::fabs(rgb[c][indx - w1 + 1] - rgb[c][indx + w1 - 1])
                                        + std::fabs(rgb[c][indx - w1 + 1] - rgb[c][indx - w3 + 3])
                                        + std::fabs(rgb[1][indx] - rgb[1][indx - w2 + 2]);
                    const float SW_Grad = eps + std::fabs(rgb[c][indx - w1 + 1] - rgb[c][indx + w1 - 1])
                                        + std::fabs(rgb[c][indx + w1 - 1] - rgb[c][indx + w3 - 3])
                                        + std::fabs(rgb[1][indx] - rgb[1][indx + w2 - 2]);
                    const float SE_Grad = eps + std::fabs(rgb[c][indx - w1 - 1] - rgb[c][indx + w1 + 1])
                                        + std::fabs(rgb[c][indx + w1 + 1] - rgb[c][indx + w3 + 3])
                                        + std::fabs(rgb[1][indx] - rgb[1][indx + w2 + 2]);

                    // Diagonal colour differences
                    const float NW_Est = rgb[c][indx - w1 - 1] - rgb[1][indx - w1 - 1];
                    const float NE_Est = rgb[c][indx - w1 + 1] - rgb[1][indx - w1 + 1];
                    const float SW_Est = rgb[c][indx + w1 - 1] - rgb[1][indx + w1 - 1];
                    const float SE_Est = rgb[c][indx + w1 + 1] - rgb[1][indx + w1 + 1];

                    // P/Q estimations
                    const float P_Est = (NW_Grad * SE_Est + SE_Grad * NW_Est) / (NW_Grad + SE_Grad);
                    const float Q_Est = (NE_Grad * SW_Est + SW_Grad * NE_Est) / (NE_Grad + SW_Grad);

                    // R@B and B@R interpolation
                    rgb[c][indx] = rgb[1][indx] + interpolatef(PQ_Disc, Q_Est, P_Est);
                }
            }

            // Step 4.3: Populate the red and blue channels at green CFA positions
            for(int row = 4; row < tileRows - 4; row++)
            {
                for(int col = 4 + (FC(row, 1, filters) & 1), indx = row * DT_RCD_TILESIZE + col;
                    col < tileCols - 4; col += 2, indx += 2)
                {
                    const float VH_Central_Value = VH_Dir[indx];
                    const float VH_Neighbourhood_Value = 0.25f * (VH_Dir[indx - w1 - 1] + VH_Dir[indx - w1 + 1]
                                                                + VH_Dir[indx + w1 - 1] + VH_Dir[indx + w1 + 1]);
                    const float VH_Disc = (std::fabs(0.5f - VH_Central_Value) < std::fabs(0.5f - VH_Neighbourhood_Value))
                                         ? VH_Neighbourhood_Value : VH_Central_Value;
                    const float rgb1 = rgb[1][indx];
                    const float N1 = eps + std::fabs(rgb1 - rgb[1][indx - w2]);
                    const float S1 = eps + std::fabs(rgb1 - rgb[1][indx + w2]);
                    const float W1 = eps + std::fabs(rgb1 - rgb[1][indx - 2]);
                    const float E1 = eps + std::fabs(rgb1 - rgb[1][indx + 2]);

                    const float rgb1mw1 = rgb[1][indx - w1];
                    const float rgb1pw1 = rgb[1][indx + w1];
                    const float rgb1m1 = rgb[1][indx - 1];
                    const float rgb1p1 = rgb[1][indx + 1];

                    for(int c = 0; c <= 2; c += 2)
                    {
                        const float SNabs = std::fabs(rgb[c][indx - w1] - rgb[c][indx + w1]);
                        const float EWabs = std::fabs(rgb[c][indx - 1] - rgb[c][indx + 1]);

                        // Cardinal gradients
                        const float N_Grad = N1 + SNabs + std::fabs(rgb[c][indx - w1] - rgb[c][indx - w3]);
                        const float S_Grad = S1 + SNabs + std::fabs(rgb[c][indx + w1] - rgb[c][indx + w3]);
                        const float W_Grad = W1 + EWabs + std::fabs(rgb[c][indx - 1] - rgb[c][indx - 3]);
                        const float E_Grad = E1 + EWabs + std::fabs(rgb[c][indx + 1] - rgb[c][indx + 3]);

                        // Cardinal colour differences
                        const float N_Est = rgb[c][indx - w1] - rgb1mw1;
                        const float S_Est = rgb[c][indx + w1] - rgb1pw1;
                        const float W_Est = rgb[c][indx - 1] - rgb1m1;
                        const float E_Est = rgb[c][indx + 1] - rgb1p1;

                        // Vertical and horizontal estimations
                        const float V_Est = (N_Grad * S_Est + S_Grad * N_Est) / (N_Grad + S_Grad);
                        const float H_Est = (E_Grad * W_Est + W_Grad * E_Est) / (E_Grad + W_Grad);

                        // R@G and B@G interpolation
                        rgb[c][indx] = rgb1 + interpolatef(VH_Disc, H_Est, V_Est);
                    }
                }
            }

            // Write output for this tile
            const int first_vertical = rowStart + ((tile_vertical == 0) ? RCD_MARGIN : RCD_BORDER);
            const int last_vertical = rowEnd - ((tile_vertical == num_vertical - 1) ? RCD_MARGIN : RCD_BORDER);
            const int first_horizontal = colStart + ((tile_horizontal == 0) ? RCD_MARGIN : RCD_BORDER);
            const int last_horizontal = colEnd - ((tile_horizontal == num_horizontal - 1) ? RCD_MARGIN : RCD_BORDER);

            for(int row = first_vertical; row < last_vertical; row++)
            {
                for(int col = first_horizontal, idx = (row - rowStart) * DT_RCD_TILESIZE + col - colStart,
                    o_idx = (row * width + col) * 4; col < last_horizontal; col++, o_idx += 4, idx++)
                {
                    out[o_idx]     = scaler * std::fmax(0.0f, rgb[0][idx]);
                    out[o_idx + 1] = scaler * std::fmax(0.0f, rgb[1][idx]);
                    out[o_idx + 2] = scaler * std::fmax(0.0f, rgb[2][idx]);
                    out[o_idx + 3] = 0.0f;
                }
            }
        }
    }
}

// ============================================================================
// DemosaicImpl - Flow integration
// ============================================================================

class DemosaicImpl : public Demosaic
{
public:
    std::string name() const override { return "demosaic"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();

        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        const float* bayer = flow.fdata();
        float* out = flow.rgb();

        // RGGB filter pattern = 0x94949494
        const uint32_t filters = 0x94949494;

        // Scaler = processed_maximum (max value after WB)
        // DT uses: fmaxf(1.0f, max3f(piece->pipe->dsc.processed_maximum))
        // We calculate max from the WB'd bayer data directly
        float max_val = 1.0f;
        for (size_t i = 0; i < npixels; i++) {
            if (bayer[i] > max_val) max_val = bayer[i];
        }
        const float scaler = max_val;

        rcd_demosaic(out, bayer, width, height, filters, scaler);
    }
};

std::unique_ptr<Demosaic> makeDemosaic()
{
    return std::make_unique<DemosaicImpl>();
}

} // namespace flow

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

#include "demosaic.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <memory>

namespace copy::modules::demosaic {

    static inline int FC(int row, int col, uint32_t filters) {
        return (filters >> (((row << 1 & 14) + (col & 1)) << 1)) & 3;
    }

    static inline float sqrf(float a) { return a * a; }
    static inline float interpolatef(float a, float b, float c) { return a * (b - c) + c; }
    static inline float _safe_in(float a, float scale) { return std::max(0.0f, a) * scale; }

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

    struct AlignedFree {
        void operator()(void* p) { std::free(p); }
    };

    template<typename T>
    using AlignedPtr = std::unique_ptr<T, AlignedFree>;

    static float* alloc_float(size_t n) {
        return static_cast<float*>(std::aligned_alloc(64, n * sizeof(float)));
    }

    static float* calloc_float(size_t n) {
        float* p = alloc_float(n);
        if (p) std::memset(p, 0, n * sizeof(float));
        return p;
    }

    static void rcd_ppg_border(float *out, const float *in, int width, int height, uint32_t filters, int margin) {
        const int border = margin + 3;
        float sum[8];
        for(int j = 0; j < height; j++) {
            for(int i = 0; i < width; i++) {
                if(i == 3 && j >= 3 && j < height - 3) i = width - 3;
                if(i == width) break;
                std::memset(sum, 0, sizeof(float) * 8);
                for(int y = j - 1; y != j + 2; y++) {
                    for(int x = i - 1; x != i + 2; x++) {
                        if((y >= 0) && (x >= 0) && (y < height) && (x < width)) {
                            int f = FC(y, x, filters);
                            sum[f] += std::max(0.0f, in[(size_t)y * width + x]);
                            sum[f + 4]++;
                        }
                    }
                }
                int f = FC(j, i, filters);
                for(int c = 0; c < 3; c++) {
                    if(c != f && sum[c + 4] > 0.0f)
                        out[4 * ((size_t)j * width + i) + c] = sum[c] / sum[c + 4];
                    else
                        out[4 * ((size_t)j * width + i) + c] = std::max(0.0f, in[(size_t)j * width + i]);
                }
            }
        }

        for(int j = 3; j < height - 3; j++) {
            float *buf = out + (size_t)4 * width * j + 4 * 3;
            const float *buf_in = in + (size_t)width * j + 3;
            for(int i = 3; i < width - 3; i++) {
                if(i == border && j >= border && j < height - border) {
                    i = width - border;
                    buf = out + (size_t)4 * width * j + 4 * i;
                    buf_in = in + (size_t)width * j + i;
                }
                if(i == width) break;

                int c = FC(j, i, filters);
                float color[4];
                float pc = std::max(0.0f, buf_in[0]);
                if(c == 0 || c == 2) {
                    color[c] = pc;
                    float pym  = std::max(0.0f, buf_in[-width]);
                    float pym2 = std::max(0.0f, buf_in[-width * 2]);
                    float pym3 = std::max(0.0f, buf_in[-width * 3]);
                    float pyM  = std::max(0.0f, buf_in[+width]);
                    float pyM2 = std::max(0.0f, buf_in[+width * 2]);
                    float pyM3 = std::max(0.0f, buf_in[+width * 3]);
                    float pxm  = std::max(0.0f, buf_in[-1]);
                    float pxm2 = std::max(0.0f, buf_in[-2]);
                    float pxm3 = std::max(0.0f, buf_in[-3]);
                    float pxM  = std::max(0.0f, buf_in[+1]);
                    float pxM2 = std::max(0.0f, buf_in[+2]);
                    float pxM3 = std::max(0.0f, buf_in[+3]);

                    float guessx = (pxm + pc + pxM) * 2.0f - pxM2 - pxm2;
                    float diffx = (std::abs(pxm2 - pc) + std::abs(pxM2 - pc) + std::abs(pxm - pxM)) * 3.0f
                                  + (std::abs(pxM3 - pxM) + std::abs(pxm3 - pxm)) * 2.0f;
                    float guessy = (pym + pc + pyM) * 2.0f - pyM2 - pym2;
                    float diffy = (std::abs(pym2 - pc) + std::abs(pyM2 - pc) + std::abs(pym - pyM)) * 3.0f
                                  + (std::abs(pyM3 - pyM) + std::abs(pym3 - pym)) * 2.0f;
                    if(diffx > diffy) {
                        float m = std::min(pym, pyM);
                        float M = std::max(pym, pyM);
                        color[1] = std::max(std::min(guessy * .25f, M), m);
                    } else {
                        float m = std::min(pxm, pxM);
                        float M = std::max(pxm, pxM);
                        color[1] = std::max(std::min(guessx * .25f, M), m);
                    }
                } else {
                    color[1] = pc;
                }
                color[3] = 0.0f;
                for(size_t k = 0; k < 4; k++) buf[k] = color[k];
                buf += 4;
                buf_in++;
            }
        }

        for(int j = 1; j < height - 1; j++) {
            float *buf = out + (size_t)4 * width * j + 4;
            for(int i = 1; i < width - 1; i++) {
                if(i == margin && j >= margin && j < height - margin) {
                    i = width - margin;
                    buf = out + (size_t)4 * (width * j + i);
                }
                int c = FC(j, i, filters);
                float color[4] = { buf[0], buf[1], buf[2], buf[3] };
                int linesize = 4 * width;
                if(c == 1 || c == 3) {
                    const float *nt = buf - linesize;
                    const float *nb = buf + linesize;
                    const float *nl = buf - 4;
                    const float *nr = buf + 4;
                    if(FC(j, i + 1, filters) == 0) {
                        color[2] = (nt[2] + nb[2] + 2.0f * color[1] - nt[1] - nb[1]) * .5f;
                        color[0] = (nl[0] + nr[0] + 2.0f * color[1] - nl[1] - nr[1]) * .5f;
                    } else {
                        color[0] = (nt[0] + nb[0] + 2.0f * color[1] - nt[1] - nb[1]) * .5f;
                        color[2] = (nl[2] + nr[2] + 2.0f * color[1] - nl[1] - nr[1]) * .5f;
                    }
                } else {
                    const float *ntl = buf - 4 - linesize;
                    const float *ntr = buf + 4 - linesize;
                    const float *nbl = buf - 4 + linesize;
                    const float *nbr = buf + 4 + linesize;
                    if(c == 0) {
                        float diff1 = std::abs(ntl[2] - nbr[2]) + std::abs(ntl[1] - color[1]) + std::abs(nbr[1] - color[1]);
                        float guess1 = ntl[2] + nbr[2] + 2.0f * color[1] - ntl[1] - nbr[1];
                        float diff2 = std::abs(ntr[2] - nbl[2]) + std::abs(ntr[1] - color[1]) + std::abs(nbl[1] - color[1]);
                        float guess2 = ntr[2] + nbl[2] + 2.0f * color[1] - ntr[1] - nbl[1];
                        if(diff1 > diff2) color[2] = guess2 * .5f;
                        else if(diff1 < diff2) color[2] = guess1 * .5f;
                        else color[2] = (guess1 + guess2) * .25f;
                    } else {
                        float diff1 = std::abs(ntl[0] - nbr[0]) + std::abs(ntl[1] - color[1]) + std::abs(nbr[1] - color[1]);
                        float guess1 = ntl[0] + nbr[0] + 2.0f * color[1] - ntl[1] - nbr[1];
                        float diff2 = std::abs(ntr[0] - nbl[0]) + std::abs(ntr[1] - color[1]) + std::abs(nbl[1] - color[1]);
                        float guess2 = ntr[0] + nbl[0] + 2.0f * color[1] - ntr[1] - nbl[1];
                        if(diff1 > diff2) color[0] = guess2 * .5f;
                        else if(diff1 < diff2) color[0] = guess1 * .5f;
                        else color[0] = (guess1 + guess2) * .25f;
                    }
                }
                for(size_t k = 0; k < 4; k++) buf[k] = color[k];
                buf += 4;
            }
        }
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const core::PipeState& state, const Params& p) {
        int width = state.width;
        int height = state.height;
        uint32_t filters = state.filters;
        float scaler = 1.0f;
        float revscaler = 1.0f;

        float* out_ptr = out.data();
        const float* in_ptr = in.data();

        if(width < 2*RCD_BORDER || height < 2*RCD_BORDER) {
            rcd_ppg_border(out_ptr, in_ptr, width, height, filters, RCD_BORDER);
            return;
        }

        rcd_ppg_border(out_ptr, in_ptr, width, height, filters, RCD_MARGIN);

        int num_vertical = 1 + (height - 2 * RCD_BORDER - 1) / RCD_TILEVALID;
        int num_horizontal = 1 + (width - 2 * RCD_BORDER - 1) / RCD_TILEVALID;

        #pragma omp parallel
        {
            AlignedPtr<float> VH_Dir(calloc_float(DT_RCD_TILESIZE * DT_RCD_TILESIZE));
            AlignedPtr<float> PQ_Dir(calloc_float(DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2));
            AlignedPtr<float> cfa(calloc_float(DT_RCD_TILESIZE * DT_RCD_TILESIZE));
            AlignedPtr<float> P_CDiff_Hpf(calloc_float(DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2));
            AlignedPtr<float> Q_CDiff_Hpf(calloc_float(DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2));
            AlignedPtr<float> rgb_buf(calloc_float(3 * DT_RCD_TILESIZE * DT_RCD_TILESIZE));
            
            // Helper to treat rgb_buf as float[3][size]
            auto rgb = [&](int c, size_t idx) -> float& {
                return rgb_buf.get()[c * DT_RCD_TILESIZE * DT_RCD_TILESIZE + idx];
            };

            float* lpf = PQ_Dir.get(); // reuse

            #pragma omp for collapse(2)
            for(int tile_vertical = 0; tile_vertical < num_vertical; tile_vertical++) {
                for(int tile_horizontal = 0; tile_horizontal < num_horizontal; tile_horizontal++) {
                    int rowStart = tile_vertical * RCD_TILEVALID;
                    int rowEnd = std::min(rowStart + DT_RCD_TILESIZE, height);
                    int colStart = tile_horizontal * RCD_TILEVALID;
                    int colEnd = std::min(colStart + DT_RCD_TILESIZE, width);
                    int tileRows = std::min(rowEnd - rowStart, DT_RCD_TILESIZE);
                    int tileCols = std::min(colEnd - colStart, DT_RCD_TILESIZE);

                    std::memset(VH_Dir.get(), 0, sizeof(float) * DT_RCD_TILESIZE * DT_RCD_TILESIZE);
                    std::memset(PQ_Dir.get(), 0, sizeof(float) * DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);
                    std::memset(cfa.get(), 0, sizeof(float) * DT_RCD_TILESIZE * DT_RCD_TILESIZE);
                    std::memset(P_CDiff_Hpf.get(), 0, sizeof(float) * DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);
                    std::memset(Q_CDiff_Hpf.get(), 0, sizeof(float) * DT_RCD_TILESIZE * DT_RCD_TILESIZE / 2);
                    std::memset(rgb_buf.get(), 0, sizeof(float) * 3 * DT_RCD_TILESIZE * DT_RCD_TILESIZE);

                    for(int row = rowStart; row < rowEnd; row++) {
                        int c0 = FC(row, colStart, filters);
                        int c1 = FC(row, colStart + 1, filters);
                        for(int col = colStart, indx = (row - rowStart) * DT_RCD_TILESIZE, in_indx = row * width + colStart; col < colEnd; col++, indx++, in_indx++) {
                            cfa.get()[indx] = rgb(c0, indx) = rgb(c1, indx) = _safe_in(in_ptr[in_indx], revscaler);
                        }
                    }

                    // Step 1
                    float bufferV[3][DT_RCD_TILESIZE - 8];
                    for(int row = 3; row < std::min(tileRows - 3, 5); row++) {
                        for(int col = 4, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++) {
                            bufferV[row - 3][col - 4] = sqrf((cfa.get()[indx - w3] - cfa.get()[indx - w1] - cfa.get()[indx + w1] + cfa.get()[indx + w3]) 
                                - 3.0f * (cfa.get()[indx - w2] + cfa.get()[indx + w2]) + 6.0f * cfa.get()[indx]);
                        }
                    }

                    float bufferH[DT_RCD_TILESIZE];
                    float* V0 = bufferV[0];
                    float* V1 = bufferV[1];
                    float* V2 = bufferV[2];

                    for(int row = 4; row < tileRows - 4; row++) {
                        for(int col = 3, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 3; col++, indx++) {
                            bufferH[col - 3] = sqrf((cfa.get()[indx - 3] - cfa.get()[indx - 1] - cfa.get()[indx + 1] + cfa.get()[indx + 3]) 
                                - 3.0f * (cfa.get()[indx - 2] + cfa.get()[indx + 2]) + 6.0f * cfa.get()[indx]);
                        }
                        for(int col = 4, indx = (row + 1) * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++) {
                            V2[col - 4] = sqrf((cfa.get()[indx - w3] - cfa.get()[indx - w1] - cfa.get()[indx + w1] + cfa.get()[indx + w3]) 
                                - 3.0f * (cfa.get()[indx - w2] + cfa.get()[indx + w2]) + 6.0f * cfa.get()[indx]);
                        }
                        for(int col = 4, indx = row * DT_RCD_TILESIZE + col; col < tileCols - 4; col++, indx++) {
                            float V_Stat = std::max(epssq, V0[col - 4] + V1[col - 4] + V2[col - 4]);
                            float H_Stat = std::max(epssq, bufferH[col - 4] + bufferH[col - 3] + bufferH[col - 2]);
                            VH_Dir.get()[indx] = V_Stat / (V_Stat + H_Stat);
                        }
                        float* tmp = V0; V0 = V1; V1 = V2; V2 = tmp;
                    }

                    // Step 2
                    for(int row = 2; row < tileRows - 2; row++) {
                        for(int col = 2 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, lp_indx = indx / 2; col < tileCols - 2; col += 2, indx += 2, lp_indx++) {
                            lpf[lp_indx] = cfa.get()[indx] + 0.5f * (cfa.get()[indx - w1] + cfa.get()[indx + w1] + cfa.get()[indx - 1] + cfa.get()[indx + 1])
                                           + 0.25f * (cfa.get()[indx - w1 - 1] + cfa.get()[indx - w1 + 1] + cfa.get()[indx + w1 - 1] + cfa.get()[indx + w1 + 1]);
                        }
                    }

                    // Step 3
                    for(int row = 4; row < tileRows - 4; row++) {
                        for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, lpindx = indx / 2; col < tileCols - 4; col += 2, indx += 2, lpindx++) {
                            float cfai = cfa.get()[indx];
                            float N_Grad = eps + std::abs(cfa.get()[indx - w1] - cfa.get()[indx + w1]) + std::abs(cfai - cfa.get()[indx - w2]) + std::abs(cfa.get()[indx - w1] - cfa.get()[indx - w3]) + std::abs(cfa.get()[indx - w2] - cfa.get()[indx - w4]);
                            float S_Grad = eps + std::abs(cfa.get()[indx - w1] - cfa.get()[indx + w1]) + std::abs(cfai - cfa.get()[indx + w2]) + std::abs(cfa.get()[indx + w1] - cfa.get()[indx + w3]) + std::abs(cfa.get()[indx + w2] - cfa.get()[indx + w4]);
                            float W_Grad = eps + std::abs(cfa.get()[indx - 1] - cfa.get()[indx + 1]) + std::abs(cfai - cfa.get()[indx - 2]) + std::abs(cfa.get()[indx - 1] - cfa.get()[indx - 3]) + std::abs(cfa.get()[indx - 2] - cfa.get()[indx - 4]);
                            float E_Grad = eps + std::abs(cfa.get()[indx - 1] - cfa.get()[indx + 1]) + std::abs(cfai - cfa.get()[indx + 2]) + std::abs(cfa.get()[indx + 1] - cfa.get()[indx + 3]) + std::abs(cfa.get()[indx + 2] - cfa.get()[indx + 4]);

                            float lpfi = lpf[lpindx];
                            float N_Est = cfa.get()[indx - w1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx - w1]);
                            float S_Est = cfa.get()[indx + w1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx + w1]);
                            float W_Est = cfa.get()[indx - 1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx - 1]);
                            float E_Est = cfa.get()[indx + 1] * (lpfi + lpfi) / (eps + lpfi + lpf[lpindx + 1]);

                            float V_Est = (S_Grad * N_Est + N_Grad * S_Est) / (N_Grad + S_Grad);
                            float H_Est = (W_Grad * E_Est + E_Grad * W_Est) / (E_Grad + W_Grad);

                            float VH_Central = VH_Dir.get()[indx];
                            float VH_Neigh = 0.25f * (VH_Dir.get()[indx - w1 - 1] + VH_Dir.get()[indx - w1 + 1] + VH_Dir.get()[indx + w1 - 1] + VH_Dir.get()[indx + w1 + 1]);
                            float VH_Disc = (std::abs(0.5f - VH_Central) < std::abs(0.5f - VH_Neigh)) ? VH_Neigh : VH_Central;

                            rgb(1, indx) = interpolatef(VH_Disc, H_Est, V_Est);
                        }
                    }

                    // Step 4
                    for(int row = 3; row < tileRows - 3; row++) {
                        for(int col = 3, indx = row * DT_RCD_TILESIZE + col, indx2 = indx / 2; col < tileCols - 3; col += 2, indx += 2, indx2++) {
                            P_CDiff_Hpf.get()[indx2] = sqrf((cfa.get()[indx - w3 - 3] - cfa.get()[indx - w1 - 1] - cfa.get()[indx + w1 + 1] + cfa.get()[indx + w3 + 3]) - 3.0f * (cfa.get()[indx - w2 - 2] + cfa.get()[indx + w2 + 2]) + 6.0f * cfa.get()[indx]);
                            Q_CDiff_Hpf.get()[indx2] = sqrf((cfa.get()[indx - w3 + 3] - cfa.get()[indx - w1 + 1] - cfa.get()[indx + w1 - 1] + cfa.get()[indx + w3 - 3]) - 3.0f * (cfa.get()[indx - w2 + 2] + cfa.get()[indx + w2 - 2]) + 6.0f * cfa.get()[indx]);
                        }
                    }

                    for(int row = 4; row < tileRows - 4; row++) {
                        for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, indx2 = indx / 2, indx3 = (indx - w1 - 1) / 2, indx4 = (indx + w1 - 1) / 2; col < tileCols - 4; col += 2, indx += 2, indx2++, indx3++, indx4++) {
                            float P_Stat = std::max(epssq, P_CDiff_Hpf.get()[indx3] + P_CDiff_Hpf.get()[indx2] + P_CDiff_Hpf.get()[indx4 + 1]);
                            float Q_Stat = std::max(epssq, Q_CDiff_Hpf.get()[indx3 + 1] + Q_CDiff_Hpf.get()[indx2] + Q_CDiff_Hpf.get()[indx4]);
                            PQ_Dir.get()[indx2] = P_Stat / (P_Stat + Q_Stat);
                        }
                    }

                    for(int row = 4; row < tileRows - 4; row++) {
                        for(int col = 4 + (FC(row, 0, filters) & 1), indx = row * DT_RCD_TILESIZE + col, c = 2 - FC(row, col, filters), pqindx = indx / 2, pqindx2 = (indx - w1 - 1) / 2, pqindx3 = (indx + w1 - 1) / 2; col < tileCols - 4; col += 2, indx += 2, pqindx++, pqindx2++, pqindx3++) {
                            float PQ_Central = PQ_Dir.get()[pqindx];
                            float PQ_Neigh = 0.25f * (PQ_Dir.get()[pqindx2] + PQ_Dir.get()[pqindx2 + 1] + PQ_Dir.get()[pqindx3] + PQ_Dir.get()[pqindx3 + 1]);
                            float PQ_Disc = (std::abs(0.5f - PQ_Central) < std::abs(0.5f - PQ_Neigh)) ? PQ_Neigh : PQ_Central;

                            float NW_Grad = eps + std::abs(rgb(c, indx - w1 - 1) - rgb(c, indx + w1 + 1)) + std::abs(rgb(c, indx - w1 - 1) - rgb(c, indx - w3 - 3)) + std::abs(rgb(1, indx) - rgb(1, indx - w2 - 2));
                            float NE_Grad = eps + std::abs(rgb(c, indx - w1 + 1) - rgb(c, indx + w1 - 1)) + std::abs(rgb(c, indx - w1 + 1) - rgb(c, indx - w3 + 3)) + std::abs(rgb(1, indx) - rgb(1, indx - w2 + 2));
                            float SW_Grad = eps + std::abs(rgb(c, indx - w1 + 1) - rgb(c, indx + w1 - 1)) + std::abs(rgb(c, indx + w1 - 1) - rgb(c, indx + w3 - 3)) + std::abs(rgb(1, indx) - rgb(1, indx + w2 - 2));
                            float SE_Grad = eps + std::abs(rgb(c, indx - w1 - 1) - rgb(c, indx + w1 + 1)) + std::abs(rgb(c, indx + w1 + 1) - rgb(c, indx + w3 + 3)) + std::abs(rgb(1, indx) - rgb(1, indx + w2 + 2));

                            float NW_Est = rgb(c, indx - w1 - 1) - rgb(1, indx - w1 - 1);
                            float NE_Est = rgb(c, indx - w1 + 1) - rgb(1, indx - w1 + 1);
                            float SW_Est = rgb(c, indx + w1 - 1) - rgb(1, indx + w1 - 1);
                            float SE_Est = rgb(c, indx + w1 + 1) - rgb(1, indx + w1 + 1);

                            float P_Est = (NW_Grad * SE_Est + SE_Grad * NW_Est) / (NW_Grad + SE_Grad);
                            float Q_Est = (NE_Grad * SW_Est + SW_Grad * NE_Est) / (NE_Grad + SW_Grad);

                            rgb(c, indx) = rgb(1, indx) + interpolatef(PQ_Disc, Q_Est, P_Est);
                        }
                    }

                    for(int row = 4; row < tileRows - 4; row++) {
                        for(int col = 4 + (FC(row, 1, filters) & 1), indx = row * DT_RCD_TILESIZE + col; col < tileCols - 4; col += 2, indx += 2) {
                            float VH_Central = VH_Dir.get()[indx];
                            float VH_Neigh = 0.25f * (VH_Dir.get()[indx - w1 - 1] + VH_Dir.get()[indx - w1 + 1] + VH_Dir.get()[indx + w1 - 1] + VH_Dir.get()[indx + w1 + 1]);
                            float VH_Disc = (std::abs(0.5f - VH_Central) < std::abs(0.5f - VH_Neigh)) ? VH_Neigh : VH_Central;
                            float rgb1 = rgb(1, indx);
                            float N1 = eps + std::abs(rgb1 - rgb(1, indx - w2));
                            float S1 = eps + std::abs(rgb1 - rgb(1, indx + w2));
                            float W1 = eps + std::abs(rgb1 - rgb(1, indx - 2));
                            float E1 = eps + std::abs(rgb1 - rgb(1, indx + 2));

                            float rgb1mw1 = rgb(1, indx - w1);
                            float rgb1pw1 = rgb(1, indx + w1);
                            float rgb1m1 =  rgb(1, indx - 1);
                            float rgb1p1 =  rgb(1, indx + 1);

                            for(int c = 0; c <= 2; c += 2) {
                                float SNabs = std::abs(rgb(c, indx - w1) - rgb(c, indx + w1));
                                float EWabs = std::abs(rgb(c, indx - 1) - rgb(c, indx + 1));
                                float N_Grad = N1 + SNabs + std::abs(rgb(c, indx - w1) - rgb(c, indx - w3));
                                float S_Grad = S1 + SNabs + std::abs(rgb(c, indx + w1) - rgb(c, indx + w3));
                                float W_Grad = W1 + EWabs + std::abs(rgb(c, indx - 1) - rgb(c, indx - 3));
                                float E_Grad = E1 + EWabs + std::abs(rgb(c, indx + 1) - rgb(c, indx + 3));

                                float N_Est = rgb(c, indx - w1) - rgb1mw1;
                                float S_Est = rgb(c, indx + w1) - rgb1pw1;
                                float W_Est = rgb(c, indx - 1) - rgb1m1;
                                float E_Est = rgb(c, indx + 1) - rgb1p1;

                                float V_Est = (N_Grad * S_Est + S_Grad * N_Est) / (N_Grad + S_Grad);
                                float H_Est = (E_Grad * W_Est + W_Grad * E_Est) / (E_Grad + W_Grad);

                                rgb(c, indx) = rgb1 + interpolatef(VH_Disc, H_Est, V_Est);
                            }
                        }
                    }

                    int first_vertical = rowStart + ((tile_vertical == 0) ? RCD_MARGIN : RCD_BORDER);
                    int last_vertical = rowEnd - ((tile_vertical == num_vertical - 1) ? RCD_MARGIN : RCD_BORDER);
                    int first_horizontal = colStart + ((tile_horizontal == 0) ? RCD_MARGIN : RCD_BORDER);
                    int last_horizontal = colEnd - ((tile_horizontal == num_horizontal - 1) ? RCD_MARGIN : RCD_BORDER);

                    for(int row = first_vertical; row < last_vertical; row++) {
                        for(int col = first_horizontal, idx = (row - rowStart) * DT_RCD_TILESIZE + col - colStart, o_idx = (row * width + col) * 4; col < last_horizontal; col++, o_idx += 4, idx++) {
                            out_ptr[o_idx] = scaler * std::max(0.0f, rgb(0, idx));
                            out_ptr[o_idx+1] = scaler * std::max(0.0f, rgb(1, idx));
                            out_ptr[o_idx+2] = scaler * std::max(0.0f, rgb(2, idx));
                            out_ptr[o_idx+3] = 0.0f;
                        }
                    }
                }
            }
        }
    }

}

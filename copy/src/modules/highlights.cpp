#include "highlights.hpp"
#include <cmath>
#include <vector>
#include <algorithm>

namespace copy::modules::highlights {

    static inline int FC(int row, int col, uint32_t filters) {
        return (filters >> (((row << 1 & 14) + (col & 1)) << 1)) & 3;
    }

    static inline float _calc_refavg(const float *in, uint32_t filters, int row, int col, int width, int height, const float *correction) {
        // Simplified version of _calc_refavg from segbased.c
        // Assumes linear=1, HL_POWERF=3.0f
        const int color = FC(row, col, filters);
        float mean[4] = {0};
        float cnt[4] = {0};

        const int dymin = std::max(0, row - 1);
        const int dxmin = std::max(0, col - 1);
        const int dymax = std::min(height - 1, row + 2);
        const int dxmax = std::min(width - 1, col + 2);

        for (int dy = dymin; dy < dymax; dy++) {
            for (int dx = dxmin; dx < dxmax; dx++) {
                float val = std::max(0.0f, in[dy * width + dx]);
                int c = FC(dy, dx, filters);
                mean[c] += val;
                cnt[c] += 1.0f;
            }
        }

        for(int c=0; c<4; c++) {
            if (cnt[c] > 0.0f) {
                mean[c] = std::pow((correction[c] * mean[c]) / cnt[c], 1.0f / 3.0f);
            }
        }

        const float croot_refavg[4] = {
            0.5f * (mean[1] + mean[2]),
            0.5f * (mean[0] + mean[2]),
            0.5f * (mean[0] + mean[1]),
            0.0f
        };

        return std::pow(croot_refavg[color], 3.0f);
    }

    static inline char _mask_dilated(const char *in, size_t w1) {
        if (in[0]) return 1;
        if (in[-w1-1] | in[-w1] | in[-w1+1] | in[-1] | in[1] | in[w1-1] | in[w1] | in[w1+1]) return 1;
        const size_t w2 = 2 * w1;
        const size_t w3 = 3 * w1;
        // Check 3-ring neighborhood
        // (Simplified check for brevity, assuming similar logic to original)
        // Original checked w3, w2, w1 rings.
        // Let's implement full check as per original for correctness
        
        // Ring 3
        if (in[-w3-2] | in[-w3-1] | in[-w3] | in[-w3+1] | in[-w3+2]) return 1;
        if (in[ w3-2] | in[ w3-1] | in[ w3] | in[ w3+1] | in[ w3+2]) return 1;
        
        // Ring 2
        if (in[-w2-3] | in[-w2-2] | in[-w2-1] | in[-w2] | in[-w2+1] | in[-w2+2] | in[-w2+3]) return 1;
        if (in[ w2-3] | in[ w2-2] | in[ w2-1] | in[ w2] | in[ w2+1] | in[ w2+2] | in[ w2+3]) return 1;

        // Ring 1 side extensions
        if (in[-w1-3] | in[-w1-2] | in[-w1+2] | in[-w1+3]) return 1;
        if (in[ w1-3] | in[ w1-2] | in[ w1+2] | in[ w1+3]) return 1;

        // Center line extensions
        if (in[-3] | in[-2] | in[2] | in[3]) return 1;

        return 0;
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const core::PipeState& state, const Params& p) {
        int width = in.width();
        int height = in.height();
        uint32_t filters = state.filters;
        const float* input = in.data();
        float* output = out.data();

        // OPPOSED mode magic
        float clipval = 0.987f * p.clip; 

        bool wbon = state.temperature.enabled;
        float icoeffs[4] = {
            wbon ? state.temperature.coeffs[0] : 1.0f,
            wbon ? state.temperature.coeffs[1] : 1.0f,
            wbon ? state.temperature.coeffs[2] : 1.0f,
            1.0f
        };
        float clips[4] = {
            clipval * icoeffs[0],
            clipval * icoeffs[1],
            clipval * icoeffs[2],
            clipval
        };

        bool late = state.chroma.late_correction;
        float correction[4] = {
            late ? (float)(state.chroma.D65coeffs[0] / state.chroma.as_shot[0]) : 1.0f,
            late ? (float)(state.chroma.D65coeffs[1] / state.chroma.as_shot[1]) : 1.0f,
            late ? (float)(state.chroma.D65coeffs[2] / state.chroma.as_shot[2]) : 1.0f,
            1.0f
        };

        size_t mwidth = width / 3;
        size_t mheight = height / 3;
        size_t msize = ((mwidth + 1) * (mheight + 1) + 15) & ~15;

        std::vector<char> mask(6 * msize, 0);
        bool anyclipped = false;

        // Pass 1
        #pragma omp parallel for reduction(|:anyclipped)
        for (int mrow = 1; mrow < (int)mheight - 1; mrow++) {
            for (int mcol = 1; mcol < (int)mwidth - 1; mcol++) {
                char mbuff[4] = {0};
                size_t grp = 3 * (mrow * width + mcol);
                for (int y = -1; y < 2; y++) {
                    for (int x = -1; x < 2; x++) {
                        size_t idx = grp + y * width + x;
                        int color = FC(mrow + y, mcol + x, filters);
                        if (input[idx] >= clips[color]) mbuff[color] = 1;
                    }
                }
                mbuff[1] |= mbuff[3];
                for (int c = 0; c < 3; c++) {
                    mask[c * msize + mrow * mwidth + mcol] = mbuff[c];
                    if (mbuff[c]) anyclipped = true;
                }
            }
        }

        float chrominance[4] = {0};
        
        if (anyclipped) {
            // Pass 2: Dilate
            // Note: Parallel dilation requires care or double buffering, but here we process strictly
            // separate from Pass 1. Dilation itself has dependencies, so standard simple loop is safe enough
            // if we accept slight race or process in order.
            // The original C code wasn't parallelized here.
            for (size_t row = 3; row < mheight - 3; row++) {
                for (size_t col = 3; col < mwidth - 3; col++) {
                    size_t mx = row * mwidth + col;
                    mask[3 * msize + mx] = _mask_dilated(&mask[mx], mwidth);
                    mask[4 * msize + mx] = _mask_dilated(&mask[msize + mx], mwidth);
                    mask[5 * msize + mx] = _mask_dilated(&mask[2 * msize + mx], mwidth);
                }
            }

            float sums[4] = {0};
            float cnts[4] = {0};
            float lo_clips[4] = {0.2f * clips[0], 0.2f * clips[1], 0.2f * clips[2], 1.0f};

            // Pass 3: Calculate chrominance
            for (int row = 3; row < (int)height - 3; row++) {
                for (int col = 3; col < (int)width - 3; col++) {
                    size_t idx = (size_t)row * width + col;
                    int color = FC(row, col, filters);
                    float inval = input[idx];

                    if (inval < clips[color] && inval > lo_clips[color] &&
                        mask[(color + 3) * msize + (row / 3) * mwidth + (col / 3)]) {
                        sums[color] += inval - _calc_refavg(input, filters, row, col, width, height, correction);
                        cnts[color] += 1.0f;
                    }
                }
            }

            for(int c=0; c<3; c++) {
                chrominance[c] = (cnts[c] > 100.0f) ? sums[c] / cnts[c] : 0.0f;
            }
        }

        // Pass 4: Apply
        #pragma omp parallel for
        for (int row = 0; row < height; row++) {
            for (int col = 0; col < width; col++) {
                size_t idx = (size_t)row * width + col;
                int color = FC(row, col, filters);
                float oval = std::max(0.0f, input[idx]);

                if (oval >= clips[color]) {
                    float ref = _calc_refavg(input, filters, row, col, width, height, correction);
                    oval = std::max(oval, ref + chrominance[color]);
                }
                output[idx] = oval;
            }
        }
    }

}

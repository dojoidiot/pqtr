#include "colorout.hpp"
#include <cmath>
#include <algorithm>

namespace copy::modules::colorout {

    static const float d50[4] = { 0.9642f, 1.0f, 0.8249f, 0.0f };

    static inline float lab_f_inv(float x) {
        const float epsilon = 0.20689655172413796f; 
        const float kappa = 24389.0f / 27.0f;
        return (x > epsilon) ? x * x * x : (116.0f * x - 16.0f) / kappa;
    }

    static inline void dt_Lab_to_XYZ(const float Lab[4], float XYZ[4]) {
        float f[4];
        f[1] = (Lab[0] + 16.0f) / 116.0f;
        f[0] = Lab[1] / 500.0f + f[1];
        f[2] = f[1] - Lab[2] / 200.0f;
        f[3] = Lab[3];
        for (int c = 0; c < 4; c++) XYZ[c] = d50[c] * lab_f_inv(f[c]);
    }

    static inline float srgb_gamma(float x) {
        if (x <= 0.0031308f) return 12.92f * x;
        else return 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
    }

    static float srgb_unbounded_coeffs[3];
    static bool srgb_coeffs_initialized = false;

    static void dt_iop_estimate_exp(const float *const x, const float *const y, const int num, float *coeff) {
        const float x0 = x[num - 1], y0 = y[num - 1];
        float g = 0.0f;
        int cnt = 0;
        for (int k = 0; k < num - 1; k++) {
            const float yy = y[k] / y0, xx = x[k] / x0;
            if (yy > 0.0f && xx > 0.0f) {
                g += std::log(y[k] / y0) / std::log(x[k] / x0);
                cnt++;
            }
        }
        if (cnt) g *= 1.0f / cnt; else g = 1.0f;
        coeff[0] = 1.0f / x0;
        coeff[1] = y0;
        coeff[2] = g;
    }

    static void init_srgb_unbounded_coeffs() {
        if (srgb_coeffs_initialized) return;
        const float x[4] = { 0.7f, 0.8f, 0.9f, 1.0f };
        const float y[4] = { srgb_gamma(0.7f), srgb_gamma(0.8f), srgb_gamma(0.9f), srgb_gamma(1.0f) };
        dt_iop_estimate_exp(x, y, 4, srgb_unbounded_coeffs);
        srgb_coeffs_initialized = true;
    }

    static inline float dt_iop_eval_exp(const float *const coeff, const float x) {
        return coeff[1] * std::pow(x * coeff[0], coeff[2]);
    }

    static inline float linear_to_sRGB(float linear) {
        float v = (linear < 0.0f) ? 0.0f : linear;
        if (v < 1.0f) return srgb_gamma(v);
        else return dt_iop_eval_exp(srgb_unbounded_coeffs, v);
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const float cmatrix[4][4]) {
        init_srgb_unbounded_coeffs();

        float row0[4] = { cmatrix[0][0], cmatrix[1][0], cmatrix[2][0], 0.0f };
        float row1[4] = { cmatrix[0][1], cmatrix[1][1], cmatrix[2][1], 0.0f };
        float row2[4] = { cmatrix[0][2], cmatrix[1][2], cmatrix[2][2], 0.0f };

        size_t count = in.count() / 4;
        const float* input = in.data();
        float* output = out.data();

        #pragma omp parallel for
        for (size_t k = 0; k < count; k++) {
            float XYZ[4];
            dt_Lab_to_XYZ(input + 4*k, XYZ);
            float rgb[4];
            for (int r = 0; r < 4; r++)
                rgb[r] = row0[r] * XYZ[0] + row1[r] * XYZ[1] + row2[r] * XYZ[2];
            
            output[4*k+0] = linear_to_sRGB(rgb[0]);
            output[4*k+1] = linear_to_sRGB(rgb[1]);
            output[4*k+2] = linear_to_sRGB(rgb[2]);
            output[4*k+3] = rgb[3];
        }
    }

}

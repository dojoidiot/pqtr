#include "denoiseprofile.hpp"
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <cstdio>

namespace copy::modules::denoiseprofile {

    struct NoiseProfile {
        const char* name;
        int iso;
        float a[3];
        float b[3];
    };

    static const NoiseProfile sony_ilce7m3_profiles[] = {
        {"ILCE-7M3 iso 100", 100, {3.61133e-06f, 1.39193e-06f, 2.02105e-06f}, {4.33706e-09f, 7.01676e-11f, 1.48355e-09f}},
        {"ILCE-7M3 iso 200", 200, {6.02756e-06f, 2.31907e-06f, 3.42744e-06f}, {1.05972e-08f, 3.48672e-09f, 5.73044e-09f}},
        {"ILCE-7M3 iso 400", 400, {1.06914e-05f, 4.09269e-06f, 6.13203e-06f}, {2.42335e-08f, 1.05841e-08f, 1.77707e-08f}},
        {"ILCE-7M3 iso 800", 800, {2.07473e-05f, 7.81336e-06f, 1.19115e-05f}, {3.40775e-08f, 1.35759e-08f, 2.65097e-08f}},
        {"ILCE-7M3 iso 1600", 1600, {3.94336e-05f, 1.48172e-05f, 2.27145e-05f}, {6.80892e-08f, 3.51493e-08f, 5.89197e-08f}},
        {"ILCE-7M3 iso 3200", 3200, {7.71938e-05f, 2.89964e-05f, 4.47198e-05f}, {1.22224e-07f, 7.95450e-08f, 1.24202e-07f}},
        {"ILCE-7M3 iso 6400", 6400, {1.52242e-04f, 5.74746e-05f, 8.87507e-05f}, {2.08717e-07f, 1.72654e-07f, 2.33172e-07f}},
        {"ILCE-7M3 iso 12800", 12800, {3.00050e-04f, 1.12560e-04f, 1.75664e-04f}, {3.21934e-07f, 3.33934e-07f, 3.92406e-07f}},
        {"ILCE-7M3 iso 25600", 25600, {5.89629e-04f, 2.21213e-04f, 3.46155e-04f}, {3.72155e-07f, 4.19684e-07f, 4.40553e-07f}},
        {nullptr, 0, {0,0,0}, {0,0,0}}
    };

    static const NoiseProfile generic_profile = {
        "generic poissonian", 0, {0.0001f, 0.0001f, 0.0001f}, {0.0f, 0.0f, 0.0f}
    };

    void set_profile(Params& p, const char* maker, const char* model, int iso) {
        const NoiseProfile* profiles = nullptr;
        if (maker && model) {
            if (strstr(maker, "SONY") || strstr(maker, "Sony")) {
                if (strstr(model, "ILCE-7M3") || strstr(model, "ILCE-7RM3") ||
                    strstr(model, "ILCE-7RM4") || strstr(model, "ILCE-7RM5")) {
                    profiles = sony_ilce7m3_profiles;
                }
            }
        }

        if (!profiles) {
            for (int c = 0; c < 3; c++) { p.a[c] = generic_profile.a[c]; p.b[c] = generic_profile.b[c]; }
            return;
        }

        const NoiseProfile* p1 = &profiles[0];
        const NoiseProfile* p2 = &profiles[0];

        for (int i = 0; profiles[i].name != nullptr; i++) {
            if (profiles[i].iso <= iso) p1 = &profiles[i];
            if (profiles[i].iso >= iso) { p2 = &profiles[i]; break; }
            p2 = &profiles[i];
        }

        if (p1->iso == p2->iso || iso <= p1->iso) {
            for (int c = 0; c < 3; c++) { p.a[c] = p1->a[c]; p.b[c] = p1->b[c]; }
        } else if (iso >= p2->iso) {
            for (int c = 0; c < 3; c++) { p.a[c] = p2->a[c]; p.b[c] = p2->b[c]; }
        } else {
            float t = (std::log(static_cast<float>(iso)) - std::log(static_cast<float>(p1->iso))) / (std::log(static_cast<float>(p2->iso)) - std::log(static_cast<float>(p1->iso)));
            for (int c = 0; c < 3; c++) {
                p.a[c] = p1->a[c] * std::pow(p2->a[c] / p1->a[c], t);
                p.b[c] = p1->b[c] + t * (p2->b[c] - p1->b[c]);
            }
        }
    }

    static inline float fast_sqrtf(float x) { return std::sqrt(x); }

    static void precondition(const float* in, float* out, int width, int height, const float* a, const float* b) {
        float sigma2[4];
        for(int c=0; c<3; c++) {
            float ratio = (a[c] > 1e-10f) ? (b[c] / a[c]) : 0.0f;
            sigma2[c] = ratio * ratio + 3.0f / 8.0f;
        }
        sigma2[3] = 0.0f;
        size_t count = (size_t)width * height;
        #pragma omp parallel for
        for(size_t i=0; i<count; i++) {
            for(int c=0; c<3; c++) {
                float val = in[i*4+c];
                float d = std::max(0.0f, val / a[c] + sigma2[c]);
                out[i*4+c] = 2.0f * fast_sqrtf(d);
            }
            out[i*4+3] = 0.0f;
        }
    }

    static void backtransform(float* buf, int width, int height, const float* a, const float* b) {
        float sigma2[3];
        for (int c = 0; c < 3; c++) {
            float ratio = (a[c] > 1e-10f) ? (b[c] / a[c]) : 0.0f;
            sigma2[c] = ratio * ratio + 1.0f / 8.0f;
        }
        const float sqrt_3_2 = std::sqrt(3.0f / 2.0f);
        size_t count = (size_t)width * height;
        #pragma omp parallel for
        for (size_t i = 0; i < count; i++) {
            for (int c = 0; c < 3; c++) {
                float x = buf[i * 4 + c];
                float x2 = x * x;
                if (x < 0.5f) {
                    buf[i * 4 + c] = 0.0f;
                } else {
                    float val = 0.25f * x2 + 0.25f * sqrt_3_2 / x - 11.0f / 8.0f / x2 + 5.0f / 8.0f * sqrt_3_2 / (x * x2) - sigma2[c];
                    buf[i * 4 + c] = a[c] * std::max(0.0f, val);
                }
            }
        }
    }

    static const float wavelet_filter[25] = {
        1.0f/256, 4.0f/256, 6.0f/256, 4.0f/256, 1.0f/256,
        4.0f/256, 16.0f/256, 24.0f/256, 16.0f/256, 4.0f/256,
        6.0f/256, 24.0f/256, 36.0f/256, 24.0f/256, 6.0f/256,
        4.0f/256, 16.0f/256, 24.0f/256, 16.0f/256, 4.0f/256,
        1.0f/256, 4.0f/256, 6.0f/256, 4.0f/256, 1.0f/256
    };

    static inline int clamp_idx(int x, int max) {
        if (x < 0) return 0;
        if (x >= max) return max - 1;
        return x;
    }

    static void wavelet_decompose(const float* in, float* coarse, float* detail, int width, int height, int scale) {
        int mult = 1 << scale;
        #pragma omp parallel for
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float sum[4] = {0, 0, 0, 0};
                float wgt = 0.0f;
                int fi = 0;
                for (int fy = -2; fy <= 2; fy++) {
                    for (int fx = -2; fx <= 2; fx++) {
                        int ny = clamp_idx(y + fy * mult, height);
                        int nx = clamp_idx(x + fx * mult, width);
                        float w = wavelet_filter[fi++];
                        wgt += w;
                        const float* px = in + (ny * width + nx) * 4;
                        for (int c = 0; c < 3; c++) sum[c] += w * px[c];
                    }
                }
                size_t idx = (y * width + x) * 4;
                for (int c = 0; c < 3; c++) {
                    float low = sum[c] / wgt;
                    coarse[idx + c] = low;
                    detail[idx + c] = in[idx + c] - low;
                }
                coarse[idx + 3] = 0.0f;
                detail[idx + 3] = 0.0f;
            }
        }
    }

    static void threshold_detail(float* detail, const float* threshold, int width, int height) {
        size_t count = (size_t)width * height;
        #pragma omp parallel for
        for (size_t i = 0; i < count; i++) {
            for (int c = 0; c < 3; c++) {
                float d = detail[i * 4 + c];
                float t = threshold[c];
                if (d > t) detail[i * 4 + c] = d - t;
                else if (d < -t) detail[i * 4 + c] = d + t;
                else detail[i * 4 + c] = 0.0f;
            }
        }
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const Params& p) {
        int width = in.width();
        int height = in.height();
        size_t count = in.count(); // actually size in floats
        size_t npixels = (size_t)width * height;

        std::vector<float> precond(count), buf1(count), buf2(count), detail(count);

        float a_scaled[3], b_scaled[3];
        for (int c = 0; c < 3; c++) {
            a_scaled[c] = p.a[c] * p.strength;
            b_scaled[c] = p.b[c] * p.strength;
        }

        precondition(in.data(), precond.data(), width, height, a_scaled, b_scaled);
        std::memcpy(buf1.data(), precond.data(), count * sizeof(float));
        std::memset(out.data(), 0, count * sizeof(float));

        int max_scale = p.max_scales;
        int min_dim = std::min(width, height);
        while ((1 << max_scale) * 4 > min_dim && max_scale > 1) max_scale--;

        for (int scale = 0; scale < max_scale; scale++) {
            wavelet_decompose(buf1.data(), buf2.data(), detail.data(), width, height, scale);
            float sigma_band = 1.0f;
            for (int s = 0; s < scale; s++) sigma_band *= 0.5f;
            float threshold[3];
            for (int c = 0; c < 3; c++) threshold[c] = sigma_band * p.strength * 1.5f;
            threshold_detail(detail.data(), threshold, width, height);
            
            float* out_data = out.data();
            const float* detail_data = detail.data();
            #pragma omp parallel for
            for (size_t i = 0; i < npixels * 4; i++) out_data[i] += detail_data[i];

            std::swap(buf1, buf2); // buf1 now contains coarse for next scale
        }

        float* out_data = out.data();
        const float* buf1_data = buf1.data();
        #pragma omp parallel for
        for (size_t i = 0; i < npixels * 4; i++) out_data[i] += buf1_data[i];

        backtransform(out.data(), width, height, a_scaled, b_scaled);
    }

}

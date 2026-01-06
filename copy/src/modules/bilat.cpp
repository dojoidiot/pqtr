#include "bilat.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>
#include <memory>

namespace copy::modules::bilat {

    // Helper functions
    static inline float dt_fast_expf(float x) { return std::exp(x); }
    static inline int my_clz(unsigned int x) { return __builtin_clz(x); }
    static inline float CLAMPS(float x, float lo, float hi) { return std::max(lo, std::min(x, hi)); }

    #define max_levels 30
    #define num_gamma 6

    static inline int dl(int size, const int level) {
        for(int l=0;l<level;l++) size = (size-1)/2+1;
        return size;
    }

    static inline float ll_expand_gaussian(const float *const coarse, const int i, const int j, const int wd, const int ht) {
        const int cw = (wd-1)/2+1;
        const int ind = (j/2)*cw+i/2;
        switch((i&1) + 2*(j&1)) {
            case 0: return 4./256. * (6.0f*(coarse[ind-cw] + coarse[ind-1] + 6.0f*coarse[ind] + coarse[ind+1] + coarse[ind+cw]) + coarse[ind-cw-1] + coarse[ind-cw+1] + coarse[ind+cw-1] + coarse[ind+cw+1]);
            case 1: return 4./256. * (24.0*(coarse[ind] + coarse[ind+1]) + 4.0*(coarse[ind-cw] + coarse[ind-cw+1] + coarse[ind+cw] + coarse[ind+cw+1]));
            case 2: return 4./256. * (24.0*(coarse[ind] + coarse[ind+cw]) + 4.0*(coarse[ind-1] + coarse[ind+1] + coarse[ind+cw-1] + coarse[ind+cw+1]));
            default: return .25f * (coarse[ind] + coarse[ind+1] + coarse[ind+cw] + coarse[ind+cw+1]);
        }
    }

    static inline void ll_fill_boundary2(float *const input, const int wd, const int ht) {
        for(int j=1;j<ht-1;j++) input[j*wd] = input[j*wd+1];
        if(wd & 1) for(int j=1;j<ht-1;j++) input[j*wd+wd-1] = input[j*wd+wd-2];
        else       for(int j=1;j<ht-1;j++) input[j*wd+wd-1] = input[j*wd+wd-2] = input[j*wd+wd-3];
        std::memcpy(input, input+wd, sizeof(float)*wd);
        if(!(ht & 1)) std::memcpy(input+wd*(ht-2), input+wd*(ht-3), sizeof(float)*wd);
        std::memcpy(input+wd*(ht-1), input+wd*(ht-2), sizeof(float)*wd);
    }

    static inline void ll_fill_boundary1(float *const input, const int wd, const int ht) {
        for(int j=1;j<ht-1;j++) input[j*wd] = input[j*wd+1];
        for(int j=1;j<ht-1;j++) input[j*wd+wd-1] = input[j*wd+wd-2];
        std::memcpy(input, input+wd, sizeof(float)*wd);
        std::memcpy(input+wd*(ht-1), input+wd*(ht-2), sizeof(float)*wd);
    }

    static void pad_by_replication(float *buf, const uint32_t w, const uint32_t h, const uint32_t padding) {
        #pragma omp parallel for
        for(int j=0;j<(int)padding;j++) {
            std::memcpy(buf + w*j, buf+padding*w, sizeof(float)*w);
            std::memcpy(buf + w*(h-padding+j), buf+w*(h-padding-1), sizeof(float)*w);
        }
    }

    static inline void gauss_expand(const float *const input, float *const fine, const int wd, const int ht) {
        #pragma omp parallel for
        for(int j=1;j<((ht-1)&~1);j++)
            for(int i=1;i<((wd-1)&~1);i++)
                fine[j*wd+i] = ll_expand_gaussian(input, i, j, wd, ht);
        ll_fill_boundary2(fine, wd, ht);
    }

    using AlignedPixel = float[4];

    static inline void _convolve_14641_vert(AlignedPixel conv, const float *in, const size_t wd) {
        static const AlignedPixel four = { 4.f, 4.f, 4.f, 4.f };
        AlignedPixel r0, r1, r2, r3, r4;
        for(int c=0;c<4;c++) {
            r0[c] = in[c]; r1[c] = in[wd+c]; r2[c] = in[2*wd+c]; r3[c] = in[3*wd+c]; r4[c] = in[4*wd+c];
        }
        AlignedPixel t;
        for(int c=0;c<4;c++) {
            r0[c] = r0[c] + r4[c];
            r1[c] = r1[c] + r2[c] + r3[c];
            r0[c] = r0[c] + r2[c] + r2[c];
            t[c] = r1[c] * four[c];
            conv[c] = r0[c] + t[c];
        }
    }

    static inline void copy_pixel(AlignedPixel out, const AlignedPixel in) {
        std::memcpy(out, in, sizeof(AlignedPixel));
    }

    static inline void gauss_reduce(const float *const input, float *const coarse, const size_t wd, const size_t ht) {
        const size_t cw = (wd-1)/2+1, ch = (ht-1)/2+1;
        #pragma omp parallel for
        for(size_t j=1;j<ch-1;j++) {
            const float *base = input + 2*(j-1)*wd;
            float *const out = coarse + j*cw + 1;
            static const AlignedPixel kernel = { 1.0f, 4.0f, 6.0f, 4.0f };
            AlignedPixel left;
            _convolve_14641_vert(left, base, wd);
            for(size_t col=0; col<cw-3; col += 2) {
                base += 4;
                AlignedPixel right;
                _convolve_14641_vert(right, base, wd);
                AlignedPixel conv;
                for(int c=0;c<4;c++) conv[c] = left[c] * kernel[c];
                out[col] = (conv[0] + conv[1] + conv[2] + conv[3] + right[0]) / 256.0f;
                out[col+1] = (left[2] + 4*(left[3]+right[1]) + 6.0f*right[0] + right[2]) / 256.0f;
                copy_pixel(left, right);
            }
            if(cw % 2) {
                base += 4;
                float right = base[0] + 4.0f*(base[wd]+base[3*wd]) + 6.0f*base[2*wd] + base[4*wd];
                AlignedPixel conv;
                for(int c=0;c<4;c++) conv[c] = left[c] * kernel[c];
                out[cw-3] = (conv[0] + conv[1] + conv[2] + conv[3] + right) / 256.0f;
            }
        }
        ll_fill_boundary1(coarse, cw, ch);
    }

    static float* alloc_float(size_t n) {
        return static_cast<float*>(std::aligned_alloc(64, n * sizeof(float)));
    }

    static inline float* ll_pad_input(const float *const input, const int wd, const int ht, const int max_supp, int *wd2, int *ht2) {
        const int stride = 4;
        *wd2 = 2*max_supp + wd;
        *ht2 = 2*max_supp + ht;
        float* out = alloc_float((size_t)*wd2 * *ht2);
        if(!out) return nullptr;
        #pragma omp parallel for
        for(int j=0;j<ht;j++) {
            for(int i=0;i<max_supp;i++) out[(j+max_supp)**wd2+i] = input[stride*wd*j]* 0.01f;
            for(int i=0;i<wd;i++) out[(j+max_supp)**wd2+i+max_supp] = input[stride*(wd*j+i)] * 0.01f;
            for(int i=wd+max_supp;i<*wd2;i++) out[(j+max_supp)**wd2+i] = input[stride*(j*wd+wd-1)] * 0.01f;
        }
        pad_by_replication(out, *wd2, *ht2, max_supp);
        return out;
    }

    static inline float ll_laplacian(const float *const coarse, const float *const fine, const int i, const int j, const int wd, const int ht) {
        const float c = ll_expand_gaussian(coarse, CLAMPS(i, 1, ((wd-1)&~1)-1), CLAMPS(j, 1, ((ht-1)&~1)-1), wd, ht);
        return fine[j*wd+i] - c;
    }

    static inline float curve_scalar(float x, float g, float sigma, float shadows, float highlights, float clarity) {
        float c = x-g;
        float val;
        if (c > 2*sigma) val = g + sigma + shadows * (c-sigma);
        else if(c < -2*sigma) val = g - sigma + highlights * (c+sigma);
        else if(c > 0.0f) {
            float t = CLAMPS(c / (2.0f*sigma), 0.0f, 1.0f);
            float t2 = t * t;
            float mt = 1.0f-t;
            val = g + sigma * 2.0f*mt*t + t2*(sigma + sigma*shadows);
        } else {
            float t = CLAMPS(-c / (2.0f*sigma), 0.0f, 1.0f);
            float t2 = t * t;
            float mt = 1.0f-t;
            val = g - sigma * 2.0f*mt*t + t2*(- sigma - sigma*highlights);
        }
        val += clarity * c * dt_fast_expf(-c*c/(2.0f*sigma*sigma/3.0f));
        return val;
    }

    static void apply_curve(float *const out, const float *const in, const uint32_t w, const uint32_t h, const uint32_t padding,
                            float g, float sigma, float shadows, float highlights, float clarity) {
        #pragma omp parallel for
        for(uint32_t j=padding;j<h-padding;j++) {
            const float *in2  = in  + j*w + padding;
            float *out2 = out + j*w + padding;
            for(uint32_t i=padding;i<w-padding;i++)
                (*out2++) = curve_scalar(*(in2++), g, sigma, shadows, highlights, clarity);
            out2 = out + j*w;
            for(uint32_t i=0;i<padding;i++) out2[i] = out2[padding];
            for(uint32_t i=w-padding;i<w;i++) out2[i] = out2[w-padding-1];
        }
        pad_by_replication(out, w, h, padding);
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const Params& p) {
        if(p.mode != 1) {
            std::memcpy(out.data(), in.data(), in.count() * sizeof(float));
            return;
        }

        int wd = in.width();
        int ht = in.height();
        if(wd <= 1 || ht <= 1) return;

        int num_levels = std::min(max_levels, 31 - my_clz(std::min(wd,ht)));
        int last_level = num_levels-1;
        int max_supp = 1<<last_level;
        int w, h;
        
        struct Free { void operator()(void* p) { std::free(p); } };
        using Ptr = std::unique_ptr<float, Free>;

        std::vector<Ptr> padded(max_levels);
        std::vector<Ptr> output(max_levels);
        std::vector<std::vector<Ptr>> buf(num_gamma);
        for(auto& v : buf) v.resize(max_levels);

        padded[0].reset(ll_pad_input(in.data(), wd, ht, max_supp, &w, &h));

        for(int l=1;l<=last_level;l++) padded[l].reset(alloc_float((size_t)dl(w,l) * dl(h,l)));
        for(int l=0;l<=last_level;l++) output[l].reset(alloc_float((size_t)dl(w,l) * dl(h,l)));
        for(int k=0;k<num_gamma;k++)
            for(int l=0;l<=last_level;l++)
                buf[k][l].reset(alloc_float((size_t)dl(w,l)*dl(h,l)));

        for(int l=1;l<last_level;l++) gauss_reduce(padded[l-1].get(), padded[l].get(), dl(w,l-1), dl(h,l-1));
        gauss_reduce(padded[last_level-1].get(), output[last_level].get(), dl(w,last_level-1), dl(h,last_level-1));

        float gamma[num_gamma];
        for(int k=0;k<num_gamma;k++) gamma[k] = (k+.5f)/(float)num_gamma;

        for(int k=0;k<num_gamma;k++) {
            apply_curve(buf[k][0].get(), padded[0].get(), w, h, max_supp, gamma[k], p.midtone, p.sigma_s, p.sigma_r, p.detail);
            for(int l=1;l<=last_level;l++)
                gauss_reduce(buf[k][l-1].get(), buf[k][l].get(), dl(w,l-1), dl(h,l-1));
        }

        for(int l=last_level-1;l >= 0; l--) {
            const int pw = dl(w,l), ph = dl(h,l);
            gauss_expand(output[l+1].get(), output[l].get(), pw, ph);
            #pragma omp parallel for
            for(int j=0;j<ph;j++) for(int i=0;i<pw;i++) {
                float v = padded[l].get()[j*pw+i];
                int hi = 1;
                for(;hi<num_gamma-1 && gamma[hi] <= v;hi++);
                int lo = hi-1;
                float a = CLAMPS((v - gamma[lo])/(gamma[hi]-gamma[lo]), 0.0f, 1.0f);
                float l0 = ll_laplacian(buf[lo][l+1].get(), buf[lo][l].get(), i, j, pw, ph);
                float l1 = ll_laplacian(buf[hi][l+1].get(), buf[hi][l].get(), i, j, pw, ph);
                output[l].get()[j*pw+i] += l0 * (1.0f-a) + l1 * a;
            }
        }

        float* out_data = out.data();
        const float* in_data = in.data();
        #pragma omp parallel for
        for(int j=0;j<ht;j++) for(int i=0;i<wd;i++) {
            out_data[4*(j*wd+i)+0] = 100.0f * output[0].get()[(j+max_supp)*w+max_supp+i];
            out_data[4*(j*wd+i)+1] = in_data[4*(j*wd+i)+1];
            out_data[4*(j*wd+i)+2] = in_data[4*(j*wd+i)+2];
            out_data[4*(j*wd+i)+3] = in_data[4*(j*wd+i)+3];
        }
    }

}

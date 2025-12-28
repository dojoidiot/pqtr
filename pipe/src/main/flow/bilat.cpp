// bilat.cpp - Local contrast (Local Laplacian filter)
//
// CLEAN COPY from darktable src/common/locallaplacian.c
// Multi-scale edge-aware contrast enhancement on Lab L channel.

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

namespace flow
{

// CLEAN COPY from DT locallaplacian.c
static constexpr int max_levels = 30;
static constexpr int num_gamma = 6;

// downsample width/height to given level
static inline int dl(int size, const int level)
{
    for (int l = 0; l < level; l++)
        size = (size - 1) / 2 + 1;
    return size;
}

// CLEAN COPY from DT locallaplacian.c ll_expand_gaussian
static inline float ll_expand_gaussian(
    const float *const coarse,
    const int i,
    const int j,
    const int wd,
    const int ht)
{
    const int cw = (wd - 1) / 2 + 1;
    const int ind = (j / 2) * cw + i / 2;
    switch ((i & 1) + 2 * (j & 1))
    {
    case 0: // both even, 3x3 stencil
        return 4.f / 256.f * (
            6.0f * (coarse[ind - cw] + coarse[ind - 1] + 6.0f * coarse[ind] + coarse[ind + 1] + coarse[ind + cw])
            + coarse[ind - cw - 1] + coarse[ind - cw + 1] + coarse[ind + cw - 1] + coarse[ind + cw + 1]);
    case 1: // i is odd, 2x3 stencil
        return 4.f / 256.f * (
            24.0f * (coarse[ind] + coarse[ind + 1]) +
            4.0f * (coarse[ind - cw] + coarse[ind - cw + 1] + coarse[ind + cw] + coarse[ind + cw + 1]));
    case 2: // j is odd, 3x2 stencil
        return 4.f / 256.f * (
            24.0f * (coarse[ind] + coarse[ind + cw]) +
            4.0f * (coarse[ind - 1] + coarse[ind + 1] + coarse[ind + cw - 1] + coarse[ind + cw + 1]));
    default: // case 3: both odd, 2x2 stencil
        return .25f * (coarse[ind] + coarse[ind + 1] + coarse[ind + cw] + coarse[ind + cw + 1]);
    }
}

// CLEAN COPY from DT locallaplacian.c ll_fill_boundary1
static inline void ll_fill_boundary1(float *const input, const int wd, const int ht)
{
    for (int j = 1; j < ht - 1; j++) input[j * wd] = input[j * wd + 1];
    for (int j = 1; j < ht - 1; j++) input[j * wd + wd - 1] = input[j * wd + wd - 2];
    memcpy(input, input + wd, sizeof(float) * wd);
    memcpy(input + wd * (ht - 1), input + wd * (ht - 2), sizeof(float) * wd);
}

// CLEAN COPY from DT locallaplacian.c ll_fill_boundary2
static inline void ll_fill_boundary2(float *const input, const int wd, const int ht)
{
    for (int j = 1; j < ht - 1; j++) input[j * wd] = input[j * wd + 1];
    if (wd & 1)
        for (int j = 1; j < ht - 1; j++) input[j * wd + wd - 1] = input[j * wd + wd - 2];
    else
        for (int j = 1; j < ht - 1; j++) input[j * wd + wd - 1] = input[j * wd + wd - 2] = input[j * wd + wd - 3];
    memcpy(input, input + wd, sizeof(float) * wd);
    if (!(ht & 1)) memcpy(input + wd * (ht - 2), input + wd * (ht - 3), sizeof(float) * wd);
    memcpy(input + wd * (ht - 1), input + wd * (ht - 2), sizeof(float) * wd);
}

// CLEAN COPY from DT locallaplacian.c pad_by_replication
static void pad_by_replication(float *buf, const int w, const int h, const int padding)
{
    for (int j = 0; j < padding; j++)
    {
        memcpy(buf + w * j, buf + padding * w, sizeof(float) * w);
        memcpy(buf + w * (h - padding + j), buf + w * (h - padding - 1), sizeof(float) * w);
    }
}

// CLEAN COPY from DT locallaplacian.c gauss_expand
static inline void gauss_expand(
    const float *const input,
    float *const fine,
    const int wd,
    const int ht)
{
    for (int j = 1; j < ((ht - 1) & ~1); j++)
        for (int i = 1; i < ((wd - 1) & ~1); i++)
            fine[j * wd + i] = ll_expand_gaussian(input, i, j, wd, ht);
    ll_fill_boundary2(fine, wd, ht);
}

// CLEAN COPY from DT locallaplacian.c gauss_reduce
static inline void gauss_reduce(
    const float *const input,
    float *const coarse,
    const size_t wd,
    const size_t ht)
{
    const size_t cw = (wd - 1) / 2 + 1, ch = (ht - 1) / 2 + 1;
    for (size_t j = 1; j < ch - 1; j++)
    {
        for (size_t i = 1; i < cw - 1; i++)
        {
            // 5x5 Gaussian kernel [1 4 6 4 1]
            float sum = 0.0f;
            const size_t fi = 2 * i, fj = 2 * j;
            for (int dj = -2; dj <= 2; dj++)
            {
                const float ky = (dj == 0) ? 6.0f : (std::abs(dj) == 1) ? 4.0f : 1.0f;
                for (int di = -2; di <= 2; di++)
                {
                    const float kx = (di == 0) ? 6.0f : (std::abs(di) == 1) ? 4.0f : 1.0f;
                    size_t si = std::min(std::max(fi + di, (size_t)0), wd - 1);
                    size_t sj = std::min(std::max(fj + dj, (size_t)0), ht - 1);
                    sum += kx * ky * input[sj * wd + si];
                }
            }
            coarse[j * cw + i] = sum / 256.0f;
        }
    }
    ll_fill_boundary1(coarse, cw, ch);
}

// CLEAN COPY from DT locallaplacian.c ll_laplacian
static inline float ll_laplacian(
    const float *const coarse,
    const float *const fine,
    const int i,
    const int j,
    const int wd,
    const int ht)
{
    const int ci = std::max(1, std::min(i, ((wd - 1) & ~1) - 1));
    const int cj = std::max(1, std::min(j, ((ht - 1) & ~1) - 1));
    const float c = ll_expand_gaussian(coarse, ci, cj, wd, ht);
    return fine[j * wd + i] - c;
}

// CLEAN COPY from DT locallaplacian.c curve_scalar
static inline float curve_scalar(
    const float x,
    const float g,
    const float sigma,
    const float shadows,
    const float highlights,
    const float clarity)
{
    const float c = x - g;
    float val;
    // blend via quadratic bezier
    if (c > 2 * sigma)
        val = g + sigma + shadows * (c - sigma);
    else if (c < -2 * sigma)
        val = g - sigma + highlights * (c + sigma);
    else if (c > 0.0f)
    {
        // shadow contrast
        const float t = std::max(0.0f, std::min(c / (2.0f * sigma), 1.0f));
        const float t2 = t * t;
        const float mt = 1.0f - t;
        val = g + sigma * 2.0f * mt * t + t2 * (sigma + sigma * shadows);
    }
    else
    {
        // highlight contrast
        const float t = std::max(0.0f, std::min(-c / (2.0f * sigma), 1.0f));
        const float t2 = t * t;
        const float mt = 1.0f - t;
        val = g - sigma * 2.0f * mt * t + t2 * (-sigma - sigma * highlights);
    }
    // midtone local contrast
    val += clarity * c * std::exp(-c * c / (2.0f * sigma * sigma / 3.0f));
    return val;
}

// CLEAN COPY from DT locallaplacian.c apply_curve
static void apply_curve(
    float *const out,
    const float *const in,
    const int w,
    const int h,
    const int padding,
    const float g,
    const float sigma,
    const float shadows,
    const float highlights,
    const float clarity)
{
    for (int j = padding; j < h - padding; j++)
    {
        const float *in2 = in + j * w + padding;
        float *out2 = out + j * w + padding;
        for (int i = padding; i < w - padding; i++)
            (*out2++) = curve_scalar(*(in2++), g, sigma, shadows, highlights, clarity);
        out2 = out + j * w;
        for (int i = 0; i < padding; i++) out2[i] = out2[padding];
        for (int i = w - padding; i < w; i++) out2[i] = out2[w - padding - 1];
    }
    pad_by_replication(out, w, h, padding);
}

// CLEAN COPY from DT locallaplacian.c local_laplacian_internal (simplified)
static void local_laplacian_internal(
    const float *const input,   // Lab buffer (4 floats per pixel)
    float *const out,           // output Lab buffer
    const int wd,
    const int ht,
    const float sigma,          // midtone
    const float shadows,        // shadow contrast
    const float highlights,     // highlight contrast
    const float clarity)        // detail
{
    if (wd <= 1 || ht <= 1) return;

    // Calculate pyramid levels
    const int num_levels = std::min(max_levels, 31 - __builtin_clz(std::min(wd, ht)));
    const int last_level = num_levels - 1;
    const int max_supp = 1 << last_level;

    // Padded dimensions
    const int w = 2 * max_supp + wd;
    const int h = 2 * max_supp + ht;

    // Allocate padded input (L channel only, [0,1])
    std::vector<float> padded0(w * h);
    for (int j = 0; j < ht; j++)
    {
        for (int i = 0; i < max_supp; i++)
            padded0[(j + max_supp) * w + i] = input[4 * wd * j] * 0.01f;
        for (int i = 0; i < wd; i++)
            padded0[(j + max_supp) * w + i + max_supp] = input[4 * (wd * j + i)] * 0.01f;
        for (int i = wd + max_supp; i < w; i++)
            padded0[(j + max_supp) * w + i] = input[4 * (j * wd + wd - 1)] * 0.01f;
    }
    pad_by_replication(padded0.data(), w, h, max_supp);

    // Allocate pyramid levels
    std::vector<std::vector<float>> padded(num_levels);
    std::vector<std::vector<float>> output(num_levels);
    padded[0] = std::move(padded0);
    for (int l = 1; l <= last_level; l++)
        padded[l].resize(dl(w, l) * dl(h, l));
    for (int l = 0; l <= last_level; l++)
        output[l].resize(dl(w, l) * dl(h, l));

    // Build gaussian pyramid of padded input
    for (int l = 1; l < last_level; l++)
        gauss_reduce(padded[l - 1].data(), padded[l].data(), dl(w, l - 1), dl(h, l - 1));
    gauss_reduce(padded[last_level - 1].data(), output[last_level].data(),
                 dl(w, last_level - 1), dl(h, last_level - 1));

    // Evenly sample brightness [0,1]
    float gamma[num_gamma];
    for (int k = 0; k < num_gamma; k++)
        gamma[k] = (k + 0.5f) / (float)num_gamma;

    // Allocate remapped pyramids
    std::vector<std::vector<std::vector<float>>> buf(num_gamma);
    for (int k = 0; k < num_gamma; k++)
    {
        buf[k].resize(num_levels);
        for (int l = 0; l <= last_level; l++)
            buf[k][l].resize(dl(w, l) * dl(h, l));
    }

    // Build remapped gaussian pyramids
    for (int k = 0; k < num_gamma; k++)
    {
        apply_curve(buf[k][0].data(), padded[0].data(), w, h, max_supp,
                    gamma[k], sigma, shadows, highlights, clarity);
        for (int l = 1; l <= last_level; l++)
            gauss_reduce(buf[k][l - 1].data(), buf[k][l].data(), dl(w, l - 1), dl(h, l - 1));
    }

    // Assemble output pyramid coarse to fine
    for (int l = last_level - 1; l >= 0; l--)
    {
        const int pw = dl(w, l), ph = dl(h, l);
        gauss_expand(output[l + 1].data(), output[l].data(), pw, ph);

        for (int j = 0; j < ph; j++)
        {
            for (int i = 0; i < pw; i++)
            {
                const float v = padded[l][j * pw + i];
                int hi = 1;
                for (; hi < num_gamma - 1 && gamma[hi] <= v; hi++);
                int lo = hi - 1;
                const float a = std::max(0.0f, std::min((v - gamma[lo]) / (gamma[hi] - gamma[lo]), 1.0f));
                const float l0 = ll_laplacian(buf[lo][l + 1].data(), buf[lo][l].data(), i, j, pw, ph);
                const float l1 = ll_laplacian(buf[hi][l + 1].data(), buf[hi][l].data(), i, j, pw, ph);
                output[l][j * pw + i] += l0 * (1.0f - a) + l1 * a;
            }
        }
    }

    // Copy result back to output (L channel only, scale back to [0,100])
    for (int j = 0; j < ht; j++)
    {
        for (int i = 0; i < wd; i++)
        {
            out[4 * (j * wd + i) + 0] = 100.0f * output[0][(j + max_supp) * w + max_supp + i];
            out[4 * (j * wd + i) + 1] = input[4 * (j * wd + i) + 1];  // copy a
            out[4 * (j * wd + i) + 2] = input[4 * (j * wd + i) + 2];  // copy b
            out[4 * (j * wd + i) + 3] = 0.0f;
        }
    }
}

class BilatImpl : public Bilat
{
    float sigma_ = 0.5f;      // midtone point
    float shadows_ = 0.5f;    // shadow contrast
    float highlights_ = 0.5f; // highlight contrast
    float clarity_ = 0.25f;   // detail/clarity

public:
    std::string name() const override { return "bilat"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setParams(float sigma, float shadows, float highlights, float clarity) override
    {
        sigma_ = sigma;
        shadows_ = shadows;
        highlights_ = highlights;
        clarity_ = clarity;
    }

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();
        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());

        float* lab = flow.rgb();  // Actually Lab data at this point

        // Allocate output buffer
        std::vector<float> out(width * height * 4);

        // Apply local Laplacian
        local_laplacian_internal(lab, out.data(), width, height,
                                  sigma_, shadows_, highlights_, clarity_);

        // Copy back to flow
        memcpy(lab, out.data(), width * height * 4 * sizeof(float));
    }
};

std::unique_ptr<Bilat> makeBilat()
{
    return std::make_unique<BilatImpl>();
}

} // namespace flow

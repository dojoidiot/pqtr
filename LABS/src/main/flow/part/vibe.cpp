// vibe.cpp - Parametric color transform with CMA-ES optimization
//
// 6 orthogonal dials:
//   0: exposure   - pre-ACES linear scale
//   1: contrast   - around midpoint
//   2: gamma      - power curve
//   3: saturation - chroma scale
//   4: lift       - shadow boost
//   5: gain       - highlight scale
//
// CMA-ES finds optimal dial values to match target.

#include <cmath>
#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <vector>

namespace vibe {

constexpr int N = 6;  // number of dials

struct Dials {
    float exposure = 1.0f;
    float contrast = 1.0f;
    float gamma = 1.0f;
    float saturation = 1.0f;
    float lift = 0.0f;
    float gain = 1.0f;
    float error = 0.0f;
    float covariance[36] = {};  // 6x6 learned dial interactions
};

// ACES filmic curve (fixed, industry standard)
static inline float aces(float x)
{
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

// Apply dials to RGB pixel (exposure BEFORE aces, rest AFTER)
static inline void apply_dials(float& r, float& g, float& b, const float* dial)
{
    // 1. Exposure (pre-ACES, in scene-linear)
    r *= dial[0];
    g *= dial[0];
    b *= dial[0];

    // 2. ACES tone curve (fixed)
    r = aces(r);
    g = aces(g);
    b = aces(b);

    // 3. Contrast (around 0.5)
    r = (r - 0.5f) * dial[1] + 0.5f;
    g = (g - 0.5f) * dial[1] + 0.5f;
    b = (b - 0.5f) * dial[1] + 0.5f;

    // 4. Gamma (power curve)
    r = std::pow(std::max(0.0f, r), dial[2]);
    g = std::pow(std::max(0.0f, g), dial[2]);
    b = std::pow(std::max(0.0f, b), dial[2]);

    // 5. Saturation (scale chroma around luminance)
    float lum = 0.299f * r + 0.587f * g + 0.114f * b;
    r = lum + (r - lum) * dial[3];
    g = lum + (g - lum) * dial[3];
    b = lum + (b - lum) * dial[3];

    // 6. Lift (add to shadows)
    r += dial[4] * (1.0f - r);
    g += dial[4] * (1.0f - g);
    b += dial[4] * (1.0f - b);

    // 7. Gain (scale highlights)
    if (r > 0.5f) r = 0.5f + (r - 0.5f) * dial[5];
    if (g > 0.5f) g = 0.5f + (g - 0.5f) * dial[5];
    if (b > 0.5f) b = 0.5f + (b - 0.5f) * dial[5];

    // Clamp
    r = std::max(0.0f, std::min(1.0f, r));
    g = std::max(0.0f, std::min(1.0f, g));
    b = std::max(0.0f, std::min(1.0f, b));
}

// Linear to sRGB (for perceptual comparison)
static inline float linear_to_srgb(float v)
{
    v = std::max(0.0f, std::min(1.0f, v));
    if (v <= 0.0031308f)
        return v * 12.92f;
    return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

// Compute MSE in sRGB space (perceptual)
static double compute_error(const float* in, const float* target, int pixels, const float* dials)
{
    double err = 0.0;
    for (int i = 0; i < pixels; i++)
    {
        float r = in[i * 3 + 0];
        float g = in[i * 3 + 1];
        float b = in[i * 3 + 2];

        apply_dials(r, g, b, dials);

        // Compare in sRGB space (how we see it)
        float sr = linear_to_srgb(r);
        float sg = linear_to_srgb(g);
        float sb = linear_to_srgb(b);

        float tr = linear_to_srgb(target[i * 3 + 0]);
        float tg = linear_to_srgb(target[i * 3 + 1]);
        float tb = linear_to_srgb(target[i * 3 + 2]);

        float dr = sr - tr;
        float dg = sg - tg;
        float db = sb - tb;

        err += 0.299 * dr * dr + 0.587 * dg * dg + 0.114 * db * db;
    }
    return err / pixels;
}

// CMA-ES optimization
Dials tune(const float* in, const float* target, int width, int height)
{
    int pixels = width * height;

    // Initial mean (identity transform)
    double mean[N] = {1.0, 1.0, 1.0, 1.0, 0.0, 1.0};

    // Initial step size
    double sigma = 0.3;

    // Covariance matrix (diagonal start)
    double C[N][N] = {};
    for (int i = 0; i < N; i++) C[i][i] = 1.0;

    // Evolution paths
    double pc[N] = {};
    double ps[N] = {};

    // CMA-ES parameters
    int lambda = 14;
    int mu = 7;
    double weights[7] = {0.35, 0.22, 0.15, 0.11, 0.08, 0.05, 0.04};

    double mueff = 0.0;
    {
        double sum_w = 0, sum_w2 = 0;
        for (int i = 0; i < mu; i++) { sum_w += weights[i]; sum_w2 += weights[i] * weights[i]; }
        mueff = sum_w * sum_w / sum_w2;
    }

    double cc = 4.0 / (N + 4.0);
    double cs = (mueff + 2.0) / (N + mueff + 5.0);
    double c1 = 2.0 / ((N + 1.3) * (N + 1.3) + mueff);
    double cmu = std::min(1.0 - c1, 2.0 * (mueff - 2.0 + 1.0 / mueff) / ((N + 2.0) * (N + 2.0) + mueff));
    double damps = 1.0 + 2.0 * std::max(0.0, std::sqrt((mueff - 1.0) / (N + 1.0)) - 1.0) + cs;
    double chiN = std::sqrt((double)N) * (1.0 - 1.0 / (4.0 * N) + 1.0 / (21.0 * N * N));

    std::mt19937 rng(42);
    std::normal_distribution<double> randn(0.0, 1.0);

    std::vector<std::vector<double>> pop(lambda, std::vector<double>(N));
    std::vector<double> fitness(lambda);
    std::vector<int> order(lambda);

    // Eigendecomposition storage
    double D[N];
    for (int i = 0; i < N; i++) D[i] = 1.0;

    float dials_f[N];
    double best_err = 1e9;
    double best_dials[N];
    std::copy(mean, mean + N, best_dials);

    int max_iter = 80;

    for (int gen = 0; gen < max_iter; gen++)
    {
        // Generate population
        for (int k = 0; k < lambda; k++)
        {
            for (int i = 0; i < N; i++)
            {
                double z = randn(rng);
                pop[k][i] = mean[i] + sigma * D[i] * z;
            }

            // Clamp to valid ranges
            pop[k][0] = std::max(0.2, std::min(3.0, pop[k][0]));  // exposure
            pop[k][1] = std::max(0.3, std::min(2.5, pop[k][1]));  // contrast
            pop[k][2] = std::max(0.4, std::min(2.5, pop[k][2]));  // gamma
            pop[k][3] = std::max(0.2, std::min(2.5, pop[k][3]));  // saturation
            pop[k][4] = std::max(-0.3, std::min(0.3, pop[k][4])); // lift
            pop[k][5] = std::max(0.3, std::min(2.5, pop[k][5]));  // gain

            for (int i = 0; i < N; i++) dials_f[i] = static_cast<float>(pop[k][i]);
            fitness[k] = compute_error(in, target, pixels, dials_f);
        }

        // Sort by fitness
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(), [&](int a, int b) { return fitness[a] < fitness[b]; });

        if (fitness[order[0]] < best_err)
        {
            best_err = fitness[order[0]];
            for (int i = 0; i < N; i++) best_dials[i] = pop[order[0]][i];
        }

        // Update mean
        double old_mean[N];
        std::copy(mean, mean + N, old_mean);

        for (int i = 0; i < N; i++)
        {
            mean[i] = 0;
            for (int k = 0; k < mu; k++)
                mean[i] += weights[k] * pop[order[k]][i];
        }

        // Update evolution paths
        double mean_diff[N];
        for (int i = 0; i < N; i++)
            mean_diff[i] = (mean[i] - old_mean[i]) / sigma;

        double ps_norm = 0;
        for (int i = 0; i < N; i++)
        {
            ps[i] = (1 - cs) * ps[i] + std::sqrt(cs * (2 - cs) * mueff) * mean_diff[i] / D[i];
            ps_norm += ps[i] * ps[i];
        }
        ps_norm = std::sqrt(ps_norm);

        // Update sigma
        sigma *= std::exp((cs / damps) * (ps_norm / chiN - 1));
        sigma = std::max(0.001, std::min(1.0, sigma));

        // Update pc
        for (int i = 0; i < N; i++)
            pc[i] = (1 - cc) * pc[i] + std::sqrt(cc * (2 - cc) * mueff) * mean_diff[i];

        // Update C diagonal (simplified)
        for (int i = 0; i < N; i++)
        {
            C[i][i] = (1 - c1 - cmu) * C[i][i] + c1 * pc[i] * pc[i];
            for (int k = 0; k < mu; k++)
            {
                double yi = (pop[order[k]][i] - old_mean[i]) / sigma;
                C[i][i] += cmu * weights[k] * yi * yi;
            }
            D[i] = std::sqrt(std::max(0.01, C[i][i]));
        }

        if (gen % 20 == 0)
        {
            std::cerr << "[vibe] gen=" << gen << " err=" << (best_err * 100)
                      << "% e=" << best_dials[0] << " c=" << best_dials[1]
                      << " g=" << best_dials[2] << " s=" << best_dials[3]
                      << std::endl;
        }
    }

    std::cerr << "[vibe] Final err=" << (best_err * 100) << "%"
              << " exp=" << best_dials[0]
              << " con=" << best_dials[1]
              << " gam=" << best_dials[2]
              << " sat=" << best_dials[3]
              << " lft=" << best_dials[4]
              << " gn=" << best_dials[5]
              << std::endl;

    Dials result;
    result.exposure = static_cast<float>(best_dials[0]);
    result.contrast = static_cast<float>(best_dials[1]);
    result.gamma = static_cast<float>(best_dials[2]);
    result.saturation = static_cast<float>(best_dials[3]);
    result.lift = static_cast<float>(best_dials[4]);
    result.gain = static_cast<float>(best_dials[5]);
    result.error = static_cast<float>(best_err);

    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++)
            result.covariance[i * N + j] = static_cast<float>(C[i][j]);

    return result;
}

// Apply dials to image
void apply(float* rgb, int width, int height, const Dials& d)
{
    float dials[N] = {d.exposure, d.contrast, d.gamma, d.saturation, d.lift, d.gain};

    for (int i = 0; i < width * height; i++)
        apply_dials(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2], dials);

    std::cerr << "[vibe] Applied dials" << std::endl;
}

} // namespace vibe

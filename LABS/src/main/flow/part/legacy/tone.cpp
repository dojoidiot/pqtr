// tone.cpp - Luminance histogram matching
//
// Maps scene-linear luminance to target luminance distribution.
// Preserves color ratios (hue unchanged).
//
// Algorithm:
//   1. Build luminance histograms (source and target)
//   2. Compute cumulative distribution functions
//   3. Create 256-entry mapping via histogram specification
//   4. Apply to RGB: out = in * (new_lum / old_lum)

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <iostream>

namespace tone {

constexpr int BINS = 256;

// Build luminance histogram from linear RGB
static void build_histogram(const float* rgb, int w, int h, double* hist) {
    std::fill(hist, hist + BINS, 0.0);

    for (int i = 0; i < w * h; i++) {
        float r = rgb[i * 3 + 0];
        float g = rgb[i * 3 + 1];
        float b = rgb[i * 3 + 2];
        float lum = 0.299f * r + 0.587f * g + 0.114f * b;

        // Apply sRGB-like curve for perceptual binning
        lum = std::max(0.0f, std::min(1.0f, lum));
        if (lum <= 0.0031308f)
            lum = lum * 12.92f;
        else
            lum = 1.055f * std::pow(lum, 1.0f / 2.4f) - 0.055f;

        int bin = static_cast<int>(lum * (BINS - 1) + 0.5f);
        bin = std::max(0, std::min(BINS - 1, bin));
        hist[bin] += 1.0;
    }

    // Normalize
    double total = w * h;
    for (int i = 0; i < BINS; i++)
        hist[i] /= total;
}

// Build CDF from histogram
static void build_cdf(const double* hist, double* cdf) {
    cdf[0] = hist[0];
    for (int i = 1; i < BINS; i++)
        cdf[i] = cdf[i - 1] + hist[i];
}

// Create mapping via histogram specification with shadow/highlight protection
static void build_mapping(const double* src_cdf, const double* tgt_cdf, float* map) {
    constexpr int SHADOW_BINS = 30;     // Protect bottom ~12% of range
    constexpr int HIGHLIGHT_START = 220; // Protect top ~14% of range
    constexpr float BLEND = 0.6f;        // Only apply 60% of histogram change

    for (int i = 0; i < BINS; i++) {
        double src_val = src_cdf[i];

        // Find closest match in target CDF
        int best_j = 0;
        double best_diff = std::abs(tgt_cdf[0] - src_val);

        for (int j = 1; j < BINS; j++) {
            double diff = std::abs(tgt_cdf[j] - src_val);
            if (diff < best_diff) {
                best_diff = diff;
                best_j = j;
            }
        }

        float hist_val = static_cast<float>(best_j) / (BINS - 1);
        float identity = static_cast<float>(i) / (BINS - 1);

        // Apply conservative blend to all values first
        float blended = identity * (1.0f - BLEND) + hist_val * BLEND;

        // Shadow protection: blend to identity for dark values
        if (i < SHADOW_BINS) {
            float t = static_cast<float>(i) / SHADOW_BINS;
            t = t * t;  // Smooth transition
            map[i] = identity * (1.0f - t) + blended * t;
        }
        // Highlight protection: blend to identity for bright values
        else if (i >= HIGHLIGHT_START) {
            float t = static_cast<float>(BINS - 1 - i) / (BINS - 1 - HIGHLIGHT_START);
            t = t * t;  // Smooth transition
            map[i] = identity * (1.0f - t) + blended * t;
        }
        else {
            map[i] = blended;
        }
    }
}

// sRGB to linear
static inline float srgb_to_linear(float v) {
    return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

// Linear to sRGB
static inline float linear_to_srgb(float v) {
    v = std::max(0.0f, std::min(1.0f, v));
    return v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

// Learn tone curve from source (HEAD) and target (reference)
void learn(const float* src, int src_w, int src_h,
           const uint8_t* tgt, int tgt_w, int tgt_h,
           float* curve) {

    // Convert target to linear float
    std::vector<float> tgt_f(tgt_w * tgt_h * 3);
    for (int i = 0; i < tgt_w * tgt_h * 3; i++)
        tgt_f[i] = srgb_to_linear(tgt[i] / 255.0f);

    // Build histograms
    double src_hist[BINS], tgt_hist[BINS];
    build_histogram(src, src_w, src_h, src_hist);
    build_histogram(tgt_f.data(), tgt_w, tgt_h, tgt_hist);

    // Build CDFs
    double src_cdf[BINS], tgt_cdf[BINS];
    build_cdf(src_hist, src_cdf);
    build_cdf(tgt_hist, tgt_cdf);

    // Build mapping
    build_mapping(src_cdf, tgt_cdf, curve);

    // Convert curve back to linear space
    for (int i = 0; i < BINS; i++) {
        curve[i] = srgb_to_linear(curve[i]);
    }

    std::cerr << "[tone] Learned curve from " << src_w << "x" << src_h
              << " -> " << tgt_w << "x" << tgt_h << std::endl;
}

// Apply tone curve to image
void apply(float* rgb, int w, int h, const float* curve) {
    for (int i = 0; i < w * h; i++) {
        float r = rgb[i * 3 + 0];
        float g = rgb[i * 3 + 1];
        float b = rgb[i * 3 + 2];

        // Current luminance (linear)
        float old_lum = 0.299f * r + 0.587f * g + 0.114f * b;

        if (old_lum < 0.0001f) continue;  // Skip black pixels

        // Convert to perceptual for lookup
        float lum_srgb = linear_to_srgb(old_lum);

        // Lookup new luminance
        float fidx = lum_srgb * (BINS - 1);
        int i0 = static_cast<int>(fidx);
        int i1 = std::min(i0 + 1, BINS - 1);
        float t = fidx - i0;
        float new_lum = curve[i0] * (1 - t) + curve[i1] * t;

        // Scale RGB by luminance ratio (preserves color ratios)
        float ratio = new_lum / old_lum;
        rgb[i * 3 + 0] = std::max(0.0f, std::min(1.0f, r * ratio));
        rgb[i * 3 + 1] = std::max(0.0f, std::min(1.0f, g * ratio));
        rgb[i * 3 + 2] = std::max(0.0f, std::min(1.0f, b * ratio));
    }

    std::cerr << "[tone] Applied to " << w << "x" << h << std::endl;
}

} // namespace tone

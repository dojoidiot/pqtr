#pragma once

// TONE - Luminance histogram matching
//
// Maps scene-linear luminance to target distribution.
// Preserves color ratios (hue unchanged).
//
// Pipeline: HEAD -> TONE -> TUNE

#include <cstdint>

namespace tone {

constexpr int BINS = 256;

// Learn tone curve from source (HEAD) and target (reference)
void learn(const float* src, int src_w, int src_h,
           const uint8_t* tgt, int tgt_w, int tgt_h,
           float* curve);

// Apply tone curve to image (in-place)
void apply(float* rgb, int w, int h, const float* curve);

} // namespace tone

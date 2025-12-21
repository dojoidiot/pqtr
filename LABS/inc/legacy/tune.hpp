#pragma once

// TUNE - Color correction NN with histogram loss
//
// Small NN learns per-pixel color adjustment after TONE.
// Uses spatial + multi-scale features for local effects.
//
// Input features (17):
//   - RGB (3)
//   - HSV (3)
//   - local 3x3 saturation variance (1)
//   - local 3x3 luminance variance (1)
//   - spatial position x, y (2)
//   - multi-scale luminance: 4x4, 16x16, 64x64 (3)
//   - multi-scale color at 16x16: RGB (3)
//   - global mean luminance (1)
//
// Output: RGB gain (multiplicative)
//
// Pipeline: HEAD -> TONE -> TUNE -> output

#include <cstdint>

namespace tune {

constexpr int IN = 17;   // Spatial + multi-scale features
constexpr int H1 = 48;
constexpr int H2 = 24;
constexpr int OUT = 3;   // RGB gain

struct Net {
    float w1[IN * H1];
    float b1[H1];
    float w2[H1 * H2];
    float b2[H2];
    float w3[H2 * OUT];
    float b3[OUT];
};

// Initialize network with random weights
void init(Net& net, unsigned seed = 42);

// Train on TONE output (scene-linear float) -> reference (sRGB uint8) pair
void train(Net& net, const float* tone, const uint8_t* ref, int w, int h,
           int epochs = 200, int samples_per_epoch = 30000, float lr = 0.005f);

// Apply trained network to scene-linear input
void apply(const Net& net, const float* tone, float* out, int w, int h);

} // namespace tune

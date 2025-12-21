#pragma once

/**
 * TINT - 3D Color LUT
 *
 * Applies a learned 17^3 color LUT for camera-specific color grading.
 * Currently disabled until coverage > 70% (causes discontinuities otherwise).
 *
 * Pipeline position: After TUNE, before VIBE
 */

#include <vector>
#include <string>

namespace tint
{
    constexpr int GRID_SIZE = 17;
    constexpr int LUT_SIZE = GRID_SIZE * GRID_SIZE * GRID_SIZE * 3;

    struct Lut3D
    {
        std::vector<float> data;  // 17^3 * 3 RGB values
        float coverage = 0.0f;
        bool valid = false;

        Lut3D() : data(LUT_SIZE, 0.0f) {
            // Initialize to identity
            for (int ri = 0; ri < GRID_SIZE; ri++) {
                for (int gi = 0; gi < GRID_SIZE; gi++) {
                    for (int bi = 0; bi < GRID_SIZE; bi++) {
                        int idx = ((ri * GRID_SIZE + gi) * GRID_SIZE + bi) * 3;
                        data[idx + 0] = static_cast<float>(ri) / (GRID_SIZE - 1);
                        data[idx + 1] = static_cast<float>(gi) / (GRID_SIZE - 1);
                        data[idx + 2] = static_cast<float>(bi) / (GRID_SIZE - 1);
                    }
                }
            }
        }
    };

    // Apply 3D LUT with trilinear interpolation (in-place)
    void apply(float *rgb, int width, int height, const Lut3D &lut);

    // Get 3D LUT from lute profile
    Lut3D fromLute(const std::string &cameraKey);

} // namespace tint

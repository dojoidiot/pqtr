// tint.cpp - 3D Color LUT
//
// Trilinear interpolation in 17^3 color cube.
// With ACES handling HDR→SDR, lower coverage (20%) is acceptable.

#include "tint.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace tint
{

    void apply(float *rgb, int width, int height, const Lut3D &lut)
    {
        // Skip if not valid or coverage too low
        // 20% threshold - ACES handles heavy lifting, TINT is refinement
        if (!lut.valid || lut.coverage < 0.20f)
        {
            if (lut.valid)
                std::cerr << "[tint] Skipped - coverage " << (lut.coverage * 100) << "% < 20%\n";
            return;
        }

        if (rgb == nullptr || width <= 0 || height <= 0)
            return;

        float scale = static_cast<float>(GRID_SIZE - 1);

        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                size_t idx = (static_cast<size_t>(y) * width + x) * 3;

                float r = std::max(0.0f, std::min(1.0f, rgb[idx + 0]));
                float g = std::max(0.0f, std::min(1.0f, rgb[idx + 1]));
                float b = std::max(0.0f, std::min(1.0f, rgb[idx + 2]));

                // Grid coordinates
                float fr = r * scale;
                float fg = g * scale;
                float fb = b * scale;

                int r0 = static_cast<int>(fr);
                int g0 = static_cast<int>(fg);
                int b0 = static_cast<int>(fb);
                int r1 = std::min(r0 + 1, GRID_SIZE - 1);
                int g1 = std::min(g0 + 1, GRID_SIZE - 1);
                int b1 = std::min(b0 + 1, GRID_SIZE - 1);
                r0 = std::min(r0, GRID_SIZE - 1);
                g0 = std::min(g0, GRID_SIZE - 1);
                b0 = std::min(b0, GRID_SIZE - 1);

                float tr = fr - r0;
                float tg = fg - g0;
                float tb = fb - b0;

                // Trilinear interpolation
                auto sample = [&](int ri, int gi, int bi, int ch) -> float {
                    int i = ((ri * GRID_SIZE + gi) * GRID_SIZE + bi) * 3 + ch;
                    return lut.data[i];
                };

                for (int ch = 0; ch < 3; ch++)
                {
                    float c000 = sample(r0, g0, b0, ch);
                    float c100 = sample(r1, g0, b0, ch);
                    float c010 = sample(r0, g1, b0, ch);
                    float c110 = sample(r1, g1, b0, ch);
                    float c001 = sample(r0, g0, b1, ch);
                    float c101 = sample(r1, g0, b1, ch);
                    float c011 = sample(r0, g1, b1, ch);
                    float c111 = sample(r1, g1, b1, ch);

                    float c00 = c000 * (1 - tr) + c100 * tr;
                    float c10 = c010 * (1 - tr) + c110 * tr;
                    float c01 = c001 * (1 - tr) + c101 * tr;
                    float c11 = c011 * (1 - tr) + c111 * tr;

                    float c0 = c00 * (1 - tg) + c10 * tg;
                    float c1 = c01 * (1 - tg) + c11 * tg;

                    rgb[idx + ch] = c0 * (1 - tb) + c1 * tb;
                }
            }
        }

        std::cerr << "[tint] Applied 3D LUT (" << (lut.coverage * 100) << "% coverage)\n";
    }

    Lut3D fromLute(const std::string &cameraKey)
    {
        Lut3D lut;
        // TODO: Extract from lute profile when coverage is sufficient
        // For now, return identity LUT
        lut.valid = false;
        lut.coverage = 0.0f;
        return lut;
    }

} // namespace tint

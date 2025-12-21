#include "drum.hpp"
#include <vector>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace
{
    // Helper to convert linear RGB to luminance
    static float to_lum(float r, float g, float b)
    {
        return 0.2126f * r + 0.7152f * g + 0.0722f * b;
    }

    // CLAHE implementation
    void clahe_process(float *lum, int width, int height, const drum::Params &params)
    {
        if (!params.enabled || params.clip_limit <= 0) return;

        const int grid_w = width / params.grid_x;
        const int grid_h = height / params.grid_y;
        const int hist_bins = 256;

        std::vector<std::vector<int>> histograms(params.grid_x * params.grid_y, std::vector<int>(hist_bins, 0));

        // 1. Create histograms for each tile
        for (int gy = 0; gy < params.grid_y; ++gy)
        {
            for (int gx = 0; gx < params.grid_x; ++gx)
            {
                const int tile_idx = gy * params.grid_x + gx;
                for (int y = gy * grid_h; y < (gy + 1) * grid_h; ++y)
                {
                    for (int x = gx * grid_w; x < (gx + 1) * grid_w; ++x)
                    {
                        float l = lum[y * width + x];
                        int bin = std::min(hist_bins - 1, static_cast<int>(l * (hist_bins - 1) + 0.5f));
                        histograms[tile_idx][bin]++;
                    }
                }
            }
        }

        // 2. Clip histograms and create cumulative distribution functions (CDFs)
        std::vector<std::vector<float>> cdfs(params.grid_x * params.grid_y, std::vector<float>(hist_bins, 0.0f));
        const int clip_threshold = static_cast<int>(params.clip_limit * (grid_w * grid_h) / hist_bins);

        for (int i = 0; i < params.grid_x * params.grid_y; ++i)
        {
            int excess = 0;
            for (int j = 0; j < hist_bins; ++j)
            {
                if (histograms[i][j] > clip_threshold)
                {
                    excess += histograms[i][j] - clip_threshold;
                    histograms[i][j] = clip_threshold;
                }
            }

            int redist_per_bin = excess / hist_bins;
            int remainder = excess % hist_bins;

            for (int j = 0; j < hist_bins; ++j)
            {
                histograms[i][j] += redist_per_bin;
                if (j < remainder) histograms[i][j]++;
            }

            long sum = 0;
            for (int j = 0; j < hist_bins; ++j)
            {
                sum += histograms[i][j];
                cdfs[i][j] = static_cast<float>(sum) / (grid_w * grid_h);
            }
        }
        
        // 3. Bilinearly interpolate new luminance values
        std::vector<float> new_lum = std::vector<float>(lum, lum + width * height);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                float gx_f = static_cast<float>(x) / grid_w - 0.5f;
                float gy_f = static_cast<float>(y) / grid_h - 0.5f;
                
                int gx0 = static_cast<int>(gx_f);
                int gy0 = static_cast<int>(gy_f);

                float wx = gx_f - gx0;
                float wy = gy_f - gy0;

                int gx1 = std::min(params.grid_x - 1, gx0 + 1);
                int gy1 = std::min(params.grid_y - 1, gy0 + 1);
                gx0 = std::max(0, gx0);
                gy0 = std::max(0, gy0);

                int bin = std::min(hist_bins - 1, static_cast<int>(lum[y * width + x] * (hist_bins - 1) + 0.5f));

                float cdf00 = cdfs[gy0 * params.grid_x + gx0][bin];
                float cdf10 = cdfs[gy0 * params.grid_x + gx1][bin];
                float cdf01 = cdfs[gy1 * params.grid_x + gx0][bin];
                float cdf11 = cdfs[gy1 * params.grid_x + gx1][bin];
                
                float cdf_y0 = cdf00 * (1 - wx) + cdf10 * wx;
                float cdf_y1 = cdf01 * (1 - wx) + cdf11 * wx;
                
                new_lum[y * width + x] = cdf_y0 * (1 - wy) + cdf_y1 * wy;
            }
        }

        // Overwrite original luminance buffer
        std::copy(new_lum.begin(), new_lum.end(), lum);
    }
}

drum::Params drum::parse(const std::string &dro_str)
{
    Params params;
    if (dro_str == "Off") {
        params.enabled = false;
    } else if (dro_str == "Lv1") {
        params.clip_limit = 2.0f;
    } else if (dro_str == "Lv2") {
        params.clip_limit = 3.5f;
    } else if (dro_str == "Lv3") {
        params.clip_limit = 5.0f;
    } else if (dro_str == "Lv4") {
        params.clip_limit = 7.0f;
    } else if (dro_str == "Lv5") {
        params.clip_limit = 10.0f;
    } else { // Auto or any other value
        params.clip_limit = 5.0f;
    }
    return params;
}

void drum::apply(float *rgb, int width, int height, const Params &params)
{
    if (!params.enabled) return;

    const size_t num_pixels = static_cast<size_t>(width) * height;
    std::vector<float> lum(num_pixels);

    // 1. Extract luminance
    for (size_t i = 0; i < num_pixels; ++i)
    {
        lum[i] = to_lum(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    }

    // 2. Process luminance with CLAHE
    std::vector<float> original_lum = lum;
    clahe_process(lum.data(), width, height, params);

    // 3. Re-apply color
    for (size_t i = 0; i < num_pixels; ++i)
    {
        float old_l = original_lum[i];
        float new_l = lum[i];
        float ratio = (old_l > 1e-6f) ? (new_l / old_l) : 1.0f;

        rgb[i * 3 + 0] *= ratio;
        rgb[i * 3 + 1] *= ratio;
        rgb[i * 3 + 2] *= ratio;
    }
}
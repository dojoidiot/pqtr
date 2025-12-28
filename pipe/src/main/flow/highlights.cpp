// highlights.cpp - Highlight reconstruction on Bayer mosaic
//
// CLEAN COPY approach from DT's "inpaint opposed" algorithm
// Core idea: For clipped pixels, estimate using average of "opposing" colors
//
// The key insight: refavg (reference average) = mean of the other two color
// channels in a 3x3 superpixel area. This is a good estimate for clipped values.

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <algorithm>

namespace flow
{

// Cube root space for perceptual uniformity (DT's HL_POWERF = 3.0)
static constexpr float HL_POWER = 3.0f;

static inline float to_cube(float x)
{
    return std::pow(std::max(0.0f, x), 1.0f / HL_POWER);
}

static inline float from_cube(float x)
{
    return std::pow(std::max(0.0f, x), HL_POWER);
}

// Get Bayer color: 0=R, 1=G, 2=B
// RGGB pattern: (0,0)=R, (0,1)=G, (1,0)=G, (1,1)=B
static inline int bayer_color(int x, int y)
{
    int px = x & 1;
    int py = y & 1;
    if (py == 0 && px == 0) return 0;  // R
    if (py == 1 && px == 1) return 2;  // B
    return 1;  // G
}

// Calculate refavg: average of opposing (other two) colors in 3x3 superpixel
// CLEAN COPY from DT opposed.c _calc_refavg concept
static float calc_refavg(const float* fdata, int width, int height,
                         int cx, int cy, int target_color)
{
    // Collect all colors in 3x3 neighborhood
    float sums[3] = {0, 0, 0};
    int counts[3] = {0, 0, 0};

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            int x = cx + dx;
            int y = cy + dy;
            if (x < 0 || x >= width || y < 0 || y >= height) continue;

            int color = bayer_color(x, y);
            float val = fdata[y * width + x];

            // Work in cube-root space for perceptual uniformity
            sums[color] += to_cube(val);
            counts[color]++;
        }
    }

    // Average each color
    float avgs[3];
    for (int c = 0; c < 3; c++)
    {
        avgs[c] = counts[c] > 0 ? sums[c] / counts[c] : 0.0f;
    }

    // refavg = average of the OTHER two colors (opposing)
    float refavg;
    if (target_color == 0)  // R: avg of G and B
        refavg = 0.5f * (avgs[1] + avgs[2]);
    else if (target_color == 2)  // B: avg of R and G
        refavg = 0.5f * (avgs[0] + avgs[1]);
    else  // G: avg of R and B
        refavg = 0.5f * (avgs[0] + avgs[2]);

    return from_cube(refavg);
}

class HighlightsImpl : public Highlights
{
    float clip_ = 1.0f;
    bool enabled_ = false;

public:
    std::string name() const override { return "highlights"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setClip(float clip) override { clip_ = clip; enabled_ = true; }

    void process(Flow& flow) override
    {
        if (!enabled_) return;

        auto& root = flow.info().root();
        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());

        float* fdata = flow.fdata();
        if (!fdata) return;

        // Get WB coefficients to determine per-channel clip thresholds
        float wb_r = 1.0f, wb_b = 1.0f;
        if (root.test("wb"))
        {
            auto& wb = root.next("wb");
            float g1 = wb.leaf("g1").dial();
            if (g1 > 0)
            {
                wb_r = wb.leaf("r").dial() / g1;
                wb_b = wb.leaf("b").dial() / g1;
            }
        }

        // Clip thresholds in fdata space (after WB multiplication)
        float clips[3] = {
            clip_ * wb_r,   // Red clips at ~2.4
            clip_,          // Green clips at 1.0
            clip_ * wb_b    // Blue clips at ~1.58
        };

        // Use 95% of clip as threshold (like DT)
        for (int c = 0; c < 3; c++) clips[c] *= 0.95f;

        // Process each pixel
        for (int y = 1; y < height - 1; y++)
        {
            for (int x = 1; x < width - 1; x++)
            {
                size_t idx = y * width + x;
                int color = bayer_color(x, y);
                float val = fdata[idx];

                // Only process clipped pixels
                if (val < clips[color]) continue;

                // Calculate opposing average
                float refavg = calc_refavg(fdata, width, height, x, y, color);

                // Use refavg as the reconstructed value
                // DT uses MAX(val, refavg + chrominance), but for simplicity
                // we just use refavg if it's reasonable
                if (refavg > 0.01f && refavg < val)
                {
                    fdata[idx] = refavg;
                }
            }
        }
    }
};

std::unique_ptr<Highlights> makeHighlights()
{
    return std::make_unique<HighlightsImpl>();
}

} // namespace flow

#pragma once

/**
 * DRUM - Local Tone Mapping (CLAHE)
 *
 * Applies Contrast Limited Adaptive Histogram Equalization to match
 * Sony DRO (Dynamic Range Optimizer) shadow lifting.
 *
 * Pipeline position: After HEAD (scene-linear RGB), before TUNE (global curve)
 *
 * DRO level mapping:
 *   Off:  skip CLAHE
 *   Lv1:  clip_limit = 2.0
 *   Lv2:  clip_limit = 3.5
 *   Lv3:  clip_limit = 5.0
 *   Lv4:  clip_limit = 7.0
 *   Lv5:  clip_limit = 10.0
 *   Auto: use Lv3 (5.0) as default
 */

#include <string>

namespace drum
{

    struct Params
    {
        int grid_x = 8;         // horizontal tiles
        int grid_y = 8;         // vertical tiles
        float clip_limit = 5.0f; // histogram clip limit (higher = more contrast)
        bool enabled = true;
    };

    /**
     * Parse DRO level from EXIF string.
     *
     * @param dro_str DRO string from EXIF (e.g., "Auto", "Off", "Lv3")
     * @return Params struct with appropriate clip_limit
     */
    Params parse(const std::string &dro_str);

    /**
     * Apply CLAHE to scene-linear RGB image.
     *
     * Operates on luminance channel, preserves color ratios.
     * Works in-place on the input buffer.
     *
     * @param rgb Interleaved RGB float data (scene-linear)
     * @param width Image width
     * @param height Image height
     * @param params CLAHE parameters
     */
    void apply(float *rgb, int width, int height, const Params &params);

} // namespace drum

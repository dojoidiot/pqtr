// split_tone.cpp - VIBE
// Split Toning Module - Shadow/Highlight Color Grading (4 dials)

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace vibe
{
namespace mods
{

bool split_tone(const View& in, View& out,
    Dial shadow_temp, Dial shadow_tint,
    Dial highlight_temp, Dial highlight_tint)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::split_tone] invalid input\n";
        return false;
    }

    shadow_temp = std::clamp(shadow_temp, 0.0f, 1.0f);
    shadow_tint = std::clamp(shadow_tint, 0.0f, 1.0f);
    highlight_temp = std::clamp(highlight_temp, 0.0f, 1.0f);
    highlight_tint = std::clamp(highlight_tint, 0.0f, 1.0f);

    const float eps = 0.01f;
    if (std::abs(shadow_temp - 0.5f) < eps &&
        std::abs(shadow_tint - 0.5f) < eps &&
        std::abs(highlight_temp - 0.5f) < eps &&
        std::abs(highlight_tint - 0.5f) < eps)
    {
        in.copyTo(out);
        return true;
    }

    const float max_shift = 0.15f;
    float sh_temp = (shadow_temp - 0.5f) * 2.0f * max_shift;
    float sh_tint = (shadow_tint - 0.5f) * 2.0f * max_shift;
    float hi_temp = (highlight_temp - 0.5f) * 2.0f * max_shift;
    float hi_tint = (highlight_tint - 0.5f) * 2.0f * max_shift;

    std::vector<View> ch(3);
    cv::split(in, ch);
    View& B = ch[0];
    View& G = ch[1];
    View& R = ch[2];

    // Compute luminance
    View lum, tmp1, tmp2;
    cv::multiply(R, 0.2126f, lum);
    cv::multiply(G, 0.7152f, tmp1);
    cv::add(lum, tmp1, lum);
    cv::multiply(B, 0.0722f, tmp2);
    cv::add(lum, tmp2, lum);

    // Shadow mask
    View shadow_mask;
    cv::divide(lum, 0.3f, shadow_mask);
    cv::subtract(1.0f, shadow_mask, shadow_mask);
    cv::max(shadow_mask, 0.0f, shadow_mask);
    cv::multiply(shadow_mask, shadow_mask, shadow_mask);

    // Highlight mask
    View highlight_mask;
    cv::subtract(lum, 0.5f, highlight_mask);
    cv::divide(highlight_mask, 0.5f, highlight_mask);
    cv::max(highlight_mask, 0.0f, highlight_mask);
    cv::multiply(highlight_mask, highlight_mask, highlight_mask);

    // Apply shadow shifts
    if (std::abs(sh_temp) > 0.001f || std::abs(sh_tint) > 0.001f)
    {
        float r_shift = sh_temp * 0.5f + sh_tint * 0.5f;
        float g_shift = -sh_tint;
        float b_shift = -sh_temp;

        View mr, mg, mb;
        cv::multiply(shadow_mask, r_shift, mr);
        cv::multiply(shadow_mask, g_shift, mg);
        cv::multiply(shadow_mask, b_shift, mb);

        cv::add(R, mr, R);
        cv::add(G, mg, G);
        cv::add(B, mb, B);
    }

    // Apply highlight shifts
    if (std::abs(hi_temp) > 0.001f || std::abs(hi_tint) > 0.001f)
    {
        float r_shift = hi_temp * 0.5f + hi_tint * 0.5f;
        float g_shift = -hi_tint;
        float b_shift = -hi_temp;

        View mr, mg, mb;
        cv::multiply(highlight_mask, r_shift, mr);
        cv::multiply(highlight_mask, g_shift, mg);
        cv::multiply(highlight_mask, b_shift, mb);

        cv::add(R, mr, R);
        cv::add(G, mg, G);
        cv::add(B, mb, B);
    }

    cv::max(R, 0.0f, R); cv::min(R, 1.0f, R);
    cv::max(G, 0.0f, G); cv::min(G, 1.0f, G);
    cv::max(B, 0.0f, B); cv::min(B, 1.0f, B);

    cv::merge(ch, out);
    return true;
}

} // namespace mods
} // namespace vibe

// selective_color.cpp - VIBE
// Selective Color Module - HSL adjustments for 8 color bands (24 dials)

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <array>

namespace vibe
{
namespace mods
{

static const std::array<float, 8> HUE_CENTERS = {0.0f, 45.0f, 90.0f, 150.0f, 195.0f, 240.0f, 285.0f, 315.0f};
static const float HUE_RANGE = 45.0f;

static float hue_weight(float pixel_hue, float target)
{
    while (pixel_hue < 0) pixel_hue += 360.0f;
    while (pixel_hue >= 360) pixel_hue -= 360.0f;
    while (target < 0) target += 360.0f;
    while (target >= 360) target -= 360.0f;

    float diff = std::abs(pixel_hue - target);
    if (diff > 180.0f) diff = 360.0f - diff;
    if (diff > HUE_RANGE) return 0.0f;
    return 0.5f * (1.0f + std::cos(M_PI * diff / HUE_RANGE));
}

bool selective_color(const View& in, View& out,
    const Dial hue_dials[8], const Dial sat_dials[8], const Dial lum_dials[8])
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::selective_color] invalid input\n";
        return false;
    }

    std::array<float, 8> hue_adj, sat_adj, lum_adj;
    bool any_active = false;

    for (int i = 0; i < 8; i++)
    {
        hue_adj[i] = (std::clamp(hue_dials[i], 0.0f, 1.0f) - 0.5f) * 60.0f;
        sat_adj[i] = (std::clamp(sat_dials[i], 0.0f, 1.0f) - 0.5f) * 2.0f;
        lum_adj[i] = (std::clamp(lum_dials[i], 0.0f, 1.0f) - 0.5f) * 2.0f;

        if (std::abs(hue_adj[i]) > 0.1f || std::abs(sat_adj[i]) > 0.01f || std::abs(lum_adj[i]) > 0.01f)
            any_active = true;
    }

    if (!any_active)
    {
        in.copyTo(out);
        return true;
    }

    cv::Mat cpu;
    in.copyTo(cpu);

    cv::Mat clamped, gamma_rgb;
    cv::max(cpu, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);
    cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

    cv::Mat rgb8;
    gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);

    cv::Mat hls;
    cv::cvtColor(rgb8, hls, cv::COLOR_RGB2HLS);

    cv::Mat hls_float;
    hls.convertTo(hls_float, CV_32FC3);

    for (int y = 0; y < hls_float.rows; y++)
    {
        cv::Vec3f* row = hls_float.ptr<cv::Vec3f>(y);
        for (int x = 0; x < hls_float.cols; x++)
        {
            float h = row[x][0] * 2.0f;
            float l = row[x][1] / 255.0f;
            float s = row[x][2] / 255.0f;

            float total_h = 0, total_s = 0, total_l = 0, total_w = 0;

            for (int band = 0; band < 8; band++)
            {
                float w = hue_weight(h, HUE_CENTERS[band]);
                if (w > 0.001f)
                {
                    total_h += w * hue_adj[band];
                    total_s += w * sat_adj[band];
                    total_l += w * lum_adj[band];
                    total_w += w;
                }
            }

            if (total_w > 0.001f)
            {
                float norm = 1.0f / total_w;
                total_h *= norm;
                total_s *= norm;
                total_l *= norm;

                h += total_h;
                while (h < 0) h += 360.0f;
                while (h >= 360) h -= 360.0f;

                if (total_s > 0)
                    s = s + (1.0f - s) * total_s;
                else
                    s = s * (1.0f + total_s);

                if (total_l > 0)
                    l = l + (1.0f - l) * total_l * 0.5f;
                else
                    l = l * (1.0f + total_l * 0.5f);
            }

            row[x][0] = std::clamp(h / 2.0f, 0.0f, 180.0f);
            row[x][1] = std::clamp(l * 255.0f, 0.0f, 255.0f);
            row[x][2] = std::clamp(s * 255.0f, 0.0f, 255.0f);
        }
    }

    cv::Mat hls_out, rgb8_out, gamma_out, linear_out;
    hls_float.convertTo(hls_out, CV_8UC3);
    cv::cvtColor(hls_out, rgb8_out, cv::COLOR_HLS2RGB);
    rgb8_out.convertTo(gamma_out, CV_32FC3, 1.0/255.0);
    cv::pow(gamma_out, 2.2f, linear_out);

    linear_out.copyTo(out);
    return true;
}

} // namespace mods
} // namespace vibe

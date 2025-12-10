// global_color.cpp - VIBE
// Global Color Module - Vibrance, Saturation, Color Density (3 dials)

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace vibe
{
namespace mods
{

static void rgb_to_lab(const View& rgb, View& lab)
{
    View clamped, gamma_rgb;
    cv::max(rgb, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);
    cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

    View rgb8, bgr8, lab8;
    gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);
    cv::cvtColor(rgb8, bgr8, cv::COLOR_RGB2BGR);
    cv::cvtColor(bgr8, lab8, cv::COLOR_BGR2Lab);
    lab8.convertTo(lab, CV_32FC3);

    std::vector<View> ch(3);
    cv::split(lab, ch);
    cv::multiply(ch[0], 100.0f/255.0f, ch[0]);
    cv::subtract(ch[1], 128.0f, ch[1]);
    cv::subtract(ch[2], 128.0f, ch[2]);
    cv::merge(ch, lab);
}

static void lab_to_rgb(const View& lab, View& rgb)
{
    std::vector<View> ch(3);
    cv::split(lab, ch);
    cv::multiply(ch[0], 255.0f/100.0f, ch[0]);
    cv::add(ch[1], 128.0f, ch[1]);
    cv::add(ch[2], 128.0f, ch[2]);

    for (auto& c : ch) {
        cv::max(c, 0.0f, c);
        cv::min(c, 255.0f, c);
    }

    View lab_scaled, lab8, bgr8, rgb8, gamma_rgb;
    cv::merge(ch, lab_scaled);
    lab_scaled.convertTo(lab8, CV_8UC3);
    cv::cvtColor(lab8, bgr8, cv::COLOR_Lab2BGR);
    cv::cvtColor(bgr8, rgb8, cv::COLOR_BGR2RGB);
    rgb8.convertTo(gamma_rgb, CV_32FC3, 1.0/255.0);
    cv::pow(gamma_rgb, 2.2f, rgb);
}

bool global_color(const View& in, View& out,
    Dial vibrance_dial, Dial saturation_dial, Dial color_density_dial)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::global_color] invalid input\n";
        return false;
    }

    vibrance_dial = std::clamp(vibrance_dial, 0.0f, 1.0f);
    saturation_dial = std::clamp(saturation_dial, 0.0f, 1.0f);
    color_density_dial = std::clamp(color_density_dial, 0.0f, 1.0f);

    float vibrance = (vibrance_dial - 0.5f) * 2.0f;
    float saturation = saturation_dial * 2.0f;
    float color_density = 0.5f + color_density_dial;

    // Skip if all neutral
    if (std::abs(vibrance) <= 0.01f &&
        std::abs(saturation - 1.0f) <= 0.01f &&
        std::abs(color_density - 1.0f) <= 0.01f)
    {
        in.copyTo(out);
        return true;
    }

    View lab;
    rgb_to_lab(in, lab);

    std::vector<View> ch(3);
    cv::split(lab, ch);
    View& L = ch[0];
    View& a = ch[1];
    View& b = ch[2];

    // Vibrance (smart saturation with skin protection)
    if (std::abs(vibrance) > 0.01f)
    {
        View a2, b2, C;
        cv::multiply(a, a, a2);
        cv::multiply(b, b, b2);
        cv::add(a2, b2, C);
        cv::sqrt(C, C);

        View hue;
        cv::phase(a, b, hue, true);

        View skin_center, skin_mask;
        cv::subtract(hue, 45.0f, skin_center);
        cv::multiply(skin_center, skin_center, skin_center);
        cv::divide(skin_center, -450.0f, skin_mask);
        cv::exp(skin_mask, skin_mask);

        View vib_weight;
        cv::divide(C, 100.0f, vib_weight);
        cv::subtract(1.0f, vib_weight, vib_weight);
        cv::max(vib_weight, 0.0f, vib_weight);
        cv::min(vib_weight, 1.0f, vib_weight);

        if (vibrance > 0) {
            View prot;
            cv::subtract(1.0f, skin_mask, prot);
            cv::multiply(prot, 0.7f, prot);
            cv::add(prot, 0.3f, prot);
            cv::multiply(vib_weight, prot, vib_weight);
        }

        View boost;
        cv::multiply(vib_weight, vibrance, boost);
        cv::add(boost, 1.0f, boost);
        cv::multiply(a, boost, a);
        cv::multiply(b, boost, b);
    }

    // Saturation
    if (std::abs(saturation - 1.0f) > 0.01f)
    {
        cv::multiply(a, saturation, a);
        cv::multiply(b, saturation, b);
    }

    // Color density
    if (std::abs(color_density - 1.0f) > 0.01f)
    {
        cv::multiply(a, color_density, a);
        cv::multiply(b, color_density, b);
        float l_contrast = 1.0f + (color_density - 1.0f) * 0.3f;
        cv::subtract(L, 50.0f, L);
        cv::multiply(L, l_contrast, L);
        cv::add(L, 50.0f, L);
    }

    cv::max(L, 0.0f, L);
    cv::min(L, 100.0f, L);
    cv::merge(ch, lab);

    lab_to_rgb(lab, out);
    cv::max(out, 0.0f, out);
    cv::min(out, 1.0f, out);

    return true;
}

} // namespace mods
} // namespace vibe

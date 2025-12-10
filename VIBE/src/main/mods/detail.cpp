// detail.cpp - VIBE
// Detail Module - Sharpen, Denoise (4 dials)

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace vibe
{
namespace mods
{

static void apply_sharpen(View& img, float amount, float radius)
{
    if (amount < 0.01f) return;

    View clamped, gamma_rgb;
    cv::max(img, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);
    cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

    View rgb8, bgr8, lab;
    gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);
    cv::cvtColor(rgb8, bgr8, cv::COLOR_RGB2BGR);
    cv::cvtColor(bgr8, lab, cv::COLOR_BGR2Lab);

    std::vector<View> ch(3);
    cv::split(lab, ch);

    View L_float;
    ch[0].convertTo(L_float, CV_32F);

    View L_blur;
    int ks = static_cast<int>(radius * 2) * 2 + 1;
    ks = std::clamp(ks, 3, 31);
    cv::GaussianBlur(L_float, L_blur, cv::Size(ks, ks), radius);

    View L_diff;
    cv::subtract(L_float, L_blur, L_diff);
    cv::scaleAdd(L_diff, amount, L_float, L_float);

    cv::max(L_float, 0.0f, L_float);
    cv::min(L_float, 255.0f, L_float);
    L_float.convertTo(ch[0], CV_8U);

    View lab_sharp, bgr_sharp, rgb8_sharp, gamma_out;
    cv::merge(ch, lab_sharp);
    cv::cvtColor(lab_sharp, bgr_sharp, cv::COLOR_Lab2BGR);
    cv::cvtColor(bgr_sharp, rgb8_sharp, cv::COLOR_BGR2RGB);
    rgb8_sharp.convertTo(gamma_out, CV_32FC3, 1.0/255.0);
    cv::pow(gamma_out, 2.2f, img);
}

static void apply_denoise_luma(View& img, float strength)
{
    if (strength < 1.0f) return;

    View clamped, gamma_rgb;
    cv::max(img, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);
    cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

    View rgb8, bgr8;
    gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);
    cv::cvtColor(rgb8, bgr8, cv::COLOR_RGB2BGR);

    cv::Mat cpu;
    bgr8.copyTo(cpu);

    cv::Mat denoised;
    int d = std::min(9, static_cast<int>(strength / 10.0f) + 3);
    float sigma = strength * 0.5f;
    cv::bilateralFilter(cpu, denoised, d, sigma, sigma);

    View denoised_u, rgb8_out, gamma_out;
    denoised.copyTo(denoised_u);
    cv::cvtColor(denoised_u, rgb8_out, cv::COLOR_BGR2RGB);
    rgb8_out.convertTo(gamma_out, CV_32FC3, 1.0/255.0);
    cv::pow(gamma_out, 2.2f, img);
}

static void apply_denoise_chroma(View& img, float strength)
{
    if (strength < 1.0f) return;

    View clamped, gamma_rgb;
    cv::max(img, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);
    cv::pow(clamped, 1.0f/2.2f, gamma_rgb);

    View rgb8, bgr8, lab;
    gamma_rgb.convertTo(rgb8, CV_8UC3, 255.0);
    cv::cvtColor(rgb8, bgr8, cv::COLOR_RGB2BGR);
    cv::cvtColor(bgr8, lab, cv::COLOR_BGR2Lab);

    std::vector<View> ch(3);
    cv::split(lab, ch);

    int ks = static_cast<int>(strength / 10.0f) * 2 + 3;
    ks = std::min(15, ks);
    if (ks % 2 == 0) ks++;

    cv::GaussianBlur(ch[1], ch[1], cv::Size(ks, ks), 0);
    cv::GaussianBlur(ch[2], ch[2], cv::Size(ks, ks), 0);

    View lab_dn, bgr_out, rgb8_out, gamma_out;
    cv::merge(ch, lab_dn);
    cv::cvtColor(lab_dn, bgr_out, cv::COLOR_Lab2BGR);
    cv::cvtColor(bgr_out, rgb8_out, cv::COLOR_BGR2RGB);
    rgb8_out.convertTo(gamma_out, CV_32FC3, 1.0/255.0);
    cv::pow(gamma_out, 2.2f, img);
}

bool detail(const View& in, View& out,
    Dial sharpen_amount, Dial sharpen_radius,
    Dial denoise_luma, Dial denoise_chroma)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::detail] invalid input\n";
        return false;
    }

    sharpen_amount = std::clamp(sharpen_amount, 0.0f, 1.0f);
    sharpen_radius = std::clamp(sharpen_radius, 0.0f, 1.0f);
    denoise_luma = std::clamp(denoise_luma, 0.0f, 1.0f);
    denoise_chroma = std::clamp(denoise_chroma, 0.0f, 1.0f);

    float amount = sharpen_amount * 2.0f;
    float radius = 0.5f + sharpen_radius * 2.5f;
    float dn_luma = 100.0f * denoise_luma * denoise_luma;
    float dn_chroma = 100.0f * denoise_chroma * denoise_chroma;

    bool needs_sharp = (amount > 0.01f);
    bool needs_dn_l = (dn_luma > 1.0f);
    bool needs_dn_c = (dn_chroma > 1.0f);

    if (!needs_sharp && !needs_dn_l && !needs_dn_c)
    {
        in.copyTo(out);
        return true;
    }

    in.copyTo(out);

    if (needs_sharp) apply_sharpen(out, amount, radius);
    if (needs_dn_l) apply_denoise_luma(out, dn_luma);
    if (needs_dn_c) apply_denoise_chroma(out, dn_chroma);

    cv::max(out, 0.0f, out);
    cv::min(out, 1.0f, out);

    return true;
}

} // namespace mods
} // namespace vibe

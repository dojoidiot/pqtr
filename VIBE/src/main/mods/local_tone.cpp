// local_tone.cpp - VIBE
// Local Tone Mapping Module (Iridix-style)

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

namespace vibe
{
namespace mods
{

static void compute_local_mean(const cv::Mat& lum, cv::Mat& local_mean, int window)
{
    int ks = window | 1;
    cv::GaussianBlur(lum, local_mean, cv::Size(ks, ks), 0);
}

static float asymmetric_weight(float intensity, float delta)
{
    float num = std::log(intensity + delta) - std::log(delta);
    float den = std::log(1.0f + delta) - std::log(delta);
    return num / den;
}

static float transform_strength(float intensity, float delta)
{
    float f = asymmetric_weight(intensity, delta);
    return 0.5f - 0.5f * std::tanh(4.0f * f - 2.0f);
}

bool local_tone(const View& in, View& out, float strength, float delta, float window_scale)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::local_tone] invalid input\n";
        return false;
    }

    cv::Mat cpu;
    in.copyTo(cpu);

    // Extract luminance
    cv::Mat lum(cpu.size(), CV_32FC1);
    for (int y = 0; y < cpu.rows; y++)
    {
        const float* src = cpu.ptr<float>(y);
        float* dst = lum.ptr<float>(y);
        for (int x = 0; x < cpu.cols; x++)
        {
            dst[x] = 0.2126f * src[x*3+2] + 0.7152f * src[x*3+1] + 0.0722f * src[x*3];
        }
    }

    int base_window = std::max(5, int(std::min(cpu.rows, cpu.cols) * window_scale));
    cv::Mat local_mean;
    compute_local_mean(lum, local_mean, base_window);

    cv::Mat result(cpu.size(), CV_32FC3);

    for (int y = 0; y < cpu.rows; y++)
    {
        const float* src = cpu.ptr<float>(y);
        const float* lm = local_mean.ptr<float>(y);
        const float* lp = lum.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < cpu.cols; x++)
        {
            float local_lum = std::max(0.001f, lm[x]);
            float pixel_lum = std::max(0.001f, lp[x]);

            float alpha = transform_strength(local_lum, delta);
            float lift_target = 0.18f + (1.0f - 0.18f) * asymmetric_weight(local_lum, delta);
            float target_lum = local_lum + strength * alpha * (lift_target - local_lum);

            float scale = target_lum / local_lum;

            if (pixel_lum > 0.7f)
            {
                float suppress = std::max(0.0f, 1.0f - (pixel_lum - 0.7f) / 0.3f);
                scale = 1.0f + (scale - 1.0f) * suppress;
            }

            scale = std::min(scale, 2.0f);

            dst[x * 3] = std::clamp(src[x * 3] * scale, 0.0f, 1.0f);
            dst[x * 3 + 1] = std::clamp(src[x * 3 + 1] * scale, 0.0f, 1.0f);
            dst[x * 3 + 2] = std::clamp(src[x * 3 + 2] * scale, 0.0f, 1.0f);
        }
    }

    result.copyTo(out);
    return true;
}

bool estimate_local_tone(const View& base, const View& target,
    float& strength, float& delta, float& window_scale)
{
    if (base.empty() || target.empty())
    {
        std::cerr << "[vibe::estimate_local_tone] invalid input\n";
        return false;
    }

    cv::Mat base_cpu, target_cpu;
    base.copyTo(base_cpu);
    target.copyTo(target_cpu);

    if (base_cpu.size() != target_cpu.size())
        cv::resize(base_cpu, base_cpu, target_cpu.size(), 0, 0, cv::INTER_AREA);

    cv::Mat target_f;
    if (target_cpu.type() == CV_8UC3)
        target_cpu.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);
    else
        target_f = target_cpu;

    cv::Mat base_gamma;
    cv::max(base_cpu, 0.0f, base_gamma);
    cv::min(base_gamma, 1.0f, base_gamma);
    cv::pow(base_gamma, 1.0f / 2.2f, base_gamma);

    auto extract_lum = [](const cv::Mat& img) -> cv::Mat {
        cv::Mat lum(img.size(), CV_32FC1);
        for (int y = 0; y < img.rows; y++)
        {
            const float* src = img.ptr<float>(y);
            float* dst = lum.ptr<float>(y);
            for (int x = 0; x < img.cols; x++)
                dst[x] = 0.2126f * src[x*3+2] + 0.7152f * src[x*3+1] + 0.0722f * src[x*3];
        }
        return lum;
    };

    cv::Mat base_lum = extract_lum(base_gamma);
    cv::Mat target_lum = extract_lum(target_f);

    float base_shadow = 0, target_shadow = 0;
    int shadow_count = 0;

    for (int y = 0; y < base_lum.rows; y++)
    {
        const float* bl = base_lum.ptr<float>(y);
        const float* tl = target_lum.ptr<float>(y);
        for (int x = 0; x < base_lum.cols; x++)
        {
            if (bl[x] < 0.3f)
            {
                base_shadow += bl[x];
                target_shadow += tl[x];
                shadow_count++;
            }
        }
    }

    if (shadow_count > 0)
    {
        base_shadow /= shadow_count;
        target_shadow /= shadow_count;
    }

    float ratio = (shadow_count > 0 && base_shadow > 0.01f) ? (target_shadow / base_shadow) : 1.0f;
    strength = std::clamp(ratio - 1.0f, 0.0f, 1.0f);
    delta = 0.02f;
    window_scale = 0.1f;

    return true;
}

} // namespace mods
} // namespace vibe

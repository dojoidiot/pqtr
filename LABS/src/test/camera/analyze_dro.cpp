// analyze_dro.cpp
// Analyze what DRO level was likely applied based on image characteristics
//
// Hypothesis: DRO Auto picks a level based on shadow content.
// High shadow content → higher DRO → more lift
// We can detect this by comparing shadow regions between RAW-derived and JPEG.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include "mods/mods.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== DRO Analysis ===" << std::endl;
    std::cout << "File: " << raw_path << std::endl;

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head)
    {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    cv::Mat camera_jpeg;
    head->view().view().copyTo(camera_jpeg);

    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    cv::Mat scene_gamma;
    cv::max(scene_resized, 0.0f, scene_gamma);
    cv::min(scene_gamma, 1.0f, scene_gamma);
    cv::pow(scene_gamma, 1.0f / 2.2f, scene_gamma);

    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

    // Extract luminance
    auto extract_lum = [](const cv::Mat& img) -> cv::Mat {
        cv::Mat lum(img.size(), CV_32FC1);
        for (int y = 0; y < img.rows; y++)
        {
            const float* src = img.ptr<float>(y);
            float* dst = lum.ptr<float>(y);
            for (int x = 0; x < img.cols; x++)
            {
                dst[x] = 0.2126f * src[x*3+2] + 0.7152f * src[x*3+1] + 0.0722f * src[x*3+0];
            }
        }
        return lum;
    };

    cv::Mat scene_lum = extract_lum(scene_gamma);
    cv::Mat target_lum = extract_lum(target_f);

    // Analyze different luminance zones
    std::cout << "\n=== Luminance Zone Analysis ===" << std::endl;

    struct Zone {
        const char* name;
        float low, high;
    };

    Zone zones[] = {
        {"Deep Shadow (0.00-0.10)", 0.00f, 0.10f},
        {"Shadow     (0.10-0.25)", 0.10f, 0.25f},
        {"Low-Mid    (0.25-0.40)", 0.25f, 0.40f},
        {"Mid        (0.40-0.60)", 0.40f, 0.60f},
        {"High-Mid   (0.60-0.75)", 0.60f, 0.75f},
        {"Highlight  (0.75-0.90)", 0.75f, 0.90f},
        {"Bright     (0.90-1.00)", 0.90f, 1.00f}
    };

    for (const auto& zone : zones)
    {
        float scene_sum = 0, target_sum = 0;
        int count = 0;

        for (int y = 0; y < scene_lum.rows; y++)
        {
            const float* sl = scene_lum.ptr<float>(y);
            const float* tl = target_lum.ptr<float>(y);

            for (int x = 0; x < scene_lum.cols; x++)
            {
                if (sl[x] >= zone.low && sl[x] < zone.high)
                {
                    scene_sum += sl[x];
                    target_sum += tl[x];
                    count++;
                }
            }
        }

        if (count > 0)
        {
            float scene_mean = scene_sum / count;
            float target_mean = target_sum / count;
            float lift_ratio = (scene_mean > 0.01f) ? target_mean / scene_mean : 1.0f;

            printf("  %s: scene=%.3f target=%.3f lift=%.2fx (%d pixels)\n",
                   zone.name, scene_mean, target_mean, lift_ratio, count);
        }
    }

    // Estimate overall DRO level based on shadow lift
    float shadow_sum_scene = 0, shadow_sum_target = 0;
    int shadow_count = 0;

    for (int y = 0; y < scene_lum.rows; y++)
    {
        const float* sl = scene_lum.ptr<float>(y);
        const float* tl = target_lum.ptr<float>(y);

        for (int x = 0; x < scene_lum.cols; x++)
        {
            if (sl[x] < 0.25f)  // Shadow region
            {
                shadow_sum_scene += sl[x];
                shadow_sum_target += tl[x];
                shadow_count++;
            }
        }
    }

    if (shadow_count > 0)
    {
        float shadow_scene = shadow_sum_scene / shadow_count;
        float shadow_target = shadow_sum_target / shadow_count;
        float lift = shadow_target / std::max(0.01f, shadow_scene);

        std::cout << "\n=== DRO Level Estimate ===" << std::endl;
        std::cout << "Shadow content: " << (100.0f * shadow_count / (scene_lum.rows * scene_lum.cols)) << "%" << std::endl;
        std::cout << "Shadow lift ratio: " << lift << "x" << std::endl;

        // Map lift ratio to DRO level
        // Based on empirical observations:
        // DRO Off: lift ~1.0
        // DRO Lv1: lift ~1.1-1.2
        // DRO Lv2: lift ~1.2-1.4
        // DRO Lv3: lift ~1.4-1.6
        // DRO Lv4: lift ~1.6-1.8
        // DRO Lv5: lift ~1.8-2.0+

        int estimated_level;
        if (lift < 1.1f) estimated_level = 0;
        else if (lift < 1.25f) estimated_level = 1;
        else if (lift < 1.4f) estimated_level = 2;
        else if (lift < 1.6f) estimated_level = 3;
        else if (lift < 1.8f) estimated_level = 4;
        else estimated_level = 5;

        std::cout << "Estimated DRO Level: " << estimated_level << std::endl;
    }

    // Also analyze color shifts per-zone
    std::cout << "\n=== Color Shift Analysis (Shadow Region) ===" << std::endl;

    float r_diff = 0, g_diff = 0, b_diff = 0;
    int color_count = 0;

    for (int y = 0; y < scene_gamma.rows; y++)
    {
        const float* src = scene_gamma.ptr<float>(y);
        const float* tgt = target_f.ptr<float>(y);
        const float* lum = scene_lum.ptr<float>(y);

        for (int x = 0; x < scene_gamma.cols; x++)
        {
            if (lum[x] < 0.25f)  // Shadow region
            {
                b_diff += tgt[x*3+0] - src[x*3+0];
                g_diff += tgt[x*3+1] - src[x*3+1];
                r_diff += tgt[x*3+2] - src[x*3+2];
                color_count++;
            }
        }
    }

    if (color_count > 0)
    {
        printf("Shadow color shift: R=%+.3f G=%+.3f B=%+.3f\n",
               r_diff / color_count, g_diff / color_count, b_diff / color_count);
    }

    return 0;
}

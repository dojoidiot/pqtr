// test_multiscale_ltm.cpp
// Multi-scale Local Tone Mapping experiment
//
// The Iridix patent describes using different window sizes (Ωi) for different
// spatial frequencies. We implement this using a Gaussian pyramid:
// - Large windows capture global adaptation (room is dark)
// - Medium windows capture object-level adaptation (person in shadow)
// - Small windows capture local detail (texture in shadow)
//
// The blended result should match DRO's per-pixel behavior better than
// single-scale approaches.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include "mods/mods.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

// Build Gaussian pyramid for local mean estimation
std::vector<cv::Mat> build_pyramid(const cv::Mat& img, int levels)
{
    std::vector<cv::Mat> pyramid;
    pyramid.push_back(img.clone());

    for (int i = 1; i < levels; i++)
    {
        cv::Mat down;
        cv::pyrDown(pyramid[i-1], down);
        pyramid.push_back(down);
    }

    return pyramid;
}

// Upsample pyramid level back to original size
cv::Mat upsample_to_size(const cv::Mat& small, cv::Size target_size)
{
    cv::Mat result = small.clone();
    while (result.rows < target_size.height || result.cols < target_size.width)
    {
        cv::Mat up;
        cv::pyrUp(result, up);
        result = up;
    }
    // Final resize to exact size
    cv::resize(result, result, target_size, 0, 0, cv::INTER_LINEAR);
    return result;
}

// Multi-scale local tone mapping
cv::Mat multiscale_ltm(
    const cv::Mat& input,
    float strength,
    float delta = 0.02f,
    int num_scales = 4)
{
    // Extract luminance
    cv::Mat lum(input.size(), CV_32FC1);
    for (int y = 0; y < input.rows; y++)
    {
        const float* src = input.ptr<float>(y);
        float* dst = lum.ptr<float>(y);
        for (int x = 0; x < input.cols; x++)
        {
            float b = src[x * 3 + 0];
            float g = src[x * 3 + 1];
            float r = src[x * 3 + 2];
            dst[x] = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        }
    }

    // Build pyramid of local means
    std::vector<cv::Mat> lum_pyramid = build_pyramid(lum, num_scales);

    // Blur each level to get smooth local means
    for (auto& level : lum_pyramid)
    {
        cv::GaussianBlur(level, level, cv::Size(15, 15), 0);
    }

    // Upsample all levels back to original size
    std::vector<cv::Mat> local_means;
    for (int i = 0; i < num_scales; i++)
    {
        local_means.push_back(upsample_to_size(lum_pyramid[i], input.size()));
    }

    // Blend local means with weights favoring medium scales
    // (small scales = fine detail, large scales = global)
    std::vector<float> weights = {0.1f, 0.3f, 0.4f, 0.2f};  // favor medium scales
    if (num_scales != 4)
    {
        weights.resize(num_scales);
        float sum = 0;
        for (int i = 0; i < num_scales; i++)
        {
            weights[i] = 1.0f / num_scales;
            sum += weights[i];
        }
    }

    cv::Mat blended_mean = cv::Mat::zeros(input.size(), CV_32FC1);
    for (int i = 0; i < num_scales; i++)
    {
        blended_mean += weights[i] * local_means[i];
    }

    // Apply tone mapping based on blended local mean
    cv::Mat result(input.size(), CV_32FC3);

    auto asymmetric_weight = [delta](float intensity) -> float {
        float num = std::log(intensity + delta) - std::log(delta);
        float den = std::log(1.0f + delta) - std::log(delta);
        return num / den;
    };

    auto transform_strength = [&asymmetric_weight](float intensity, float delta) -> float {
        float f = asymmetric_weight(intensity);
        return 0.5f - 0.5f * std::tanh(4.0f * f - 2.0f);
    };

    for (int y = 0; y < input.rows; y++)
    {
        const float* src = input.ptr<float>(y);
        const float* lm = blended_mean.ptr<float>(y);
        const float* pl = lum.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < input.cols; x++)
        {
            float local_lum = std::max(0.001f, lm[x]);
            float pixel_lum = std::max(0.001f, pl[x]);

            // Compute adaptive strength
            float alpha = transform_strength(local_lum, delta);

            // Target luminance lift
            float lift_target = 0.18f + (1.0f - 0.18f) * asymmetric_weight(local_lum);
            float target_lum = local_lum + strength * alpha * (lift_target - local_lum);

            // Scale factor
            float scale = target_lum / local_lum;

            // Highlight protection
            if (pixel_lum > 0.7f)
            {
                float suppress = 1.0f - (pixel_lum - 0.7f) / 0.3f;
                suppress = std::max(0.0f, suppress);
                scale = 1.0f + (scale - 1.0f) * suppress;
            }

            scale = std::min(scale, 2.0f);

            dst[x * 3 + 0] = std::max(0.0f, std::min(1.0f, src[x * 3 + 0] * scale));
            dst[x * 3 + 1] = std::max(0.0f, std::min(1.0f, src[x * 3 + 1] * scale));
            dst[x * 3 + 2] = std::max(0.0f, std::min(1.0f, src[x * 3 + 2] * scale));
        }
    }

    return result;
}

float measure_error(const cv::Mat& a, const cv::Mat& b)
{
    cv::Mat diff;
    cv::absdiff(a, b, diff);
    cv::Scalar mean = cv::mean(diff);
    return (mean[0] + mean[1] + mean[2]) / 3.0f * 100.0f;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Multi-Scale Local Tone Mapping Test ===" << std::endl;
    std::cout << "File: " << raw_path << std::endl;

    // Decode RAW
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

    // Resize and gamma
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    cv::Mat scene_gamma;
    cv::max(scene_resized, 0.0f, scene_gamma);
    cv::min(scene_gamma, 1.0f, scene_gamma);
    cv::pow(scene_gamma, 1.0f / 2.2f, scene_gamma);

    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

    float error_baseline = measure_error(scene_gamma, target_f);
    std::cout << "\nBaseline error (gamma only): " << error_baseline << "%" << std::endl;

    // Test multi-scale LTM
    std::cout << "\n=== Testing Multi-Scale LTM ===" << std::endl;

    float best_error = error_baseline;
    float best_strength = 0.0f;
    int best_scales = 4;

    for (int scales : {3, 4, 5})
    {
        for (float strength : {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f})
        {
            cv::Mat result = multiscale_ltm(scene_gamma, strength, 0.02f, scales);
            float error = measure_error(result, target_f);

            if (error < best_error)
            {
                best_error = error;
                best_strength = strength;
                best_scales = scales;
                std::cout << "  scales=" << scales << " strength=" << strength
                          << " -> error=" << error << "% (new best)" << std::endl;
            }
        }
    }

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Baseline error: " << error_baseline << "%" << std::endl;
    std::cout << "Best multi-scale LTM error: " << best_error << "%" << std::endl;
    std::cout << "  scales=" << best_scales << ", strength=" << best_strength << std::endl;

    // Now test polynomial + multi-scale LTM
    std::cout << "\n=== Testing Polynomial + Multi-Scale LTM ===" << std::endl;

    // Estimate polynomial
    float poly_coeffs[30];
    cv::UMat scene_umat, target_umat;
    scene_linear.copyTo(scene_umat);
    camera_jpeg.copyTo(target_umat);

    if (pipe::mods::estimate_poly_color(scene_umat, target_umat, poly_coeffs, 50000))
    {
        cv::UMat poly_input, poly_output;
        scene_gamma.copyTo(poly_input);

        if (pipe::mods::poly_color(poly_input, poly_output, poly_coeffs))
        {
            cv::Mat poly_result;
            poly_output.copyTo(poly_result);

            float error_poly = measure_error(poly_result, target_f);
            std::cout << "Polynomial only error: " << error_poly << "%" << std::endl;

            // Apply multi-scale LTM to polynomial result
            float combined_best = error_poly;
            float combined_strength = 0.0f;

            for (float strength : {0.1f, 0.2f, 0.3f, 0.4f, 0.5f})
            {
                cv::Mat combined = multiscale_ltm(poly_result, strength, 0.02f, best_scales);
                float error = measure_error(combined, target_f);

                if (error < combined_best)
                {
                    combined_best = error;
                    combined_strength = strength;
                    std::cout << "  Poly+MS_LTM strength=" << strength
                              << " -> error=" << error << "%" << std::endl;
                }
            }

            std::cout << "\nFinal Results:" << std::endl;
            std::cout << "  Baseline (gamma): " << error_baseline << "%" << std::endl;
            std::cout << "  Multi-scale LTM alone: " << best_error << "%" << std::endl;
            std::cout << "  Polynomial alone: " << error_poly << "%" << std::endl;
            std::cout << "  Poly + MS_LTM: " << combined_best << "%" << std::endl;

            // Save comparison
            cv::Mat final_result = multiscale_ltm(poly_result, combined_strength, 0.02f, best_scales);
            cv::Mat result_8u;
            final_result.convertTo(result_8u, CV_8UC3, 255.0);

            cv::Mat comparison;
            cv::hconcat(camera_jpeg, result_8u, comparison);
            cv::imwrite("tmp/var/tune/multiscale_ltm.png", comparison);
            std::cout << "\nSaved: tmp/var/tune/multiscale_ltm.png" << std::endl;
        }
    }

    return 0;
}

// test_regional_poly.cpp
// Per-region polynomial fitting
//
// Key insight: DRO applies different transforms to different luminance regions.
// A single polynomial averages over all regions, losing spatial variance.
//
// Approach: Fit separate polynomials for shadow/midtone/highlight regions,
// then blend based on local luminance.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include "mods/mods.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <random>
#include <vector>

// Solve least squares for polynomial coefficients
void solve_poly(const std::vector<std::vector<float>>& inputs,
               const std::vector<float>& outputs,
               float* coeffs)
{
    int n = inputs.size();
    if (n < 20) {
        // Not enough samples, use identity
        for (int i = 0; i < 10; i++) coeffs[i] = 0;
        return;
    }

    cv::Mat AtA(10, 10, CV_64FC1, cv::Scalar(0));
    cv::Mat Atb(10, 1, CV_64FC1, cv::Scalar(0));

    for (int i = 0; i < n; i++)
    {
        const auto& row = inputs[i];
        double y = outputs[i];
        for (int j = 0; j < 10; j++)
        {
            Atb.at<double>(j, 0) += row[j] * y;
            for (int k = 0; k < 10; k++)
                AtA.at<double>(j, k) += row[j] * row[k];
        }
    }

    cv::Mat x;
    cv::solve(AtA, Atb, x, cv::DECOMP_SVD);
    for (int i = 0; i < 10; i++)
        coeffs[i] = static_cast<float>(x.at<double>(i, 0));
}

// Apply polynomial
float apply_poly(float r, float g, float b, const float* coeffs)
{
    float terms[10] = {
        1.0f, r, g, b,
        r*r, g*g, b*b,
        r*g, r*b, g*b
    };
    float out = 0;
    for (int i = 0; i < 10; i++)
        out += coeffs[i] * terms[i];
    return out;
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
    std::cout << "=== Regional Polynomial Test ===" << std::endl;
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

    float error_baseline = measure_error(scene_gamma, target_f);
    std::cout << "\nBaseline error (gamma only): " << error_baseline << "%" << std::endl;

    // Collect samples grouped by luminance region
    // Regions: [0-0.2], [0.2-0.4], [0.4-0.6], [0.6-0.8], [0.8-1.0]
    const int NUM_REGIONS = 5;
    std::vector<std::vector<std::vector<float>>> region_samples_r(NUM_REGIONS);
    std::vector<std::vector<std::vector<float>>> region_samples_g(NUM_REGIONS);
    std::vector<std::vector<std::vector<float>>> region_samples_b(NUM_REGIONS);
    std::vector<std::vector<float>> region_targets_r(NUM_REGIONS);
    std::vector<std::vector<float>> region_targets_g(NUM_REGIONS);
    std::vector<std::vector<float>> region_targets_b(NUM_REGIONS);

    std::cout << "\nSampling pixels by luminance region..." << std::endl;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist_y(0, scene_gamma.rows - 1);
    std::uniform_int_distribution<int> dist_x(0, scene_gamma.cols - 1);

    for (int i = 0; i < 200000; i++)  // More samples for regional fitting
    {
        int y = dist_y(rng);
        int x = dist_x(rng);

        const float* src = scene_gamma.ptr<float>(y) + x * 3;
        const float* tgt = target_f.ptr<float>(y) + x * 3;

        float b = src[0], g = src[1], r = src[2];
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;

        // Determine region
        int region = std::min(NUM_REGIONS - 1, (int)(lum * NUM_REGIONS));

        std::vector<float> features = {
            1.0f, r, g, b,
            r*r, g*g, b*b,
            r*g, r*b, g*b
        };

        region_samples_r[region].push_back(features);
        region_samples_g[region].push_back(features);
        region_samples_b[region].push_back(features);
        region_targets_r[region].push_back(tgt[2]);
        region_targets_g[region].push_back(tgt[1]);
        region_targets_b[region].push_back(tgt[0]);
    }

    // Fit polynomials per region
    float region_coeffs[NUM_REGIONS][30];

    std::cout << "\nFitting polynomials per region:" << std::endl;
    for (int reg = 0; reg < NUM_REGIONS; reg++)
    {
        int n = region_samples_r[reg].size();
        std::cout << "  Region " << reg << " [" << (reg*0.2f) << "-" << ((reg+1)*0.2f) << "]: "
                  << n << " samples" << std::endl;

        solve_poly(region_samples_r[reg], region_targets_r[reg], region_coeffs[reg]);
        solve_poly(region_samples_g[reg], region_targets_g[reg], region_coeffs[reg] + 10);
        solve_poly(region_samples_b[reg], region_targets_b[reg], region_coeffs[reg] + 20);
    }

    // Apply with blending between regions
    std::cout << "\nApplying regional polynomials..." << std::endl;

    cv::Mat result(scene_gamma.size(), CV_32FC3);

    for (int y = 0; y < scene_gamma.rows; y++)
    {
        const float* src = scene_gamma.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < scene_gamma.cols; x++)
        {
            float b = src[x*3+0], g = src[x*3+1], r = src[x*3+2];
            float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;

            // Soft region selection with blending
            float pos = lum * NUM_REGIONS;
            int reg0 = std::max(0, std::min(NUM_REGIONS - 1, (int)pos));
            int reg1 = std::min(NUM_REGIONS - 1, reg0 + 1);
            float blend = pos - reg0;
            blend = std::max(0.0f, std::min(1.0f, blend));

            // Apply both region polynomials and blend
            float r_out0 = apply_poly(r, g, b, region_coeffs[reg0]);
            float g_out0 = apply_poly(r, g, b, region_coeffs[reg0] + 10);
            float b_out0 = apply_poly(r, g, b, region_coeffs[reg0] + 20);

            float r_out1 = apply_poly(r, g, b, region_coeffs[reg1]);
            float g_out1 = apply_poly(r, g, b, region_coeffs[reg1] + 10);
            float b_out1 = apply_poly(r, g, b, region_coeffs[reg1] + 20);

            float r_out = r_out0 * (1.0f - blend) + r_out1 * blend;
            float g_out = g_out0 * (1.0f - blend) + g_out1 * blend;
            float b_out = b_out0 * (1.0f - blend) + b_out1 * blend;

            dst[x*3+0] = std::max(0.0f, std::min(1.0f, b_out));
            dst[x*3+1] = std::max(0.0f, std::min(1.0f, g_out));
            dst[x*3+2] = std::max(0.0f, std::min(1.0f, r_out));
        }
    }

    float error_regional = measure_error(result, target_f);

    // Also compute single polynomial for comparison
    std::vector<std::vector<float>> all_samples_r, all_samples_g, all_samples_b;
    std::vector<float> all_targets_r, all_targets_g, all_targets_b;

    for (int reg = 0; reg < NUM_REGIONS; reg++)
    {
        all_samples_r.insert(all_samples_r.end(), region_samples_r[reg].begin(), region_samples_r[reg].end());
        all_samples_g.insert(all_samples_g.end(), region_samples_g[reg].begin(), region_samples_g[reg].end());
        all_samples_b.insert(all_samples_b.end(), region_samples_b[reg].begin(), region_samples_b[reg].end());
        all_targets_r.insert(all_targets_r.end(), region_targets_r[reg].begin(), region_targets_r[reg].end());
        all_targets_g.insert(all_targets_g.end(), region_targets_g[reg].begin(), region_targets_g[reg].end());
        all_targets_b.insert(all_targets_b.end(), region_targets_b[reg].begin(), region_targets_b[reg].end());
    }

    float single_coeffs[30];
    solve_poly(all_samples_r, all_targets_r, single_coeffs);
    solve_poly(all_samples_g, all_targets_g, single_coeffs + 10);
    solve_poly(all_samples_b, all_targets_b, single_coeffs + 20);

    cv::Mat single_result(scene_gamma.size(), CV_32FC3);
    for (int y = 0; y < scene_gamma.rows; y++)
    {
        const float* src = scene_gamma.ptr<float>(y);
        float* dst = single_result.ptr<float>(y);

        for (int x = 0; x < scene_gamma.cols; x++)
        {
            float b = src[x*3+0], g = src[x*3+1], r = src[x*3+2];
            dst[x*3+0] = std::max(0.0f, std::min(1.0f, apply_poly(r, g, b, single_coeffs + 20)));
            dst[x*3+1] = std::max(0.0f, std::min(1.0f, apply_poly(r, g, b, single_coeffs + 10)));
            dst[x*3+2] = std::max(0.0f, std::min(1.0f, apply_poly(r, g, b, single_coeffs)));
        }
    }

    float error_single = measure_error(single_result, target_f);

    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Baseline (gamma): " << error_baseline << "%" << std::endl;
    std::cout << "Single polynomial: " << error_single << "%" << std::endl;
    std::cout << "Regional polynomials (" << NUM_REGIONS << " regions): " << error_regional << "%" << std::endl;

    // Save comparison
    cv::Mat result_8u;
    result.convertTo(result_8u, CV_8UC3, 255.0);
    cv::Mat comparison;
    cv::hconcat(camera_jpeg, result_8u, comparison);
    cv::imwrite("tmp/var/tune/regional_poly.png", comparison);
    std::cout << "\nSaved: tmp/var/tune/regional_poly.png" << std::endl;

    return 0;
}

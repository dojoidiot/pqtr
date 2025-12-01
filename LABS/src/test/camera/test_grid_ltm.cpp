// test_grid_ltm.cpp
// Grid-based Local Tone Mapping - simulating what a camera can actually do
//
// Camera constraints require a simple algorithm:
// 1. Downsample to coarse grid (e.g., 16x16 or 32x32)
// 2. Compute local luminance per cell
// 3. Look up lift factor from precomputed LUT
// 4. Bilinear interpolate lift map to full resolution
// 5. Apply lift
//
// This is what DRO likely does - not fancy multi-scale pyramids.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include "mods/mods.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

// Compute grid-based local mean
cv::Mat compute_grid_local_mean(const cv::Mat& lum, int grid_size)
{
    int cell_h = lum.rows / grid_size;
    int cell_w = lum.cols / grid_size;

    // Compute mean per cell
    cv::Mat grid_means(grid_size, grid_size, CV_32FC1);

    for (int gy = 0; gy < grid_size; gy++)
    {
        for (int gx = 0; gx < grid_size; gx++)
        {
            int y0 = gy * cell_h;
            int x0 = gx * cell_w;
            int y1 = std::min((gy + 1) * cell_h, lum.rows);
            int x1 = std::min((gx + 1) * cell_w, lum.cols);

            float sum = 0;
            int count = 0;
            for (int y = y0; y < y1; y++)
            {
                const float* row = lum.ptr<float>(y);
                for (int x = x0; x < x1; x++)
                {
                    sum += row[x];
                    count++;
                }
            }
            grid_means.at<float>(gy, gx) = (count > 0) ? sum / count : 0.5f;
        }
    }

    // Upsample with bilinear interpolation
    cv::Mat local_mean;
    cv::resize(grid_means, local_mean, lum.size(), 0, 0, cv::INTER_LINEAR);

    return local_mean;
}

// DRO-style lift function (asymmetric - more lift for darker regions)
float dro_lift_factor(float local_lum, float strength)
{
    // Lift curve: darker regions get more lift
    // At local_lum=0.1, lift by ~strength
    // At local_lum=0.5, lift by ~0.3*strength
    // At local_lum=0.8, lift by ~0
    float lift = strength * (1.0f - local_lum) * (1.0f - local_lum);
    return 1.0f + lift;
}

// Apply grid-based local tone mapping
cv::Mat grid_ltm(const cv::Mat& input, float strength, int grid_size)
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

    // Compute grid-based local mean
    cv::Mat local_mean = compute_grid_local_mean(lum, grid_size);

    // Compute lift map
    cv::Mat lift_map(input.size(), CV_32FC1);
    for (int y = 0; y < input.rows; y++)
    {
        const float* lm = local_mean.ptr<float>(y);
        float* lift = lift_map.ptr<float>(y);
        for (int x = 0; x < input.cols; x++)
        {
            lift[x] = dro_lift_factor(lm[x], strength);
        }
    }

    // Apply lift while preserving color ratios
    cv::Mat result(input.size(), CV_32FC3);
    for (int y = 0; y < input.rows; y++)
    {
        const float* src = input.ptr<float>(y);
        const float* lift = lift_map.ptr<float>(y);
        const float* pl = lum.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < input.cols; x++)
        {
            float scale = lift[x];

            // Reduce lift in highlights
            if (pl[x] > 0.6f)
            {
                float suppress = 1.0f - (pl[x] - 0.6f) / 0.4f;
                suppress = std::max(0.0f, suppress);
                scale = 1.0f + (scale - 1.0f) * suppress;
            }

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
    std::cout << "=== Grid-Based Local Tone Mapping Test ===" << std::endl;
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

    // Test grid LTM with various parameters
    std::cout << "\n=== Testing Grid-Based LTM ===" << std::endl;

    float best_error = error_baseline;
    float best_strength = 0.0f;
    int best_grid = 16;

    for (int grid : {8, 16, 32, 64})
    {
        for (float strength : {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f})
        {
            cv::Mat result = grid_ltm(scene_gamma, strength, grid);
            float error = measure_error(result, target_f);

            if (error < best_error)
            {
                best_error = error;
                best_strength = strength;
                best_grid = grid;
                std::cout << "  grid=" << grid << " strength=" << strength
                          << " -> error=" << error << "% (new best)" << std::endl;
            }
        }
    }

    // Now try polynomial first, then grid LTM
    std::cout << "\n=== Testing Polynomial + Grid LTM ===" << std::endl;

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

            float combined_best = error_poly;
            float combined_strength = 0.0f;
            int combined_grid = 16;

            for (int grid : {16, 32})
            {
                for (float strength : {0.3f, 0.5f, 0.7f, 1.0f, 1.5f})
                {
                    cv::Mat combined = grid_ltm(poly_result, strength, grid);
                    float error = measure_error(combined, target_f);

                    if (error < combined_best)
                    {
                        combined_best = error;
                        combined_strength = strength;
                        combined_grid = grid;
                        std::cout << "  Poly+Grid grid=" << grid << " strength=" << strength
                                  << " -> error=" << error << "%" << std::endl;
                    }
                }
            }

            std::cout << "\nFinal Results:" << std::endl;
            std::cout << "  Baseline (gamma): " << error_baseline << "%" << std::endl;
            std::cout << "  Grid LTM alone: " << best_error << "%" << std::endl;
            std::cout << "  Polynomial alone: " << error_poly << "%" << std::endl;
            std::cout << "  Poly + Grid LTM: " << combined_best << "%" << std::endl;

            // Save final comparison
            cv::Mat final_result = grid_ltm(poly_result, combined_strength, combined_grid);
            cv::Mat result_8u;
            final_result.convertTo(result_8u, CV_8UC3, 255.0);

            cv::Mat comparison;
            cv::hconcat(camera_jpeg, result_8u, comparison);
            cv::imwrite("tmp/var/tune/grid_ltm.png", comparison);
            std::cout << "\nSaved: tmp/var/tune/grid_ltm.png" << std::endl;
        }
    }

    return 0;
}

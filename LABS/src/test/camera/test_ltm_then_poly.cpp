// test_ltm_then_poly.cpp
// Test applying Local TM BEFORE polynomial
//
// Hypothesis: DRO happens early in the pipeline, modifying the luminance
// distribution before color grading. If we apply LTM first to match
// the luminance distribution, then the polynomial can fit color better.

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

// Grid-based local luminance lift
cv::Mat apply_local_lift(const cv::Mat& input, float strength, int grid_size)
{
    // Extract luminance
    cv::Mat lum(input.size(), CV_32FC1);
    for (int y = 0; y < input.rows; y++)
    {
        const float* src = input.ptr<float>(y);
        float* dst = lum.ptr<float>(y);
        for (int x = 0; x < input.cols; x++)
        {
            dst[x] = 0.2126f * src[x*3+2] + 0.7152f * src[x*3+1] + 0.0722f * src[x*3+0];
        }
    }

    // Compute grid means
    int cell_h = lum.rows / grid_size;
    int cell_w = lum.cols / grid_size;
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

    // Upsample
    cv::Mat local_mean;
    cv::resize(grid_means, local_mean, lum.size(), 0, 0, cv::INTER_LINEAR);

    // Apply lift
    cv::Mat result(input.size(), CV_32FC3);
    for (int y = 0; y < input.rows; y++)
    {
        const float* src = input.ptr<float>(y);
        const float* lm = local_mean.ptr<float>(y);
        const float* pl = lum.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < input.cols; x++)
        {
            float local = std::max(0.01f, lm[x]);
            float lift = strength * (1.0f - local) * (1.0f - local);
            float scale = 1.0f + lift;

            // Suppress in highlights
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
    std::cout << "=== LTM-then-Polynomial Test ===" << std::endl;
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

    // Test: First polynomial (current approach)
    float poly_coeffs[30];
    cv::UMat scene_umat, target_umat;
    scene_linear.copyTo(scene_umat);
    camera_jpeg.copyTo(target_umat);

    pipe::mods::estimate_poly_color(scene_umat, target_umat, poly_coeffs, 50000);

    cv::UMat poly_in, poly_out;
    scene_gamma.copyTo(poly_in);
    pipe::mods::poly_color(poly_in, poly_out, poly_coeffs);
    cv::Mat poly_result;
    poly_out.copyTo(poly_result);

    float error_poly_only = measure_error(poly_result, target_f);
    std::cout << "Polynomial only error: " << error_poly_only << "%" << std::endl;

    // Test: LTM first, then fit polynomial to LTM'd image
    std::cout << "\n=== Testing LTM-first approach ===" << std::endl;

    float best_error = error_poly_only;
    float best_strength = 0.0f;

    for (float ltm_strength : {0.3f, 0.5f, 0.7f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f})
    {
        // Apply LTM to gamma image
        cv::Mat ltm_result = apply_local_lift(scene_gamma, ltm_strength, 16);

        // Now fit polynomial from LTM'd image to target
        cv::UMat ltm_umat;
        ltm_result.copyTo(ltm_umat);

        float ltm_poly_coeffs[30];
        // Note: estimate_poly_color expects linear input, so we need to adjust
        // For now, estimate directly from gamma-encoded images

        // Fit polynomial from LTM result to target
        // Using sampling approach
        std::vector<std::vector<float>> samples_r, samples_g, samples_b;
        std::vector<float> targets_r, targets_g, targets_b;

        std::mt19937 rng(42);
        std::uniform_int_distribution<int> dist_y(0, ltm_result.rows - 1);
        std::uniform_int_distribution<int> dist_x(0, ltm_result.cols - 1);

        for (int i = 0; i < 50000; i++)
        {
            int y = dist_y(rng);
            int x = dist_x(rng);

            const float* src = ltm_result.ptr<float>(y) + x * 3;
            const float* tgt = target_f.ptr<float>(y) + x * 3;

            float b = src[0], g = src[1], r = src[2];

            std::vector<float> features = {
                1.0f, r, g, b,
                r*r, g*g, b*b,
                r*g, r*b, g*b
            };

            samples_r.push_back(features);
            samples_g.push_back(features);
            samples_b.push_back(features);

            targets_r.push_back(tgt[2]);
            targets_g.push_back(tgt[1]);
            targets_b.push_back(tgt[0]);
        }

        // Solve
        auto solve = [](const std::vector<std::vector<float>>& inputs,
                       const std::vector<float>& outputs,
                       float* coeffs) {
            int n = inputs.size();
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
        };

        solve(samples_r, targets_r, ltm_poly_coeffs);
        solve(samples_g, targets_g, ltm_poly_coeffs + 10);
        solve(samples_b, targets_b, ltm_poly_coeffs + 20);

        // Apply polynomial to LTM'd image
        cv::Mat final_result(ltm_result.size(), CV_32FC3);
        for (int y = 0; y < ltm_result.rows; y++)
        {
            const float* src = ltm_result.ptr<float>(y);
            float* dst = final_result.ptr<float>(y);

            for (int x = 0; x < ltm_result.cols; x++)
            {
                float b = src[x*3+0], g = src[x*3+1], r = src[x*3+2];

                float terms[10] = {
                    1.0f, r, g, b,
                    r*r, g*g, b*b,
                    r*g, r*b, g*b
                };

                float r_out = 0, g_out = 0, b_out = 0;
                for (int i = 0; i < 10; i++)
                {
                    r_out += ltm_poly_coeffs[i] * terms[i];
                    g_out += ltm_poly_coeffs[10+i] * terms[i];
                    b_out += ltm_poly_coeffs[20+i] * terms[i];
                }

                dst[x*3+0] = std::max(0.0f, std::min(1.0f, b_out));
                dst[x*3+1] = std::max(0.0f, std::min(1.0f, g_out));
                dst[x*3+2] = std::max(0.0f, std::min(1.0f, r_out));
            }
        }

        float error = measure_error(final_result, target_f);

        if (error < best_error)
        {
            best_error = error;
            best_strength = ltm_strength;
            std::cout << "  LTM strength=" << ltm_strength << " -> error=" << error << "% (new best)" << std::endl;

            // Save best result
            cv::Mat result_8u;
            final_result.convertTo(result_8u, CV_8UC3, 255.0);
            cv::Mat comparison;
            cv::hconcat(camera_jpeg, result_8u, comparison);
            cv::imwrite("tmp/var/tune/ltm_then_poly.png", comparison);
        }
    }

    std::cout << "\n=== Final Results ===" << std::endl;
    std::cout << "Baseline (gamma): " << error_baseline << "%" << std::endl;
    std::cout << "Polynomial only: " << error_poly_only << "%" << std::endl;
    std::cout << "LTM-first + Polynomial: " << best_error << "% (strength=" << best_strength << ")" << std::endl;

    if (best_error < error_poly_only)
        std::cout << "\nSaved: tmp/var/tune/ltm_then_poly.png" << std::endl;

    return 0;
}

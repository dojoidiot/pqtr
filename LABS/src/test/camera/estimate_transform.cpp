// estimate_transform.cpp
// Estimate the complete RGB→RGB transform from scene-linear to camera JPEG
// using polynomial regression. This captures:
// - Color matrix
// - Tone curve
// - Saturation/color grading
// - Any remaining transforms
//
// Model: Each output channel = polynomial of input RGB
// O_r = a0 + a1*R + a2*G + a3*B + a4*R² + a5*G² + a6*B² + a7*RG + a8*RB + a9*GB
// (10 coefficients per output channel = 30 total)

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <random>

// Least squares solver for polynomial coefficients
void solve_polynomial(const std::vector<std::vector<float>>& inputs,
                     const std::vector<float>& outputs,
                     float* coeffs, int num_coeffs) {
    int n = inputs.size();

    // Build normal equations: A^T A x = A^T b
    cv::Mat AtA(num_coeffs, num_coeffs, CV_64FC1, cv::Scalar(0));
    cv::Mat Atb(num_coeffs, 1, CV_64FC1, cv::Scalar(0));

    for (int i = 0; i < n; i++) {
        const auto& row = inputs[i];
        double y = outputs[i];

        for (int j = 0; j < num_coeffs; j++) {
            Atb.at<double>(j, 0) += row[j] * y;
            for (int k = 0; k < num_coeffs; k++) {
                AtA.at<double>(j, k) += row[j] * row[k];
            }
        }
    }

    // Solve using SVD (more stable than direct inverse)
    cv::Mat x;
    cv::solve(AtA, Atb, x, cv::DECOMP_SVD);

    for (int i = 0; i < num_coeffs; i++) {
        coeffs[i] = static_cast<float>(x.at<double>(i, 0));
    }
}

// Apply polynomial transform
float apply_poly(float r, float g, float b, const float* coeffs) {
    // 10 terms: 1, R, G, B, R², G², B², RG, RB, GB
    return coeffs[0] +
           coeffs[1] * r + coeffs[2] * g + coeffs[3] * b +
           coeffs[4] * r * r + coeffs[5] * g * g + coeffs[6] * b * b +
           coeffs[7] * r * g + coeffs[8] * r * b + coeffs[9] * g * b;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Polynomial Color Transform Estimation ===" << std::endl;
    std::cout << "File: " << raw_path << std::endl;

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    cv::Mat camera_jpeg;
    head->view().view().copyTo(camera_jpeg);

    // Resize scene-linear to match preview
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    // Clamp and apply gamma to get to comparable space
    cv::Mat scene_gamma;
    cv::max(scene_resized, 0.0f, scene_gamma);
    cv::min(scene_gamma, 1.0f, scene_gamma);
    cv::pow(scene_gamma, 1.0f / 2.2f, scene_gamma);

    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

    // Sample pixels for regression (use subset for speed)
    std::cout << "\nSampling pixels for regression..." << std::endl;

    std::vector<std::vector<float>> samples_r, samples_g, samples_b;
    std::vector<float> targets_r, targets_g, targets_b;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist_y(0, scene_gamma.rows - 1);
    std::uniform_int_distribution<int> dist_x(0, scene_gamma.cols - 1);

    const int NUM_SAMPLES = 50000;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        int y = dist_y(rng);
        int x = dist_x(rng);

        const float* src = scene_gamma.ptr<float>(y) + x * 3;
        const float* tgt = target_f.ptr<float>(y) + x * 3;

        float b = src[0], g = src[1], r = src[2];

        // Build feature vector: 1, R, G, B, R², G², B², RG, RB, GB
        std::vector<float> features = {
            1.0f, r, g, b,
            r*r, g*g, b*b,
            r*g, r*b, g*b
        };

        samples_r.push_back(features);
        samples_g.push_back(features);
        samples_b.push_back(features);

        targets_r.push_back(tgt[2]);  // R
        targets_g.push_back(tgt[1]);  // G
        targets_b.push_back(tgt[0]);  // B
    }

    std::cout << "Solving polynomial regression..." << std::endl;

    float coeffs_r[10], coeffs_g[10], coeffs_b[10];
    solve_polynomial(samples_r, targets_r, coeffs_r, 10);
    solve_polynomial(samples_g, targets_g, coeffs_g, 10);
    solve_polynomial(samples_b, targets_b, coeffs_b, 10);

    // Print coefficients
    std::cout << "\n=== Polynomial Coefficients ===" << std::endl;
    std::cout << "  Output = c0 + c1*R + c2*G + c3*B + c4*R² + c5*G² + c6*B² + c7*RG + c8*RB + c9*GB" << std::endl;

    printf("\nR_out: %.4f + %.4f*R + %.4f*G + %.4f*B + %.4f*R² + %.4f*G² + %.4f*B² + %.4f*RG + %.4f*RB + %.4f*GB\n",
           coeffs_r[0], coeffs_r[1], coeffs_r[2], coeffs_r[3],
           coeffs_r[4], coeffs_r[5], coeffs_r[6], coeffs_r[7], coeffs_r[8], coeffs_r[9]);
    printf("G_out: %.4f + %.4f*R + %.4f*G + %.4f*B + %.4f*R² + %.4f*G² + %.4f*B² + %.4f*RG + %.4f*RB + %.4f*GB\n",
           coeffs_g[0], coeffs_g[1], coeffs_g[2], coeffs_g[3],
           coeffs_g[4], coeffs_g[5], coeffs_g[6], coeffs_g[7], coeffs_g[8], coeffs_g[9]);
    printf("B_out: %.4f + %.4f*R + %.4f*G + %.4f*B + %.4f*R² + %.4f*G² + %.4f*B² + %.4f*RG + %.4f*RB + %.4f*GB\n",
           coeffs_b[0], coeffs_b[1], coeffs_b[2], coeffs_b[3],
           coeffs_b[4], coeffs_b[5], coeffs_b[6], coeffs_b[7], coeffs_b[8], coeffs_b[9]);

    // Interpret linear terms as color matrix
    std::cout << "\n=== Effective 3x3 Matrix (linear terms) ===" << std::endl;
    printf("  [%.4f  %.4f  %.4f]  (R_out from R,G,B)\n", coeffs_r[1], coeffs_r[2], coeffs_r[3]);
    printf("  [%.4f  %.4f  %.4f]  (G_out from R,G,B)\n", coeffs_g[1], coeffs_g[2], coeffs_g[3]);
    printf("  [%.4f  %.4f  %.4f]  (B_out from R,G,B)\n", coeffs_b[1], coeffs_b[2], coeffs_b[3]);

    // Apply polynomial transform to full image
    std::cout << "\n=== Applying Transform ===" << std::endl;

    cv::Mat result(scene_gamma.size(), CV_32FC3);

    for (int y = 0; y < scene_gamma.rows; y++) {
        const float* src = scene_gamma.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < scene_gamma.cols; x++) {
            float b = src[x*3 + 0];
            float g = src[x*3 + 1];
            float r = src[x*3 + 2];

            float r_out = apply_poly(r, g, b, coeffs_r);
            float g_out = apply_poly(r, g, b, coeffs_g);
            float b_out = apply_poly(r, g, b, coeffs_b);

            dst[x*3 + 0] = std::max(0.0f, std::min(1.0f, b_out));
            dst[x*3 + 1] = std::max(0.0f, std::min(1.0f, g_out));
            dst[x*3 + 2] = std::max(0.0f, std::min(1.0f, r_out));
        }
    }

    // Measure error
    cv::Mat diff;
    cv::absdiff(result, target_f, diff);
    cv::Scalar mean_diff = cv::mean(diff);
    float error = (mean_diff[0] + mean_diff[1] + mean_diff[2]) / 3.0f * 100.0f;

    std::cout << "Polynomial transform error: " << error << "%" << std::endl;

    // Compare to baseline
    cv::Mat diff_base;
    cv::absdiff(scene_gamma, target_f, diff_base);
    cv::Scalar mean_base = cv::mean(diff_base);
    float error_base = (mean_base[0] + mean_base[1] + mean_base[2]) / 3.0f * 100.0f;

    std::cout << "Baseline error (gamma only): " << error_base << "%" << std::endl;
    std::cout << "Improvement: " << (error_base - error) << " percentage points" << std::endl;

    // Save comparison
    cv::Mat result_8u;
    result.convertTo(result_8u, CV_8UC3, 255.0);

    cv::Mat comparison;
    cv::hconcat(camera_jpeg, result_8u, comparison);
    cv::imwrite("tmp/var/tune/poly_transform.png", comparison);
    std::cout << "\nSaved: tmp/var/tune/poly_transform.png (left: camera, right: polynomial)" << std::endl;

    return 0;
}

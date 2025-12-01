// poly_color.cpp
// Polynomial Color Transform Module
//
// Applies a quadratic polynomial transform to map input RGB to output RGB.
// This captures cross-channel color transforms that 1D curves cannot:
// - Color matrix (linear terms)
// - Tone curve nonlinearity (quadratic terms)
// - Cross-channel interactions (product terms)
//
// Model per output channel:
//   Out = c0 + c1*R + c2*G + c3*B + c4*R² + c5*G² + c6*B² + c7*RG + c8*RB + c9*GB
//
// 10 coefficients per channel × 3 channels = 30 total parameters.
// This is the "camera phase" transform that maps scene-linear to camera JPEG.

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <random>

namespace pipe
{
namespace mods
{
    // Number of polynomial coefficients per channel
    static constexpr int POLY_COEFFS = 10;
    static constexpr int POLY_TOTAL = POLY_COEFFS * 3;  // 30 total

    // Apply polynomial transform
    // Input:  CV_32FC3 scene-linear BGR [0-1]
    // Output: CV_32FC3 scene-linear BGR [0-1]
    // coeffs: 30 floats [R_coeffs(10), G_coeffs(10), B_coeffs(10)]
    //
    // The polynomial was estimated in gamma space, so we:
    // 1. Convert linear → gamma
    // 2. Apply polynomial transform
    // 3. Convert gamma → linear
    bool poly_color(
        const cv::UMat &input,
        cv::UMat &output,
        const float* coeffs)
    {
        if (input.empty() || coeffs == nullptr)
        {
            std::cerr << "[PolyColor] Error: Invalid input\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[PolyColor] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            const float* c_r = coeffs;              // R output coefficients
            const float* c_g = coeffs + POLY_COEFFS; // G output coefficients
            const float* c_b = coeffs + 2 * POLY_COEFFS; // B output coefficients

            cv::Mat input_cpu;
            input.copyTo(input_cpu);

            cv::Mat output_cpu(input_cpu.size(), CV_32FC3);

            for (int y = 0; y < input_cpu.rows; y++)
            {
                const float* src = input_cpu.ptr<float>(y);
                float* dst = output_cpu.ptr<float>(y);

                for (int x = 0; x < input_cpu.cols; x++)
                {
                    // OpenCV BGR order - read linear values
                    float b_lin = std::max(0.0f, std::min(1.0f, src[x * 3 + 0]));
                    float g_lin = std::max(0.0f, std::min(1.0f, src[x * 3 + 1]));
                    float r_lin = std::max(0.0f, std::min(1.0f, src[x * 3 + 2]));

                    // Linear → gamma (polynomial was estimated in gamma space)
                    float b = std::pow(b_lin, 1.0f / 2.2f);
                    float g = std::pow(g_lin, 1.0f / 2.2f);
                    float r = std::pow(r_lin, 1.0f / 2.2f);

                    // Polynomial terms: 1, R, G, B, R², G², B², RG, RB, GB
                    float terms[POLY_COEFFS] = {
                        1.0f, r, g, b,
                        r * r, g * g, b * b,
                        r * g, r * b, g * b
                    };

                    // Compute output channels (in gamma space)
                    float r_out = 0, g_out = 0, b_out = 0;
                    for (int i = 0; i < POLY_COEFFS; i++)
                    {
                        r_out += c_r[i] * terms[i];
                        g_out += c_g[i] * terms[i];
                        b_out += c_b[i] * terms[i];
                    }

                    // Clamp gamma output
                    r_out = std::max(0.0f, std::min(1.0f, r_out));
                    g_out = std::max(0.0f, std::min(1.0f, g_out));
                    b_out = std::max(0.0f, std::min(1.0f, b_out));

                    // Gamma → linear
                    dst[x * 3 + 0] = std::pow(b_out, 2.2f);
                    dst[x * 3 + 1] = std::pow(g_out, 2.2f);
                    dst[x * 3 + 2] = std::pow(r_out, 2.2f);
                }
            }

            output_cpu.copyTo(output);
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "[PolyColor] Error: " << e.what() << "\n";
            return false;
        }
    }

    // Least squares solver helper
    static void solve_least_squares(
        const std::vector<std::vector<float>>& inputs,
        const std::vector<float>& outputs,
        float* coeffs)
    {
        int n = inputs.size();

        // Build normal equations: A^T A x = A^T b
        cv::Mat AtA(POLY_COEFFS, POLY_COEFFS, CV_64FC1, cv::Scalar(0));
        cv::Mat Atb(POLY_COEFFS, 1, CV_64FC1, cv::Scalar(0));

        for (int i = 0; i < n; i++)
        {
            const auto& row = inputs[i];
            double y = outputs[i];

            for (int j = 0; j < POLY_COEFFS; j++)
            {
                Atb.at<double>(j, 0) += row[j] * y;
                for (int k = 0; k < POLY_COEFFS; k++)
                {
                    AtA.at<double>(j, k) += row[j] * row[k];
                }
            }
        }

        // Solve using SVD
        cv::Mat x;
        cv::solve(AtA, Atb, x, cv::DECOMP_SVD);

        for (int i = 0; i < POLY_COEFFS; i++)
        {
            coeffs[i] = static_cast<float>(x.at<double>(i, 0));
        }
    }

    // Estimate polynomial coefficients from base→target image pair
    // base:   Scene-linear RGB (CV_32FC3)
    // target: Camera JPEG (CV_8UC3)
    // coeffs: Output array (30 floats)
    // num_samples: Number of random pixels to sample (default 50000)
    bool estimate_poly_color(
        const cv::UMat& base,
        const cv::UMat& target,
        float* coeffs,
        int num_samples)
    {
        if (base.empty() || target.empty() || coeffs == nullptr)
        {
            std::cerr << "[EstimatePolyColor] Error: Invalid input\n";
            return false;
        }

        try
        {
            // Resize base to target size if needed
            cv::Mat base_cpu;
            if (base.size() != target.size())
            {
                cv::UMat base_resized;
                cv::resize(base, base_resized, target.size(), 0, 0, cv::INTER_AREA);
                base_resized.copyTo(base_cpu);
            }
            else
            {
                base.copyTo(base_cpu);
            }

            // Convert base to gamma-encoded
            cv::Mat base_gamma;
            cv::max(base_cpu, 0.0f, base_gamma);
            cv::min(base_gamma, 1.0f, base_gamma);
            cv::pow(base_gamma, 1.0f / 2.2f, base_gamma);

            // Target to float
            cv::Mat target_cpu;
            target.copyTo(target_cpu);
            cv::Mat target_f;
            target_cpu.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

            // Sample pixels
            std::vector<std::vector<float>> samples_r, samples_g, samples_b;
            std::vector<float> targets_r, targets_g, targets_b;

            std::mt19937 rng(42);
            std::uniform_int_distribution<int> dist_y(0, base_gamma.rows - 1);
            std::uniform_int_distribution<int> dist_x(0, base_gamma.cols - 1);

            for (int i = 0; i < num_samples; i++)
            {
                int y = dist_y(rng);
                int x = dist_x(rng);

                const float* src = base_gamma.ptr<float>(y) + x * 3;
                const float* tgt = target_f.ptr<float>(y) + x * 3;

                float b = src[0], g = src[1], r = src[2];

                // Feature vector: 1, R, G, B, R², G², B², RG, RB, GB
                std::vector<float> features = {
                    1.0f, r, g, b,
                    r * r, g * g, b * b,
                    r * g, r * b, g * b
                };

                samples_r.push_back(features);
                samples_g.push_back(features);
                samples_b.push_back(features);

                targets_r.push_back(tgt[2]);  // R
                targets_g.push_back(tgt[1]);  // G
                targets_b.push_back(tgt[0]);  // B
            }

            // Solve for each output channel
            solve_least_squares(samples_r, targets_r, coeffs);
            solve_least_squares(samples_g, targets_g, coeffs + POLY_COEFFS);
            solve_least_squares(samples_b, targets_b, coeffs + 2 * POLY_COEFFS);

            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[EstimatePolyColor] Error: " << e.what() << "\n";
            return false;
        }
    }

    // Initialize identity polynomial (no transform)
    void identity_poly_color(float* coeffs)
    {
        // R_out = R, G_out = G, B_out = B
        // coeffs[1] = 1 for R, coeffs[12] = 1 for G, coeffs[23] = 1 for B
        for (int i = 0; i < POLY_TOTAL; i++)
        {
            coeffs[i] = 0.0f;
        }
        coeffs[1] = 1.0f;   // R from R
        coeffs[POLY_COEFFS + 2] = 1.0f;   // G from G
        coeffs[2 * POLY_COEFFS + 3] = 1.0f;   // B from B
    }

} // namespace mods
} // namespace pipe

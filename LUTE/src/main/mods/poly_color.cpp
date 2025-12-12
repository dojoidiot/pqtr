// poly_color.cpp - VIBE
// Polynomial Color Transform Module

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <random>

namespace vibe
{
namespace mods
{

bool poly_color(const View& in, View& out, Grid coeffs)
{
    if (in.empty() || coeffs == nullptr || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::poly_color] invalid input\n";
        return false;
    }

    const float* cr = coeffs;
    const float* cg = coeffs + POLY_COEFFS;
    const float* cb = coeffs + 2 * POLY_COEFFS;

    cv::Mat cpu;
    in.copyTo(cpu);

    cv::Mat result(cpu.size(), CV_32FC3);

    for (int y = 0; y < cpu.rows; y++)
    {
        const float* src = cpu.ptr<float>(y);
        float* dst = result.ptr<float>(y);

        for (int x = 0; x < cpu.cols; x++)
        {
            float bl = std::clamp(src[x * 3], 0.0f, 1.0f);
            float gl = std::clamp(src[x * 3 + 1], 0.0f, 1.0f);
            float rl = std::clamp(src[x * 3 + 2], 0.0f, 1.0f);

            float b = std::pow(bl, 1.0f / 2.2f);
            float g = std::pow(gl, 1.0f / 2.2f);
            float r = std::pow(rl, 1.0f / 2.2f);

            float terms[POLY_COEFFS] = {
                1.0f, r, g, b,
                r * r, g * g, b * b,
                r * g, r * b, g * b
            };

            float ro = 0, go = 0, bo = 0;
            for (int i = 0; i < POLY_COEFFS; i++)
            {
                ro += cr[i] * terms[i];
                go += cg[i] * terms[i];
                bo += cb[i] * terms[i];
            }

            dst[x * 3] = std::pow(std::clamp(bo, 0.0f, 1.0f), 2.2f);
            dst[x * 3 + 1] = std::pow(std::clamp(go, 0.0f, 1.0f), 2.2f);
            dst[x * 3 + 2] = std::pow(std::clamp(ro, 0.0f, 1.0f), 2.2f);
        }
    }

    result.copyTo(out);
    return true;
}

static void solve_ls(const std::vector<std::vector<float>>& A, const std::vector<float>& b, float* x)
{
    int n = A.size();
    cv::Mat AtA(POLY_COEFFS, POLY_COEFFS, CV_64FC1, cv::Scalar(0));
    cv::Mat Atb(POLY_COEFFS, 1, CV_64FC1, cv::Scalar(0));

    for (int i = 0; i < n; i++)
    {
        const auto& row = A[i];
        double yi = b[i];
        for (int j = 0; j < POLY_COEFFS; j++)
        {
            Atb.at<double>(j, 0) += row[j] * yi;
            for (int k = 0; k < POLY_COEFFS; k++)
                AtA.at<double>(j, k) += row[j] * row[k];
        }
    }

    cv::Mat sol;
    cv::solve(AtA, Atb, sol, cv::DECOMP_SVD);
    for (int i = 0; i < POLY_COEFFS; i++)
        x[i] = float(sol.at<double>(i, 0));
}

bool estimate_poly_color(const View& base, const View& target, float* coeffs, int num_samples)
{
    if (base.empty() || target.empty() || coeffs == nullptr)
    {
        std::cerr << "[vibe::estimate_poly_color] invalid input\n";
        return false;
    }

    cv::Mat base_cpu;
    if (base.size() != target.size())
    {
        View resized;
        cv::resize(base, resized, target.size(), 0, 0, cv::INTER_AREA);
        resized.copyTo(base_cpu);
    }
    else
    {
        base.copyTo(base_cpu);
    }

    cv::Mat base_gamma;
    cv::max(base_cpu, 0.0f, base_gamma);
    cv::min(base_gamma, 1.0f, base_gamma);
    cv::pow(base_gamma, 1.0f / 2.2f, base_gamma);

    cv::Mat target_cpu;
    target.copyTo(target_cpu);
    cv::Mat target_f;
    target_cpu.convertTo(target_f, CV_32FC3, 1.0f / 255.0f);

    std::vector<std::vector<float>> samples;
    std::vector<float> tgt_r, tgt_g, tgt_b;

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dy(0, base_gamma.rows - 1);
    std::uniform_int_distribution<int> dx(0, base_gamma.cols - 1);

    for (int i = 0; i < num_samples; i++)
    {
        int y = dy(rng), x = dx(rng);
        const float* src = base_gamma.ptr<float>(y) + x * 3;
        const float* tgt = target_f.ptr<float>(y) + x * 3;

        float b = src[0], g = src[1], r = src[2];
        samples.push_back({1.0f, r, g, b, r*r, g*g, b*b, r*g, r*b, g*b});
        tgt_r.push_back(tgt[2]);
        tgt_g.push_back(tgt[1]);
        tgt_b.push_back(tgt[0]);
    }

    solve_ls(samples, tgt_r, coeffs);
    solve_ls(samples, tgt_g, coeffs + POLY_COEFFS);
    solve_ls(samples, tgt_b, coeffs + 2 * POLY_COEFFS);

    return true;
}

void identity_poly_color(float* coeffs)
{
    for (int i = 0; i < POLY_TOTAL; i++)
        coeffs[i] = 0.0f;
    coeffs[1] = 1.0f;  // R from R
    coeffs[POLY_COEFFS + 2] = 1.0f;  // G from G
    coeffs[2 * POLY_COEFFS + 3] = 1.0f;  // B from B
}

} // namespace mods
} // namespace vibe

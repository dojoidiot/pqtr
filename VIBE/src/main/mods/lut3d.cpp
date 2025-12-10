// lut3d.cpp - VIBE
// 3D LUT Module - Maps RGB tuples to RGB tuples

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace vibe
{
namespace mods
{

static inline int idx3d(int r, int g, int b, int ch, int gs)
{
    return ((r * gs + g) * gs + b) * 3 + ch;
}

static void trilinear(float ri, float gi, float bi, Grid lut, int gs, float& ro, float& go, float& bo)
{
    float scale = float(gs - 1);
    float rp = ri * scale, gp = gi * scale, bp = bi * scale;

    int r0 = std::clamp(int(rp), 0, gs - 2);
    int g0 = std::clamp(int(gp), 0, gs - 2);
    int b0 = std::clamp(int(bp), 0, gs - 2);
    int r1 = r0 + 1, g1 = g0 + 1, b1 = b0 + 1;

    float rf = rp - r0, gf = gp - g0, bf = bp - b0;

    for (int ch = 0; ch < 3; ch++)
    {
        float c000 = lut[idx3d(r0, g0, b0, ch, gs)];
        float c001 = lut[idx3d(r0, g0, b1, ch, gs)];
        float c010 = lut[idx3d(r0, g1, b0, ch, gs)];
        float c011 = lut[idx3d(r0, g1, b1, ch, gs)];
        float c100 = lut[idx3d(r1, g0, b0, ch, gs)];
        float c101 = lut[idx3d(r1, g0, b1, ch, gs)];
        float c110 = lut[idx3d(r1, g1, b0, ch, gs)];
        float c111 = lut[idx3d(r1, g1, b1, ch, gs)];

        float c00 = c000 * (1 - bf) + c001 * bf;
        float c01 = c010 * (1 - bf) + c011 * bf;
        float c10 = c100 * (1 - bf) + c101 * bf;
        float c11 = c110 * (1 - bf) + c111 * bf;

        float c0 = c00 * (1 - gf) + c01 * gf;
        float c1 = c10 * (1 - gf) + c11 * gf;

        float val = c0 * (1 - rf) + c1 * rf;
        if (ch == 0) ro = val;
        else if (ch == 1) go = val;
        else bo = val;
    }
}

bool lut3d_apply(const View& in, View& out, Grid lut, int grid_size)
{
    if (in.empty() || lut == nullptr || grid_size < 2 || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::lut3d_apply] invalid input\n";
        return false;
    }

    cv::Mat cpu;
    in.copyTo(cpu);

    cv::Mat result(cpu.size(), CV_32FC3);
    const float gamma = 2.2f, inv_gamma = 1.0f / gamma;

    for (int y = 0; y < cpu.rows; y++)
    {
        const float* ip = cpu.ptr<float>(y);
        float* op = result.ptr<float>(y);

        for (int x = 0; x < cpu.cols; x++)
        {
            int i = x * 3;
            float bl = std::clamp(ip[i], 0.0f, 1.0f);
            float gl = std::clamp(ip[i+1], 0.0f, 1.0f);
            float rl = std::clamp(ip[i+2], 0.0f, 1.0f);

            float rg = std::pow(rl, inv_gamma);
            float gg = std::pow(gl, inv_gamma);
            float bg = std::pow(bl, inv_gamma);

            float ro = 0, go = 0, bo = 0;
            trilinear(rg, gg, bg, lut, grid_size, ro, go, bo);

            op[i] = std::pow(bo, gamma);
            op[i+1] = std::pow(go, gamma);
            op[i+2] = std::pow(ro, gamma);
        }
    }

    result.copyTo(out);
    return true;
}

bool lut3d_estimate(const View& base, const View& target, float* lut, int grid_size)
{
    if (base.empty() || target.empty() || lut == nullptr || grid_size < 2)
    {
        std::cerr << "[vibe::lut3d_estimate] invalid input\n";
        return false;
    }

    int lut_total = grid_size * grid_size * grid_size * 3;

    View target_r;
    if (base.size() != target.size())
        cv::resize(target, target_r, base.size());
    else
        target.copyTo(target_r);

    auto to8bit = [](const View& src, cv::Mat& dst) {
        cv::Mat clamped, gamma, temp;
        src.copyTo(temp);
        cv::max(temp, 0.0f, clamped);
        cv::min(clamped, 1.0f, clamped);
        cv::pow(clamped, 1.0f/2.2f, gamma);
        gamma.convertTo(dst, CV_8UC3, 255.0);
    };

    cv::Mat base_cpu, target_cpu;
    to8bit(base, base_cpu);
    to8bit(target_r, target_cpu);

    std::vector<double> sum(lut_total, 0.0);
    std::vector<int> count(grid_size * grid_size * grid_size, 0);
    float bin_size = 256.0f / grid_size;

    for (int y = 0; y < base_cpu.rows; y++)
    {
        const uchar* bp = base_cpu.ptr<uchar>(y);
        const uchar* tp = target_cpu.ptr<uchar>(y);

        for (int x = 0; x < base_cpu.cols; x++)
        {
            int i = x * 3;
            int ri = std::min(grid_size - 1, int(bp[i+2] / bin_size));
            int gi = std::min(grid_size - 1, int(bp[i+1] / bin_size));
            int bi = std::min(grid_size - 1, int(bp[i] / bin_size));

            int cell = (ri * grid_size + gi) * grid_size + bi;

            sum[cell * 3] += tp[i+2] / 255.0;
            sum[cell * 3 + 1] += tp[i+1] / 255.0;
            sum[cell * 3 + 2] += tp[i] / 255.0;
            count[cell]++;
        }
    }

    for (int ri = 0; ri < grid_size; ri++)
    {
        for (int gi = 0; gi < grid_size; gi++)
        {
            for (int bi = 0; bi < grid_size; bi++)
            {
                int cell = (ri * grid_size + gi) * grid_size + bi;
                int base_i = cell * 3;

                if (count[cell] > 0)
                {
                    lut[base_i] = float(sum[base_i] / count[cell]);
                    lut[base_i + 1] = float(sum[base_i + 1] / count[cell]);
                    lut[base_i + 2] = float(sum[base_i + 2] / count[cell]);
                }
                else
                {
                    lut[base_i] = float(ri) / (grid_size - 1);
                    lut[base_i + 1] = float(gi) / (grid_size - 1);
                    lut[base_i + 2] = float(bi) / (grid_size - 1);
                }
            }
        }
    }

    return true;
}

} // namespace mods
} // namespace vibe

// lut_curve.cpp - VIBE
// LUT-based Per-Channel Curve Module

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace vibe
{
namespace mods
{

bool lut_curve(const View& in, View& out, Grid lut, int lut_size)
{
    if (in.empty() || lut == nullptr || lut_size < 2 || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::lut_curve] invalid input\n";
        return false;
    }

    // Build full 256-entry LUTs
    std::vector<float> full_r(256), full_g(256), full_b(256);
    float step = 1.0f / (lut_size - 1);

    const float* lut_r = lut;
    const float* lut_g = lut + lut_size;
    const float* lut_b = lut + 2 * lut_size;

    for (int i = 0; i < 256; i++)
    {
        float val = i / 255.0f;
        float pos = val / step;
        int idx0 = std::min(static_cast<int>(pos), lut_size - 1);
        int idx1 = std::min(idx0 + 1, lut_size - 1);
        float frac = pos - idx0;

        full_r[i] = lut_r[idx0] + frac * (lut_r[idx1] - lut_r[idx0]);
        full_g[i] = lut_g[idx0] + frac * (lut_g[idx1] - lut_g[idx0]);
        full_b[i] = lut_b[idx0] + frac * (lut_b[idx1] - lut_b[idx0]);
    }

    View clamped, gamma;
    cv::max(in, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);
    cv::pow(clamped, 1.0f/2.2f, gamma);

    View in8u;
    gamma.convertTo(in8u, CV_8UC3, 255.0);

    cv::Mat cpu;
    in8u.copyTo(cpu);

    for (int y = 0; y < cpu.rows; y++)
    {
        uchar* ptr = cpu.ptr<uchar>(y);
        for (int x = 0; x < cpu.cols; x++)
        {
            int idx = x * 3;
            ptr[idx + 0] = static_cast<uchar>(full_b[ptr[idx + 0]] * 255.0f + 0.5f);
            ptr[idx + 1] = static_cast<uchar>(full_g[ptr[idx + 1]] * 255.0f + 0.5f);
            ptr[idx + 2] = static_cast<uchar>(full_r[ptr[idx + 2]] * 255.0f + 0.5f);
        }
    }

    View result8u, result_f;
    cpu.copyTo(result8u);
    result8u.convertTo(result_f, CV_32FC3, 1.0/255.0);
    cv::pow(result_f, 2.2f, out);

    return true;
}

bool estimate_lut(const View& base, const View& target, float* lut, int lut_size)
{
    if (base.empty() || target.empty() || lut == nullptr || lut_size < 2)
    {
        std::cerr << "[vibe::estimate_lut] invalid input\n";
        return false;
    }

    View target_r;
    if (base.size() != target.size())
        cv::resize(target, target_r, base.size());
    else
        target.copyTo(target_r);

    // Convert to 8-bit
    auto to8bit = [](const View& src, View& dst) {
        View clamped, gamma;
        cv::max(src, 0.0f, clamped);
        cv::min(clamped, 1.0f, clamped);
        cv::pow(clamped, 1.0f/2.2f, gamma);
        gamma.convertTo(dst, CV_8UC3, 255.0);
    };

    View base8, target8;
    to8bit(base, base8);
    to8bit(target_r, target8);

    cv::Mat base_cpu, target_cpu;
    base8.copyTo(base_cpu);
    target8.copyTo(target_cpu);

    float bin_size = 256.0f / lut_size;
    std::vector<double> sum_r(lut_size), sum_g(lut_size), sum_b(lut_size);
    std::vector<double> wgt_r(lut_size), wgt_g(lut_size), wgt_b(lut_size);

    for (int y = 0; y < base_cpu.rows; y++)
    {
        const uchar* bp = base_cpu.ptr<uchar>(y);
        const uchar* tp = target_cpu.ptr<uchar>(y);
        for (int x = 0; x < base_cpu.cols; x++)
        {
            int i = x * 3;
            uchar bb = bp[i], bg = bp[i+1], br = bp[i+2];

            int maxc = std::max({br, bg, bb});
            int minc = std::min({br, bg, bb});
            float sat = (maxc > 10) ? float(maxc - minc) / maxc : 0.0f;
            float w = 1.0f + 2.0f * sat;

            int binb = std::min(lut_size - 1, int(bb / bin_size));
            int bing = std::min(lut_size - 1, int(bg / bin_size));
            int binr = std::min(lut_size - 1, int(br / bin_size));

            sum_b[binb] += w * tp[i];
            sum_g[bing] += w * tp[i+1];
            sum_r[binr] += w * tp[i+2];
            wgt_b[binb] += w;
            wgt_g[bing] += w;
            wgt_r[binr] += w;
        }
    }

    float* lr = lut;
    float* lg = lut + lut_size;
    float* lb = lut + 2 * lut_size;

    for (int i = 0; i < lut_size; i++)
    {
        float def = (i + 0.5f) * bin_size / 255.0f;
        lr[i] = (wgt_r[i] > 0.1) ? float(sum_r[i] / wgt_r[i]) / 255.0f : def;
        lg[i] = (wgt_g[i] > 0.1) ? float(sum_g[i] / wgt_g[i]) / 255.0f : def;
        lb[i] = (wgt_b[i] > 0.1) ? float(sum_b[i] / wgt_b[i]) / 255.0f : def;
    }

    // Smooth
    auto smooth = [lut_size](float* ch, const std::vector<double>& w) {
        std::vector<float> s(lut_size);
        s[0] = ch[0];
        s[lut_size-1] = ch[lut_size-1];
        for (int i = 1; i < lut_size - 1; i++)
            s[i] = (w[i] < 100) ? 0.5f * (ch[i-1] + ch[i+1])
                                : 0.25f * ch[i-1] + 0.5f * ch[i] + 0.25f * ch[i+1];
        for (int i = 0; i < lut_size; i++) ch[i] = s[i];
    };

    smooth(lr, wgt_r);
    smooth(lg, wgt_g);
    smooth(lb, wgt_b);

    return true;
}

} // namespace mods
} // namespace vibe

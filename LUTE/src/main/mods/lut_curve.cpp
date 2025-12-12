// lut_curve.cpp - LUTE
// LUT-based Per-Channel Curve Module

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace lute
{
namespace mods
{

// Manual gamma to avoid OpenCL cv::pow crash
static void apply_gamma(cv::Mat& img, float gamma)
{
    for (int y = 0; y < img.rows; y++)
    {
        float* row = img.ptr<float>(y);
        for (int x = 0; x < img.cols * 3; x++)
            row[x] = std::pow(std::clamp(row[x], 0.0f, 1.0f), gamma);
    }
}

bool lut_curve(const View& in, View& out, Grid lut, int lut_size)
{
    if (in.empty() || lut == nullptr || lut_size < 2 || in.type() != CV_32FC3)
    {
        std::cerr << "[lute::lut_curve] invalid input\n";
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

    cv::Mat cpu;
    in.copyTo(cpu);
    apply_gamma(cpu, 1.0f / 2.2f);

    cv::Mat cpu8u;
    cpu.convertTo(cpu8u, CV_8UC3, 255.0);

    for (int y = 0; y < cpu8u.rows; y++)
    {
        uchar* ptr = cpu8u.ptr<uchar>(y);
        for (int x = 0; x < cpu8u.cols; x++)
        {
            int idx = x * 3;
            ptr[idx + 0] = static_cast<uchar>(full_b[ptr[idx + 0]] * 255.0f + 0.5f);
            ptr[idx + 1] = static_cast<uchar>(full_g[ptr[idx + 1]] * 255.0f + 0.5f);
            ptr[idx + 2] = static_cast<uchar>(full_r[ptr[idx + 2]] * 255.0f + 0.5f);
        }
    }

    cv::Mat result_f;
    cpu8u.convertTo(result_f, CV_32FC3, 1.0 / 255.0);
    apply_gamma(result_f, 2.2f);

    result_f.copyTo(out);
    return true;
}

bool estimate_lut(const View& base, const View& target, float* lut, int lut_size)
{
    if (base.empty() || target.empty() || lut == nullptr || lut_size < 2)
    {
        std::cerr << "[lute::estimate_lut] invalid input\n";
        return false;
    }

    cv::Mat base_cpu, target_cpu;
    base.copyTo(base_cpu);

    if (base.size() != target.size())
    {
        cv::Mat temp;
        target.copyTo(temp);
        cv::resize(temp, target_cpu, base.size());
    }
    else
    {
        target.copyTo(target_cpu);
    }

    // Convert base to 8-bit
    apply_gamma(base_cpu, 1.0f / 2.2f);
    cv::Mat base8u;
    base_cpu.convertTo(base8u, CV_8UC3, 255.0);

    // Convert target to 8-bit (may already be uint8)
    cv::Mat target8u;
    if (target_cpu.type() == CV_8UC3)
    {
        target8u = target_cpu;
    }
    else
    {
        apply_gamma(target_cpu, 1.0f / 2.2f);
        target_cpu.convertTo(target8u, CV_8UC3, 255.0);
    }

    float bin_size = 256.0f / lut_size;
    std::vector<double> sum_r(lut_size), sum_g(lut_size), sum_b(lut_size);
    std::vector<double> wgt_r(lut_size), wgt_g(lut_size), wgt_b(lut_size);

    for (int y = 0; y < base8u.rows; y++)
    {
        const uchar* bp = base8u.ptr<uchar>(y);
        const uchar* tp = target8u.ptr<uchar>(y);
        for (int x = 0; x < base8u.cols; x++)
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
} // namespace lute

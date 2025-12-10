// hsv_lut.cpp - VIBE
// HSV-space LUT for per-hue/saturation color corrections

#include "mods.h"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

namespace vibe
{
namespace mods
{

bool hsv_lut_apply(const View& in, View& out, Grid lut)
{
    if (in.empty() || lut == nullptr || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::hsv_lut_apply] invalid input\n";
        return false;
    }

    cv::Mat cpu;
    in.copyTo(cpu);

    cv::Mat gamma_rgb, gamma_8u;
    cv::pow(cpu, 1.0f / 2.2f, gamma_rgb);
    cv::max(gamma_rgb, 0.0f, gamma_rgb);
    cv::min(gamma_rgb, 1.0f, gamma_rgb);
    gamma_rgb.convertTo(gamma_8u, CV_8UC3, 255.0);

    cv::Mat hsv_8u, hsv;
    cv::cvtColor(gamma_8u, hsv_8u, cv::COLOR_BGR2HSV);
    hsv_8u.convertTo(hsv, CV_32FC3);

    for (int y = 0; y < hsv.rows; y++)
    {
        float* ptr = hsv.ptr<float>(y);
        for (int x = 0; x < hsv.cols; x++)
        {
            int i = x * 3;
            float h = ptr[i];
            float s = ptr[i + 1] / 255.0f;
            float v = ptr[i + 2] / 255.0f;

            float h_norm = (h / 180.0f) * 360.0f;
            float h_pos = (h_norm / 360.0f) * HSV_H_BINS;
            float s_pos = s * (HSV_S_BINS - 1);

            int h0 = int(h_pos) % HSV_H_BINS;
            int h1 = (h0 + 1) % HSV_H_BINS;
            int s0 = std::clamp(int(s_pos), 0, HSV_S_BINS - 1);
            int s1 = std::min(s0 + 1, HSV_S_BINS - 1);

            float hf = h_pos - int(h_pos);
            float sf = s_pos - s0;

            auto lookup = [lut](int hi, int si) {
                return &lut[(hi * HSV_S_BINS + si) * 3];
            };

            const float* c00 = lookup(h0, s0);
            const float* c01 = lookup(h0, s1);
            const float* c10 = lookup(h1, s0);
            const float* c11 = lookup(h1, s1);

            float dh = (1-hf)*(1-sf)*c00[0] + (1-hf)*sf*c01[0] + hf*(1-sf)*c10[0] + hf*sf*c11[0];
            float ds = (1-hf)*(1-sf)*c00[1] + (1-hf)*sf*c01[1] + hf*(1-sf)*c10[1] + hf*sf*c11[1];
            float dv = (1-hf)*(1-sf)*c00[2] + (1-hf)*sf*c01[2] + hf*(1-sf)*c10[2] + hf*sf*c11[2];

            float new_h = h + dh * 0.5f;
            if (new_h < 0) new_h += 180.0f;
            if (new_h >= 180.0f) new_h -= 180.0f;

            ptr[i] = new_h;
            ptr[i + 1] = std::clamp(s + ds, 0.0f, 1.0f) * 255.0f;
            ptr[i + 2] = std::clamp(v + dv, 0.0f, 1.0f) * 255.0f;
        }
    }

    cv::Mat hsv_out_8u, result_gamma_8u, result_gamma, result_linear;
    hsv.convertTo(hsv_out_8u, CV_8UC3);
    cv::cvtColor(hsv_out_8u, result_gamma_8u, cv::COLOR_HSV2BGR);
    result_gamma_8u.convertTo(result_gamma, CV_32FC3, 1.0 / 255.0);
    cv::pow(result_gamma, 2.2f, result_linear);

    result_linear.copyTo(out);
    return true;
}

bool hsv_lut_estimate(const View& base, const View& target, float* lut)
{
    if (base.empty() || target.empty() || lut == nullptr)
    {
        std::cerr << "[vibe::hsv_lut_estimate] invalid input\n";
        return false;
    }

    View target_r;
    if (base.size() != target.size())
        cv::resize(target, target_r, base.size());
    else
        target.copyTo(target_r);

    auto to8bit = [](const View& src, cv::Mat& dst) {
        cv::Mat temp, clamped, gamma;
        src.copyTo(temp);
        cv::max(temp, 0.0f, clamped);
        cv::min(clamped, 1.0f, clamped);
        cv::pow(clamped, 1.0f / 2.2f, gamma);
        gamma.convertTo(dst, CV_8UC3, 255.0);
    };

    cv::Mat base_cpu, target_cpu;
    to8bit(base, base_cpu);
    to8bit(target_r, target_cpu);

    cv::Mat base_hsv, target_hsv;
    cv::cvtColor(base_cpu, base_hsv, cv::COLOR_BGR2HSV);
    cv::cvtColor(target_cpu, target_hsv, cv::COLOR_BGR2HSV);

    std::vector<double> sum_dh(HSV_H_BINS * HSV_S_BINS, 0.0);
    std::vector<double> sum_ds(HSV_H_BINS * HSV_S_BINS, 0.0);
    std::vector<double> sum_dv(HSV_H_BINS * HSV_S_BINS, 0.0);
    std::vector<double> counts(HSV_H_BINS * HSV_S_BINS, 0.0);

    for (int y = 0; y < base_hsv.rows; y++)
    {
        const uchar* bp = base_hsv.ptr<uchar>(y);
        const uchar* tp = target_hsv.ptr<uchar>(y);

        for (int x = 0; x < base_hsv.cols; x++)
        {
            int i = x * 3;
            float bh = bp[i], bs = bp[i+1] / 255.0f, bv = bp[i+2] / 255.0f;
            float th = tp[i], ts = tp[i+1] / 255.0f, tv = tp[i+2] / 255.0f;

            if (bv < 0.05f || bs < 0.05f) continue;

            int h_bin = int((bh / 180.0f) * HSV_H_BINS) % HSV_H_BINS;
            int s_bin = std::clamp(int(bs * HSV_S_BINS), 0, HSV_S_BINS - 1);
            int bin = h_bin * HSV_S_BINS + s_bin;

            float dh = th - bh;
            if (dh > 90.0f) dh -= 180.0f;
            if (dh < -90.0f) dh += 180.0f;
            dh *= 2.0f;

            float w = bs;
            sum_dh[bin] += w * dh;
            sum_ds[bin] += w * (ts - bs);
            sum_dv[bin] += w * (tv - bv);
            counts[bin] += w;
        }
    }

    for (int i = 0; i < HSV_H_BINS * HSV_S_BINS; i++)
    {
        if (counts[i] > 1.0)
        {
            lut[i * 3] = float(sum_dh[i] / counts[i]);
            lut[i * 3 + 1] = float(sum_ds[i] / counts[i]);
            lut[i * 3 + 2] = float(sum_dv[i] / counts[i]);
        }
        else
        {
            lut[i * 3] = lut[i * 3 + 1] = lut[i * 3 + 2] = 0.0f;
        }
    }

    // Smooth
    std::vector<float> smoothed(HSV_H_BINS * HSV_S_BINS * 3);
    for (int h = 0; h < HSV_H_BINS; h++)
    {
        for (int s = 0; s < HSV_S_BINS; s++)
        {
            int center = h * HSV_S_BINS + s;
            int hp = ((h - 1 + HSV_H_BINS) % HSV_H_BINS) * HSV_S_BINS + s;
            int hn = ((h + 1) % HSV_H_BINS) * HSV_S_BINS + s;
            int sp = (s > 0) ? h * HSV_S_BINS + (s - 1) : center;
            int sn = (s < HSV_S_BINS - 1) ? h * HSV_S_BINS + (s + 1) : center;

            for (int c = 0; c < 3; c++)
            {
                smoothed[center * 3 + c] = 0.4f * lut[center * 3 + c] +
                    0.15f * (lut[hp * 3 + c] + lut[hn * 3 + c] + lut[sp * 3 + c] + lut[sn * 3 + c]);
            }
        }
    }

    for (int i = 0; i < HSV_H_BINS * HSV_S_BINS * 3; i++)
        lut[i] = smoothed[i];

    return true;
}

void hsv_lut_identity(float* lut)
{
    for (int i = 0; i < HSV_H_BINS * HSV_S_BINS * 3; i++)
        lut[i] = 0.0f;
}

} // namespace mods
} // namespace vibe

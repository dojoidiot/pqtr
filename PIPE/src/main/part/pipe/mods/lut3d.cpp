// lut3d.cpp
// 3D LUT Module - Maps RGB tuples to RGB tuples
// Captures any transform including hue-dependent corrections
//
// Grid size 9³ = 729 points × 3 channels = 2,187 parameters
// Suitable for social media compression where subtle distinctions are lost

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

namespace pipe
{
namespace mods
{
    // 3D LUT layout: lut[r][g][b][channel] stored as flat array
    // Index: ((r * grid_size + g) * grid_size + b) * 3 + channel
    inline int lut3d_index(int r, int g, int b, int ch, int grid_size)
    {
        return ((r * grid_size + g) * grid_size + b) * 3 + ch;
    }

    // Trilinear interpolation for 3D LUT lookup
    // input: normalized RGB [0,1]
    // output: interpolated RGB [0,1]
    void trilinear_lookup(
        float r_in, float g_in, float b_in,
        const float* lut, int grid_size,
        float& r_out, float& g_out, float& b_out)
    {
        // Scale to grid coordinates
        float scale = static_cast<float>(grid_size - 1);
        float r_pos = r_in * scale;
        float g_pos = g_in * scale;
        float b_pos = b_in * scale;

        // Integer indices (clamped)
        int r0 = std::max(0, std::min(grid_size - 2, static_cast<int>(r_pos)));
        int g0 = std::max(0, std::min(grid_size - 2, static_cast<int>(g_pos)));
        int b0 = std::max(0, std::min(grid_size - 2, static_cast<int>(b_pos)));
        int r1 = r0 + 1;
        int g1 = g0 + 1;
        int b1 = b0 + 1;

        // Fractional parts
        float r_frac = r_pos - r0;
        float g_frac = g_pos - g0;
        float b_frac = b_pos - b0;

        // Fetch 8 corner values for each output channel
        for (int ch = 0; ch < 3; ch++)
        {
            float c000 = lut[lut3d_index(r0, g0, b0, ch, grid_size)];
            float c001 = lut[lut3d_index(r0, g0, b1, ch, grid_size)];
            float c010 = lut[lut3d_index(r0, g1, b0, ch, grid_size)];
            float c011 = lut[lut3d_index(r0, g1, b1, ch, grid_size)];
            float c100 = lut[lut3d_index(r1, g0, b0, ch, grid_size)];
            float c101 = lut[lut3d_index(r1, g0, b1, ch, grid_size)];
            float c110 = lut[lut3d_index(r1, g1, b0, ch, grid_size)];
            float c111 = lut[lut3d_index(r1, g1, b1, ch, grid_size)];

            // Trilinear interpolation
            float c00 = c000 * (1 - b_frac) + c001 * b_frac;
            float c01 = c010 * (1 - b_frac) + c011 * b_frac;
            float c10 = c100 * (1 - b_frac) + c101 * b_frac;
            float c11 = c110 * (1 - b_frac) + c111 * b_frac;

            float c0 = c00 * (1 - g_frac) + c01 * g_frac;
            float c1 = c10 * (1 - g_frac) + c11 * g_frac;

            float val = c0 * (1 - r_frac) + c1 * r_frac;

            if (ch == 0) r_out = val;
            else if (ch == 1) g_out = val;
            else b_out = val;
        }
    }

    // Apply 3D LUT to image
    // Input:  CV_32FC3 linear RGB [0,1]
    // Output: CV_32FC3 linear RGB [0,1]
    // Note: LUT is estimated in gamma space, so we convert linear→gamma, apply LUT, gamma→linear
    bool lut3d_apply(
        const cv::UMat& input,
        cv::UMat& output,
        const float* lut,
        int grid_size)
    {
        if (input.empty() || lut == nullptr || grid_size < 2)
        {
            std::cerr << "[Lut3D] Error: Invalid input\n";
            return false;
        }

        if (input.type() != CV_32FC3)
        {
            std::cerr << "[Lut3D] Error: Input must be CV_32FC3\n";
            return false;
        }

        try
        {
            // Work on CPU for now (GPU optimization later)
            cv::Mat img_cpu;
            input.copyTo(img_cpu);

            cv::Mat result(img_cpu.size(), CV_32FC3);

            const float gamma = 2.2f;
            const float inv_gamma = 1.0f / gamma;

            for (int y = 0; y < img_cpu.rows; y++)
            {
                const float* in_ptr = img_cpu.ptr<float>(y);
                float* out_ptr = result.ptr<float>(y);

                for (int x = 0; x < img_cpu.cols; x++)
                {
                    int idx = x * 3;
                    // OpenCV BGR order - clamp and convert linear to gamma
                    float b_lin = std::max(0.0f, std::min(1.0f, in_ptr[idx + 0]));
                    float g_lin = std::max(0.0f, std::min(1.0f, in_ptr[idx + 1]));
                    float r_lin = std::max(0.0f, std::min(1.0f, in_ptr[idx + 2]));

                    // Linear → Gamma (sRGB) for LUT lookup
                    float r_gamma = std::pow(r_lin, inv_gamma);
                    float g_gamma = std::pow(g_lin, inv_gamma);
                    float b_gamma = std::pow(b_lin, inv_gamma);

                    float r_out, g_out, b_out;
                    trilinear_lookup(r_gamma, g_gamma, b_gamma, lut, grid_size, r_out, g_out, b_out);

                    // Gamma → Linear (back to scene-linear)
                    out_ptr[idx + 0] = std::pow(b_out, gamma);
                    out_ptr[idx + 1] = std::pow(g_out, gamma);
                    out_ptr[idx + 2] = std::pow(r_out, gamma);
                }
            }

            result.copyTo(output);
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Lut3D] Error: " << e.what() << "\n";
            return false;
        }
    }

    // Estimate 3D LUT from base image to target image
    // One-shot deterministic: bin all pixels, average per cell
    //
    // base:      Our processed RAW (after linear GEOS optimization)
    // target:    Camera JPEG (what we want to match)
    // lut:       Output array (must be preallocated: grid_size³ × 3 floats)
    // grid_size: LUT dimension (9 for social media)
    bool lut3d_estimate(
        const cv::UMat& base,
        const cv::UMat& target,
        float* lut,
        int grid_size)
    {
        if (base.empty() || target.empty() || lut == nullptr || grid_size < 2)
        {
            std::cerr << "[Lut3D] Error: Invalid input\n";
            return false;
        }

        int lut_total = grid_size * grid_size * grid_size * 3;

        try
        {
            // Resize target to match base if needed
            cv::UMat target_resized;
            if (base.size() != target.size())
            {
                cv::resize(target, target_resized, base.size());
            }
            else
            {
                target.copyTo(target_resized);
            }

            // Convert both to 8-bit BGR for binning
            cv::UMat base_8u, target_8u;

            if (base.type() == CV_32FC3)
            {
                cv::UMat clamped;
                cv::max(base, 0.0f, clamped);
                cv::min(clamped, 1.0f, clamped);
                cv::UMat gamma;
                cv::pow(clamped, 1.0f/2.2f, gamma);
                gamma.convertTo(base_8u, CV_8UC3, 255.0);
            }
            else
            {
                base.convertTo(base_8u, CV_8UC3);
            }

            if (target_resized.type() == CV_32FC3)
            {
                cv::UMat clamped;
                cv::max(target_resized, 0.0f, clamped);
                cv::min(clamped, 1.0f, clamped);
                cv::UMat gamma;
                cv::pow(clamped, 1.0f/2.2f, gamma);
                gamma.convertTo(target_8u, CV_8UC3, 255.0);
            }
            else
            {
                target_resized.convertTo(target_8u, CV_8UC3);
            }

            cv::Mat base_cpu, target_cpu;
            base_8u.copyTo(base_cpu);
            target_8u.copyTo(target_cpu);

            // Accumulators: sum of target values and count per cell
            std::vector<double> sum(lut_total, 0.0);
            std::vector<int> count(grid_size * grid_size * grid_size, 0);

            float bin_size = 256.0f / grid_size;

            for (int y = 0; y < base_cpu.rows; y++)
            {
                const uchar* b_ptr = base_cpu.ptr<uchar>(y);
                const uchar* t_ptr = target_cpu.ptr<uchar>(y);

                for (int x = 0; x < base_cpu.cols; x++)
                {
                    int idx = x * 3;
                    // BGR order
                    int base_b = b_ptr[idx + 0];
                    int base_g = b_ptr[idx + 1];
                    int base_r = b_ptr[idx + 2];

                    int tgt_b = t_ptr[idx + 0];
                    int tgt_g = t_ptr[idx + 1];
                    int tgt_r = t_ptr[idx + 2];

                    // Quantize base to grid cell
                    int ri = std::min(grid_size - 1, static_cast<int>(base_r / bin_size));
                    int gi = std::min(grid_size - 1, static_cast<int>(base_g / bin_size));
                    int bi = std::min(grid_size - 1, static_cast<int>(base_b / bin_size));

                    int cell_idx = (ri * grid_size + gi) * grid_size + bi;

                    // Accumulate target RGB (normalized)
                    sum[cell_idx * 3 + 0] += tgt_r / 255.0;
                    sum[cell_idx * 3 + 1] += tgt_g / 255.0;
                    sum[cell_idx * 3 + 2] += tgt_b / 255.0;
                    count[cell_idx]++;
                }
            }

            // Compute averages, use identity for empty cells
            int empty_cells = 0;
            for (int ri = 0; ri < grid_size; ri++)
            {
                for (int gi = 0; gi < grid_size; gi++)
                {
                    for (int bi = 0; bi < grid_size; bi++)
                    {
                        int cell_idx = (ri * grid_size + gi) * grid_size + bi;
                        int lut_base = cell_idx * 3;

                        if (count[cell_idx] > 0)
                        {
                            lut[lut_base + 0] = static_cast<float>(sum[lut_base + 0] / count[cell_idx]);
                            lut[lut_base + 1] = static_cast<float>(sum[lut_base + 1] / count[cell_idx]);
                            lut[lut_base + 2] = static_cast<float>(sum[lut_base + 2] / count[cell_idx]);
                        }
                        else
                        {
                            // Identity for empty cells
                            lut[lut_base + 0] = static_cast<float>(ri) / (grid_size - 1);
                            lut[lut_base + 1] = static_cast<float>(gi) / (grid_size - 1);
                            lut[lut_base + 2] = static_cast<float>(bi) / (grid_size - 1);
                            empty_cells++;
                        }
                    }
                }
            }

            std::cerr << "[Lut3D] Estimated " << grid_size << "³ LUT, "
                      << empty_cells << "/" << (grid_size*grid_size*grid_size)
                      << " cells empty (identity)\n";

            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Lut3D] Error: " << e.what() << "\n";
            return false;
        }
    }

} // namespace mods
} // namespace pipe

// camera_decompose.cpp
// Camera Phase: Decomposed transform estimation
//
// Instead of a monolithic 3D LUT, decompose into:
// 1. Luminance curve (1D) - Y_out = f(Y_in)
// 2. Saturation curve (1D) - S_out = g(S_in, Y_in)
// 3. Hue shift curve (1D) - H_out = H_in + h(H_in)
// 4. Color matrix (3x3) - for any remaining cross-channel effects
//
// This is more efficient than 3D LUT because:
// - 1D curves only need 256 samples each
// - Every luminance value has data (well-sampled)
// - Hue/saturation operate on polar coordinates (natural for color)

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

// RGB to YCbCr (approximate luminance + chroma)
void rgb_to_ycbcr(float r, float g, float b, float& y, float& cb, float& cr)
{
    y = 0.299f * r + 0.587f * g + 0.114f * b;
    cb = 0.564f * (b - y);
    cr = 0.713f * (r - y);
}

void ycbcr_to_rgb(float y, float cb, float cr, float& r, float& g, float& b)
{
    r = y + 1.403f * cr;
    g = y - 0.344f * cb - 0.714f * cr;
    b = y + 1.773f * cb;
}

// RGB to HSL
void rgb_to_hsl(float r, float g, float b, float& h, float& s, float& l)
{
    float max_c = std::max({r, g, b});
    float min_c = std::min({r, g, b});
    float delta = max_c - min_c;

    l = (max_c + min_c) / 2.0f;

    if (delta < 0.0001f) {
        h = 0;
        s = 0;
    } else {
        s = (l > 0.5f) ? delta / (2.0f - max_c - min_c) : delta / (max_c + min_c);

        if (max_c == r) {
            h = (g - b) / delta + (g < b ? 6.0f : 0.0f);
        } else if (max_c == g) {
            h = (b - r) / delta + 2.0f;
        } else {
            h = (r - g) / delta + 4.0f;
        }
        h /= 6.0f;
    }
}

void hsl_to_rgb(float h, float s, float l, float& r, float& g, float& b)
{
    if (s < 0.0001f) {
        r = g = b = l;
        return;
    }

    auto hue2rgb = [](float p, float q, float t) {
        if (t < 0) t += 1;
        if (t > 1) t -= 1;
        if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f/2.0f) return q;
        if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
        return p;
    };

    float q = (l < 0.5f) ? l * (1 + s) : l + s - l * s;
    float p = 2 * l - q;

    r = hue2rgb(p, q, h + 1.0f/3.0f);
    g = hue2rgb(p, q, h);
    b = hue2rgb(p, q, h - 1.0f/3.0f);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Camera Phase: Decomposed Transform ===" << std::endl;
    std::cout << "RAW: " << raw_path << std::endl;

    // Load RAW
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

    // Resize scene-linear to match camera JPEG
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    // Convert scene-linear to gamma space
    cv::Mat scene_gamma;
    cv::Mat scene_clamped;
    cv::max(scene_resized, 0.0f, scene_clamped);
    cv::min(scene_clamped, 1.0f, scene_clamped);
    cv::pow(scene_clamped, 1.0f/2.2f, scene_gamma);

    std::cout << "Images: " << scene_gamma.cols << "x" << scene_gamma.rows << std::endl;

    // ========================================
    // STEP 1: Estimate luminance curve
    // ========================================
    std::cout << "\n--- Step 1: Luminance Curve ---" << std::endl;

    // Bin by input luminance, measure output luminance
    std::vector<double> lum_sum(256, 0.0);
    std::vector<int> lum_count(256, 0);

    for (int y = 0; y < scene_gamma.rows; y++) {
        const float* s_ptr = scene_gamma.ptr<float>(y);
        const uchar* t_ptr = camera_jpeg.ptr<uchar>(y);

        for (int x = 0; x < scene_gamma.cols; x++) {
            // Input luminance (from gamma-encoded scene)
            float sb = s_ptr[x*3 + 0];
            float sg = s_ptr[x*3 + 1];
            float sr = s_ptr[x*3 + 2];
            float y_in = 0.299f * sr + 0.587f * sg + 0.114f * sb;

            // Output luminance (from camera JPEG)
            float tb = t_ptr[x*3 + 0] / 255.0f;
            float tg = t_ptr[x*3 + 1] / 255.0f;
            float tr = t_ptr[x*3 + 2] / 255.0f;
            float y_out = 0.299f * tr + 0.587f * tg + 0.114f * tb;

            int bin = std::min(255, static_cast<int>(y_in * 255.0f));
            lum_sum[bin] += y_out;
            lum_count[bin]++;
        }
    }

    // Build luminance curve
    std::vector<float> lum_curve(256);
    int filled_lum = 0;
    for (int i = 0; i < 256; i++) {
        if (lum_count[i] > 0) {
            lum_curve[i] = lum_sum[i] / lum_count[i];
            filled_lum++;
        } else {
            lum_curve[i] = i / 255.0f;  // Identity
        }
    }
    std::cout << "Luminance curve: " << filled_lum << "/256 bins filled" << std::endl;

    // Ensure monotonicity
    for (int i = 1; i < 256; i++) {
        if (lum_curve[i] < lum_curve[i-1])
            lum_curve[i] = lum_curve[i-1];
    }

    // ========================================
    // STEP 2: Estimate saturation scaling
    // ========================================
    std::cout << "\n--- Step 2: Saturation Curve ---" << std::endl;

    // For each luminance zone, measure saturation scaling
    // sat_out / sat_in as a function of luminance
    std::vector<double> sat_scale_sum(256, 0.0);
    std::vector<int> sat_count(256, 0);

    for (int y = 0; y < scene_gamma.rows; y++) {
        const float* s_ptr = scene_gamma.ptr<float>(y);
        const uchar* t_ptr = camera_jpeg.ptr<uchar>(y);

        for (int x = 0; x < scene_gamma.cols; x++) {
            float sb = s_ptr[x*3 + 0];
            float sg = s_ptr[x*3 + 1];
            float sr = s_ptr[x*3 + 2];

            float tb = t_ptr[x*3 + 0] / 255.0f;
            float tg = t_ptr[x*3 + 1] / 255.0f;
            float tr = t_ptr[x*3 + 2] / 255.0f;

            // Convert to HSL
            float h_in, s_in, l_in;
            float h_out, s_out, l_out;
            rgb_to_hsl(sr, sg, sb, h_in, s_in, l_in);
            rgb_to_hsl(tr, tg, tb, h_out, s_out, l_out);

            // Only use pixels with enough saturation to measure
            if (s_in > 0.05f && s_out > 0.01f) {
                float scale = s_out / s_in;
                int bin = std::min(255, static_cast<int>(l_in * 255.0f));
                sat_scale_sum[bin] += scale;
                sat_count[bin]++;
            }
        }
    }

    // Build saturation curve
    std::vector<float> sat_curve(256);
    int filled_sat = 0;
    for (int i = 0; i < 256; i++) {
        if (sat_count[i] > 10) {  // Need enough samples
            sat_curve[i] = sat_scale_sum[i] / sat_count[i];
            filled_sat++;
        } else {
            sat_curve[i] = 1.0f;  // No change
        }
    }
    std::cout << "Saturation curve: " << filled_sat << "/256 bins filled" << std::endl;

    // Smooth saturation curve
    std::vector<float> sat_smoothed(256);
    sat_smoothed[0] = sat_curve[0];
    sat_smoothed[255] = sat_curve[255];
    for (int i = 1; i < 255; i++) {
        sat_smoothed[i] = 0.25f * sat_curve[i-1] + 0.5f * sat_curve[i] + 0.25f * sat_curve[i+1];
    }
    sat_curve = sat_smoothed;

    // ========================================
    // STEP 3: Apply decomposed transform
    // ========================================
    std::cout << "\n--- Applying Transform ---" << std::endl;

    cv::Mat result(scene_gamma.size(), CV_32FC3);

    for (int y = 0; y < scene_gamma.rows; y++) {
        const float* s_ptr = scene_gamma.ptr<float>(y);
        float* r_ptr = result.ptr<float>(y);

        for (int x = 0; x < scene_gamma.cols; x++) {
            float sb = s_ptr[x*3 + 0];
            float sg = s_ptr[x*3 + 1];
            float sr = s_ptr[x*3 + 2];

            // Convert to HSL
            float h, s, l;
            rgb_to_hsl(sr, sg, sb, h, s, l);

            // Apply luminance curve
            int l_bin = std::min(255, static_cast<int>(l * 255.0f));
            float l_new = lum_curve[l_bin];

            // Apply saturation curve
            int l_new_bin = std::min(255, static_cast<int>(l_new * 255.0f));
            float s_new = s * sat_curve[l_new_bin];
            s_new = std::min(1.0f, std::max(0.0f, s_new));

            // Hue unchanged for now
            float h_new = h;

            // Convert back to RGB
            float ro, go, bo;
            hsl_to_rgb(h_new, s_new, l_new, ro, go, bo);

            r_ptr[x*3 + 0] = std::max(0.0f, std::min(1.0f, bo));
            r_ptr[x*3 + 1] = std::max(0.0f, std::min(1.0f, go));
            r_ptr[x*3 + 2] = std::max(0.0f, std::min(1.0f, ro));
        }
    }

    // ========================================
    // Compute error
    // ========================================
    cv::Mat target_f;
    camera_jpeg.convertTo(target_f, CV_32FC3, 1.0/255.0);

    cv::Mat diff;
    cv::absdiff(result, target_f, diff);

    cv::Scalar mean_diff = cv::mean(diff);
    float mae = (mean_diff[0] + mean_diff[1] + mean_diff[2]) / 3.0f;

    std::cout << "\n=== RESULT ===" << std::endl;
    std::cout << "Decomposed transform error: " << (mae * 100.0f) << "%" << std::endl;

    // Save comparison
    cv::Mat result_8u, target_8u;
    result.convertTo(result_8u, CV_8UC3, 255.0);
    camera_jpeg.convertTo(target_8u, CV_8UC3);

    cv::Mat comparison;
    cv::hconcat(target_8u, result_8u, comparison);
    cv::imwrite("tmp/var/tune/decompose_compare.png", comparison);
    std::cout << "Saved: tmp/var/tune/decompose_compare.png" << std::endl;

    // ========================================
    // STEP 4: Measure residual with 3D LUT
    // ========================================
    std::cout << "\n--- Step 4: Residual 3D LUT ---" << std::endl;

    // Now estimate a 3D LUT on the RESIDUAL (what's left after decomposed transform)
    // This should be much smaller and better conditioned

    const int GRID = 17;
    int total_cells = GRID * GRID * GRID;
    std::vector<double> res_sum(total_cells * 3, 0.0);
    std::vector<int> res_count(total_cells, 0);

    for (int y = 0; y < result.rows; y++) {
        const float* r_ptr = result.ptr<float>(y);
        const uchar* t_ptr = camera_jpeg.ptr<uchar>(y);

        for (int x = 0; x < result.cols; x++) {
            float rb = r_ptr[x*3 + 0];
            float rg = r_ptr[x*3 + 1];
            float rr = r_ptr[x*3 + 2];

            float tb = t_ptr[x*3 + 0] / 255.0f;
            float tg = t_ptr[x*3 + 1] / 255.0f;
            float tr = t_ptr[x*3 + 2] / 255.0f;

            // Bin by our result
            int ri = std::min(GRID-1, static_cast<int>(rr * GRID));
            int gi = std::min(GRID-1, static_cast<int>(rg * GRID));
            int bi = std::min(GRID-1, static_cast<int>(rb * GRID));

            int cell = (ri * GRID + gi) * GRID + bi;

            res_sum[cell*3 + 0] += tr;
            res_sum[cell*3 + 1] += tg;
            res_sum[cell*3 + 2] += tb;
            res_count[cell]++;
        }
    }

    // Build residual LUT
    std::vector<float> res_lut(total_cells * 3);
    int res_filled = 0;
    for (int ri = 0; ri < GRID; ri++) {
        for (int gi = 0; gi < GRID; gi++) {
            for (int bi = 0; bi < GRID; bi++) {
                int cell = (ri * GRID + gi) * GRID + bi;
                if (res_count[cell] > 0) {
                    res_lut[cell*3 + 0] = res_sum[cell*3 + 0] / res_count[cell];
                    res_lut[cell*3 + 1] = res_sum[cell*3 + 1] / res_count[cell];
                    res_lut[cell*3 + 2] = res_sum[cell*3 + 2] / res_count[cell];
                    res_filled++;
                } else {
                    // Identity
                    res_lut[cell*3 + 0] = static_cast<float>(ri) / (GRID - 1);
                    res_lut[cell*3 + 1] = static_cast<float>(gi) / (GRID - 1);
                    res_lut[cell*3 + 2] = static_cast<float>(bi) / (GRID - 1);
                }
            }
        }
    }

    std::cout << "Residual LUT: " << res_filled << "/" << total_cells
              << " (" << (100.0f * res_filled / total_cells) << "%) filled" << std::endl;

    // Apply residual LUT (trilinear)
    cv::Mat final_result(result.size(), CV_32FC3);

    auto trilinear = [&](float r, float g, float b, float& ro, float& go, float& bo) {
        float scale = GRID - 1;
        float fr = r * scale, fg = g * scale, fb = b * scale;
        int r0 = std::max(0, std::min(GRID-2, static_cast<int>(fr)));
        int g0 = std::max(0, std::min(GRID-2, static_cast<int>(fg)));
        int b0 = std::max(0, std::min(GRID-2, static_cast<int>(fb)));
        float dr = fr - r0, dg = fg - g0, db = fb - b0;

        auto idx = [&](int ri, int gi, int bi, int ch) {
            return ((ri * GRID + gi) * GRID + bi) * 3 + ch;
        };

        for (int ch = 0; ch < 3; ch++) {
            float c000 = res_lut[idx(r0, g0, b0, ch)];
            float c001 = res_lut[idx(r0, g0, b0+1, ch)];
            float c010 = res_lut[idx(r0, g0+1, b0, ch)];
            float c011 = res_lut[idx(r0, g0+1, b0+1, ch)];
            float c100 = res_lut[idx(r0+1, g0, b0, ch)];
            float c101 = res_lut[idx(r0+1, g0, b0+1, ch)];
            float c110 = res_lut[idx(r0+1, g0+1, b0, ch)];
            float c111 = res_lut[idx(r0+1, g0+1, b0+1, ch)];

            float c00 = c000 * (1-db) + c001 * db;
            float c01 = c010 * (1-db) + c011 * db;
            float c10 = c100 * (1-db) + c101 * db;
            float c11 = c110 * (1-db) + c111 * db;
            float c0 = c00 * (1-dg) + c01 * dg;
            float c1 = c10 * (1-dg) + c11 * dg;
            float val = c0 * (1-dr) + c1 * dr;

            if (ch == 0) ro = val;
            else if (ch == 1) go = val;
            else bo = val;
        }
    };

    for (int y = 0; y < result.rows; y++) {
        const float* r_ptr = result.ptr<float>(y);
        float* f_ptr = final_result.ptr<float>(y);

        for (int x = 0; x < result.cols; x++) {
            float rb = std::max(0.0f, std::min(1.0f, r_ptr[x*3 + 0]));
            float rg = std::max(0.0f, std::min(1.0f, r_ptr[x*3 + 1]));
            float rr = std::max(0.0f, std::min(1.0f, r_ptr[x*3 + 2]));

            float fo, go, bo;
            trilinear(rr, rg, rb, fo, go, bo);

            f_ptr[x*3 + 0] = std::max(0.0f, std::min(1.0f, bo));
            f_ptr[x*3 + 1] = std::max(0.0f, std::min(1.0f, go));
            f_ptr[x*3 + 2] = std::max(0.0f, std::min(1.0f, fo));
        }
    }

    // Final error
    cv::Mat final_diff;
    cv::absdiff(final_result, target_f, final_diff);
    cv::Scalar final_mean = cv::mean(final_diff);
    float final_mae = (final_mean[0] + final_mean[1] + final_mean[2]) / 3.0f;

    std::cout << "\n=== FINAL RESULT ===" << std::endl;
    std::cout << "Decomposed + Residual LUT error: " << (final_mae * 100.0f) << "%" << std::endl;

    // Save final comparison
    cv::Mat final_8u;
    final_result.convertTo(final_8u, CV_8UC3, 255.0);
    cv::Mat final_comparison;
    cv::hconcat(target_8u, final_8u, final_comparison);
    cv::imwrite("tmp/var/tune/final_compare.png", final_comparison);
    std::cout << "Saved: tmp/var/tune/final_compare.png" << std::endl;

    return 0;
}

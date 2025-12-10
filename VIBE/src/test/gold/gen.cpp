// gen.cpp - VIBE Test
// Generate golden reference images based on pure algorithm theory
// NO OpenCV mod dependencies - validates both CV and DAWN against ground truth

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cmath>
#include <vector>
#include <utility>

#include "../theory.h"

static const char* GOLD_DIR = "src/test/gold/";

static theory::Image create_test_image(int width = 256, int height = 256)
{
    theory::Image img(width, height);
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            float u = float(x) / (width - 1);
            float v = float(y) / (height - 1);
            img.at(y, x, 0) = u * 0.8f + 0.1f;        // B
            img.at(y, x, 1) = v * 0.8f + 0.1f;        // G
            img.at(y, x, 2) = (1.0f - u) * 0.6f + 0.2f; // R
        }
    }
    return img;
}

static void save_gold(const theory::Image& img, const char* name)
{
    // Copy to OpenCV Mat for saving
    cv::Mat cpu(img.height, img.width, CV_32FC3);
    for (int y = 0; y < img.height; y++)
    {
        float* row = cpu.ptr<float>(y);
        for (int x = 0; x < img.width; x++)
        {
            row[x * 3 + 0] = std::clamp(img.at(y, x, 0), 0.0f, 1.0f);
            row[x * 3 + 1] = std::clamp(img.at(y, x, 1), 0.0f, 1.0f);
            row[x * 3 + 2] = std::clamp(img.at(y, x, 2), 0.0f, 1.0f);
        }
    }

    // Convert to 16-bit for precision
    cv::Mat out;
    cpu.convertTo(out, CV_16UC3, 65535.0);

    std::string path = std::string(GOLD_DIR) + name + ".png";
    cv::imwrite(path, out);
    std::cout << "  " << name << ".png\n";
}

int main()
{
    std::cout << "Generating golden reference images from theory...\n\n";

    theory::Image input = create_test_image();

    // Save input
    {
        cv::Mat cpu(input.height, input.width, CV_32FC3);
        for (int y = 0; y < input.height; y++)
        {
            float* row = cpu.ptr<float>(y);
            for (int x = 0; x < input.width; x++)
            {
                row[x * 3 + 0] = input.at(y, x, 0);
                row[x * 3 + 1] = input.at(y, x, 1);
                row[x * 3 + 2] = input.at(y, x, 2);
            }
        }
        cv::Mat out;
        cpu.convertTo(out, CV_16UC3, 65535.0);
        cv::imwrite(std::string(GOLD_DIR) + "input.png", out);
        std::cout << "  input.png\n";
    }

    theory::Image out(input.width, input.height);

    // Dial mods
    theory::exposure(input, out, 0.7f);
    save_gold(out, "exposure");

    theory::white_balance(input, out, 0.6f, 0.4f);
    save_gold(out, "white_balance");

    theory::tone_map(input, out, 0.6f, 0.4f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f);
    save_gold(out, "tone_map");

    theory::global_color(input, out, 0.6f, 0.55f, 0.5f);
    save_gold(out, "global_color");

    // Geometric has different output dimensions (cropped size)
    {
        auto [gw, gh] = theory::geometric_output_size(input.width, input.height,
                                                       0.1f, 0.1f, 0.1f, 0.1f);
        theory::Image geo_out(gw, gh);
        theory::geometric(input, geo_out, 0.1f, 0.1f, 0.1f, 0.1f, 0.2f, 0.5f);
        save_gold(geo_out, "geometric");
    }

    {
        float hue[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        float sat[8] = {0.6f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        float lum[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        theory::selective_color(input, out, hue, sat, lum);
        save_gold(out, "selective_color");
    }

    theory::split_tone(input, out, 0.4f, 0.5f, 0.6f, 0.5f);
    save_gold(out, "split_tone");

    theory::detail(input, out, 0.6f, 0.4f, 0.3f, 0.5f);
    save_gold(out, "detail");

    // Meta mods
    theory::baseline(input, out, 0.7f, 0.95f);
    save_gold(out, "baseline");

    theory::sigmoid(input, out, 1.5f, 0.0f, 1.0f, 0.000152f);
    save_gold(out, "sigmoid");

    {
        float curve[768];  // 256 per channel (B, G, R)
        for (int c = 0; c < 3; c++)
            for (int i = 0; i < 256; i++)
                curve[c * 256 + i] = std::pow(float(i) / 255.0f, 1.1f);
        theory::base_curve(input, out, curve);
        save_gold(out, "base_curve");
    }

    {
        float matrix[9] = {1.1f, -0.05f, -0.05f,
                          -0.05f, 1.1f, -0.05f,
                          -0.05f, -0.05f, 1.1f};
        theory::color_matrix(input, out, matrix);
        save_gold(out, "color_matrix");
    }

    {
        float lut[96];
        for (int i = 0; i < 32; i++)
        {
            float v = float(i) / 31.0f;
            lut[i] = std::pow(v, 0.9f);
            lut[32 + i] = v;
            lut[64 + i] = std::pow(v, 1.1f);
        }
        theory::lut_curve(input, out, lut, 32);
        save_gold(out, "lut_curve");
    }

    {
        int gs = 9;
        std::vector<float> lut(gs * gs * gs * 3);
        for (int ri = 0; ri < gs; ri++)
            for (int gi = 0; gi < gs; gi++)
                for (int bi = 0; bi < gs; bi++)
                {
                    int idx = ((ri * gs + gi) * gs + bi) * 3;
                    lut[idx] = float(ri) / (gs - 1);
                    lut[idx + 1] = float(gi) / (gs - 1);
                    lut[idx + 2] = float(bi) / (gs - 1);
                }
        theory::lut3d(input, out, lut.data(), gs);
        save_gold(out, "lut3d");
    }

    {
        const int h_bins = 36, s_bins = 12;
        std::vector<float> lut(h_bins * s_bins * 3, 0.0f);
        theory::hsv_lut(input, out, lut.data(), h_bins, s_bins);
        save_gold(out, "hsv_lut");
    }

    {
        float coeffs[30] = {0};
        coeffs[1] = 1.0f;   // R -> R
        coeffs[12] = 1.0f;  // G -> G
        coeffs[23] = 1.0f;  // B -> B
        theory::poly_color(input, out, coeffs);
        save_gold(out, "poly_color");
    }

    theory::local_tone(input, out, 0.5f, 0.02f, 0.1f);
    save_gold(out, "local_tone");

    std::cout << "\nDone. 18 theory-based images generated.\n";
    return 0;
}

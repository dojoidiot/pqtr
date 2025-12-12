// test_white_balance.cpp
// Unit test for white_balance module
//
// Usage: test_white_balance <input.png> <output.png> [temperature] [tint]
//
// temperature: 0.0-1.0, default 0.5 (6500K daylight)
//   0.0 = 2000K (warm correction - adds blue)
//   0.5 = 6500K (neutral)
//   1.0 = 12000K (cool correction - adds orange)
//
// tint: 0.0-1.0, default 0.5 (neutral)
//   0.0 = green shift
//   0.5 = neutral
//   1.0 = magenta shift

#include <iostream>
#include <string>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "pipe/mods/mods.h"

// Simple inverse gamma (sRGB → linear approximation)
void to_linear(const cv::UMat& input, cv::UMat& output)
{
    cv::UMat normalized;
    input.convertTo(normalized, CV_32FC3, 1.0/255.0);
    cv::pow(normalized, 2.2f, output);
}

// Simple gamma (linear → sRGB approximation)
void to_gamma(const cv::UMat& input, cv::UMat& output)
{
    // Clamp to [0,1] before gamma
    cv::UMat clamped;
    cv::max(input, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);

    cv::UMat gamma_corrected;
    cv::pow(clamped, 1.0f/2.2f, gamma_corrected);
    gamma_corrected.convertTo(output, CV_8UC3, 255.0);
}

// Print image statistics per channel
void print_stats(const std::string& label, const cv::UMat& img)
{
    cv::Mat cpu;
    img.copyTo(cpu);

    std::vector<cv::Mat> channels(3);
    cv::split(cpu, channels);

    double minB, maxB, minG, maxG, minR, maxR;
    cv::minMaxLoc(channels[0], &minB, &maxB);
    cv::minMaxLoc(channels[1], &minG, &maxG);
    cv::minMaxLoc(channels[2], &minR, &maxR);

    cv::Scalar mean = cv::mean(cpu);

    std::cout << "  " << label << ":" << std::endl;
    std::cout << "    R: min=" << minR << " max=" << maxR << " mean=" << mean[2] << std::endl;
    std::cout << "    G: min=" << minG << " max=" << maxG << " mean=" << mean[1] << std::endl;
    std::cout << "    B: min=" << minB << " max=" << maxB << " mean=" << mean[0] << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.png> [temperature] [tint]" << std::endl;
        std::cerr << "  temperature: 0.0-1.0, default 0.5 (6500K)" << std::endl;
        std::cerr << "              0.0=2000K (warm), 0.5=6500K (daylight), 1.0=12000K (cool)" << std::endl;
        std::cerr << "  tint: 0.0-1.0, default 0.5 (neutral)" << std::endl;
        std::cerr << "        0.0=green, 0.5=neutral, 1.0=magenta" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    float temperature = (argc > 3) ? std::stof(argv[3]) : 0.5f;
    float tint = (argc > 4) ? std::stof(argv[4]) : 0.5f;

    // Calculate Kelvin for display
    float kelvin = 2000.0f * std::pow(6.0f, temperature);
    float tint_value = (tint - 0.5f) * 200.0f; // -100 to +100

    try
    {
        std::cout << "White Balance Test" << std::endl;
        std::cout << "  Input: " << inputPath << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Temperature: " << temperature << " → " << kelvin << "K" << std::endl;
        std::cout << "  Tint: " << tint << " → " << tint_value << std::endl;

        // Load input image
        cv::Mat input_cpu = cv::imread(inputPath);
        if (input_cpu.empty())
        {
            throw std::runtime_error("Failed to load input image");
        }
        std::cout << "  Size: " << input_cpu.cols << "x" << input_cpu.rows << std::endl;

        cv::UMat input;
        input_cpu.copyTo(input);

        // Convert to linear
        cv::UMat linear;
        to_linear(input, linear);

        std::cout << "\nBefore white balance:" << std::endl;
        print_stats("Linear RGB", linear);

        // Apply white balance
        cv::UMat balanced;
        if (!pipe::mods::white_balance(linear, balanced, temperature, tint))
        {
            throw std::runtime_error("White balance failed");
        }

        std::cout << "\nAfter white balance:" << std::endl;
        print_stats("Balanced", balanced);

        // Apply gamma and save
        cv::UMat output;
        to_gamma(balanced, output);

        cv::Mat output_cpu;
        output.copyTo(output_cpu);
        if (!cv::imwrite(outputPath, output_cpu))
        {
            throw std::runtime_error("Failed to save output image");
        }

        std::cout << "\n✓ Saved: " << outputPath << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

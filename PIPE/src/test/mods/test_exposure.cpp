// test_exposure.cpp
// Unit test for exposure module
//
// Usage: test_exposure <input.png> <output.png> [dial]
//
// dial: 0.0-1.0, default 0.5 (neutral)
//   0.0 = -4 EV (very dark)
//   0.5 = 0 EV (no change)
//   1.0 = +4 EV (very bright)

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

// Print image statistics
void print_stats(const std::string& label, const cv::UMat& img)
{
    cv::Mat cpu;
    img.copyTo(cpu);

    double minVal, maxVal;
    cv::minMaxLoc(cpu.reshape(1), &minVal, &maxVal);
    cv::Scalar mean = cv::mean(cpu);

    std::cout << "  " << label << ": min=" << minVal
              << " max=" << maxVal
              << " mean=[" << mean[0] << "," << mean[1] << "," << mean[2] << "]"
              << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.png> [dial]" << std::endl;
        std::cerr << "  dial: 0.0-1.0, default 0.5 (neutral)" << std::endl;
        std::cerr << "        0.0 = -4 EV, 0.5 = 0 EV, 1.0 = +4 EV" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    float dial = (argc > 3) ? std::stof(argv[3]) : 0.5f;

    // Calculate EV for display
    float ev = (dial - 0.5f) * 8.0f;
    float multiplier = std::pow(2.0f, ev);

    try
    {
        std::cout << "Exposure Test" << std::endl;
        std::cout << "  Input: " << inputPath << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Dial: " << dial << " → EV=" << ev << " (×" << multiplier << ")" << std::endl;

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

        std::cout << "\nBefore exposure:" << std::endl;
        print_stats("Linear RGB", linear);

        // Apply exposure
        cv::UMat exposed;
        if (!pipe::mods::exposure(linear, exposed, dial))
        {
            throw std::runtime_error("Exposure failed");
        }

        std::cout << "\nAfter exposure:" << std::endl;
        print_stats("Exposed", exposed);

        // Apply gamma and save
        cv::UMat output;
        to_gamma(exposed, output);

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

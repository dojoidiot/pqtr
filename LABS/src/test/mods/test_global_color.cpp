// test_global_color.cpp
// Unit test for global_color module (3 dials)
//
// Usage: test_global_color <input.png> <output.png> [vibrance] [saturation] [color_density]
//
// All dials: 0.0-1.0, default 0.5 (neutral)

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
    // Clamp before gamma
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

    // Split channels
    std::vector<cv::Mat> channels;
    cv::split(cpu, channels);

    std::cout << "  " << label << ":" << std::endl;
    const char* names[] = {"R", "G", "B"};
    for (int i = 0; i < 3; i++) {
        double minVal, maxVal;
        cv::minMaxLoc(channels[i], &minVal, &maxVal);
        cv::Scalar mean = cv::mean(channels[i]);
        std::cout << "    " << names[i] << ": min=" << minVal
                  << " max=" << maxVal
                  << " mean=" << mean[0] << std::endl;
    }

    // Compute average saturation (simple approximation)
    cv::Mat hsv;
    cv::Mat rgb8;
    cpu.convertTo(rgb8, CV_8UC3, 255.0);
    cv::cvtColor(rgb8, hsv, cv::COLOR_RGB2HSV);
    std::vector<cv::Mat> hsv_channels;
    cv::split(hsv, hsv_channels);
    cv::Scalar sat_mean = cv::mean(hsv_channels[1]);
    std::cout << "    Avg Saturation (HSV): " << sat_mean[0] << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.png> [vibrance] [saturation] [color_density]" << std::endl;
        std::cerr << "  All dials: 0.0-1.0, default 0.5 (neutral)" << std::endl;
        std::cerr << "  vibrance:      0.5 = 0 (no boost)" << std::endl;
        std::cerr << "  saturation:    0.5 = 1.0 (neutral)" << std::endl;
        std::cerr << "  color_density: 0.5 = 1.0 (neutral)" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    float vibrance = (argc > 3) ? std::stof(argv[3]) : 0.5f;
    float saturation = (argc > 4) ? std::stof(argv[4]) : 0.5f;
    float color_density = (argc > 5) ? std::stof(argv[5]) : 0.5f;

    // Convert dials to display values
    float vibrance_val = (vibrance - 0.5f) * 2.0f;
    float saturation_val = saturation * 2.0f;
    float color_density_val = 0.5f + color_density;

    try
    {
        std::cout << "Global Color Test (3 dials)" << std::endl;
        std::cout << "  Input: " << inputPath << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Dials:" << std::endl;
        std::cout << "    vibrance:      " << vibrance << " -> " << vibrance_val << std::endl;
        std::cout << "    saturation:    " << saturation << " -> " << saturation_val << std::endl;
        std::cout << "    color_density: " << color_density << " -> " << color_density_val << std::endl;

        // Load input image
        cv::Mat input_cpu = cv::imread(inputPath);
        if (input_cpu.empty())
        {
            throw std::runtime_error("Failed to load input image");
        }
        std::cout << "  Size: " << input_cpu.cols << "x" << input_cpu.rows << std::endl;

        // Convert BGR to RGB
        cv::cvtColor(input_cpu, input_cpu, cv::COLOR_BGR2RGB);

        cv::UMat input;
        input_cpu.copyTo(input);

        // Convert to linear (assume input is gamma-encoded)
        cv::UMat linear;
        to_linear(input, linear);

        std::cout << "\nBefore global color:" << std::endl;
        print_stats("Linear RGB", linear);

        // Apply global color
        cv::UMat adjusted;
        if (!pipe::mods::global_color(linear, adjusted, vibrance, saturation, color_density))
        {
            throw std::runtime_error("Global color adjustment failed");
        }

        std::cout << "\nAfter global color:" << std::endl;
        print_stats("Adjusted", adjusted);

        // Apply gamma and save
        cv::UMat output;
        to_gamma(adjusted, output);

        cv::Mat output_cpu;
        output.copyTo(output_cpu);
        cv::cvtColor(output_cpu, output_cpu, cv::COLOR_RGB2BGR);
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

// test_geometric.cpp
// Unit test for geometric module (6 dials)
//
// Usage: test_geometric <input.png> <output.png> [crop_top] [crop_right] [crop_bottom] [crop_left] [zoom] [tilt]
//
// All dials: 0.0-1.0

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
    cv::UMat clamped;
    cv::max(input, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);

    cv::UMat gamma_corrected;
    cv::pow(clamped, 1.0f/2.2f, gamma_corrected);
    gamma_corrected.convertTo(output, CV_8UC3, 255.0);
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.png> [crop_top] [crop_right] [crop_bottom] [crop_left] [zoom] [tilt]" << std::endl;
        std::cerr << "  crop_*: 0.0-1.0, default 0.0 (0% to 50% inset)" << std::endl;
        std::cerr << "  zoom:   0.0-1.0, default 0.0 (1x to 4x)" << std::endl;
        std::cerr << "  tilt:   0.0-1.0, default 0.5 (-45° to +45°, 0.5 = 0°)" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    float crop_top = (argc > 3) ? std::stof(argv[3]) : 0.0f;
    float crop_right = (argc > 4) ? std::stof(argv[4]) : 0.0f;
    float crop_bottom = (argc > 5) ? std::stof(argv[5]) : 0.0f;
    float crop_left = (argc > 6) ? std::stof(argv[6]) : 0.0f;
    float zoom = (argc > 7) ? std::stof(argv[7]) : 0.0f;
    float tilt = (argc > 8) ? std::stof(argv[8]) : 0.5f;

    // Convert dials to display values
    float crop_top_pct = crop_top * 50.0f;
    float crop_right_pct = crop_right * 50.0f;
    float crop_bottom_pct = crop_bottom * 50.0f;
    float crop_left_pct = crop_left * 50.0f;
    float zoom_val = 1.0f + zoom * 3.0f;
    float tilt_val = (tilt - 0.5f) * 90.0f;

    try
    {
        std::cout << "Geometric Test (6 dials)" << std::endl;
        std::cout << "  Input: " << inputPath << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Dials:" << std::endl;
        std::cout << "    crop_top:    " << crop_top << " -> " << crop_top_pct << "%" << std::endl;
        std::cout << "    crop_right:  " << crop_right << " -> " << crop_right_pct << "%" << std::endl;
        std::cout << "    crop_bottom: " << crop_bottom << " -> " << crop_bottom_pct << "%" << std::endl;
        std::cout << "    crop_left:   " << crop_left << " -> " << crop_left_pct << "%" << std::endl;
        std::cout << "    zoom:        " << zoom << " -> " << zoom_val << "x" << std::endl;
        std::cout << "    tilt:        " << tilt << " -> " << tilt_val << "°" << std::endl;

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

        // Convert to linear
        cv::UMat linear;
        to_linear(input, linear);

        std::cout << "\nBefore geometric:" << std::endl;
        std::cout << "  Size: " << linear.cols << "x" << linear.rows << std::endl;

        // Apply geometric
        cv::UMat adjusted;
        if (!pipe::mods::geometric(linear, adjusted, crop_top, crop_right, crop_bottom, crop_left, zoom, tilt))
        {
            throw std::runtime_error("Geometric adjustment failed");
        }

        std::cout << "\nAfter geometric:" << std::endl;
        std::cout << "  Size: " << adjusted.cols << "x" << adjusted.rows << std::endl;

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

// test_tone_map.cpp
// Unit test for tone_map module
//
// Usage: test_tone_map <input.png> <output.png> [white_point] [contrast]
//
// Tests the tone mapping module by:
// 1. Loading a scene-linear sRGB image (or converting from gamma)
// 2. Applying tone mapping with specified parameters
// 3. Applying gamma and saving result
// 4. Printing before/after statistics

#include <iostream>
#include <string>
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
    cv::UMat gamma_corrected;
    cv::pow(input, 1.0f/2.2f, gamma_corrected);
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
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.png> [white_point] [contrast]" << std::endl;
        std::cerr << "  white_point: Scene value mapping to white (default: 1.0)" << std::endl;
        std::cerr << "  contrast: Midtone contrast (default: 1.0)" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    float white_point = (argc > 3) ? std::stof(argv[3]) : 1.0f;
    float contrast = (argc > 4) ? std::stof(argv[4]) : 1.0f;

    try
    {
        std::cout << "Tone Map Test" << std::endl;
        std::cout << "  Input: " << inputPath << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Parameters: white_point=" << white_point << " contrast=" << contrast << std::endl;

        // Load input image
        cv::Mat input_cpu = cv::imread(inputPath);
        if (input_cpu.empty())
        {
            throw std::runtime_error("Failed to load input image");
        }
        std::cout << "  Size: " << input_cpu.cols << "x" << input_cpu.rows << std::endl;

        cv::UMat input;
        input_cpu.copyTo(input);

        // Convert to linear (assume input is gamma-encoded)
        cv::UMat linear;
        to_linear(input, linear);

        std::cout << "\nBefore tone mapping:" << std::endl;
        print_stats("Linear RGB", linear);

        // Apply tone mapping
        cv::UMat tone_mapped;
        if (!pipe::mods::tone_map(linear, tone_mapped, white_point, contrast))
        {
            throw std::runtime_error("Tone mapping failed");
        }

        std::cout << "\nAfter tone mapping:" << std::endl;
        print_stats("Tone mapped", tone_mapped);

        // Apply gamma and save
        cv::UMat output;
        to_gamma(tone_mapped, output);

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

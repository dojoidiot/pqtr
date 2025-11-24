// test_detail.cpp
// Unit test for detail module (4 dials)
//
// Usage: test_detail <input.png> <output.png> [sharpen_amount] [sharpen_radius] [denoise_luma] [denoise_chroma]
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

// Print image statistics
void print_stats(const std::string& label, const cv::UMat& img)
{
    cv::Mat cpu;
    img.copyTo(cpu);

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
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.png> [sharpen_amount] [sharpen_radius] [denoise_luma] [denoise_chroma]" << std::endl;
        std::cerr << "  sharpen_amount: 0.0-1.0, default 0.6 (0 to 2.0x)" << std::endl;
        std::cerr << "  sharpen_radius: 0.0-1.0, default 0.4 (0.5 to 3.0 px)" << std::endl;
        std::cerr << "  denoise_luma:   0.0-1.0, default 0.3" << std::endl;
        std::cerr << "  denoise_chroma: 0.0-1.0, default 0.5" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    float sharpen_amount = (argc > 3) ? std::stof(argv[3]) : 0.6f;
    float sharpen_radius = (argc > 4) ? std::stof(argv[4]) : 0.4f;
    float denoise_luma = (argc > 5) ? std::stof(argv[5]) : 0.3f;
    float denoise_chroma = (argc > 6) ? std::stof(argv[6]) : 0.5f;

    // Convert dials to display values
    float amount_val = sharpen_amount * 2.0f;
    float radius_val = 0.5f + sharpen_radius * 2.5f;
    float luma_val = 100.0f * denoise_luma * denoise_luma;
    float chroma_val = 100.0f * denoise_chroma * denoise_chroma;

    try
    {
        std::cout << "Detail Test (4 dials)" << std::endl;
        std::cout << "  Input: " << inputPath << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Dials:" << std::endl;
        std::cout << "    sharpen_amount: " << sharpen_amount << " -> " << amount_val << "x" << std::endl;
        std::cout << "    sharpen_radius: " << sharpen_radius << " -> " << radius_val << " px" << std::endl;
        std::cout << "    denoise_luma:   " << denoise_luma << " -> " << luma_val << std::endl;
        std::cout << "    denoise_chroma: " << denoise_chroma << " -> " << chroma_val << std::endl;

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

        std::cout << "\nBefore detail:" << std::endl;
        print_stats("Linear RGB", linear);

        // Apply detail
        cv::UMat adjusted;
        if (!pipe::mods::detail(linear, adjusted, sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma))
        {
            throw std::runtime_error("Detail adjustment failed");
        }

        std::cout << "\nAfter detail:" << std::endl;
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

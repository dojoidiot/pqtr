// test_tone_map.cpp
// Unit test for tone_map module (7 dials)
//
// Usage: test_tone_map <input.png> <output.png> [contrast] [highlights] [shadows] [toe_pivot] [shoulder_pivot] [white_point] [black_point]
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
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.png> [contrast] [highlights] [shadows] [toe_pivot] [shoulder_pivot] [white_point] [black_point]" << std::endl;
        std::cerr << "  All dials: 0.0-1.0, default 0.5 (neutral)" << std::endl;
        std::cerr << "  contrast:       0.5 = 1.0 (linear)" << std::endl;
        std::cerr << "  highlights:     0.5 = 0 (no adjustment)" << std::endl;
        std::cerr << "  shadows:        0.5 = 0 (no adjustment)" << std::endl;
        std::cerr << "  toe_pivot:      0.5 = 0.3 (shadow region ends)" << std::endl;
        std::cerr << "  shoulder_pivot: 0.5 = 0.7 (highlight region begins)" << std::endl;
        std::cerr << "  white_point:    0.5 = bypass Reinhard" << std::endl;
        std::cerr << "  black_point:    0.5 = 0 (neutral)" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    float contrast = (argc > 3) ? std::stof(argv[3]) : 0.5f;
    float highlights = (argc > 4) ? std::stof(argv[4]) : 0.5f;
    float shadows = (argc > 5) ? std::stof(argv[5]) : 0.5f;
    float toe_pivot = (argc > 6) ? std::stof(argv[6]) : 0.5f;
    float shoulder_pivot = (argc > 7) ? std::stof(argv[7]) : 0.5f;
    float white_point = (argc > 8) ? std::stof(argv[8]) : 0.5f;
    float black_point = (argc > 9) ? std::stof(argv[9]) : 0.5f;

    // Convert dials to display values
    float contrast_val = 0.5f * std::exp(contrast * 1.386f);
    float highlights_val = (highlights - 0.5f) * 2.0f;
    float shadows_val = (shadows - 0.5f) * 2.0f;
    float toe_pivot_val = 0.1f + toe_pivot * 0.4f;
    float shoulder_pivot_val = 0.5f + shoulder_pivot * 0.4f;
    float white_point_val = std::exp(white_point * 2.773f);
    float black_point_val = black_point * 0.1f;

    try
    {
        std::cout << "Tone Map Test (7 dials)" << std::endl;
        std::cout << "  Input: " << inputPath << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Dials:" << std::endl;
        std::cout << "    contrast:       " << contrast << " -> " << contrast_val << std::endl;
        std::cout << "    highlights:     " << highlights << " -> " << highlights_val << std::endl;
        std::cout << "    shadows:        " << shadows << " -> " << shadows_val << std::endl;
        std::cout << "    toe_pivot:      " << toe_pivot << " -> " << toe_pivot_val << std::endl;
        std::cout << "    shoulder_pivot: " << shoulder_pivot << " -> " << shoulder_pivot_val << std::endl;
        std::cout << "    white_point:    " << white_point << " -> " << white_point_val << std::endl;
        std::cout << "    black_point:    " << black_point << " -> " << black_point_val << std::endl;

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
        if (!pipe::mods::tone_map(linear, tone_mapped, contrast, highlights, shadows, toe_pivot, shoulder_pivot, white_point, black_point))
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

// test_selective_color.cpp
// Unit test for selective_color module (24 dials)
//
// Usage: test_selective_color <input.png> <output.png> <band> <hue> <sat> <lum>
//        band: 0-7 (red, orange, yellow, green, cyan, blue, purple, magenta)
//        hue/sat/lum: 0.0-1.0, default 0.5 (neutral)

#include <iostream>
#include <string>
#include <cmath>
#include <array>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "pipe/mods/mods.h"

static const char* BAND_NAMES[] = {"red", "orange", "yellow", "green", "cyan", "blue", "purple", "magenta"};

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
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.png> [band] [hue] [sat] [lum]" << std::endl;
        std::cerr << "  band: 0-7 (red, orange, yellow, green, cyan, blue, purple, magenta)" << std::endl;
        std::cerr << "  hue/sat/lum: 0.0-1.0, default 0.5 (neutral)" << std::endl;
        std::cerr << "\nExample: test_selective_color colorchecker.png out.png 0 0.6 0.7 0.5" << std::endl;
        std::cerr << "         (shifts red hue +6°, boosts red saturation)" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];
    int band = (argc > 3) ? std::stoi(argv[3]) : 0;
    float hue = (argc > 4) ? std::stof(argv[4]) : 0.5f;
    float sat = (argc > 5) ? std::stof(argv[5]) : 0.5f;
    float lum = (argc > 6) ? std::stof(argv[6]) : 0.5f;

    if (band < 0 || band > 7) {
        std::cerr << "Error: band must be 0-7" << std::endl;
        return 1;
    }

    // Convert dials to display values
    float hue_val = (hue - 0.5f) * 60.0f;
    float sat_val = (sat - 0.5f) * 2.0f;
    float lum_val = (lum - 0.5f) * 2.0f;

    try
    {
        std::cout << "Selective Color Test (24 dials)" << std::endl;
        std::cout << "  Input: " << inputPath << std::endl;
        std::cout << "  Output: " << outputPath << std::endl;
        std::cout << "  Band: " << band << " (" << BAND_NAMES[band] << ")" << std::endl;
        std::cout << "  Adjustments:" << std::endl;
        std::cout << "    hue:        " << hue << " -> " << hue_val << "°" << std::endl;
        std::cout << "    saturation: " << sat << " -> " << sat_val << std::endl;
        std::cout << "    luminance:  " << lum << " -> " << lum_val << std::endl;

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

        std::cout << "\nBefore selective color:" << std::endl;
        print_stats("Linear RGB", linear);

        // Set up dial arrays (all neutral except target band)
        float hue_dials[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        float sat_dials[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
        float lum_dials[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

        hue_dials[band] = hue;
        sat_dials[band] = sat;
        lum_dials[band] = lum;

        // Apply selective color
        cv::UMat adjusted;
        if (!pipe::mods::selective_color(linear, adjusted, hue_dials, sat_dials, lum_dials))
        {
            throw std::runtime_error("Selective color adjustment failed");
        }

        std::cout << "\nAfter selective color:" << std::endl;
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

// gen_test_images.cpp
// Generate synthetic test images for module unit tests
//
// Each image is designed to verify specific module behavior:
// - Known input values → predictable output values
// - Mathematically verifiable results
//
// Usage: gen_test_images <output_dir>

#include <iostream>
#include <string>
#include <cmath>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

// Generate exposure test image
// Gray ramp from 0.0 to 1.0 in linear space
// After +1 EV: values should double (clamped at 1.0)
// After -1 EV: values should halve
void gen_exposure_test(const std::string& dir)
{
    const int width = 256;
    const int height = 64;

    // Create linear gray ramp (stored as gamma-encoded PNG)
    cv::Mat linear(height, width, CV_32FC3);

    for (int x = 0; x < width; x++)
    {
        float val = static_cast<float>(x) / 255.0f;  // 0.0 to 1.0
        for (int y = 0; y < height; y++)
        {
            linear.at<cv::Vec3f>(y, x) = cv::Vec3f(val, val, val);
        }
    }

    // Apply gamma for storage (test will undo this)
    cv::Mat gamma;
    cv::pow(linear, 1.0f/2.2f, gamma);
    gamma.convertTo(gamma, CV_8UC3, 255.0);

    std::string path = dir + "/exposure_ramp.png";
    cv::imwrite(path, gamma);
    std::cout << "✓ " << path << " (gray ramp 0.0-1.0 linear)" << std::endl;

    // Also create a mid-gray patch for precise testing
    cv::Mat midgray(64, 64, CV_32FC3, cv::Scalar(0.18f, 0.18f, 0.18f));  // 18% gray
    cv::pow(midgray, 1.0f/2.2f, gamma);
    gamma.convertTo(gamma, CV_8UC3, 255.0);

    path = dir + "/exposure_midgray.png";
    cv::imwrite(path, gamma);
    std::cout << "✓ " << path << " (18% gray, linear=0.18)" << std::endl;
}

// Generate white balance test image
// Neutral gray patches + primary color patches
// WB changes should shift colors predictably
void gen_white_balance_test(const std::string& dir)
{
    const int patch = 64;
    const int width = patch * 4;
    const int height = patch * 2;

    cv::Mat linear(height, width, CV_32FC3, cv::Scalar(0, 0, 0));

    // Row 1: Neutral grays (should remain neutral after WB)
    // 18% gray, 50% gray, 90% gray, white
    float grays[] = {0.18f, 0.50f, 0.90f, 1.0f};
    for (int i = 0; i < 4; i++)
    {
        cv::Rect roi(i * patch, 0, patch, patch);
        linear(roi) = cv::Scalar(grays[i], grays[i], grays[i]);
    }

    // Row 2: Primary colors (predictable shifts)
    // Red, Green, Blue, Yellow
    cv::Rect r1(0 * patch, patch, patch, patch);
    cv::Rect r2(1 * patch, patch, patch, patch);
    cv::Rect r3(2 * patch, patch, patch, patch);
    cv::Rect r4(3 * patch, patch, patch, patch);

    linear(r1) = cv::Scalar(0.0f, 0.0f, 0.5f);   // Red (BGR)
    linear(r2) = cv::Scalar(0.0f, 0.5f, 0.0f);   // Green
    linear(r3) = cv::Scalar(0.5f, 0.0f, 0.0f);   // Blue
    linear(r4) = cv::Scalar(0.0f, 0.5f, 0.5f);   // Yellow (R+G)

    // Apply gamma for storage
    cv::Mat gamma;
    cv::pow(linear, 1.0f/2.2f, gamma);
    gamma.convertTo(gamma, CV_8UC3, 255.0);

    std::string path = dir + "/white_balance_patches.png";
    cv::imwrite(path, gamma);
    std::cout << "✓ " << path << " (grays + RGB + yellow)" << std::endl;
}

// Generate tone map test image
// HDR ramp from 0.0 to 4.0 (beyond SDR range)
// Tone mapping should compress highlights while preserving shadows
void gen_tone_map_test(const std::string& dir)
{
    const int width = 256;
    const int height = 64;

    // Create HDR ramp 0.0 to 4.0 (2 stops over SDR white)
    cv::Mat hdr(height, width, CV_32FC3);

    for (int x = 0; x < width; x++)
    {
        float val = (static_cast<float>(x) / 255.0f) * 4.0f;  // 0.0 to 4.0
        for (int y = 0; y < height; y++)
        {
            hdr.at<cv::Vec3f>(y, x) = cv::Vec3f(val, val, val);
        }
    }

    // For storage, we need to represent HDR in 8-bit
    // Store as-is but clipped - the test knows true values
    // Actually, let's store the linear values scaled to fit
    cv::Mat scaled;
    hdr.convertTo(scaled, CV_32FC3, 0.25);  // Scale 0-4 to 0-1
    cv::Mat gamma;
    cv::pow(scaled, 1.0f/2.2f, gamma);
    gamma.convertTo(gamma, CV_8UC3, 255.0);

    std::string path = dir + "/tone_map_hdr_ramp.png";
    cv::imwrite(path, gamma);
    std::cout << "✓ " << path << " (HDR ramp 0.0-4.0, stored as 0.0-1.0)" << std::endl;

    // Also create bright patches at specific EV levels
    const int patch = 64;
    cv::Mat hdr_patches(patch, patch * 5, CV_32FC3);

    float evs[] = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};  // EV 0 to +4
    for (int i = 0; i < 5; i++)
    {
        float val = std::pow(2.0f, evs[i]);  // 1, 2, 4, 8, 16
        cv::Rect roi(i * patch, 0, patch, patch);
        hdr_patches(roi) = cv::Scalar(val, val, val);
    }

    // Store scaled
    hdr_patches.convertTo(scaled, CV_32FC3, 1.0/16.0);  // Scale 0-16 to 0-1
    cv::pow(scaled, 1.0f/2.2f, gamma);
    gamma.convertTo(gamma, CV_8UC3, 255.0);

    path = dir + "/tone_map_ev_patches.png";
    cv::imwrite(path, gamma);
    std::cout << "✓ " << path << " (patches at EV 0,1,2,3,4)" << std::endl;
}

// Generate ColorChecker-style reference
// 24 patches with known sRGB values (from BabelColor)
void gen_colorchecker(const std::string& dir)
{
    const int patch = 64;
    const int cols = 6;
    const int rows = 4;

    cv::Mat img(rows * patch, cols * patch, CV_8UC3);

    // ColorChecker Classic sRGB values (from BabelColor, D50 reference)
    // Values are gamma-encoded sRGB
    uint8_t cc[24][3] = {
        // Row 1: Natural colors
        {115, 82, 68},    // Dark skin
        {194, 150, 130},  // Light skin
        {98, 122, 157},   // Blue sky
        {87, 108, 67},    // Foliage
        {133, 128, 177},  // Blue flower
        {103, 189, 170},  // Bluish green

        // Row 2: Miscellaneous
        {214, 126, 44},   // Orange
        {80, 91, 166},    // Purplish blue
        {193, 90, 99},    // Moderate red
        {94, 60, 108},    // Purple
        {157, 188, 64},   // Yellow green
        {224, 163, 46},   // Orange yellow

        // Row 3: Primary/secondary
        {56, 61, 150},    // Blue
        {70, 148, 73},    // Green
        {175, 54, 60},    // Red
        {231, 199, 31},   // Yellow
        {187, 86, 149},   // Magenta
        {8, 133, 161},    // Cyan

        // Row 4: Grayscale
        {243, 243, 242},  // White
        {200, 200, 200},  // Neutral 8
        {160, 160, 160},  // Neutral 6.5
        {122, 122, 121},  // Neutral 5
        {85, 85, 85},     // Neutral 3.5
        {52, 52, 52}      // Black
    };

    for (int i = 0; i < 24; i++)
    {
        int row = i / cols;
        int col = i % cols;
        cv::Rect roi(col * patch, row * patch, patch, patch);
        // OpenCV is BGR
        img(roi) = cv::Scalar(cc[i][2], cc[i][1], cc[i][0]);
    }

    std::string path = dir + "/colorchecker_srgb.png";
    cv::imwrite(path, img);
    std::cout << "✓ " << path << " (24-patch ColorChecker)" << std::endl;
}

// Generate gradient test for banding detection
void gen_gradient_test(const std::string& dir)
{
    const int width = 512;
    const int height = 64;

    // Smooth gradient that would show banding if quantized
    cv::Mat linear(height, width, CV_32FC3);

    for (int x = 0; x < width; x++)
    {
        // Very subtle gradient in shadow region (most prone to banding)
        float val = 0.01f + (static_cast<float>(x) / width) * 0.09f;  // 0.01 to 0.10
        for (int y = 0; y < height; y++)
        {
            linear.at<cv::Vec3f>(y, x) = cv::Vec3f(val, val, val);
        }
    }

    cv::Mat gamma;
    cv::pow(linear, 1.0f/2.2f, gamma);
    gamma.convertTo(gamma, CV_8UC3, 255.0);

    std::string path = dir + "/gradient_shadow.png";
    cv::imwrite(path, gamma);
    std::cout << "✓ " << path << " (subtle shadow gradient for banding test)" << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <output_dir>" << std::endl;
        return 1;
    }

    std::string dir = argv[1];

    std::cout << "Generating synthetic test images..." << std::endl;
    std::cout << "Output: " << dir << std::endl;
    std::cout << std::endl;

    gen_exposure_test(dir);
    gen_white_balance_test(dir);
    gen_tone_map_test(dir);
    gen_colorchecker(dir);
    gen_gradient_test(dir);

    std::cout << std::endl;
    std::cout << "Done." << std::endl;

    return 0;
}

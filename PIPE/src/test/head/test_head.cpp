// test_head.cpp
// Test RAWS decode → HEAD → display (no BODY processing)
//
// Usage: test_head <source.ARW> <output.png>
//
// Outputs scene-linear data from RAWS with only sigmoid+gamma for display.
// Use this to verify RAWS output before any PIPE processing.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

// Sigmoid from mods (scene→display compression)
namespace pipe::mods {
    bool sigmoid_default(const cv::UMat& in, cv::UMat& out);
}

// sRGB gamma encoding
static void applyGamma(const cv::UMat& linear, cv::UMat& gamma)
{
    cv::UMat clamped;
    cv::max(linear, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);

    cv::UMat lowMask, highMask;
    cv::compare(clamped, 0.0031308f, lowMask, cv::CMP_LE);
    cv::compare(clamped, 0.0031308f, highMask, cv::CMP_GT);

    cv::UMat lowPart, highPart;
    cv::multiply(clamped, 12.92f, lowPart);

    cv::UMat temp;
    cv::pow(clamped, 1.0f / 2.4f, temp);
    cv::multiply(temp, 1.055f, temp);
    cv::subtract(temp, 0.055f, highPart);

    gamma.create(linear.size(), linear.type());
    lowPart.copyTo(gamma, lowMask);
    highPart.copyTo(gamma, highMask);
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cerr << "Usage: " << argv[0] << " <source.ARW> <output.png>\n";
        std::cerr << "\nTests RAWS decode path. Output is scene-linear + sigmoid + gamma.\n";
        return 1;
    }

    std::string sourcePath = argv[1];
    std::string outputPath = argv[2];

    try
    {
        std::cout << "=== HEAD TEST ===" << std::endl;
        std::cout << "Source: " << sourcePath << std::endl;

        // Create pipe and decode
        pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
        pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(sourcePath));

        std::cout << "Decoding..." << std::endl;
        pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
        if (!head)
        {
            throw std::runtime_error("Failed to decode: " + sourcePath);
        }

        pipe::Info info = head->data().info();
        std::cout << "Size: " << info["width"] << "x" << info["height"] << std::endl;
        std::cout << "Camera: " << info["camera_model"] << std::endl;

        // Get scene-linear data from HEAD (this is RAWS output)
        cv::UMat linear = head->data().view();
        std::cout << "Scene-linear: " << linear.cols << "x" << linear.rows
                  << " type=" << linear.type() << std::endl;

        // Apply sigmoid (scene→display)
        cv::UMat tonemapped;
        pipe::mods::sigmoid_default(linear, tonemapped);
        std::cout << "Sigmoid applied" << std::endl;

        // Apply sRGB gamma
        cv::UMat gamma;
        applyGamma(tonemapped, gamma);
        std::cout << "Gamma applied" << std::endl;

        // Convert to 8-bit
        cv::Mat out8;
        gamma.convertTo(out8, CV_8UC3, 255.0);

        // Convert RGB to BGR for OpenCV imwrite
        cv::cvtColor(out8, out8, cv::COLOR_RGB2BGR);

        // Save
        if (!cv::imwrite(outputPath, out8))
        {
            throw std::runtime_error("Failed to save: " + outputPath);
        }

        std::cout << "Output: " << outputPath << std::endl;
        std::cout << "[OK]" << std::endl;
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

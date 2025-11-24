// gamma_oetf.cpp
// Gamma OETF Module - Gold
// Applies sRGB gamma curve

#include <opencv2/core.hpp>
#include <iostream>
#include <cmath>

namespace sony
{
    namespace gamma_oetf
    {

        bool process(
            const cv::UMat &input,
            cv::UMat &output)
        {
            if (input.empty())
            {
                std::cerr << "[GammaOETF] Error: Input image is empty\n";
                return false;
            }

            if (input.type() != CV_32FC3)
            {
                std::cerr << "[GammaOETF] Error: Input must be CV_32FC3\n";
                return false;
            }

            try
            {
                // Clip and clamp exactly like old gamma module
                cv::UMat clipped;
                cv::min(input, 1.0f, clipped);
                cv::max(clipped, 0.0f, clipped);

                // Download to CPU for sRGB curve
                cv::Mat input_cpu = clipped.getMat(cv::ACCESS_READ);
                cv::Mat output_cpu(input_cpu.size(), CV_32FC3);

                const float threshold = 0.0031308f;
                const float a = 12.92f;
                const float b = 1.055f;
                const float c = 0.055f;
                const float gamma_inv = 1.0f / 2.4f;

                // Apply sRGB curve pixel by pixel (matches old implementation)
                for (int y = 0; y < input_cpu.rows; y++)
                {
                    const float *in_row = input_cpu.ptr<float>(y);
                    float *out_row = output_cpu.ptr<float>(y);

                    for (int x = 0; x < input_cpu.cols * 3; x++)
                    {
                        float val = in_row[x];
                        if (val <= threshold)
                        {
                            out_row[x] = val * a;
                        }
                        else
                        {
                            out_row[x] = b * std::pow(val, gamma_inv) - c;
                        }
                    }
                }

                output_cpu.copyTo(output);
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[GammaOETF] Error: " << e.what() << "\n";
                return false;
            }
        }

    } // namespace gamma_oetf
} // namespace sony

// view.cpp
// Display conversion utilities (linear → sRGB)

#include "view.hpp"
#include <opencv2/imgproc.hpp>

namespace pipe::internal
{

    bool applyGamma(const View& linear, View& gamma)
    {
        cv::UMat clamped;
        cv::max(linear, 0.0f, clamped);

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

        return true;
    }

    View toDisplayView(const View& linear, int max_dim)
    {
        View scaled = linear;

        if (max_dim > 0)
        {
            float scale = (float)max_dim / std::max(linear.cols, linear.rows);
            if (scale < 1.0f)
            {
                View small;
                cv::resize(linear, small, cv::Size(), scale, scale, cv::INTER_AREA);
                scaled = small;
            }
        }

        cv::UMat gamma;
        applyGamma(scaled, gamma);

        cv::UMat out8;
        gamma.convertTo(out8, CV_8UC3, 255.0);

        return out8;
    }

} // namespace pipe::internal

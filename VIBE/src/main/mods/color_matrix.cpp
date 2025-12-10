// color_matrix.cpp - VIBE
// Color Matrix Module - Applies camera RGB → sRGB color matrix

#include "mods.h"
#include <iostream>

namespace vibe
{
namespace mods
{

bool color_matrix(const View& in, View& out, const cv::Matx33f& matrix)
{
    if (in.empty() || in.type() != CV_32FC3)
    {
        std::cerr << "[vibe::color_matrix] invalid input\n";
        return false;
    }

    cv::Mat matMat(matrix);
    View transformed;
    cv::transform(in, transformed, matMat);

    cv::max(transformed, 0.0f, out);
    cv::min(out, 1.0f, out);

    return true;
}

} // namespace mods
} // namespace vibe

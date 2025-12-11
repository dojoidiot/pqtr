// compare.cpp - Compare two images feature-wise
// Usage: compare <image1.png> <image2.png>

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "diff.hpp"

using namespace geos::internal;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <image1.png> <image2.png>" << std::endl;
        return 1;
    }

    cv::Mat img1 = cv::imread(argv[1]);
    cv::Mat img2 = cv::imread(argv[2]);

    if (img1.empty() || img2.empty()) {
        std::cerr << "Failed to load images" << std::endl;
        return 1;
    }

    cv::UMat u1, u2;
    img1.copyTo(u1);
    img2.copyTo(u2);

    StyleFeatures f1 = extractStyleFromBGR(u1);
    StyleFeatures f2 = extractStyleFromBGR(u2);

    std::cout << "Comparing: " << argv[1] << " vs " << argv[2] << std::endl;
    std::cout << std::endl;

    printFeatureAnalysis(f1, f2);

    return 0;
}

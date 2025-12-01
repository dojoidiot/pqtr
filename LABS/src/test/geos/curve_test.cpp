// curve_test.cpp - Test base curve effect
// Outputs images with and without base curve

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <image.ARW> [output_dir]" << std::endl;
        return 1;
    }

    std::string arwPath = argv[1];
    std::string outDir = (argc > 2) ? argv[2] : "tmp";

    std::cout << "Testing base curve on: " << arwPath << std::endl;

    // Open RAW
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(arwPath));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
    if (!head) {
        std::cerr << "Failed to open " << arwPath << std::endl;
        return 1;
    }

    std::cout << "Has base curve: " << (head->hasBaseCurve() ? "YES" : "NO") << std::endl;

    if (head->hasBaseCurve()) {
        const float* curve = head->baseCurve();

        // Print curve statistics for each channel
        for (int c = 0; c < 3; c++) {
            const char* names[] = {"Blue", "Green", "Red"};
            float minV = 1.0f, maxV = 0.0f, midV = curve[c * 256 + 128];

            for (int i = 0; i < 256; i++) {
                float v = curve[c * 256 + i];
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
            }

            std::cout << names[c] << " curve: min=" << minV
                      << " mid=" << midV << " max=" << maxV << std::endl;

            // Print a few sample points
            std::cout << "  [0]=" << curve[c * 256 + 0]
                      << " [64]=" << curve[c * 256 + 64]
                      << " [128]=" << curve[c * 256 + 128]
                      << " [192]=" << curve[c * 256 + 192]
                      << " [255]=" << curve[c * 256 + 255] << std::endl;
        }
    }

    // Create body WITH base curve
    int workingSize = 1080;
    pipe::Body& body1 = head->body(workingSize);
    pipe::Body::Link& link1 = body1.add("with_curve");

    if (head->hasBaseCurve()) {
        link1.baseCurve().setCurve(head->baseCurve());
        std::cout << "Applied base curve to link" << std::endl;
    }

    cv::UMat withCurve = body1.view();
    cv::Mat withCurveMat;
    withCurve.copyTo(withCurveMat);

    std::string withPath = outDir + "/with_curve.png";
    cv::imwrite(withPath, withCurveMat);
    std::cout << "Saved: " << withPath << std::endl;

    // Create another body WITHOUT base curve
    pqtr::Hold<pipe::Pipe> pipeline2 = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink2(pqtr::Tool::read(arwPath));
    pqtr::Hold<pipe::Head> head2 = pipeline2->open(std::move(rawSink2));

    pipe::Body& body2 = head2->body(workingSize);
    pipe::Body::Link& link2 = body2.add("no_curve");
    // Don't apply base curve

    cv::UMat noCurve = body2.view();
    cv::Mat noCurveMat;
    noCurve.copyTo(noCurveMat);

    std::string noPath = outDir + "/no_curve.png";
    cv::imwrite(noPath, noCurveMat);
    std::cout << "Saved: " << noPath << std::endl;

    // Save target for comparison
    cv::UMat targetView = head->view().view();
    cv::Mat targetMat;
    targetView.copyTo(targetMat);
    int targetH = workingSize * targetMat.rows / targetMat.cols;
    cv::resize(targetMat, targetMat, cv::Size(workingSize, targetH), 0, 0, cv::INTER_AREA);

    std::string targetPath = outDir + "/target.png";
    cv::imwrite(targetPath, targetMat);
    std::cout << "Saved: " << targetPath << std::endl;

    return 0;
}

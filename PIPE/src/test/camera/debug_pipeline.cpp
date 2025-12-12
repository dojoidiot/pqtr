// debug_pipeline.cpp
// Debug what each stage of the pipeline produces
// Save images at each step to visually diagnose

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Debug Pipeline ===" << std::endl;

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    cv::Mat camera_jpeg;
    head->view().view().copyTo(camera_jpeg);

    std::cout << "Scene linear: " << scene_linear.cols << "x" << scene_linear.rows
              << " type=" << scene_linear.type() << std::endl;
    std::cout << "Camera JPEG: " << camera_jpeg.cols << "x" << camera_jpeg.rows
              << " type=" << camera_jpeg.type() << std::endl;

    // Check scene_linear statistics
    cv::Scalar mean_linear = cv::mean(scene_linear);
    std::cout << "\nScene linear mean (BGR): B=" << mean_linear[0]
              << " G=" << mean_linear[1] << " R=" << mean_linear[2] << std::endl;

    double minVal, maxVal;
    cv::minMaxLoc(scene_linear.reshape(1), &minVal, &maxVal);
    std::cout << "Scene linear range: [" << minVal << ", " << maxVal << "]" << std::endl;

    // Resize to preview size
    cv::Mat scene_resized;
    cv::resize(scene_linear, scene_resized, camera_jpeg.size(), 0, 0, cv::INTER_AREA);

    // Step 1: Just clamp to [0,1]
    cv::Mat clamped;
    cv::max(scene_resized, 0.0f, clamped);
    cv::min(clamped, 1.0f, clamped);

    cv::Scalar mean_clamped = cv::mean(clamped);
    std::cout << "\nClamped mean (BGR): B=" << mean_clamped[0]
              << " G=" << mean_clamped[1] << " R=" << mean_clamped[2] << std::endl;

    // Step 2: Apply gamma 2.2
    cv::Mat gamma;
    cv::pow(clamped, 1.0f / 2.2f, gamma);

    cv::Mat gamma_8u;
    gamma.convertTo(gamma_8u, CV_8UC3, 255.0);
    cv::imwrite("tmp/var/tune/debug_gamma.png", gamma_8u);
    std::cout << "Saved: tmp/var/tune/debug_gamma.png" << std::endl;

    // Compare channel means after gamma
    cv::Scalar mean_gamma = cv::mean(gamma);
    cv::Mat camera_f;
    camera_jpeg.convertTo(camera_f, CV_32FC3, 1.0f/255.0f);
    cv::Scalar mean_camera = cv::mean(camera_f);

    std::cout << "\nAfter gamma:" << std::endl;
    std::cout << "  Our:    B=" << mean_gamma[0] << " G=" << mean_gamma[1] << " R=" << mean_gamma[2] << std::endl;
    std::cout << "  Camera: B=" << mean_camera[0] << " G=" << mean_camera[1] << " R=" << mean_camera[2] << std::endl;
    std::cout << "  Ratio:  B=" << (mean_camera[0]/mean_gamma[0])
              << " G=" << (mean_camera[1]/mean_gamma[1])
              << " R=" << (mean_camera[2]/mean_gamma[2]) << std::endl;

    // Save side-by-side comparison
    cv::Mat comparison;
    cv::hconcat(camera_jpeg, gamma_8u, comparison);
    cv::imwrite("tmp/var/tune/debug_compare.png", comparison);
    std::cout << "\nSaved: tmp/var/tune/debug_compare.png (left: camera, right: gamma 2.2)" << std::endl;

    // Create error heatmap
    cv::Mat diff;
    cv::absdiff(gamma, camera_f, diff);
    cv::Mat diff_gray;
    cv::cvtColor(diff, diff_gray, cv::COLOR_BGR2GRAY);
    diff_gray = diff_gray * 5.0f; // Amplify for visibility

    cv::Mat heatmap;
    diff_gray.convertTo(heatmap, CV_8UC1, 255.0);
    cv::applyColorMap(heatmap, heatmap, cv::COLORMAP_JET);
    cv::imwrite("tmp/var/tune/debug_error.png", heatmap);
    std::cout << "Saved: tmp/var/tune/debug_error.png (error heatmap)" << std::endl;

    return 0;
}

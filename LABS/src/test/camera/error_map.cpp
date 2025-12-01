// error_map.cpp
// Visualize WHERE the camera match error is located
// This helps identify if errors are:
// - Uniform (global transform issue)
// - In shadows (DRO lifting)
// - In specific colors (hue-dependent)
// - At edges (sharpening/alignment)

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Error Map Analysis ===" << std::endl;

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    // Get camera JPEG (target)
    cv::Mat target;
    head->view().view().copyTo(target);

    // Get our output with neutral dials + base curve
    pipe::Body& body = head->body(target.cols);  // Match preview size
    pipe::Body::Link& link = body.add("test");

    if (head->hasBaseCurve()) {
        link.baseCurve().setCurve(head->baseCurve());
    }

    cv::UMat output_umat = body.view();
    cv::Mat output;
    output_umat.copyTo(output);

    std::cout << "Target: " << target.cols << "x" << target.rows << std::endl;
    std::cout << "Output: " << output.cols << "x" << output.rows << std::endl;

    // Resize to match if needed
    if (target.size() != output.size()) {
        cv::resize(target, target, output.size());
    }

    // Convert both to float for accurate diff
    cv::Mat target_f, output_f;
    target.convertTo(target_f, CV_32FC3, 1.0/255.0);
    output.convertTo(output_f, CV_32FC3, 1.0/255.0);

    // Compute per-pixel error
    cv::Mat diff;
    cv::absdiff(output_f, target_f, diff);

    // Convert to grayscale error magnitude
    cv::Mat error_mag(diff.size(), CV_32FC1);
    for (int y = 0; y < diff.rows; y++) {
        const float* d_ptr = diff.ptr<float>(y);
        float* e_ptr = error_mag.ptr<float>(y);
        for (int x = 0; x < diff.cols; x++) {
            float b = d_ptr[x*3 + 0];
            float g = d_ptr[x*3 + 1];
            float r = d_ptr[x*3 + 2];
            e_ptr[x] = (b + g + r) / 3.0f;
        }
    }

    // Statistics
    double min_err, max_err;
    cv::minMaxLoc(error_mag, &min_err, &max_err);
    cv::Scalar mean_err = cv::mean(error_mag);

    std::cout << "\nError statistics:" << std::endl;
    std::cout << "  Min: " << (min_err * 100) << "%" << std::endl;
    std::cout << "  Max: " << (max_err * 100) << "%" << std::endl;
    std::cout << "  Mean: " << (mean_err[0] * 100) << "%" << std::endl;

    // Create heat map (scale to visible range)
    cv::Mat error_vis;
    error_mag.convertTo(error_vis, CV_8UC1, 255.0 * 5.0);  // 5x amplification
    cv::applyColorMap(error_vis, error_vis, cv::COLORMAP_JET);

    // Create side-by-side: target | output | error map
    cv::Mat target_8u, output_8u;
    target.convertTo(target_8u, CV_8UC3);
    output.convertTo(output_8u, CV_8UC3);

    cv::Mat row1, row2;
    cv::hconcat(target_8u, output_8u, row1);

    // Pad error_vis to match width
    cv::Mat error_padded;
    cv::copyMakeBorder(error_vis, error_padded, 0, 0, target_8u.cols / 2, target_8u.cols / 2,
                       cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    cv::resize(error_padded, error_padded, cv::Size(row1.cols, row1.rows / 2));

    cv::Mat combined;
    cv::vconcat(row1, error_padded, combined);

    cv::imwrite("tmp/var/tune/error_map.png", combined);
    std::cout << "\nSaved: tmp/var/tune/error_map.png" << std::endl;
    std::cout << "  Top left: Camera JPEG (target)" << std::endl;
    std::cout << "  Top right: Our output" << std::endl;
    std::cout << "  Bottom: Error heat map (blue=low, red=high, 5x amplified)" << std::endl;

    // Also save raw diff (amplified)
    cv::Mat diff_vis;
    diff.convertTo(diff_vis, CV_8UC3, 255.0 * 5.0);
    cv::imwrite("tmp/var/tune/error_diff.png", diff_vis);
    std::cout << "Saved: tmp/var/tune/error_diff.png (color diff, 5x amplified)" << std::endl;

    // Analyze error distribution by region
    std::cout << "\n--- Regional Analysis ---" << std::endl;

    // Divide into 4x4 grid
    int grid = 4;
    int cell_h = error_mag.rows / grid;
    int cell_w = error_mag.cols / grid;

    for (int gy = 0; gy < grid; gy++) {
        for (int gx = 0; gx < grid; gx++) {
            cv::Rect roi(gx * cell_w, gy * cell_h, cell_w, cell_h);
            cv::Mat cell = error_mag(roi);
            cv::Scalar cell_mean = cv::mean(cell);
            std::cout << "  [" << gy << "," << gx << "]: " << (cell_mean[0] * 100) << "%";
        }
        std::cout << std::endl;
    }

    return 0;
}

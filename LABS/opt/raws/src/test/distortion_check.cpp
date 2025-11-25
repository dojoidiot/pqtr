// distortion_check.cpp
// Compare distortion between processed RAW and camera preview
// Uses cross-correlation at grid intersections to measure pixel shift

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <cmath>

// Compute normalized cross-correlation to find best match offset
// Returns (dx, dy) shift and correlation score
void find_shift(const cv::Mat& img1, const cv::Mat& img2,
                int cx, int cy, int patch_size, int search_radius,
                int& best_dx, int& best_dy, double& best_score)
{
    int half = patch_size / 2;

    // Extract grayscale patch from img1 (reference)
    cv::Rect roi1(cx - half, cy - half, patch_size, patch_size);
    if (roi1.x < 0 || roi1.y < 0 ||
        roi1.x + roi1.width > img1.cols || roi1.y + roi1.height > img1.rows) {
        best_dx = best_dy = 0;
        best_score = -1;
        return;
    }

    cv::Mat patch1;
    cv::cvtColor(img1(roi1), patch1, cv::COLOR_BGR2GRAY);
    patch1.convertTo(patch1, CV_32F);

    // Normalize patch
    cv::Scalar mean1, std1;
    cv::meanStdDev(patch1, mean1, std1);
    if (std1[0] < 1.0) {
        best_dx = best_dy = 0;
        best_score = -1;
        return;
    }
    patch1 = (patch1 - mean1[0]) / std1[0];

    best_score = -2;
    best_dx = 0;
    best_dy = 0;

    // Search in neighborhood
    for (int dy = -search_radius; dy <= search_radius; dy++) {
        for (int dx = -search_radius; dx <= search_radius; dx++) {
            int x2 = cx + dx;
            int y2 = cy + dy;

            cv::Rect roi2(x2 - half, y2 - half, patch_size, patch_size);
            if (roi2.x < 0 || roi2.y < 0 ||
                roi2.x + roi2.width > img2.cols || roi2.y + roi2.height > img2.rows) {
                continue;
            }

            cv::Mat patch2;
            cv::cvtColor(img2(roi2), patch2, cv::COLOR_BGR2GRAY);
            patch2.convertTo(patch2, CV_32F);

            cv::Scalar mean2, std2;
            cv::meanStdDev(patch2, mean2, std2);
            if (std2[0] < 1.0) continue;
            patch2 = (patch2 - mean2[0]) / std2[0];

            // Normalized cross-correlation
            double ncc = patch1.dot(patch2) / (patch_size * patch_size);

            if (ncc > best_score) {
                best_score = ncc;
                best_dx = dx;
                best_dy = dy;
            }
        }
    }
}

int main(int argc, char** argv)
{
    std::string raw_file = "./tmp/sony.png";
    std::string preview_file = "./tmp/sony_preview.png";

    if (argc > 2) {
        raw_file = argv[1];
        preview_file = argv[2];
    }

    std::cout << "\n=== DISTORTION CHECK ===\n" << std::endl;
    std::cout << "RAW processed: " << raw_file << std::endl;
    std::cout << "Camera preview: " << preview_file << std::endl;

    cv::Mat raw = cv::imread(raw_file);
    cv::Mat preview = cv::imread(preview_file);

    if (raw.empty() || preview.empty()) {
        std::cerr << "Error: Could not load images" << std::endl;
        return 1;
    }

    std::cout << "RAW size: " << raw.cols << "x" << raw.rows << std::endl;
    std::cout << "Preview size: " << preview.cols << "x" << preview.rows << std::endl;

    // Scale preview to match RAW size
    cv::Mat preview_scaled;
    cv::resize(preview, preview_scaled, raw.size(), 0, 0, cv::INTER_LINEAR);

    // Grid parameters
    int grid_spacing = 500;
    int patch_size = 64;
    int search_radius = 50;

    std::cout << "\nGrid spacing: " << grid_spacing << "px" << std::endl;
    std::cout << "Patch size: " << patch_size << "px" << std::endl;
    std::cout << "Search radius: " << search_radius << "px" << std::endl;
    std::cout << std::endl;

    // Check each grid intersection
    std::cout << "Grid intersection shifts (RAW vs Preview):" << std::endl;
    std::cout << "  X    Y   |  dx    dy  | score | dist" << std::endl;
    std::cout << "-----------|------------|-------|------" << std::endl;

    double total_dist = 0;
    int count = 0;

    for (int gy = 1; gy * grid_spacing < raw.rows; gy++) {
        for (int gx = 1; gx * grid_spacing < raw.cols; gx++) {
            int cx = gx * grid_spacing;
            int cy = gy * grid_spacing;

            int dx, dy;
            double score;
            find_shift(raw, preview_scaled, cx, cy, patch_size, search_radius, dx, dy, score);

            if (score > 0.5) {  // Only report good matches
                double dist = std::sqrt(dx*dx + dy*dy);
                total_dist += dist;
                count++;

                printf(" %4d %4d | %4d %4d | %5.3f | %5.1f\n",
                       cx, cy, dx, dy, score, dist);
            }
        }
    }

    std::cout << std::endl;
    if (count > 0) {
        std::cout << "Average shift: " << (total_dist / count) << " pixels" << std::endl;
        std::cout << "Points measured: " << count << std::endl;
    } else {
        std::cout << "No valid matches found" << std::endl;
    }

    // Create visualization
    cv::Mat viz;
    cv::addWeighted(raw, 0.5, preview_scaled, 0.5, 0, viz);

    // Draw shift vectors at grid points
    for (int gy = 1; gy * grid_spacing < raw.rows; gy++) {
        for (int gx = 1; gx * grid_spacing < raw.cols; gx++) {
            int cx = gx * grid_spacing;
            int cy = gy * grid_spacing;

            int dx, dy;
            double score;
            find_shift(raw, preview_scaled, cx, cy, patch_size, search_radius, dx, dy, score);

            if (score > 0.5) {
                // Draw shift vector (magnified 5x for visibility)
                cv::circle(viz, cv::Point(cx, cy), 5, cv::Scalar(0, 255, 0), -1);
                cv::line(viz, cv::Point(cx, cy),
                         cv::Point(cx + dx * 5, cy + dy * 5),
                         cv::Scalar(0, 0, 255), 2);
            }
        }
    }

    std::string viz_file = "./tmp/distortion_check.png";
    cv::imwrite(viz_file, viz);
    std::cout << "\nVisualization saved to: " << viz_file << std::endl;

    return 0;
}

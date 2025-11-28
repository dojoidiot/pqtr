// Batch test: tune each ARW to its JPEG using the tune binary
// Each RAW gets its own folder in tmp/var/pics/<name>/
// Then creates a comparison grid from all tail.png vs tune.jpg
//
// Output structure per image:
//   tmp/var/pics/<name>/
//     - tune.jpg   (camera JPEG - target)
//     - head.png   (target at working size)
//     - body.png   (baseline)
//     - tune.json  (optimized link)
//     - tail.png   (optimized output)
//     - diff.png   (visual difference x5)
//
// Final output: tmp/var/pics/compare.png (grid of all comparisons)

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <filesystem>
#include <iostream>
#include <vector>
#include <iomanip>
#include <cstdlib>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
    std::string picsDir = "var/pics";
    std::string outDir = "tmp/var/pics";
    int thumbSize = 360;  // Size for comparison thumbnails

    if (argc > 1) picsDir = argv[1];
    if (argc > 2) outDir = argv[2];

    // Create output directory
    fs::create_directories(outDir);

    // Find all ARW files
    std::vector<std::string> rawFiles;
    for (const auto& entry : fs::directory_iterator(picsDir))
    {
        if (entry.path().extension() == ".ARW")
            rawFiles.push_back(entry.path().string());
    }
    std::sort(rawFiles.begin(), rawFiles.end());

    if (rawFiles.empty())
    {
        std::cerr << "No ARW files found in " << picsDir << std::endl;
        return 1;
    }

    std::cout << "=== Batch Test ===" << std::endl;
    std::cout << "Found " << rawFiles.size() << " RAW files" << std::endl;
    std::cout << "Tuning each ARW to match its JPEG..." << std::endl;
    std::cout << std::endl;

    std::vector<std::string> processedDirs;

    for (size_t idx = 0; idx < rawFiles.size(); idx++)
    {
        const std::string& rawFile = rawFiles[idx];
        std::string baseName = fs::path(rawFile).stem().string();
        std::string jpgFile = fs::path(rawFile).replace_extension(".JPG").string();

        if (!fs::exists(jpgFile))
        {
            std::cout << "[SKIP] " << baseName << " - no matching JPEG" << std::endl;
            continue;
        }

        // Create directory for this image
        std::string imageDir = outDir + "/" + baseName;
        fs::create_directories(imageDir);

        std::cout << "[" << (idx + 1) << "/" << rawFiles.size() << "] " << baseName << std::endl;

        // Copy source RAW to image directory
        fs::copy_file(rawFile, imageDir + "/head.ARW", fs::copy_options::overwrite_existing);

        // Build tune command
        std::string cmd = "LD_LIBRARY_PATH=lib/opencv/build/lib bin/tune "
                          + rawFile + " " + jpgFile
                          + " --save-area " + imageDir
                          + " --fine --fine-area " + imageDir;

        // Run tune
        int ret = std::system(cmd.c_str());
        if (ret != 0)
        {
            std::cerr << "  [ERROR] tune failed with code " << ret << std::endl;
            continue;
        }

        processedDirs.push_back(imageDir);
        std::cout << std::endl;
    }

    // Create comparison grid from all processed images
    std::cout << "[GRID] Creating comparison..." << std::endl;

    std::vector<cv::Mat> comparisons;

    for (const auto& dir : processedDirs)
    {
        std::string baseName = fs::path(dir).filename().string();
        std::string tuneJpg = dir + "/tune.jpg";
        std::string tailPng = dir + "/tail.png";

        if (!fs::exists(tuneJpg) || !fs::exists(tailPng))
        {
            std::cerr << "  [SKIP] " << baseName << " - missing files" << std::endl;
            continue;
        }

        cv::Mat jpeg = cv::imread(tuneJpg);
        cv::Mat tail = cv::imread(tailPng);

        if (jpeg.empty() || tail.empty())
        {
            std::cerr << "  [SKIP] " << baseName << " - failed to load" << std::endl;
            continue;
        }

        // Resize both to thumbnail size maintaining aspect
        double scaleJpeg = std::min((double)thumbSize / jpeg.cols, (double)thumbSize / jpeg.rows);
        double scaleTail = std::min((double)thumbSize / tail.cols, (double)thumbSize / tail.rows);

        cv::Mat jpegThumb, tailThumb;
        cv::resize(jpeg, jpegThumb, cv::Size(), scaleJpeg, scaleJpeg, cv::INTER_AREA);
        cv::resize(tail, tailThumb, cv::Size(), scaleTail, scaleTail, cv::INTER_AREA);

        // Use the larger dimensions for the cell
        int cellW = std::max(jpegThumb.cols, tailThumb.cols);
        int cellH = std::max(jpegThumb.rows, tailThumb.rows);

        // Create side-by-side (JPEG left, TAIL right)
        cv::Mat sideBySide(cellH, cellW * 2, CV_8UC3, cv::Scalar(32, 32, 32));

        // Center each thumbnail in its cell
        int jpegX = (cellW - jpegThumb.cols) / 2;
        int jpegY = (cellH - jpegThumb.rows) / 2;
        int tailX = cellW + (cellW - tailThumb.cols) / 2;
        int tailY = (cellH - tailThumb.rows) / 2;

        jpegThumb.copyTo(sideBySide(cv::Rect(jpegX, jpegY, jpegThumb.cols, jpegThumb.rows)));
        tailThumb.copyTo(sideBySide(cv::Rect(tailX, tailY, tailThumb.cols, tailThumb.rows)));

        // Add labels
        cv::putText(sideBySide, "JPEG", cv::Point(10, 25),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        cv::putText(sideBySide, "TAIL", cv::Point(cellW + 10, 25),
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 255), 2);
        cv::putText(sideBySide, baseName, cv::Point(10, cellH - 10),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200), 1);

        comparisons.push_back(sideBySide);
    }

    if (comparisons.empty())
    {
        std::cerr << "No comparisons to create" << std::endl;
        return 1;
    }

    // Build grid
    int cols = 2;  // 2 comparisons per row
    int rows = (comparisons.size() + cols - 1) / cols;

    int cellWidth = comparisons[0].cols;
    int cellHeight = comparisons[0].rows;

    cv::Mat grid(rows * cellHeight, cols * cellWidth, CV_8UC3, cv::Scalar(32, 32, 32));

    for (size_t i = 0; i < comparisons.size(); i++)
    {
        int row = i / cols;
        int col = i % cols;
        cv::Rect roi(col * cellWidth, row * cellHeight, cellWidth, cellHeight);
        comparisons[i].copyTo(grid(roi));
    }

    std::string compareFile = outDir + "/compare.png";
    cv::imwrite(compareFile, grid);
    std::cout << "  Saved: " << compareFile << std::endl;

    std::cout << std::endl;
    std::cout << "[OK] Done - " << comparisons.size() << " images tuned and compared" << std::endl;

    return 0;
}

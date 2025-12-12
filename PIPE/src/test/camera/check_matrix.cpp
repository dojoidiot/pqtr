// check_matrix.cpp
// Debug what color matrix we're actually using

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

    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    // Get the scene-linear output
    cv::Mat scene_linear;
    head->data().view().copyTo(scene_linear);

    // Sample some pixels to understand the color transform
    std::cout << "=== Scene Linear Sample (center pixel) ===" << std::endl;
    int cy = scene_linear.rows / 2;
    int cx = scene_linear.cols / 2;

    const float* p = scene_linear.ptr<float>(cy) + cx * 3;
    std::cout << "Center pixel BGR: " << p[0] << ", " << p[1] << ", " << p[2] << std::endl;

    // The matrix from exiftool: 1177 -211 91 -54 1267 -159 72 -232 1216
    // Divided by 1024:
    float m_exif[9] = {
        1177.0f/1024.0f, -211.0f/1024.0f, 91.0f/1024.0f,
        -54.0f/1024.0f, 1267.0f/1024.0f, -159.0f/1024.0f,
        72.0f/1024.0f, -232.0f/1024.0f, 1216.0f/1024.0f
    };

    // The fallback matrix from doc (0x7310):
    // 1344 -211 -76 / -9 1224 -159 / 7 -41 1090
    float m_fallback[9] = {
        1344.0f/1024.0f, -211.0f/1024.0f, -76.0f/1024.0f,
        -9.0f/1024.0f, 1224.0f/1024.0f, -159.0f/1024.0f,
        7.0f/1024.0f, -41.0f/1024.0f, 1090.0f/1024.0f
    };

    std::cout << "\n=== Color Matrices ===" << std::endl;
    std::cout << "Matrix from exiftool (0x7800):" << std::endl;
    printf("  [%.4f  %.4f  %.4f]\n", m_exif[0], m_exif[1], m_exif[2]);
    printf("  [%.4f  %.4f  %.4f]\n", m_exif[3], m_exif[4], m_exif[5]);
    printf("  [%.4f  %.4f  %.4f]\n", m_exif[6], m_exif[7], m_exif[8]);

    std::cout << "\nFallback matrix (0x7310 from doc):" << std::endl;
    printf("  [%.4f  %.4f  %.4f]\n", m_fallback[0], m_fallback[1], m_fallback[2]);
    printf("  [%.4f  %.4f  %.4f]\n", m_fallback[3], m_fallback[4], m_fallback[5]);
    printf("  [%.4f  %.4f  %.4f]\n", m_fallback[6], m_fallback[7], m_fallback[8]);

    // Check row sums (should be ~1 for white balance neutrality)
    float sum_exif[3] = {
        m_exif[0] + m_exif[1] + m_exif[2],
        m_exif[3] + m_exif[4] + m_exif[5],
        m_exif[6] + m_exif[7] + m_exif[8]
    };
    float sum_fallback[3] = {
        m_fallback[0] + m_fallback[1] + m_fallback[2],
        m_fallback[3] + m_fallback[4] + m_fallback[5],
        m_fallback[6] + m_fallback[7] + m_fallback[8]
    };

    std::cout << "\nRow sums (should be ~1.0 for neutral grays):" << std::endl;
    printf("  Exiftool: R=%.4f  G=%.4f  B=%.4f\n", sum_exif[0], sum_exif[1], sum_exif[2]);
    printf("  Fallback: R=%.4f  G=%.4f  B=%.4f\n", sum_fallback[0], sum_fallback[1], sum_fallback[2]);

    // Check if exiftool matrix has issues
    std::cout << "\n=== Analysis ===" << std::endl;

    // A neutral gray (equal RGB) through each matrix
    float gray = 0.5f;
    float out_exif[3] = {
        m_exif[0] * gray + m_exif[1] * gray + m_exif[2] * gray,
        m_exif[3] * gray + m_exif[4] * gray + m_exif[5] * gray,
        m_exif[6] * gray + m_exif[7] * gray + m_exif[8] * gray
    };
    float out_fallback[3] = {
        m_fallback[0] * gray + m_fallback[1] * gray + m_fallback[2] * gray,
        m_fallback[3] * gray + m_fallback[4] * gray + m_fallback[5] * gray,
        m_fallback[6] * gray + m_fallback[7] * gray + m_fallback[8] * gray
    };

    printf("Neutral gray (0.5,0.5,0.5) through matrices:\n");
    printf("  Exiftool: R=%.4f  G=%.4f  B=%.4f\n", out_exif[0], out_exif[1], out_exif[2]);
    printf("  Fallback: R=%.4f  G=%.4f  B=%.4f\n", out_fallback[0], out_fallback[1], out_fallback[2]);

    // Note about matrix order - OpenCV uses BGR
    std::cout << "\nNote: RAWS color_matrix applies: [R' G' B'] = M × [R G B]" << std::endl;
    std::cout << "But data is stored BGR in OpenCV, so need to check order." << std::endl;

    return 0;
}

// analyze_error.cpp
// Analyze where the camera match error comes from
// Breaks down error by: neutral vs saturated pixels, luminance zones, etc.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <raw_file>" << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::cout << "=== Error Analysis ===" << std::endl;
    std::cout << "RAW: " << raw_path << std::endl;

    // Create pipe and load RAW
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));

    if (!head) {
        std::cerr << "Failed to decode RAW" << std::endl;
        return 1;
    }

    // Get preview (target)
    pipe::View previewView = head->view().view();
    cv::Mat target;
    previewView.copyTo(target);

    // Create body and link with neutral dials
    pipe::Body& body = head->body(1080);
    pipe::Body::Link& link = body.add("analysis");

    // Set base curve from RAWS
    if (head->hasBaseCurve()) {
        link.baseCurve().setCurve(head->baseCurve());
    }

    // All dials default to 0.5 (neutral), no need to set explicitly

    // Get our processed output (body.view() returns 8-bit BGR)
    cv::UMat output_umat = body.view();

    cv::Mat output_8u;
    output_umat.copyTo(output_8u);

    // Check type
    std::cout << "Output type: " << output_8u.type() << " (CV_8UC3=" << CV_8UC3 << ")" << std::endl;

    // Resize target to match output
    cv::Mat target_resized;
    cv::resize(target, target_resized, output_8u.size());

    std::cout << "Output: " << output_8u.cols << "x" << output_8u.rows << std::endl;
    std::cout << "Target: " << target_resized.cols << "x" << target_resized.rows << std::endl;

    // Analyze error by region
    double total_err = 0;
    double neutral_err = 0;
    double saturated_err = 0;
    double shadow_err = 0;  // L < 0.33
    double mid_err = 0;     // 0.33 <= L < 0.66
    double highlight_err = 0; // L >= 0.66

    int total_px = 0;
    int neutral_px = 0;
    int saturated_px = 0;
    int shadow_px = 0;
    int mid_px = 0;
    int highlight_px = 0;

    const int chroma_threshold = 30;

    for (int y = 0; y < output_8u.rows; y++) {
        const uchar* out_ptr = output_8u.ptr<uchar>(y);
        const uchar* tgt_ptr = target_resized.ptr<uchar>(y);

        for (int x = 0; x < output_8u.cols; x++) {
            int idx = x * 3;

            int ob = out_ptr[idx + 0];
            int og = out_ptr[idx + 1];
            int or_ = out_ptr[idx + 2];

            int tb = tgt_ptr[idx + 0];
            int tg = tgt_ptr[idx + 1];
            int tr = tgt_ptr[idx + 2];

            // Per-pixel L2 error (normalized to 0-1)
            float err_b = std::abs(ob - tb) / 255.0f;
            float err_g = std::abs(og - tg) / 255.0f;
            float err_r = std::abs(or_ - tr) / 255.0f;
            float px_err = (err_b + err_g + err_r) / 3.0f;

            total_err += px_err;
            total_px++;

            // Check if pixel is neutral (on output)
            int max_val = std::max({ob, og, or_});
            int min_val = std::min({ob, og, or_});
            int chroma = max_val - min_val;

            if (chroma < chroma_threshold) {
                neutral_err += px_err;
                neutral_px++;
            } else {
                saturated_err += px_err;
                saturated_px++;
            }

            // Luminance zone (using output)
            float lum = (0.0722f * ob + 0.7152f * og + 0.2126f * or_) / 255.0f;

            if (lum < 0.33f) {
                shadow_err += px_err;
                shadow_px++;
            } else if (lum < 0.66f) {
                mid_err += px_err;
                mid_px++;
            } else {
                highlight_err += px_err;
                highlight_px++;
            }
        }
    }

    std::cout << "\n=== Error Breakdown ===" << std::endl;
    std::cout << "Total error: " << (100.0 * total_err / total_px) << "%" << std::endl;
    std::cout << std::endl;

    std::cout << "By saturation:" << std::endl;
    std::cout << "  Neutral pixels (" << neutral_px << ", " << (100.0 * neutral_px / total_px) << "%): "
              << (neutral_px > 0 ? (100.0 * neutral_err / neutral_px) : 0) << "% error" << std::endl;
    std::cout << "  Saturated pixels (" << saturated_px << ", " << (100.0 * saturated_px / total_px) << "%): "
              << (saturated_px > 0 ? (100.0 * saturated_err / saturated_px) : 0) << "% error" << std::endl;

    std::cout << std::endl;
    std::cout << "By luminance zone:" << std::endl;
    std::cout << "  Shadows (" << shadow_px << ", " << (100.0 * shadow_px / total_px) << "%): "
              << (shadow_px > 0 ? (100.0 * shadow_err / shadow_px) : 0) << "% error" << std::endl;
    std::cout << "  Mids (" << mid_px << ", " << (100.0 * mid_px / total_px) << "%): "
              << (mid_px > 0 ? (100.0 * mid_err / mid_px) : 0) << "% error" << std::endl;
    std::cout << "  Highlights (" << highlight_px << ", " << (100.0 * highlight_px / total_px) << "%): "
              << (highlight_px > 0 ? (100.0 * highlight_err / highlight_px) : 0) << "% error" << std::endl;

    // Also compute contribution to total error
    std::cout << std::endl;
    std::cout << "Contribution to total error:" << std::endl;
    std::cout << "  Neutral: " << (100.0 * neutral_err / total_err) << "%" << std::endl;
    std::cout << "  Saturated: " << (100.0 * saturated_err / total_err) << "%" << std::endl;
    std::cout << "  Shadows: " << (100.0 * shadow_err / total_err) << "%" << std::endl;
    std::cout << "  Mids: " << (100.0 * mid_err / total_err) << "%" << std::endl;
    std::cout << "  Highlights: " << (100.0 * highlight_err / total_err) << "%" << std::endl;

    return 0;
}

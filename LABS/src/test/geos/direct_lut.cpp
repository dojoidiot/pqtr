// direct_lut.cpp
// Test: Direct 3D LUT estimation without base curve or dials
// Hypothesis: A single LUT measured from flat→JPEG achieves near-zero loss
//
// This proves that camera JPEG matching is a measurement problem, not optimization.

#include <tool.hpp>
#include <sink.hpp>
#include <hold.hpp>
#include <pipe.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <cmath>
#include <vector>

// Direct LUT estimation: measure the transform, don't optimize it
// Higher resolution than current 17³ for better accuracy
constexpr int GRID = 33;  // 33³ = 35,937 cells

inline int lut_index(int r, int g, int b, int ch) {
    return ((r * GRID + g) * GRID + b) * 3 + ch;
}

// Estimate LUT directly from flat→target pixel correspondence
// LUT operates in gamma space: input and output are both gamma-encoded
void estimate_direct_lut(
    const cv::Mat& flat,    // Scene-linear from RAWS (CV_32FC3)
    const cv::Mat& target,  // Camera JPEG (CV_8UC3, gamma-encoded)
    float* lut)
{
    // Accumulators
    int total_cells = GRID * GRID * GRID;
    std::vector<double> sum(total_cells * 3, 0.0);
    std::vector<int> count(total_cells, 0);

    // Convert flat to gamma space for binning
    // This is key: we bin in the space where we'll look up
    cv::Mat flat_gamma;
    cv::Mat flat_clamped;
    cv::max(flat, 0.0f, flat_clamped);
    cv::min(flat_clamped, 1.0f, flat_clamped);
    cv::pow(flat_clamped, 1.0f/2.2f, flat_gamma);

    // Resize target to match flat if needed
    cv::Mat target_resized;
    if (flat.size() != target.size()) {
        cv::resize(target, target_resized, flat.size());
    } else {
        target_resized = target;
    }

    // Ensure target is 8UC3
    cv::Mat target_8u;
    if (target_resized.type() == CV_8UC3) {
        target_8u = target_resized;
    } else if (target_resized.type() == CV_32FC3) {
        target_resized.convertTo(target_8u, CV_8UC3, 255.0);
    } else {
        target_resized.convertTo(target_8u, CV_8UC3);
    }

    float bin_size = 1.0f / GRID;

    // Debug: check value ranges
    double minVal, maxVal;
    cv::minMaxLoc(flat_gamma.reshape(1), &minVal, &maxVal);
    std::cout << "[DirectLUT] Flat gamma range: " << minVal << " to " << maxVal << std::endl;

    // Check what percentage of values are in each third
    int low_count = 0, mid_count = 0, high_count = 0;
    for (int y = 0; y < flat_gamma.rows; y++) {
        const float* ptr = flat_gamma.ptr<float>(y);
        for (int x = 0; x < flat_gamma.cols * 3; x++) {
            float v = ptr[x];
            if (v < 0.33f) low_count++;
            else if (v < 0.66f) mid_count++;
            else high_count++;
        }
    }
    int total = low_count + mid_count + high_count;
    std::cout << "[DirectLUT] Value distribution: low=" << (100.0f * low_count / total)
              << "% mid=" << (100.0f * mid_count / total)
              << "% high=" << (100.0f * high_count / total) << "%" << std::endl;

    for (int y = 0; y < flat_gamma.rows; y++) {
        const float* f_ptr = flat_gamma.ptr<float>(y);
        const uchar* t_ptr = target_8u.ptr<uchar>(y);

        for (int x = 0; x < flat_gamma.cols; x++) {
            int idx = x * 3;

            // Flat BGR (gamma space, 0-1)
            float b_in = f_ptr[idx + 0];
            float g_in = f_ptr[idx + 1];
            float r_in = f_ptr[idx + 2];

            // Quantize to grid cell
            int ri = std::min(GRID - 1, static_cast<int>(r_in / bin_size));
            int gi = std::min(GRID - 1, static_cast<int>(g_in / bin_size));
            int bi = std::min(GRID - 1, static_cast<int>(b_in / bin_size));

            int cell = (ri * GRID + gi) * GRID + bi;

            // Target BGR (0-255 → 0-1)
            float b_out = t_ptr[idx + 0] / 255.0f;
            float g_out = t_ptr[idx + 1] / 255.0f;
            float r_out = t_ptr[idx + 2] / 255.0f;

            // Accumulate
            sum[cell * 3 + 0] += r_out;
            sum[cell * 3 + 1] += g_out;
            sum[cell * 3 + 2] += b_out;
            count[cell]++;
        }
    }

    // Compute averages
    int empty = 0;
    int filled = 0;

    for (int ri = 0; ri < GRID; ri++) {
        for (int gi = 0; gi < GRID; gi++) {
            for (int bi = 0; bi < GRID; bi++) {
                int cell = (ri * GRID + gi) * GRID + bi;
                int base = cell * 3;

                if (count[cell] > 0) {
                    lut[base + 0] = static_cast<float>(sum[base + 0] / count[cell]);
                    lut[base + 1] = static_cast<float>(sum[base + 1] / count[cell]);
                    lut[base + 2] = static_cast<float>(sum[base + 2] / count[cell]);
                    filled++;
                } else {
                    // Identity for empty cells
                    lut[base + 0] = static_cast<float>(ri) / (GRID - 1);
                    lut[base + 1] = static_cast<float>(gi) / (GRID - 1);
                    lut[base + 2] = static_cast<float>(bi) / (GRID - 1);
                    empty++;
                }
            }
        }
    }

    std::cout << "[DirectLUT] Grid " << GRID << "³: "
              << filled << " filled, " << empty << " empty ("
              << (100.0f * empty / total_cells) << "% identity)" << std::endl;
}

// Trilinear interpolation
void trilinear_lookup(float r, float g, float b, const float* lut,
                      float& r_out, float& g_out, float& b_out)
{
    float scale = static_cast<float>(GRID - 1);
    float r_pos = r * scale;
    float g_pos = g * scale;
    float b_pos = b * scale;

    int r0 = std::max(0, std::min(GRID - 2, static_cast<int>(r_pos)));
    int g0 = std::max(0, std::min(GRID - 2, static_cast<int>(g_pos)));
    int b0 = std::max(0, std::min(GRID - 2, static_cast<int>(b_pos)));
    int r1 = r0 + 1, g1 = g0 + 1, b1 = b0 + 1;

    float rf = r_pos - r0;
    float gf = g_pos - g0;
    float bf = b_pos - b0;

    for (int ch = 0; ch < 3; ch++) {
        float c000 = lut[lut_index(r0, g0, b0, ch)];
        float c001 = lut[lut_index(r0, g0, b1, ch)];
        float c010 = lut[lut_index(r0, g1, b0, ch)];
        float c011 = lut[lut_index(r0, g1, b1, ch)];
        float c100 = lut[lut_index(r1, g0, b0, ch)];
        float c101 = lut[lut_index(r1, g0, b1, ch)];
        float c110 = lut[lut_index(r1, g1, b0, ch)];
        float c111 = lut[lut_index(r1, g1, b1, ch)];

        float c00 = c000 * (1 - bf) + c001 * bf;
        float c01 = c010 * (1 - bf) + c011 * bf;
        float c10 = c100 * (1 - bf) + c101 * bf;
        float c11 = c110 * (1 - bf) + c111 * bf;

        float c0 = c00 * (1 - gf) + c01 * gf;
        float c1 = c10 * (1 - gf) + c11 * gf;

        float val = c0 * (1 - rf) + c1 * rf;

        if (ch == 0) r_out = val;
        else if (ch == 1) g_out = val;
        else b_out = val;
    }
}

// Apply LUT to image
cv::Mat apply_direct_lut(const cv::Mat& flat, const float* lut)
{
    // Convert to gamma space
    cv::Mat flat_gamma;
    cv::Mat flat_clamped;
    cv::max(flat, 0.0f, flat_clamped);
    cv::min(flat_clamped, 1.0f, flat_clamped);
    cv::pow(flat_clamped, 1.0f/2.2f, flat_gamma);

    cv::Mat result(flat.size(), CV_32FC3);

    for (int y = 0; y < flat_gamma.rows; y++) {
        const float* in = flat_gamma.ptr<float>(y);
        float* out = result.ptr<float>(y);

        for (int x = 0; x < flat_gamma.cols; x++) {
            int idx = x * 3;
            float b_in = in[idx + 0];
            float g_in = in[idx + 1];
            float r_in = in[idx + 2];

            float r_out, g_out, b_out;
            trilinear_lookup(r_in, g_in, b_in, lut, r_out, g_out, b_out);

            // Output stays in gamma space (for comparison with JPEG)
            out[idx + 0] = b_out;
            out[idx + 1] = g_out;
            out[idx + 2] = r_out;
        }
    }

    return result;
}

// Compute L2 loss between result and target
float compute_loss(const cv::Mat& result, const cv::Mat& target)
{
    cv::Mat target_resized;
    if (result.size() != target.size()) {
        cv::resize(target, target_resized, result.size());
    } else {
        target_resized = target;
    }

    // Convert target to float 0-1
    cv::Mat target_f;
    target_resized.convertTo(target_f, CV_32FC3, 1.0/255.0);

    // Per-pixel L2
    cv::Mat diff;
    cv::absdiff(result, target_f, diff);
    cv::multiply(diff, diff, diff);

    cv::Scalar mean_sq = cv::mean(diff);
    float mse = (mean_sq[0] + mean_sq[1] + mean_sq[2]) / 3.0f;
    float rmse = std::sqrt(mse);

    return rmse;  // 0-1 scale, multiply by 100 for percentage
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <raw_file> <jpeg_reference|preview> [output.png]" << std::endl;
        std::cerr << "\nTests direct LUT estimation (no optimization)." << std::endl;
        std::cerr << "Proves camera matching is measurement, not optimization." << std::endl;
        return 1;
    }

    std::string raw_path = argv[1];
    std::string target_arg = argv[2];

    std::cout << "=== Direct LUT Test ===" << std::endl;
    std::cout << "RAW:  " << raw_path << std::endl;
    std::cout << "Target: " << target_arg << std::endl;
    std::cout << std::endl;

    // Create pipe and load RAW
    pqtr::Hold<pipe::Pipe> pipeline = pipe::make();
    pqtr::Hold<pqtr::Sink> rawSink(pqtr::Tool::read(raw_path));

    std::cout << "Decoding RAW..." << std::endl;
    pqtr::Hold<pipe::Head> head = pipeline->open(std::move(rawSink));
    if (!head) {
        std::cerr << "Failed to decode RAW: " << raw_path << std::endl;
        return 1;
    }

    // Get target image
    cv::Mat target;
    bool usePreview = (target_arg == "preview" || target_arg == "Preview");

    if (usePreview) {
        std::cout << "Using embedded preview as target..." << std::endl;
        pipe::View previewView = head->view().view();
        previewView.copyTo(target);
    } else {
        std::cout << "Loading external reference..." << std::endl;
        target = cv::imread(target_arg, cv::IMREAD_COLOR);
        if (target.empty()) {
            std::cerr << "Failed to load target: " << target_arg << std::endl;
            return 1;
        }
    }

    // Get flat image (scene-linear, no styling) from head->data()
    // This is the decoded RAW BEFORE any processing
    cv::UMat flat_umat = head->data().view();
    cv::Mat flat;
    flat_umat.copyTo(flat);

    std::cout << "Flat image: " << flat.cols << "x" << flat.rows << " type=" << flat.type() << " (CV_32FC3=" << CV_32FC3 << ")" << std::endl;
    std::cout << "Target image: " << target.cols << "x" << target.rows << " type=" << target.type() << " (CV_8UC3=" << CV_8UC3 << ")" << std::endl;
    std::cout << std::endl;

    // Ensure flat is CV_32FC3
    if (flat.type() != CV_32FC3) {
        std::cerr << "ERROR: Flat image is not CV_32FC3" << std::endl;
        return 1;
    }

    // Allocate LUT
    std::vector<float> lut(GRID * GRID * GRID * 3);

    // Estimate LUT directly from pixel correspondence
    std::cout << "Estimating " << GRID << "³ LUT from pixel correspondence..." << std::endl;
    estimate_direct_lut(flat, target, lut.data());

    // Apply LUT
    std::cout << "Applying LUT..." << std::endl;
    cv::Mat result = apply_direct_lut(flat, lut.data());

    // Compute loss
    float loss = compute_loss(result, target);
    std::cout << std::endl;
    std::cout << "=== RESULT ===" << std::endl;
    std::cout << "Direct LUT loss: " << (loss * 100.0f) << "%" << std::endl;
    std::cout << std::endl;

    if (loss < 0.03f) {
        std::cout << "SUCCESS: <3% loss proves camera matching is measurement, not optimization." << std::endl;
    } else if (loss < 0.05f) {
        std::cout << "GOOD: <5% loss. Direct LUT captures most of camera transform." << std::endl;
    } else {
        std::cout << "INVESTIGATE: " << (loss * 100.0f) << "% loss. Check alignment or LUT resolution." << std::endl;
    }

    // Save result for visual inspection
    if (argc > 3) {
        std::string out_path = argv[3];
        cv::Mat result_8u;
        result.convertTo(result_8u, CV_8UC3, 255.0);
        cv::imwrite(out_path, result_8u);
        std::cout << "Saved result to: " << out_path << std::endl;
    }

    return 0;
}

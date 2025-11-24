// main.cpp
// C++ RAW image processing pipeline using production modules
// Matches Python reference: RAW → BLC → WB → Demosaic → Gamma → PNG

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/core/ocl.hpp>

// Production modules (custom GPL-free Sony decoder)
#include "sony_arw2.h"
#include "blc.h"
#include "wb_gain.h"
#include "demosaic.h"
#include "gamma_oetf.h"

using namespace mods;

// Helper to print value range
void printValueRange(const cv::UMat& image, const std::string& stage_name) {
    cv::Mat cpu_image = image.getMat(cv::ACCESS_READ);
    double min_val, max_val;
    cv::minMaxLoc(cpu_image, &min_val, &max_val);
    cv::Scalar mean_scalar = cv::mean(cpu_image);
    double mean_val = cpu_image.channels() == 3 ?
        (mean_scalar[0] + mean_scalar[1] + mean_scalar[2]) / 3.0 : mean_scalar[0];

    printf("  [%s]: Range [%.4f, %.4f], Mean=%.4f\n",
           stage_name.c_str(), min_val, max_val, mean_val);
}

int main(int argc, char** argv) {
    std::cout << "\n=== C++ RAW PROCESSING PIPELINE ===\n" << std::endl;
    std::cout << "Pipeline: RAW → BLC → WB → Demosaic → Gamma → PNG\n" << std::endl;

    // Parse arguments
    std::string input_file;
    if (argc > 1) {
        input_file = argv[1];
    } else {
        input_file = "../../var/sony_arw2.ARW";
    }

    // Generate output filename: replace .ARW extension with .jpg
    std::string basename = input_file.substr(input_file.find_last_of('/') + 1);
    size_t dot_pos = basename.find_last_of('.');
    if (dot_pos != std::string::npos) {
        basename = basename.substr(0, dot_pos) + ".jpg";
    } else {
        basename += ".jpg";
    }

    // Extract directory from input file to determine tmp/ location
    std::string input_dir = input_file.substr(0, input_file.find_last_of('/'));
    std::string test_root = input_dir.substr(0, input_dir.find_last_of('/'));  // Remove /var
    std::string output_file = test_root + "/tmp/" + basename;

    std::cout << "Input RAW file: " << input_file << std::endl;
    std::cout << "Output PNG: " << output_file << std::endl;
    std::cout << "OpenCV GPU support: " << (cv::ocl::haveOpenCL() ? "YES" : "NO") << std::endl;
    std::cout << std::endl;

    try {
        Params params;  // Default parameters

        // Stage 1: Load RAW file
        std::cout << "Step 1: Loading RAW data..." << std::endl;

        RawLoader loader;
        loader.setFilePath(input_file);

        cv::UMat bayer_raw, dummy_input;
        if (!loader.process(dummy_input, bayer_raw, params)) {
            std::cerr << "ERROR: Failed to load RAW file" << std::endl;
            return 1;
        }

        const RawMetadata& metadata = loader.metadata();
        std::cout << "RAW data loaded successfully." << std::endl;
        std::cout << "  - Shape: " << metadata.height << "x" << metadata.width << std::endl;
        printValueRange(bayer_raw, "RAW Bayer");
        std::cout << std::endl;

        // Stage 2: Black Level Correction (normalized)
        std::cout << "Step 2: Correcting black levels..." << std::endl;

        BLC blc(metadata);
        cv::UMat bayer_normalized;

        Params blc_params;
        blc_params["normalize"] = 1.0f;  // Enable normalization like Python

        if (!blc.process(bayer_raw, bayer_normalized, blc_params)) {
            std::cerr << "ERROR: BLC failed" << std::endl;
            return 1;
        }

        std::cout << "  - Applying black level correction. Average value: " << metadata.black_level << std::endl;
        std::cout << "  - Normalizing to range [0, 1]." << std::endl;
        double blc_min, blc_max;
        cv::Mat blc_cpu = bayer_normalized.getMat(cv::ACCESS_READ);
        cv::minMaxLoc(blc_cpu, &blc_min, &blc_max);
        printf("  - Min/Max after BLC: %.4f/%.4f\n", blc_min, blc_max);
        std::cout << std::endl;

        // Stage 3: White Balance
        std::cout << "Step 3: Applying White Balance..." << std::endl;

        WBGain wb_gain(metadata);
        cv::UMat bayer_wb;

        if (!wb_gain.process(bayer_normalized, bayer_wb, params)) {
            std::cerr << "ERROR: WB_Gain failed" << std::endl;
            return 1;
        }

        // Display normalized WB coefficients
        float g_ref = metadata.wb_rggb[1] > 0 ? metadata.wb_rggb[1] : 1024.0f;
        float wb_r_norm = metadata.wb_rggb[0] / g_ref;
        float wb_b_norm = metadata.wb_rggb[2] / g_ref;
        printf("  - Applying white balance. R=%.4f, B=%.4f\n", wb_r_norm, wb_b_norm);

        double wb_min, wb_max;
        cv::Mat wb_cpu = bayer_wb.getMat(cv::ACCESS_READ);
        cv::minMaxLoc(wb_cpu, &wb_min, &wb_max);
        printf("  - Min/Max after WB: %.4f/%.4f\n", wb_min, wb_max);
        std::cout << std::endl;

        // Stage 4: Demosaic
        std::cout << "Step 4: Demosaicing..." << std::endl;

        Demosaic demosaic(metadata);
        cv::UMat rgb_linear;

        if (!demosaic.process(bayer_wb, rgb_linear, params)) {
            std::cerr << "ERROR: Demosaic failed" << std::endl;
            return 1;
        }

        cv::Mat rgb_cpu = rgb_linear.getMat(cv::ACCESS_READ);
        std::cout << "  - Image shape after Demosaic: " << rgb_cpu.rows << "x"
                  << rgb_cpu.cols << "x" << rgb_cpu.channels() << std::endl;

        double dm_min, dm_max;
        cv::minMaxLoc(rgb_cpu, &dm_min, &dm_max);
        printf("  - Min/Max after Demosaic: %.4f/%.4f\n", dm_min, dm_max);
        std::cout << std::endl;

        // Stage 5: Gamma Correction
        std::cout << "Step 5: Applying Gamma Correction..." << std::endl;

        GammaOETF gamma_oetf;
        cv::UMat gamma_encoded;

        if (!gamma_oetf.process(rgb_linear, gamma_encoded, params)) {
            std::cerr << "ERROR: Gamma OETF failed" << std::endl;
            return 1;
        }

        std::cout << "  - Applying sRGB gamma curve." << std::endl;
        cv::Mat gamma_cpu = gamma_encoded.getMat(cv::ACCESS_READ);
        double gm_min, gm_max;
        cv::minMaxLoc(gamma_cpu, &gm_min, &gm_max);
        printf("  - Min/Max after Gamma: %.4f/%.4f\n", gm_min, gm_max);
        std::cout << std::endl;

        // Stage 6: Save Output Image
        std::cout << "Step 6: Saving Output Image..." << std::endl;

        // Convert float32 [0,1] → uint8 [0,255]
        std::cout << "  - Converting to 8-bit integer format." << std::endl;
        cv::UMat output_8bit;
        gamma_encoded.convertTo(output_8bit, CV_8UC3, 255.0);

        // Save as PNG
        cv::Mat output_cpu = output_8bit.getMat(cv::ACCESS_READ);
        cv::imwrite(output_file, output_cpu);
        std::cout << "  - Full resolution image saved to: " << output_file << std::endl;

        // Stage 7: Create social media sized version
        std::cout << "\nStep 7: Creating social media version..." << std::endl;

        // Calculate dimensions for 1080px longest side
        int target_size = 1080;
        int rows = output_cpu.rows;
        int cols = output_cpu.cols;

        double scale;
        if (rows > cols) {
            scale = static_cast<double>(target_size) / rows;
        } else {
            scale = static_cast<double>(target_size) / cols;
        }

        int new_rows = static_cast<int>(rows * scale);
        int new_cols = static_cast<int>(cols * scale);

        std::cout << "  - Original size: " << rows << "x" << cols << std::endl;
        std::cout << "  - Social media size: " << new_rows << "x" << new_cols << std::endl;

        // Resize using high-quality interpolation
        cv::Mat social_sized;
        cv::resize(output_cpu, social_sized, cv::Size(new_cols, new_rows), 0, 0, cv::INTER_AREA);

        // Generate social media output filename
        size_t ext_pos = output_file.find_last_of('.');
        std::string social_output = output_file.substr(0, ext_pos) + "_social" + output_file.substr(ext_pos);

        cv::imwrite(social_output, social_sized);
        std::cout << "  - Social media image saved to: " << social_output << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\nPipeline finished successfully." << std::endl;
    return 0;
}

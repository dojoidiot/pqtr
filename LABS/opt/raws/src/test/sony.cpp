// test_gold.cpp
// Test program for sony_Decoder decoder
// Loads RAW file → Sink, calls prepare(), validates output

#include <iostream>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/core/ocl.hpp>
#include <tool.hpp>
#include "sony.h"

int main(int argc, char** argv) {
    std::cout << "\n=== GOLD DECODER TEST ===\n" << std::endl;

    // Parse arguments
    std::string input_file;
    if (argc > 1) {
        input_file = argv[1];
    } else {
        input_file = "./var/sony.ARW";
    }

    std::cout << "Input RAW file: " << input_file << std::endl;
    std::cout << "OpenCV GPU support: " << (cv::ocl::haveOpenCL() ? "YES" : "NO") << std::endl;
    std::cout << std::endl;

    try {
        // Step 1: Load file into Sink
        std::cout << "Step 1: Loading file into Sink..." << std::endl;
        pqtr::Sink* sink = pqtr::Tool::read(input_file);
        std::cout << "  - File size: " << sink->size() << " bytes" << std::endl;
        std::cout << std::endl;

        // Step 2: Prepare OpenCV structures
        std::cout << "Step 2: Preparing decoder..." << std::endl;
        cv::UMat bayer_data;
        sony::Info info;
        sony::RawMetadata metadata;
        std::cout << std::endl;

        // Step 3: Decode
        std::cout << "Step 3: Calling sony::Decoder::prepare()..." << std::endl;
        if (!sony::Decoder::prepare(*sink, bayer_data, info, metadata)) {
            delete sink;
            std::cerr << "ERROR: Decoder failed" << std::endl;
            return 1;
        }
        delete sink;

        std::cout << "  ✓ Decode successful" << std::endl;
        std::cout << std::endl;

        // Step 4: Validate output
        std::cout << "Step 4: Validating output..." << std::endl;

        // Check data dimensions
        cv::Mat bayer_cpu = bayer_data.getMat(cv::ACCESS_READ);
        std::cout << "  - Bayer data: " << bayer_cpu.rows << "x" << bayer_cpu.cols
                  << " (type: " << bayer_cpu.type() << ")" << std::endl;

        // Check value range
        double min_val, max_val;
        cv::minMaxLoc(bayer_cpu, &min_val, &max_val);
        cv::Scalar mean_scalar = cv::mean(bayer_cpu);
        double mean_val = mean_scalar[0];

        std::cout << "  - Value range: [" << min_val << ", " << max_val << "]" << std::endl;
        std::cout << "  - Mean value: " << mean_val << std::endl;
        std::cout << std::endl;

        // Check metadata (using struct)
        std::cout << "Step 5: Metadata (RawMetadata struct)..." << std::endl;
        std::cout << "  - Camera: " << metadata.camera_make << " " << metadata.camera_model << std::endl;
        std::cout << "  - Dimensions: " << metadata.width << "x" << metadata.height << std::endl;
        std::cout << "  - Black level: " << metadata.black_level << std::endl;
        std::cout << "  - White level: " << metadata.white_level << std::endl;
        std::cout << "  - WB multipliers: R=" << metadata.wb_rggb[0]
                  << " G=" << metadata.wb_rggb[1]
                  << " B=" << metadata.wb_rggb[2] << std::endl;
        std::cout << "  - Bayer pattern: " << metadata.bayer_pattern << std::endl;
        std::cout << "  - ISO: " << metadata.iso << std::endl;
        std::cout << "  - Shutter: " << metadata.shutter_speed << "s" << std::endl;
        std::cout << "  - Aperture: f/" << metadata.aperture << std::endl;
        std::cout << "  - Focal length: " << metadata.focal_length << "mm" << std::endl;
        std::cout << "  - Lens: " << metadata.lens_model << std::endl;
        std::cout << std::endl;

        // Also show Info map
        std::cout << "Step 5b: Info map validation..." << std::endl;
        std::cout << "  - Info map size: " << info.size() << " keys" << std::endl;
        std::cout << "  - Info camera_make: " << info["camera_make"] << std::endl;
        std::cout << std::endl;

        // Validation checks
        std::cout << "Step 6: Validation..." << std::endl;
        bool passed = true;

        if (bayer_cpu.type() != CV_16UC1) {
            std::cerr << "  ✗ FAIL: Expected CV_16UC1, got type " << bayer_cpu.type() << std::endl;
            passed = false;
        } else {
            std::cout << "  ✓ Data type correct (CV_16UC1)" << std::endl;
        }

        if (metadata.width == 0 || metadata.height == 0) {
            std::cerr << "  ✗ FAIL: Missing dimensions" << std::endl;
            passed = false;
        } else {
            std::cout << "  ✓ Dimensions present" << std::endl;
        }

        if (metadata.camera_make.empty()) {
            std::cerr << "  ✗ FAIL: Missing camera make" << std::endl;
            passed = false;
        } else {
            std::cout << "  ✓ Camera make present" << std::endl;
        }

        if (info.size() < 10) {
            std::cerr << "  ✗ FAIL: Info map too small" << std::endl;
            passed = false;
        } else {
            std::cout << "  ✓ Info map populated" << std::endl;
        }

        if (min_val < 0 || max_val > 65535) {
            std::cerr << "  ✗ FAIL: Values out of uint16 range" << std::endl;
            passed = false;
        } else {
            std::cout << "  ✓ Values in valid range" << std::endl;
        }

        std::cout << std::endl;

        if (!passed) {
            std::cerr << "✗ VALIDATION FAILED - STOPPING" << std::endl;
            return 1;
        }

        std::cout << "✓ DECODE VALIDATION PASSED" << std::endl;
        std::cout << std::endl;

        // Step 7: Process through pipeline
        std::cout << "Step 7: Processing through pipeline..." << std::endl;
        cv::UMat rgb_output;

        if (!sony::Decoder::process(bayer_data, metadata, rgb_output)) {
            std::cerr << "  ✗ FAIL: Processing failed" << std::endl;
            return 1;
        }

        std::cout << "  ✓ Processing complete" << std::endl;

        // Check output
        cv::Mat rgb_cpu = rgb_output.getMat(cv::ACCESS_READ);
        std::cout << "  - RGB output: " << rgb_cpu.rows << "x" << rgb_cpu.cols
                  << "x" << rgb_cpu.channels() << std::endl;
        std::cout << "  - Type: " << rgb_cpu.type() << " (expected CV_32FC3 = " << CV_32FC3 << ")" << std::endl;

        double rgb_min, rgb_max;
        cv::minMaxLoc(rgb_cpu, &rgb_min, &rgb_max);
        std::cout << "  - Value range: [" << rgb_min << ", " << rgb_max << "]" << std::endl;
        std::cout << std::endl;

        // Step 8: Save output
        std::cout << "Step 8: Saving output..." << std::endl;
        // Output to tmp directory (run from opt/raws/)
        std::string output_file = "./tmp/sony.jpg";

        // Convert to 8-bit for saving
        cv::UMat output_8bit;
        rgb_output.convertTo(output_8bit, CV_8UC3, 255.0);
        cv::Mat output_cpu = output_8bit.getMat(cv::ACCESS_READ);

        if (cv::imwrite(output_file, output_cpu)) {
            std::cout << "  ✓ Saved to: " << output_file << std::endl;
        } else {
            std::cerr << "  ✗ FAIL: Could not save image" << std::endl;
            return 1;
        }

        std::cout << std::endl;
        std::cout << "✓ ALL TESTS PASSED" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
}

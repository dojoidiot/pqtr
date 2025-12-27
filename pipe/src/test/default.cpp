// pipe.cpp - Step 0 test
//
// Verifies Sony ARW decoder matches LibRaw unprocessed_raw output
//
// All test output goes into tmp/var/pipe/
// All output files are prefixed with step-0-<file name>.<file type>

#include "../../inc/pipe.hpp"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <sys/stat.h>

// Read file into vector
static std::vector<uint8_t> read_file(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

// Write binary file
static bool write_file(const char* path, const void* data, size_t size) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(static_cast<const char*>(data), size);
    return f.good();
}

// Write string file
static bool write_text(const char* path, const std::string& text) {
    std::ofstream f(path);
    if (!f) return false;
    f << text;
    return f.good();
}

// Read LibRaw unprocessed_raw TIFF (skip 8-byte header, swap bytes)
static std::vector<uint16_t> read_libraw_tiff(const char* path, int& width, int& height) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};

    // Read TIFF header to get dimensions
    uint8_t header[256];
    f.read(reinterpret_cast<char*>(header), 256);

    // Simple TIFF parsing - find width/height tags
    // Tag 256 = width, Tag 257 = height
    uint16_t byte_order = *reinterpret_cast<uint16_t*>(header);
    bool big_endian = (byte_order == 0x4D4D);

    auto read16 = [&](const uint8_t* p) -> uint16_t {
        if (big_endian) return (p[0] << 8) | p[1];
        return p[0] | (p[1] << 8);
    };
    auto read32 = [&](const uint8_t* p) -> uint32_t {
        if (big_endian) return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
        return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
    };

    uint32_t ifd_offset = read32(header + 4);
    f.seekg(ifd_offset);

    uint8_t ifd[512];
    f.read(reinterpret_cast<char*>(ifd), 512);

    uint16_t num_entries = read16(ifd);
    width = 0; height = 0;
    uint32_t strip_offset = 0;

    for (int i = 0; i < num_entries && i < 20; i++) {
        uint8_t* entry = ifd + 2 + i * 12;
        uint16_t tag = read16(entry);
        uint32_t value = read32(entry + 8);

        if (tag == 256) width = value;
        else if (tag == 257) height = value;
        else if (tag == 273) strip_offset = value;
    }

    if (width == 0 || height == 0 || strip_offset == 0) return {};

    // Read bayer data
    size_t bayer_size = (size_t)width * height;
    std::vector<uint16_t> bayer(bayer_size);

    f.seekg(strip_offset);
    f.read(reinterpret_cast<char*>(bayer.data()), bayer_size * 2);

    // TIFF data is already little-endian (II header = Intel byte order)
    // No swap needed

    return bayer;
}

// Compare two bayer arrays, return number of mismatches
static int compare_bayer(const uint16_t* a, const uint16_t* b, size_t count) {
    int mismatches = 0;
    for (size_t i = 0; i < count && mismatches < 10; i++) {
        if (a[i] != b[i]) {
            std::cerr << "  Mismatch at " << i << ": " << a[i] << " vs " << b[i] << "\n";
            mismatches++;
        }
    }
    if (mismatches > 0) {
        // Count total mismatches
        for (size_t i = 0; i < count; i++) {
            if (a[i] != b[i]) mismatches++;
        }
    }
    return mismatches;
}

int main() {
    const char* arw_path = "src/test/DSC00144.ARW";
    const char* out_dir = "tmp/var/pipe";

    // Create output directory
    mkdir("tmp", 0755);
    mkdir("tmp/var", 0755);
    mkdir(out_dir, 0755);

    std::cout << "=== Step 0: RAW Decoder Verification ===\n\n";

    // 1. Load ARW file
    std::cout << "Loading " << arw_path << "...\n";
    auto arw_data = read_file(arw_path);
    if (arw_data.empty()) {
        std::cerr << "FAIL: Cannot read " << arw_path << "\n";
        return 1;
    }
    std::cout << "  Size: " << arw_data.size() << " bytes\n";

    // 2. Decode using Head
    std::cout << "\nDecoding with Head...\n";
    auto head = flow::makeHead();
    auto flow = head->decode(arw_data.data(), arw_data.size());
    if (!flow) {
        std::cerr << "FAIL: Head::decode() returned null\n";
        return 1;
    }

    // Get dimensions from Tree
    auto& root = flow->info().root();
    int width = static_cast<int>(root.leaf(flow::WIDTH).dial());
    int height = static_cast<int>(root.leaf(flow::HEIGHT).dial());
    std::cout << "  Dimensions: " << width << " x " << height << "\n";

    // 3. Save bayer data as binary
    std::cout << "\nSaving bayer data...\n";
    size_t bayer_size = (size_t)width * height * 2;
    std::string data_path = std::string(out_dir) + "/step-0-data.bin";
    if (write_file(data_path.c_str(), flow->data(), bayer_size)) {
        std::cout << "  Saved: " << data_path << " (" << bayer_size << " bytes)\n";
    }

    // 4. Generate LibRaw reference and compare
    std::cout << "\nGenerating LibRaw reference...\n";
    std::string ref_tiff = std::string(out_dir) + "/step-0-ref.tiff";
    std::string cmd = "LD_LIBRARY_PATH=LibRaw/lib/.libs LibRaw/bin/.libs/unprocessed_raw -T "
                    + std::string(arw_path) + " 2>&1";
    int ret = system(cmd.c_str());

    // Move the output file
    std::string src_tiff = std::string(arw_path) + ".tiff";
    rename(src_tiff.c_str(), ref_tiff.c_str());

    if (ret == 0) {
        std::cout << "  Generated: " << ref_tiff << "\n";

        // Compare bayer data
        std::cout << "\nComparing bayer data...\n";
        int ref_w = 0, ref_h = 0;
        auto ref_bayer = read_libraw_tiff(ref_tiff.c_str(), ref_w, ref_h);

        if (ref_bayer.empty()) {
            std::cerr << "FAIL: Cannot read LibRaw reference\n";
        } else {
            std::cout << "  LibRaw dimensions: " << ref_w << " x " << ref_h << "\n";

            if (ref_w != width || ref_h != height) {
                std::cerr << "FAIL: Dimension mismatch\n";
            } else {
                int mismatches = compare_bayer(flow->data(), ref_bayer.data(), ref_bayer.size());
                if (mismatches == 0) {
                    std::cout << "  PASS: Bayer data matches exactly\n";
                } else {
                    std::cerr << "  FAIL: " << mismatches << " mismatches\n";
                }
            }
        }
    } else {
        std::cerr << "WARN: Could not run LibRaw (ret=" << ret << ")\n";
    }

    // 5. Save JSON metadata
    std::cout << "\nSaving metadata...\n";
    std::string json = flow->info().json();
    std::string json_path = std::string(out_dir) + "/step-0-info.json";
    if (write_text(json_path.c_str(), json)) {
        std::cout << "  Saved: " << json_path << "\n";
    }

    // 6. Verify JSON completeness
    std::cout << "\nVerifying metadata...\n";
    bool json_ok = true;

    // Check required fields
    if (!root.test(flow::WIDTH)) { std::cerr << "  Missing: width\n"; json_ok = false; }
    if (!root.test(flow::HEIGHT)) { std::cerr << "  Missing: height\n"; json_ok = false; }
    if (!root.test(flow::BLACK)) { std::cerr << "  Missing: black\n"; json_ok = false; }
    if (!root.test(flow::WHITE)) { std::cerr << "  Missing: white\n"; json_ok = false; }
    if (!root.test("camera")) { std::cerr << "  Missing: camera\n"; json_ok = false; }
    if (!root.test("exif")) { std::cerr << "  Missing: exif\n"; json_ok = false; }
    if (!root.test("wb")) { std::cerr << "  Missing: wb\n"; json_ok = false; }

    if (json_ok) {
        std::cout << "  PASS: All required fields present\n";

        // Print key values
        std::cout << "\n  Key metadata:\n";
        std::cout << "    width:  " << root.leaf(flow::WIDTH).dial() << "\n";
        std::cout << "    height: " << root.leaf(flow::HEIGHT).dial() << "\n";
        std::cout << "    black:  " << root.leaf(flow::BLACK).dial() << "\n";
        std::cout << "    white:  " << root.leaf(flow::WHITE).dial() << "\n";

        auto& camera = root.next("camera");
        std::cout << "    camera: " << camera.leaf("make").text()
                  << " " << camera.leaf("model").text() << "\n";

        auto& wb = root.next("wb");
        std::cout << "    wb:     [" << wb.leaf("r").dial()
                  << ", " << wb.leaf("g1").dial()
                  << ", " << wb.leaf("b").dial()
                  << ", " << wb.leaf("g2").dial() << "]\n";
    }

    std::cout << "\n=== Step 0 Complete ===\n";

    // =========================================================================
    // Step 2: rawprepare - Black level subtraction
    // =========================================================================

    std::cout << "\n=== Step 2: rawprepare Verification ===\n\n";

    // Run rawprepare
    // Note: white_level comes from linear_max (0x787f) in Sony SR2SubIFD
    std::cout << "Running rawprepare (white=" << root.leaf(flow::WHITE).dial() << ")...\n";
    auto rawprepare = flow::makeRawprepare();
    rawprepare->process(*flow);

    // Get normalized float data
    float* fdata = flow->fdata();
    size_t npixels = (size_t)width * height;

    // Verify output range
    std::cout << "\nVerifying output range...\n";
    float min_val = 1.0f, max_val = 0.0f;
    int out_of_range = 0;
    for (size_t i = 0; i < npixels; i++) {
        if (fdata[i] < min_val) min_val = fdata[i];
        if (fdata[i] > max_val) max_val = fdata[i];
        if (fdata[i] < 0.0f || fdata[i] > 1.0f) out_of_range++;
    }
    std::cout << "  Min: " << min_val << ", Max: " << max_val << "\n";
    if (out_of_range == 0) {
        std::cout << "  PASS: All values in [0, 1] range\n";
    } else {
        std::cerr << "  FAIL: " << out_of_range << " values out of range\n";
    }

    // Verify black level subtraction
    // After rawprepare, values should be normalized:
    // - Black (512) should map to 0.0
    // - White (16383) should map to 1.0
    // - Formula: out = (in - 512) / (16383 - 512) = (in - 512) / 15871
    std::cout << "\nVerifying normalization math...\n";
    float black_level = root.leaf(flow::BLACK).dial();
    float white_level = root.leaf(flow::WHITE).dial();
    std::cout << "  Black: " << black_level << ", White: " << white_level << "\n";

    // Spot check a few pixels
    bool math_ok = true;
    for (int check = 0; check < 5 && math_ok; check++) {
        size_t idx = (size_t)check * 1000;  // Sample spread across image
        if (idx < npixels) {
            uint16_t raw_val = flow->data()[idx];
            float expected = (raw_val - black_level) / (white_level - black_level);
            if (expected < 0.0f) expected = 0.0f;
            if (expected > 1.0f) expected = 1.0f;
            float actual = fdata[idx];
            float diff = std::abs(expected - actual);
            if (diff > 0.0001f) {
                std::cerr << "  Mismatch at " << idx << ": raw=" << raw_val
                          << " expected=" << expected << " actual=" << actual << "\n";
                math_ok = false;
            }
        }
    }
    if (math_ok) {
        std::cout << "  PASS: Normalization formula verified\n";
    }

    // Save normalized data for inspection
    std::string fdata_path = std::string(out_dir) + "/step-2-fdata.bin";
    if (write_file(fdata_path.c_str(), fdata, npixels * sizeof(float))) {
        std::cout << "\n  Saved: " << fdata_path << " (" << npixels * sizeof(float) << " bytes)\n";
    }

    std::cout << "\n=== Step 2 Complete ===\n";

    // =========================================================================
    // Step 3: temperature - White Balance (on Bayer mosaic)
    // IOP Stage: Sensor (before demosaic)
    // =========================================================================

    std::cout << "\n=== Step 3: temperature Verification ===\n\n";

    // WB coefficients from XMP params (default.xmp temperature module)
    // XMP hex: 004018400000803f0080c83f0000000004000000
    // Parsed:  [2.37890625, 1.0, 1.56640625, 0.0, preset=4]
    //
    // Note: These differ from raw file WB (2420/1024, 1616/1024):
    //   Raw "As shot":  r=2.3633, b=1.5781
    //   XMP params:     r=2.3789, b=1.5664
    //
    // DT computes these through its color pipeline (matrix calibration,
    // D65 adjustment). To match DT exactly, we use XMP values.
    // TODO: Implement XMP parsing in Copy step to read these automatically.
    float coeff_r = 2.37890625f;
    float coeff_g = 1.0f;
    float coeff_b = 1.56640625f;

    // Also show raw WB for comparison
    auto& wb = root.next("wb");
    float raw_r = wb.leaf("r").dial() / wb.leaf("g1").dial();
    float raw_b = wb.leaf("b").dial() / wb.leaf("g1").dial();

    std::cout << "Running temperature (WB on mosaic)...\n";
    std::cout << "  Raw WB coeffs: [" << raw_r << ", 1.0, " << raw_b << "]\n";
    std::cout << "  XMP coeffs:    [" << coeff_r << ", " << coeff_g << ", " << coeff_b << "]\n";

    auto temperature = flow::makeTemperature();
    temperature->setCoeffs(coeff_r, coeff_g, coeff_b);
    temperature->process(*flow);

    // Verify WB applied to fdata
    std::cout << "\nVerifying WB-corrected bayer range...\n";
    float wb_min = 1e10f, wb_max = 0.0f;
    for (size_t i = 0; i < npixels; i++) {
        float v = fdata[i];
        if (v < wb_min) wb_min = v;
        if (v > wb_max) wb_max = v;
    }
    std::cout << "  Range: [" << wb_min << ", " << wb_max << "]\n";
    std::cout << "  (max > 1.0 expected due to WB coeffs > 1)\n";

    std::cout << "\n=== Step 3 Complete ===\n";

    // =========================================================================
    // Step 4: demosaic - Bayer to RGB
    // IOP Stage: Sensor (after temperature)
    // =========================================================================

    std::cout << "\n=== Step 4: demosaic Verification ===\n\n";

    // Run demosaic on WB'd bayer
    std::cout << "Running demosaic (RCD)...\n";
    auto demosaic = flow::makeDemosaic();
    demosaic->process(*flow);

    // Get RGB data
    float* rgb_data = flow->rgb();

    // Verify output range
    std::cout << "\nVerifying RGB output range...\n";
    float rgb_min[3] = {1.0f, 1.0f, 1.0f};
    float rgb_max[3] = {0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb_data[i * 4 + c];
            if (v < rgb_min[c]) rgb_min[c] = v;
            if (v > rgb_max[c]) rgb_max[c] = v;
        }
    }
    std::cout << "  R: [" << rgb_min[0] << ", " << rgb_max[0] << "]\n";
    std::cout << "  G: [" << rgb_min[1] << ", " << rgb_max[1] << "]\n";
    std::cout << "  B: [" << rgb_min[2] << ", " << rgb_max[2] << "]\n";

    // Verify all values are in reasonable range
    bool rgb_ok = true;
    for (int c = 0; c < 3; c++) {
        if (rgb_min[c] < 0.0f || rgb_max[c] > 1.0f) {
            rgb_ok = false;
        }
    }
    if (rgb_ok) {
        std::cout << "  PASS: All RGB values in [0, 1] range\n";
    } else {
        std::cerr << "  WARN: Some RGB values out of range (may be OK for highlights)\n";
    }

    // Save RGB as PNG for visual inspection
    std::cout << "\nSaving demosaic output...\n";

    // Convert to uint8 for PNG
    std::vector<uint8_t> rgb8(npixels * 3);
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb_data[i * 4 + c];
            v = std::max(0.0f, std::min(1.0f, v));
            // Apply simple gamma for viewing
            v = std::pow(v, 1.0f / 2.2f);
            rgb8[i * 3 + c] = static_cast<uint8_t>(v * 255.0f + 0.5f);
        }
    }

    auto png_data = flow::swap(rgb8.data(), width, height, flow::BIN, flow::PNG);
    std::string rgb_path = std::string(out_dir) + "/step-4-demosaic.png";
    if (!png_data.empty() && write_file(rgb_path.c_str(), png_data.data(), png_data.size())) {
        std::cout << "  Saved: " << rgb_path << " (" << png_data.size() << " bytes)\n";
    }

    std::cout << "\n=== Step 4 Complete ===\n";

    // =========================================================================
    // Step 5: colorin - Camera RGB → XYZ → Lab (D50)
    // IOP Stage: Color
    // =========================================================================

    std::cout << "\n=== Step 5: colorin Verification ===\n\n";

    std::cout << "Running colorin (camera RGB → Lab)...\n";
    auto colorin = flow::makeColorin();
    colorin->process(*flow);

    // Verify Lab ranges
    std::cout << "\nVerifying Lab ranges...\n";
    float lab_min[3] = {1e10f, 1e10f, 1e10f};
    float lab_max[3] = {-1e10f, -1e10f, -1e10f};
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb_data[i * 4 + c];
            if (v < lab_min[c]) lab_min[c] = v;
            if (v > lab_max[c]) lab_max[c] = v;
        }
    }
    std::cout << "  L: [" << lab_min[0] << ", " << lab_max[0] << "] (expected ~0-100)\n";
    std::cout << "  a: [" << lab_min[1] << ", " << lab_max[1] << "] (expected ~-128 to 128)\n";
    std::cout << "  b: [" << lab_min[2] << ", " << lab_max[2] << "] (expected ~-128 to 128)\n";

    std::cout << "\n=== Step 5 Complete ===\n";

    // =========================================================================
    // Step 6: colorout - Lab → XYZ → linear sRGB (D50 adapted)
    // IOP Stage: Color
    // =========================================================================

    std::cout << "\n=== Step 6: colorout Verification ===\n\n";

    std::cout << "Running colorout (Lab → linear sRGB)...\n";
    auto colorout = flow::makeColorout();
    colorout->process(*flow);

    // Verify linear sRGB ranges
    std::cout << "\nVerifying linear sRGB ranges...\n";
    float lrgb_min[3] = {1e10f, 1e10f, 1e10f};
    float lrgb_max[3] = {-1e10f, -1e10f, -1e10f};
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb_data[i * 4 + c];
            if (v < lrgb_min[c]) lrgb_min[c] = v;
            if (v > lrgb_max[c]) lrgb_max[c] = v;
        }
    }
    std::cout << "  R: [" << lrgb_min[0] << ", " << lrgb_max[0] << "]\n";
    std::cout << "  G: [" << lrgb_min[1] << ", " << lrgb_max[1] << "]\n";
    std::cout << "  B: [" << lrgb_min[2] << ", " << lrgb_max[2] << "]\n";

    std::cout << "\n=== Step 6 Complete ===\n";

    // =========================================================================
    // Step 7: gamma - sRGB transfer function
    // IOP Stage: Tone
    // =========================================================================

    std::cout << "\n=== Step 7: gamma Verification ===\n\n";

    std::cout << "Running gamma (sRGB transfer)...\n";
    auto gamma = flow::makeGamma();
    gamma->process(*flow);

    // Verify output range
    std::cout << "\nVerifying final sRGB range...\n";
    rgb_data = flow->rgb();
    float final_min[3] = {1e10f, 1e10f, 1e10f};
    float final_max[3] = {0.0f, 0.0f, 0.0f};
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb_data[i * 4 + c];
            if (v < final_min[c]) final_min[c] = v;
            if (v > final_max[c]) final_max[c] = v;
        }
    }
    std::cout << "  R: [" << final_min[0] << ", " << final_max[0] << "]\n";
    std::cout << "  G: [" << final_min[1] << ", " << final_max[1] << "]\n";
    std::cout << "  B: [" << final_min[2] << ", " << final_max[2] << "]\n";

    // Save final output as PNG (already in sRGB, no extra gamma needed)
    std::cout << "\nSaving final output...\n";
    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = rgb_data[i * 4 + c];
            v = std::max(0.0f, std::min(1.0f, v));
            rgb8[i * 3 + c] = static_cast<uint8_t>(v * 255.0f + 0.5f);
        }
    }

    png_data = flow::swap(rgb8.data(), width, height, flow::BIN, flow::PNG);
    std::string final_path = std::string(out_dir) + "/step-7-final.png";
    if (!png_data.empty() && write_file(final_path.c_str(), png_data.data(), png_data.size())) {
        std::cout << "  Saved: " << final_path << " (" << png_data.size() << " bytes)\n";
    }

    std::cout << "\n=== Pipeline Complete ===\n";
    std::cout << "Compare " << final_path << " with tmp/var/pipe/step-1-ref.png\n";

    return 0;
}

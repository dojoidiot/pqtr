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
    return 0;
}

// head_sony_test.cpp - Test Sony ARW decoder against LibRaw reference
//
// Usage: head_sony_test
// Reads: src/test/raws/sony.ARW
// Compares: our output vs src/test/dark/head_sony_bayer.bin
// Uses: diff --bits

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>
#include "../../main/labs/plug/sony/sony.h"

static std::vector<uint8_t> read_file(const char* path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return {};
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

static bool write_file(const char* path, const void* data, size_t size)
{
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data), size);
    return true;
}

int main()
{
    printf("=== Sony ARW Head Test ===\n");

    // Load RAW file
    auto raw_data = read_file("src/test/raws/sony.ARW");
    if (raw_data.empty()) {
        fprintf(stderr, "Cannot read src/test/raws/sony.ARW\n");
        return 1;
    }
    printf("Loaded RAW: %zu bytes\n", raw_data.size());

    // Decode with our decoder
    sony::BayerU16 bayer;
    sony::Info info;
    sony::RawMetadata meta;

    if (!sony::Decoder::prepare(raw_data.data(), raw_data.size(), bayer, info, meta)) {
        fprintf(stderr, "Sony decoder failed\n");
        return 1;
    }

    printf("Our decoder:\n");
    printf("  Dimensions: %d x %d\n", meta.width, meta.height);
    printf("  Black: %d, White: %d\n", meta.black_level, meta.white_level);
    printf("  WB: R=%d G1=%d B=%d G2=%d\n",
           meta.wb_rggb[0], meta.wb_rggb[1], meta.wb_rggb[2], meta.wb_rggb[3]);
    printf("  Bayer: %zu pixels (%zu bytes)\n", bayer.size(), bayer.bytes());

    // Write our output
    const char* out_path = "/tmp/head_sony_bayer.bin";
    if (!write_file(out_path, bayer.ptr(), bayer.bytes())) {
        fprintf(stderr, "Cannot write output\n");
        return 1;
    }
    printf("Wrote: %s\n", out_path);

    // Load reference
    auto ref_data = read_file("src/test/dark/head_sony_bayer.bin");
    if (ref_data.empty()) {
        fprintf(stderr, "Cannot read reference\n");
        return 1;
    }
    printf("Reference: %zu bytes\n", ref_data.size());

    // Compare sizes
    if (bayer.bytes() != ref_data.size()) {
        printf("\nFAIL: Size mismatch!\n");
        printf("  Our output: %zu bytes (%d x %d = %zu pixels)\n",
               bayer.bytes(), meta.width, meta.height, bayer.size());
        printf("  Reference:  %zu bytes (%zu pixels)\n",
               ref_data.size(), ref_data.size() / 2);

        // Reference is 6048 x 4024 = 24337152 pixels = 48674304 bytes
        int ref_w = 6048, ref_h = 4024;
        if (ref_data.size() == (size_t)ref_w * ref_h * 2) {
            printf("  Reference appears to be %d x %d (full sensor)\n", ref_w, ref_h);
        }
        return 1;
    }

    // Run diff
    printf("\nRunning diff...\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "./tmp/build/diff src/test/dark/head_sony_bayer.bin %s --bits",
             out_path);
    int ret = system(cmd);

    return ret;
}

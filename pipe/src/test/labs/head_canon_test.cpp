// head_canon_test.cpp - Test Canon CR2 decoder against LibRaw reference
//
// Usage: head_canon_test
// Reads: src/test/raws/canon.cr2
// Compares: our output vs src/test/dark/head_canon_bayer.bin
// Uses: diff --bits

#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>
#include "../../main/labs/plug/canon/canon.h"

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
    printf("=== Canon CR2 Head Test ===\n");

    // Load RAW file
    auto raw_data = read_file("src/test/raws/canon.cr2");
    if (raw_data.empty()) {
        fprintf(stderr, "Cannot read src/test/raws/canon.cr2\n");
        return 1;
    }
    printf("Loaded RAW: %zu bytes\n", raw_data.size());

    // Decode with our decoder
    canon::BayerU16 bayer;
    canon::RawMetadata meta;

    if (!canon::Decoder::prepare(raw_data.data(), raw_data.size(), bayer, meta)) {
        fprintf(stderr, "Canon decoder failed\n");
        return 1;
    }

    printf("Our decoder:\n");
    printf("  Dimensions: %d x %d\n", meta.width, meta.height);
    printf("  Black: %d, White: %d\n", meta.black_level, meta.white_level);
    printf("  WB: R=%d G1=%d G2=%d B=%d\n",
           meta.wb_rggb[0], meta.wb_rggb[1], meta.wb_rggb[2], meta.wb_rggb[3]);
    printf("  Bayer: %zu pixels (%zu bytes)\n", bayer.size(), bayer.bytes());

    // Write our output
    const char* out_path = "/tmp/head_canon_bayer.bin";
    if (!write_file(out_path, bayer.ptr(), bayer.bytes())) {
        fprintf(stderr, "Cannot write output\n");
        return 1;
    }
    printf("Wrote: %s\n", out_path);

    // Load reference
    auto ref_data = read_file("src/test/dark/head_canon_bayer.bin");
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

        // Reference is 3944 x 2622 = 10341168 pixels = 20682336 bytes
        int ref_w = 3944, ref_h = 2622;
        if (ref_data.size() == (size_t)ref_w * ref_h * 2) {
            printf("  Reference appears to be %d x %d (full sensor)\n", ref_w, ref_h);
        }
        return 1;
    }

    // Run diff
    printf("\nRunning diff...\n");
    fflush(stdout);
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "./tmp/build/diff src/test/dark/head_canon_bayer.bin %s --bits",
             out_path);
    int ret = system(cmd);

    return ret;
}

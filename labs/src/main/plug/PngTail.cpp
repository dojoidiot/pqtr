// PngTail.cpp - saves flow data as PNG (using stb_image_write for compatibility with pipe)

#include "pqtr.hpp"
#include <iostream>
#include <vector>
#include <cstdint>
#include <sys/stat.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../../../pipe/src/main/labs/stb_image_write.h"

namespace pqtr::Labs {

class PngTail : public Tail
{
    std::string path_;
public:
    explicit PngTail(const std::string &path) : path_(path) {}
    void *save(Flow &flow) override;
};

void* PngTail::save(Flow& flow) {
    int width = flow.width();
    int height = flow.height();
    size_t npixels = static_cast<size_t>(width) * height;

    // Ensure parent directory exists
    size_t last_slash = path_.rfind('/');
    if (last_slash != std::string::npos) {
        std::string dir = path_.substr(0, last_slash);
        mkdir(dir.c_str(), 0755);
    }

    // Convert float RGBA (0-1) to uint8 RGB - exactly as pipe/gold.cpp
    float* src = static_cast<float*>(flow.data());
    std::vector<uint8_t> png_data(npixels * 3);

    for (size_t i = 0; i < npixels; i++) {
        for (int c = 0; c < 3; c++) {
            float v = src[i * 4 + c];
            v = (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;
            png_data[i * 3 + c] = static_cast<uint8_t>(v * 255.0f + 0.5f);
        }
    }

    // Write PNG using stb_image_write (same as pipe/gold.cpp)
    if (!stbi_write_png(path_.c_str(), width, height, 3, png_data.data(), width * 3)) {
        std::cerr << "PngTail: failed to write " << path_ << "\n";
        return nullptr;
    }

    std::cout << "PngTail: saved " << path_ << " (" << width << "x" << height << ")\n";

    return nullptr;
}

std::unique_ptr<Tail> pngTail(const std::string &path) {
    return std::make_unique<PngTail>(path);
}

}  // namespace pqtr::Labs

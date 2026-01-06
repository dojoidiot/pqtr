#pragma once

#include <cstdio>
#include <vector>
#include <string>
#include "core/image_buffer.hpp"

namespace copy::debug {

    template <typename T>
    void dump_buffer(const std::string& name, const core::ImageBuffer<T>& buf) {
        std::string filename = "dump_" + name + ".bin";
        FILE* f = std::fopen(filename.c_str(), "wb");
        if (!f) return;
        std::fwrite(buf.data(), sizeof(T), buf.count(), f);
        std::fclose(f);
        std::printf("Dumped %s (%dx%dx%d)\n", filename.c_str(), buf.width(), buf.height(), buf.channels());
    }

}

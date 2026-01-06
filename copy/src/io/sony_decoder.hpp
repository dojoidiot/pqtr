#pragma once

#include "../core/types.hpp"
#include "../core/metadata.hpp"
#include "../core/image_buffer.hpp"
#include <string>
#include <vector>

namespace copy::io {

    class SonyDecoder {
    public:
        static bool read_meta(const std::string& filename, core::MetaData& meta);
        static bool decode(const std::string& filename, const core::MetaData& meta, core::ImageBuffer<core::u16>& out_buffer);

    private:
        static void decrypt(uint8_t* data, uint32_t length, uint32_t key);
    };

}

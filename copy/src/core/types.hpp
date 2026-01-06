#pragma once

#include <cstdint>
#include <cstddef>

namespace copy::core {

    // Common types
    using u16 = uint16_t;
    using f32 = float;
    
    // Aligned pixel type for float image buffers (4 channels)
    using Pixel = float[4];

}

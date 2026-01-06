#include "rawprepare.hpp"
#include <cmath>

namespace copy::modules::rawprepare {

    void process(const core::ImageBuffer<core::u16>& in, core::ImageBuffer<core::f32>& out, const Params& p) {
        float sub[4];
        float div[4];
        float white = static_cast<float>(p.raw_white_point);

        for (int i = 0; i < 4; i++) {
            sub[i] = static_cast<float>(p.raw_black_level_separate[i]);
            div[i] = white - sub[i];
        }

        int width = in.width();
        int height = in.height();
        
        // Ensure output buffer has correct dimensions
        if (out.width() != width || out.height() != height) {
            // Should be handled by caller, but for safety
        }

        const core::u16* input_data = in.data();
        core::f32* output_data = out.data();

        #pragma omp parallel for
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                // Bayer pattern index: (row & 1) * 2 + (col & 1)
                // Assuming top-left crop is 0,0 for full image
                int id = ((j + p.top) & 1) * 2 + ((i + p.left) & 1);
                
                size_t idx = static_cast<size_t>(j) * width + i;
                output_data[idx] = (static_cast<float>(input_data[idx]) - sub[id]) / div[id];
            }
        }
    }

}

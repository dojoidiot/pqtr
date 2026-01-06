#include "exposure.hpp"
#include <cmath>

namespace copy::modules::exposure {

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const Params& p) {
        float black = 0.0f;
        float white = std::exp2(-p.exposure);
        float scale = 1.0f / (white - black);
        size_t count = in.count();
        const float* input = in.data();
        float* output = out.data();

        #pragma omp parallel for
        for(size_t k=0; k<count; k++) {
            output[k] = (input[k] - black) * scale;
        }
    }

}

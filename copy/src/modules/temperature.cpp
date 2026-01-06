#include "temperature.hpp"

namespace copy::modules::temperature {

    static inline int FC(int row, int col, uint32_t filters) {
        return (filters >> (((row << 1 & 14) + (col & 1)) << 1)) & 3;
    }

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, core::PipeState& state, const Params& p) {
        state.temperature.enabled = 1;
        for(int k=0; k<4; k++) state.temperature.coeffs[k] = p.coeffs[k];

        int width = in.width();
        int height = in.height();
        const core::f32* input_data = in.data();
        core::f32* output_data = out.data();
        uint32_t filters = state.filters;

        #pragma omp parallel for
        for(int j=0; j<height; j++) {
            for(int i=0; i<width; i++) {
                size_t idx = static_cast<size_t>(j) * width + i;
                output_data[idx] = input_data[idx] * p.coeffs[FC(j, i, filters)];
            }
        }
    }

}

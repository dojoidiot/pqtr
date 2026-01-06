#include "colorin.hpp"
#include <cmath>
#include <algorithm>

namespace copy::modules::colorin {

    static const float XYZ_to_REC2020[4][4] = {
        { 1.647250295f, -0.393625855f, -0.235971376f, 0.f },
        { -0.682616651f, 1.647609591f, 0.012813044f, 0.f },
        { 0.029678674f, -0.062945843f, 1.253884912f, 0.f },
        { 0.f, 0.f, 0.f, 0.f }
    };

    void process(const core::ImageBuffer<core::f32>& in, core::ImageBuffer<core::f32>& out, const float cam_to_xyz[3][3]) {
        size_t npixels = in.count() / 4; // Assume 4 channels
        const float* input = in.data();
        float* output = out.data();

        #pragma omp parallel for
        for (size_t i = 0; i < npixels; i++) {
            const float* pin = input + i * 4;
            float* pout = output + i * 4;

            float xyz[3];
            xyz[0] = cam_to_xyz[0][0] * pin[0] + cam_to_xyz[0][1] * pin[1] + cam_to_xyz[0][2] * pin[2];
            xyz[1] = cam_to_xyz[1][0] * pin[0] + cam_to_xyz[1][1] * pin[1] + cam_to_xyz[1][2] * pin[2];
            xyz[2] = cam_to_xyz[2][0] * pin[0] + cam_to_xyz[2][1] * pin[1] + cam_to_xyz[2][2] * pin[2];

            pout[0] = XYZ_to_REC2020[0][0] * xyz[0] + XYZ_to_REC2020[0][1] * xyz[1] + XYZ_to_REC2020[0][2] * xyz[2];
            pout[1] = XYZ_to_REC2020[1][0] * xyz[0] + XYZ_to_REC2020[1][1] * xyz[1] + XYZ_to_REC2020[1][2] * xyz[2];
            pout[2] = XYZ_to_REC2020[2][0] * xyz[0] + XYZ_to_REC2020[2][1] * xyz[1] + XYZ_to_REC2020[2][2] * xyz[2];
            pout[3] = 0.0f;
        }
    }

}
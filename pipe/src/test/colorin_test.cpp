// colorin_test.cpp - Test camera→XYZ matrix step
//
// Minimal test: load raw, apply WB, apply cam→XYZ, output
// Compare with DT's colorin output to verify matrix

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <fstream>

// LibRaw for reference
#include <libraw/libraw.h>

int main(int argc, char** argv)
{
    const char* raw_file = "src/test/DSC00144.ARW";

    // Load with LibRaw to get matrices
    LibRaw raw;
    if (raw.open_file(raw_file) != LIBRAW_SUCCESS) {
        printf("Failed to open raw file\n");
        return 1;
    }
    raw.unpack();

    // Get camera→XYZ matrix (cam_xyz)
    printf("cam_xyz (camera→XYZ):\n");
    float cam_xyz[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            cam_xyz[i][j] = raw.imgdata.rawdata.color.cam_xyz[i][j];
            printf("  %9.6f", cam_xyz[i][j]);
        }
        printf("\n");
    }

    // Get WB coefficients (as-shot)
    printf("\ncam_mul (as-shot WB):\n");
    float wb[4];
    for (int i = 0; i < 4; i++) {
        wb[i] = raw.imgdata.color.cam_mul[i];
        printf("  [%d] %f\n", i, wb[i]);
    }

    // Normalize WB (green = 1.0)
    float wb_norm[3] = {
        wb[0] / wb[1],  // R
        1.0f,           // G
        wb[2] / wb[1]   // B
    };
    printf("\nNormalized WB: R=%.6f G=%.6f B=%.6f\n", wb_norm[0], wb_norm[1], wb_norm[2]);

    // XMP WB (from default.xmp temperature module)
    float xmp_wb[3] = { 2.37890625f, 1.0f, 1.56640625f };
    printf("XMP WB:        R=%.6f G=%.6f B=%.6f\n", xmp_wb[0], xmp_wb[1], xmp_wb[2]);

    // Test with a sample camera RGB value
    // This simulates a pixel after demosaic but before WB
    float test_cam[3] = { 0.3f, 0.4f, 0.35f };

    printf("\n=== Test Pipeline ===\n");
    printf("Input camera RGB: [%.4f, %.4f, %.4f]\n", test_cam[0], test_cam[1], test_cam[2]);

    // Step 1: Apply WB (using XMP values to match DT)
    float after_wb[3] = {
        test_cam[0] * xmp_wb[0],
        test_cam[1] * xmp_wb[1],
        test_cam[2] * xmp_wb[2]
    };
    printf("After WB:         [%.4f, %.4f, %.4f]\n", after_wb[0], after_wb[1], after_wb[2]);

    // Step 2: Apply camera→XYZ matrix
    float xyz[3] = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            xyz[i] += cam_xyz[i][j] * after_wb[j];
        }
    }
    printf("After cam→XYZ:    [%.4f, %.4f, %.4f]\n", xyz[0], xyz[1], xyz[2]);

    // D50 white point for Lab
    const float d50[3] = { 0.9642f, 1.0f, 0.8249f };

    // Step 3: XYZ → Lab (D50)
    auto lab_f = [](float t) -> float {
        const float delta = 6.0f / 29.0f;
        if (t > delta * delta * delta)
            return std::cbrt(t);
        else
            return t / (3.0f * delta * delta) + 4.0f / 29.0f;
    };

    float fx = lab_f(xyz[0] / d50[0]);
    float fy = lab_f(xyz[1] / d50[1]);
    float fz = lab_f(xyz[2] / d50[2]);

    float lab[3] = {
        116.0f * fy - 16.0f,
        500.0f * (fx - fy),
        200.0f * (fy - fz)
    };
    printf("After XYZ→Lab:    [%.4f, %.4f, %.4f]\n", lab[0], lab[1], lab[2]);

    // Step 4: Lab → XYZ (D50)
    auto lab_f_inv = [](float t) -> float {
        const float delta = 6.0f / 29.0f;
        if (t > delta)
            return t * t * t;
        else
            return 3.0f * delta * delta * (t - 4.0f / 29.0f);
    };

    float fy2 = (lab[0] + 16.0f) / 116.0f;
    float fx2 = lab[1] / 500.0f + fy2;
    float fz2 = fy2 - lab[2] / 200.0f;

    float xyz2[3] = {
        d50[0] * lab_f_inv(fx2),
        d50[1] * lab_f_inv(fy2),
        d50[2] * lab_f_inv(fz2)
    };
    printf("After Lab→XYZ:    [%.4f, %.4f, %.4f]\n", xyz2[0], xyz2[1], xyz2[2]);

    // Step 5: XYZ → sRGB (D50 adapted matrix from DT)
    // From colorspaces_inline_conversions.h line 497-500
    const float xyz_to_srgb[3][3] = {
        {  3.1338561f, -1.6168667f, -0.4906146f },
        { -0.9787684f,  1.9161415f,  0.0334540f },
        {  0.0719453f, -0.2289914f,  1.4052427f }
    };

    float linear_srgb[3] = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            linear_srgb[i] += xyz_to_srgb[i][j] * xyz2[j];
        }
    }
    printf("After XYZ→sRGB:   [%.4f, %.4f, %.4f]\n", linear_srgb[0], linear_srgb[1], linear_srgb[2]);

    // Step 6: sRGB gamma
    auto srgb_gamma = [](float v) -> float {
        v = std::max(0.0f, std::min(1.0f, v));
        if (v <= 0.0031308f)
            return 12.92f * v;
        else
            return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
    };

    float final_srgb[3] = {
        srgb_gamma(linear_srgb[0]),
        srgb_gamma(linear_srgb[1]),
        srgb_gamma(linear_srgb[2])
    };
    printf("After gamma:      [%.4f, %.4f, %.4f]\n", final_srgb[0], final_srgb[1], final_srgb[2]);

    // Now compare with our current approach (direct cam→sRGB)
    printf("\n=== Our Current Approach ===\n");

    // LibRaw's rgb_cam (camera→sRGB normalized)
    const float rgb_cam[3][3] = {
        { 1.635386f, -0.421962f, -0.213424f },
        { -0.131072f, 1.542707f, -0.411635f },
        { 0.011430f, -0.419593f, 1.408163f }
    };

    float our_srgb[3] = {0};
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            our_srgb[i] += rgb_cam[i][j] * after_wb[j];
        }
    }
    printf("Our cam→sRGB:     [%.4f, %.4f, %.4f]\n", our_srgb[0], our_srgb[1], our_srgb[2]);

    float our_final[3] = {
        srgb_gamma(our_srgb[0]),
        srgb_gamma(our_srgb[1]),
        srgb_gamma(our_srgb[2])
    };
    printf("Our after gamma:  [%.4f, %.4f, %.4f]\n", our_final[0], our_final[1], our_final[2]);

    printf("\n=== Comparison ===\n");
    printf("DT pipeline:  [%.4f, %.4f, %.4f]\n", final_srgb[0], final_srgb[1], final_srgb[2]);
    printf("Our pipeline: [%.4f, %.4f, %.4f]\n", our_final[0], our_final[1], our_final[2]);
    printf("Ratio (ours/DT): [%.4f, %.4f, %.4f]\n",
           our_final[0]/final_srgb[0],
           our_final[1]/final_srgb[1],
           our_final[2]/final_srgb[2]);

    return 0;
}

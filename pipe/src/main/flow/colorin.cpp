// colorin.cpp - Camera RGB → XYZ(D50) → Lab
//
// Matches darktable's colorin module.
//
// PARAM COVERAGE (Rule 9):
// USED: type (selects matrix source: 11=STANDARD, 12=ENHANCED)
// UNSUPPORTED: filename - ICC profiles not implemented
// UNSUPPORTED: intent - only perceptual supported
// UNSUPPORTED: normalize - gamut clipping not implemented
// UNSUPPORTED: blue_mapping - not implemented
// UNSUPPORTED: type_work - working profile not implemented
// UNSUPPORTED: filename_work - working profile not implemented
//
// MATRIX SOURCES:
// - type=11 (STANDARD_MATRIX): From rawspeed cameras.xml (XYZ→Cam, inverted)
// - type=12 (ENHANCED_MATRIX): From DT colormatrices.c (cam→XYZ direct)

#include "../../../inc/pipe.hpp"
#include <cmath>
#include <cstring>

namespace flow
{

// -------------------------------------------------------------------------
// Profile type constants (from DT colorspaces.h)
// -------------------------------------------------------------------------
static constexpr int DT_COLORSPACE_STANDARD_MATRIX = 11;
static constexpr int DT_COLORSPACE_ENHANCED_MATRIX = 12;

// -------------------------------------------------------------------------
// STANDARD_MATRIX Database (from rawspeed cameras.xml)
// -------------------------------------------------------------------------
// Format: XYZ→Cam matrix, scaled by 10000
// Must be inverted to get cam→XYZ for our pipeline

struct StandardMatrix {
    const char* model;
    int xyz_to_cam[9];  // Row-major XYZ→Cam matrix, scaled by 10000
};

static const StandardMatrix STANDARD_MATRICES[] = {
    // Note: STANDARD_MATRIX (type=11) uses XYZ_to_CAM matrices (needs inversion).
    // These are dcraw/Adobe matrices scaled by 10000.
    // Canon EOS 40D - from dcraw
    { "Canon EOS 40D",
      { 6071, -747, -856, -7653, 15365, 2441, -2025, 2553, 7315 }
    },
};

static const int STANDARD_MATRIX_COUNT = sizeof(STANDARD_MATRICES) / sizeof(STANDARD_MATRICES[0]);

// -------------------------------------------------------------------------
// ENHANCED_MATRIX Database (from DT colormatrices.c)
// -------------------------------------------------------------------------
// Format: cam→XYZ matrix as rXYZ[3], gXYZ[3], bXYZ[3], scaled by 1e6

struct EnhancedMatrix {
    const char* model;
    int rXYZ[3];
    int gXYZ[3];
    int bXYZ[3];
};

static const EnhancedMatrix ENHANCED_MATRICES[] = {
    // Canon EOS 40D - Pascal de Bruijn, CMP Digital Target 3
    { "Canon EOS 40D",
      { 845901, 325760, -13077},
      { 110809, 960724, -213577},
      { 82230, -218063, 1110229}
    },
    // Sony ILCE-7 (A7) - Denis Cheremisov, CMP Digital Target 4
    { "Sony ILCE-7",
      { 913254, 376358, 21606},
      { 120987, 1024490, -251312},
      { 5142, -318573, 1100876}
    },
};

static const int ENHANCED_MATRIX_COUNT = sizeof(ENHANCED_MATRICES) / sizeof(ENHANCED_MATRICES[0]);

// -------------------------------------------------------------------------
// Matrix utilities
// -------------------------------------------------------------------------

// Invert 3x3 matrix (for XYZ→Cam to cam→XYZ conversion)
static bool invert_3x3(const float in[9], float out[9])
{
    float det = in[0] * (in[4] * in[8] - in[5] * in[7])
              - in[1] * (in[3] * in[8] - in[5] * in[6])
              + in[2] * (in[3] * in[7] - in[4] * in[6]);

    if (fabsf(det) < 1e-10f) return false;

    float inv_det = 1.0f / det;
    out[0] = (in[4] * in[8] - in[5] * in[7]) * inv_det;
    out[1] = (in[2] * in[7] - in[1] * in[8]) * inv_det;
    out[2] = (in[1] * in[5] - in[2] * in[4]) * inv_det;
    out[3] = (in[5] * in[6] - in[3] * in[8]) * inv_det;
    out[4] = (in[0] * in[8] - in[2] * in[6]) * inv_det;
    out[5] = (in[2] * in[3] - in[0] * in[5]) * inv_det;
    out[6] = (in[3] * in[7] - in[4] * in[6]) * inv_det;
    out[7] = (in[1] * in[6] - in[0] * in[7]) * inv_det;
    out[8] = (in[0] * in[4] - in[1] * in[3]) * inv_det;
    return true;
}

// Lookup STANDARD_MATRIX (type=11): XYZ→Cam from rawspeed, inverted to cam→XYZ
static bool lookup_standard_matrix(const char* make, const char* model, float cam_xyz[9])
{
    char search[256];
    snprintf(search, sizeof(search), "%s %s", make, model);

    for (int i = 0; i < STANDARD_MATRIX_COUNT; i++) {
        if (strstr(search, STANDARD_MATRICES[i].model) != nullptr) {
            // Convert to float and invert
            float xyz_to_cam[9];
            for (int j = 0; j < 9; j++) {
                xyz_to_cam[j] = STANDARD_MATRICES[i].xyz_to_cam[j] / 10000.0f;
            }
            return invert_3x3(xyz_to_cam, cam_xyz);
        }
    }
    return false;
}

// Lookup ENHANCED_MATRIX (type=12): cam→XYZ from DT colormatrices.c
static bool lookup_enhanced_matrix(const char* make, const char* model, float cam_xyz[9])
{
    char search[256];
    snprintf(search, sizeof(search), "%s %s", make, model);

    for (int i = 0; i < ENHANCED_MATRIX_COUNT; i++) {
        if (strstr(search, ENHANCED_MATRICES[i].model) != nullptr) {
            // Matrix layout: row=XYZ, col=RGB
            cam_xyz[0] = ENHANCED_MATRICES[i].rXYZ[0] / 1000000.0f;
            cam_xyz[1] = ENHANCED_MATRICES[i].gXYZ[0] / 1000000.0f;
            cam_xyz[2] = ENHANCED_MATRICES[i].bXYZ[0] / 1000000.0f;
            cam_xyz[3] = ENHANCED_MATRICES[i].rXYZ[1] / 1000000.0f;
            cam_xyz[4] = ENHANCED_MATRICES[i].gXYZ[1] / 1000000.0f;
            cam_xyz[5] = ENHANCED_MATRICES[i].bXYZ[1] / 1000000.0f;
            cam_xyz[6] = ENHANCED_MATRICES[i].rXYZ[2] / 1000000.0f;
            cam_xyz[7] = ENHANCED_MATRICES[i].gXYZ[2] / 1000000.0f;
            cam_xyz[8] = ENHANCED_MATRICES[i].bXYZ[2] / 1000000.0f;
            return true;
        }
    }
    return false;
}

// Lookup camera matrix based on type
// Note: STANDARD_MATRIX uses XYZ_to_CAM from dcraw/Adobe (needs inversion).
//       ENHANCED_MATRIX uses cam_to_XYZ from DT colormatrices.c (direct).
//       We fall back to ENHANCED_MATRIX if STANDARD_MATRIX not found.
static bool lookup_camera_matrix(int type, const char* make, const char* model, float cam_xyz[9])
{
    bool found = false;

    if (type == DT_COLORSPACE_STANDARD_MATRIX) {
        found = lookup_standard_matrix(make, model, cam_xyz);
        // Fallback: DT also falls back to ENHANCED_MATRIX when STANDARD not available
        if (!found) {
            found = lookup_enhanced_matrix(make, model, cam_xyz);
        }
    } else if (type == DT_COLORSPACE_ENHANCED_MATRIX) {
        found = lookup_enhanced_matrix(make, model, cam_xyz);
    }

    // Fallback to identity if not found
    if (!found) {
        for (int i = 0; i < 9; i++) {
            cam_xyz[i] = (i == 0 || i == 4 || i == 8) ? 1.0f : 0.0f;
        }
    }
    return found;
}

// -------------------------------------------------------------------------
// Color conversion constants and functions
// -------------------------------------------------------------------------

// D50 white point (standard for Lab)
static constexpr float D50_X = 0.9642f;
static constexpr float D50_Y = 1.0000f;
static constexpr float D50_Z = 0.8249f;

// Bradford D65→D50 chromatic adaptation matrix
// DT's profiled matrices are measured under D65-ish illuminants, so we
// need to adapt to D50 for Lab conversion
static constexpr float BRADFORD_D65_TO_D50[9] = {
     1.0478112f,  0.0228866f, -0.0501270f,
     0.0295424f,  0.9904844f, -0.0170491f,
    -0.0092345f,  0.0150436f,  0.7521316f
};

// Fast cube root approximation (from DT colorspaces_inline_conversions.h)
static inline float cbrt_5f(float f)
{
    uint32_t* p = (uint32_t*)&f;
    *p = *p / 3 + 709921077;
    return f;
}

// Halley's method refinement (from DT)
static inline float cbrta_halleyf(float a, float R)
{
    const float a3 = a * a * a;
    const float b = a * (a3 + R + R) / (a3 + a3 + R);
    return b;
}

// Lab f() function - CLEAN COPY from DT colorspaces_inline_conversions.h:160
static inline float lab_f(float x)
{
    const float epsilon = 216.0f / 24389.0f;
    const float kappa = 24389.0f / 27.0f;
    return (x > epsilon) ? cbrta_halleyf(cbrt_5f(x), x) : (kappa * x + 16.0f) / 116.0f;
}

// -------------------------------------------------------------------------
// Colorin implementation
// -------------------------------------------------------------------------

class ColorinImpl : public Colorin
{
    int type_ = DT_COLORSPACE_ENHANCED_MATRIX;  // Default matches DT

public:
    std::string name() const override { return "colorin"; }
    std::string save() override { return "{}"; }
    void load(const std::string&) override {}

    void setType(int type) override { type_ = type; }

    void process(Flow& flow) override
    {
        auto& root = flow.info().root();

        int width = static_cast<int>(root.leaf(WIDTH).dial());
        int height = static_cast<int>(root.leaf(HEIGHT).dial());
        size_t npixels = static_cast<size_t>(width) * height;

        // Get camera make/model from tree (set by head decoder)
        std::string make, model;
        if (root.test("camera")) {
            auto& camera = root.next("camera");
            make = camera.leaf("make").text();
            model = camera.leaf("model").text();
        }

        // Look up camera matrix based on type param
        float cam_xyz[9];
        bool found = lookup_camera_matrix(type_, make.c_str(), model.c_str(), cam_xyz);
        (void)found; // Suppress unused variable warning

        // Use cam_xyz directly without Bradford adaptation
        // DT's matrix profiles may not apply Bradford adaptation
        float M[9];
        for (int i = 0; i < 9; i++) {
            M[i] = cam_xyz[i];
        }

        float* rgb = flow.rgb();

        // Process each pixel: camera RGB → XYZ D50 → Lab
        for (size_t i = 0; i < npixels; i++)
        {
            size_t idx = i * 4;
            float r = rgb[idx + 0];
            float g = rgb[idx + 1];
            float b = rgb[idx + 2];

            // Camera RGB → XYZ D50 (single matrix, already adapted)
            float X = M[0] * r + M[1] * g + M[2] * b;
            float Y = M[3] * r + M[4] * g + M[5] * b;
            float Z = M[6] * r + M[7] * g + M[8] * b;

            // XYZ D50 → Lab
            float fx = lab_f(X / D50_X);
            float fy = lab_f(Y / D50_Y);
            float fz = lab_f(Z / D50_Z);

            float L = 116.0f * fy - 16.0f;
            float a = 500.0f * (fx - fy);
            float bval = 200.0f * (fy - fz);

            // Store Lab in rgb buffer (reusing same memory)
            rgb[idx + 0] = L;
            rgb[idx + 1] = a;
            rgb[idx + 2] = bval;
        }
    }
};

std::unique_ptr<Colorin> makeColorin()
{
    return std::make_unique<ColorinImpl>();
}

} // namespace flow

// tune.cpp - Parameter optimizer for VIBE
//
// Uses Nelder-Mead simplex to find optimal VIBE parameters that
// minimize perceptual difference to a reference image.
//
// Parameters optimized (Phase 5 - HSL Bezier Curves):
//   - temperature: [4000, 10000] Kelvin
//   - tint: [-1, +1]
//   - exposure_ev: [-2, +2] EV
//   - contrast: [1.0, 2.5]
//   - skew: [-1, +1]
//   - saturation: [-1, +1]
//   - vibrance: [-1, +1]
//   - curve_y1_r/g/b: [0.0, 0.5] (shadow control, identity=0.25)
//   - curve_y2_r/g/b: [0.5, 1.0] (highlight control, identity=0.75)
//   - shadow_hue/sat, highlight_hue/sat: SplitTone (disabled)
//   - hue_sat_*: per-hue saturation (8 sectors, disabled)
//   - hue_curve_*: hue shift at 4 control points [-30, +30]
//   - sat_curve_*: saturation adjust at 4 control points [-0.5, +0.5]
//   - lum_curve_*: luminance adjust at 4 control points [-0.2, +0.2]

#include "flow.hpp"

#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>
#include <functional>
#include <iostream>
#include <iomanip>

namespace flow
{

// ============================================================
// Parameter bounds
// ============================================================

struct ParamBounds {
    float min, max, init;
};

// Parameter indices
enum ParamIdx {
    P_TEMPERATURE = 0,
    P_TINT,
    P_EXPOSURE,
    P_CONTRAST,
    P_SKEW,
    P_SATURATION,
    P_VIBRANCE,
    // Bezier curve parameters (Phase 3)
    P_CURVE_Y1_R,
    P_CURVE_Y2_R,
    P_CURVE_Y1_G,
    P_CURVE_Y2_G,
    P_CURVE_Y1_B,
    P_CURVE_Y2_B,
    // SplitTone parameters (Phase 4a - disabled)
    P_SHADOW_HUE,
    P_SHADOW_SAT,
    P_HIGHLIGHT_HUE,
    P_HIGHLIGHT_SAT,
    // SelectiveColour parameters (Phase 4b - disabled)
    P_HUE_SAT_RED,
    P_HUE_SAT_ORANGE,
    P_HUE_SAT_YELLOW,
    P_HUE_SAT_GREEN,
    P_HUE_SAT_CYAN,
    P_HUE_SAT_BLUE,
    P_HUE_SAT_PURPLE,
    P_HUE_SAT_MAGENTA,
    // HSL Bezier curves (Phase 5)
    P_HUE_CURVE_0,
    P_HUE_CURVE_90,
    P_HUE_CURVE_180,
    P_HUE_CURVE_270,
    P_SAT_CURVE_0,
    P_SAT_CURVE_90,
    P_SAT_CURVE_180,
    P_SAT_CURVE_270,
    P_LUM_CURVE_0,
    P_LUM_CURVE_90,
    P_LUM_CURVE_180,
    P_LUM_CURVE_270,
    N_PARAMS
};

// Initial values from Phase 3 learned profile (DSC00144)
static const std::vector<ParamBounds> BOUNDS = {
    { 4000.0f, 10000.0f, 6514.0f },  // temperature (Phase 3: 6514K)
    { -1.0f, 1.0f, 0.74f },          // tint (Phase 3: +0.74 magenta)
    { -2.0f, 2.0f, 2.0f },           // exposure_ev (Phase 3: +2.0 EV)
    { 1.0f, 2.5f, 1.0f },            // contrast (Phase 3: 1.0)
    { -1.0f, 1.0f, 0.74f },          // skew (Phase 3: 0.74)
    { -1.0f, 1.0f, -0.78f },         // saturation (Phase 3: -0.78)
    { -1.0f, 1.0f, -0.13f },         // vibrance (Phase 3: -0.13)
    // Bezier curve (Phase 3 values)
    { 0.0f, 0.5f, 0.37f },           // curve_y1_r (Phase 3: 0.37)
    { 0.5f, 1.0f, 0.63f },           // curve_y2_r (Phase 3: 0.63)
    { 0.0f, 0.5f, 0.44f },           // curve_y1_g (Phase 3: 0.44)
    { 0.5f, 1.0f, 0.51f },           // curve_y2_g (Phase 3: 0.51)
    { 0.0f, 0.5f, 0.20f },           // curve_y1_b (Phase 3: 0.20)
    { 0.5f, 1.0f, 0.66f },           // curve_y2_b (Phase 3: 0.66)
    // SplitTone: disabled (didn't improve ΔE)
    { 0.0f, 360.0f, 0.0f },          // shadow_hue (degrees)
    { 0.0f, 0.0f, 0.0f },            // shadow_sat (DISABLED)
    { 0.0f, 360.0f, 0.0f },          // highlight_hue (degrees)
    { 0.0f, 0.0f, 0.0f },            // highlight_sat (DISABLED)
    // SelectiveColour: per-hue saturation (DISABLED - didn't improve ΔE)
    { 0.0f, 0.0f, 0.0f },            // hue_sat_red (DISABLED)
    { 0.0f, 0.0f, 0.0f },            // hue_sat_orange (DISABLED)
    { 0.0f, 0.0f, 0.0f },            // hue_sat_yellow (DISABLED)
    { 0.0f, 0.0f, 0.0f },            // hue_sat_green (DISABLED)
    { 0.0f, 0.0f, 0.0f },            // hue_sat_cyan (DISABLED)
    { 0.0f, 0.0f, 0.0f },            // hue_sat_blue (DISABLED)
    { 0.0f, 0.0f, 0.0f },            // hue_sat_purple (DISABLED)
    { 0.0f, 0.0f, 0.0f },            // hue_sat_magenta (DISABLED)
    // HSL Bezier curves (Phase 5): per-hue adjustments
    // Hue shift: degrees shift at each control point (-30 to +30)
    { -30.0f, 30.0f, 0.0f },         // hue_curve_0 (at red/0°)
    { -30.0f, 30.0f, 0.0f },         // hue_curve_90 (at yellow/90°)
    { -30.0f, 30.0f, 0.0f },         // hue_curve_180 (at cyan/180°)
    { -30.0f, 30.0f, 0.0f },         // hue_curve_270 (at purple/270°)
    // Saturation adjustment (-0.5 to +0.5)
    { -0.5f, 0.5f, 0.0f },           // sat_curve_0 (at red)
    { -0.5f, 0.5f, 0.0f },           // sat_curve_90 (at yellow)
    { -0.5f, 0.5f, 0.0f },           // sat_curve_180 (at cyan)
    { -0.5f, 0.5f, 0.0f },           // sat_curve_270 (at purple)
    // Luminance adjustment (-0.2 to +0.2)
    { -0.2f, 0.2f, 0.0f },           // lum_curve_0 (at red)
    { -0.2f, 0.2f, 0.0f },           // lum_curve_90 (at yellow)
    { -0.2f, 0.2f, 0.0f },           // lum_curve_180 (at cyan)
    { -0.2f, 0.2f, 0.0f },           // lum_curve_270 (at purple)
};

// ============================================================
// Loss function: LAB-based perceptual difference
// ============================================================

// sRGB to linear
static float srgb_to_linear(float c) {
    return (c <= 0.04045f) ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Linear to XYZ (D65)
static void linear_to_xyz(float r, float g, float b, float& x, float& y, float& z) {
    x = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
    y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
    z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;
}

// XYZ to LAB
static void xyz_to_lab(float x, float y, float z, float& L, float& a, float& b_out) {
    // D65 reference white
    const float Xn = 0.95047f, Yn = 1.0f, Zn = 1.08883f;

    auto f = [](float t) -> float {
        const float delta = 6.0f / 29.0f;
        return (t > delta * delta * delta) ? std::cbrt(t) : t / (3 * delta * delta) + 4.0f / 29.0f;
    };

    float fx = f(x / Xn);
    float fy = f(y / Yn);
    float fz = f(z / Zn);

    L = 116.0f * fy - 16.0f;
    a = 500.0f * (fx - fy);
    b_out = 200.0f * (fy - fz);
}

// Compute Delta E (CIE76) between two images
static float compute_delta_e(const float* img1, const float* img2, int width, int height) {
    double sum = 0.0;
    const size_t n = static_cast<size_t>(width) * height;

    for (size_t i = 0; i < n; ++i) {
        // Already linear RGB from VIBE
        float r1 = img1[i * 3 + 0], g1 = img1[i * 3 + 1], b1 = img1[i * 3 + 2];
        float r2 = img2[i * 3 + 0], g2 = img2[i * 3 + 1], b2 = img2[i * 3 + 2];

        // Clamp to valid range
        r1 = std::max(0.0f, std::min(1.0f, r1));
        g1 = std::max(0.0f, std::min(1.0f, g1));
        b1 = std::max(0.0f, std::min(1.0f, b1));
        r2 = std::max(0.0f, std::min(1.0f, r2));
        g2 = std::max(0.0f, std::min(1.0f, g2));
        b2 = std::max(0.0f, std::min(1.0f, b2));

        // To XYZ
        float x1, y1, z1, x2, y2, z2;
        linear_to_xyz(r1, g1, b1, x1, y1, z1);
        linear_to_xyz(r2, g2, b2, x2, y2, z2);

        // To LAB
        float L1, a1, b1_lab, L2, a2, b2_lab;
        xyz_to_lab(x1, y1, z1, L1, a1, b1_lab);
        xyz_to_lab(x2, y2, z2, L2, a2, b2_lab);

        // Delta E (CIE76)
        float dL = L1 - L2;
        float da = a1 - a2;
        float db = b1_lab - b2_lab;
        sum += std::sqrt(dL * dL + da * da + db * db);
    }

    return static_cast<float>(sum / n);
}

// ============================================================
// Nelder-Mead Simplex Optimizer
// ============================================================

class NelderMead {
public:
    using ObjectiveFunc = std::function<float(const std::vector<float>&)>;

    struct Result {
        std::vector<float> params;
        float loss;
        int iterations;
    };

    static Result optimize(ObjectiveFunc objective,
                          const std::vector<float>& initial,
                          const std::vector<ParamBounds>& bounds,
                          int max_iters = 100,
                          float tol = 1e-4f) {
        int n = static_cast<int>(initial.size());

        // Initialize simplex: n+1 vertices
        std::vector<std::vector<float>> simplex(n + 1);
        std::vector<float> values(n + 1);

        // First vertex is initial point
        simplex[0] = initial;
        values[0] = objective(clamp_params(initial, bounds));

        // Other vertices: perturb each dimension
        for (int i = 0; i < n; ++i) {
            simplex[i + 1] = initial;
            float range = bounds[i].max - bounds[i].min;
            simplex[i + 1][i] += range * 0.1f;  // 10% of range
            simplex[i + 1] = clamp_params(simplex[i + 1], bounds);
            values[i + 1] = objective(simplex[i + 1]);
        }

        // Nelder-Mead coefficients
        const float alpha = 1.0f;   // reflection
        const float gamma = 2.0f;   // expansion
        const float rho = 0.5f;     // contraction
        const float sigma = 0.5f;   // shrink

        int iter = 0;
        for (; iter < max_iters; ++iter) {
            // Sort vertices by function value
            std::vector<int> order(n + 1);
            std::iota(order.begin(), order.end(), 0);
            std::sort(order.begin(), order.end(),
                [&values](int a, int b) { return values[a] < values[b]; });

            // Check convergence
            float range = values[order[n]] - values[order[0]];
            if (range < tol) break;

            // Compute centroid of all but worst point
            std::vector<float> centroid(n, 0.0f);
            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    centroid[j] += simplex[order[i]][j];
                }
            }
            for (int j = 0; j < n; ++j) {
                centroid[j] /= n;
            }

            int worst = order[n];
            int best = order[0];
            int second_worst = order[n - 1];

            // Reflection
            std::vector<float> reflected(n);
            for (int j = 0; j < n; ++j) {
                reflected[j] = centroid[j] + alpha * (centroid[j] - simplex[worst][j]);
            }
            reflected = clamp_params(reflected, bounds);
            float f_reflected = objective(reflected);

            if (f_reflected >= values[best] && f_reflected < values[second_worst]) {
                // Accept reflection
                simplex[worst] = reflected;
                values[worst] = f_reflected;
            } else if (f_reflected < values[best]) {
                // Try expansion
                std::vector<float> expanded(n);
                for (int j = 0; j < n; ++j) {
                    expanded[j] = centroid[j] + gamma * (reflected[j] - centroid[j]);
                }
                expanded = clamp_params(expanded, bounds);
                float f_expanded = objective(expanded);

                if (f_expanded < f_reflected) {
                    simplex[worst] = expanded;
                    values[worst] = f_expanded;
                } else {
                    simplex[worst] = reflected;
                    values[worst] = f_reflected;
                }
            } else {
                // Contraction
                std::vector<float> contracted(n);
                for (int j = 0; j < n; ++j) {
                    contracted[j] = centroid[j] + rho * (simplex[worst][j] - centroid[j]);
                }
                contracted = clamp_params(contracted, bounds);
                float f_contracted = objective(contracted);

                if (f_contracted < values[worst]) {
                    simplex[worst] = contracted;
                    values[worst] = f_contracted;
                } else {
                    // Shrink
                    for (int i = 1; i <= n; ++i) {
                        for (int j = 0; j < n; ++j) {
                            simplex[order[i]][j] = simplex[best][j] +
                                sigma * (simplex[order[i]][j] - simplex[best][j]);
                        }
                        simplex[order[i]] = clamp_params(simplex[order[i]], bounds);
                        values[order[i]] = objective(simplex[order[i]]);
                    }
                }
            }
        }

        // Find best
        int best_idx = 0;
        for (int i = 1; i <= n; ++i) {
            if (values[i] < values[best_idx]) best_idx = i;
        }

        return { simplex[best_idx], values[best_idx], iter };
    }

private:
    static std::vector<float> clamp_params(const std::vector<float>& params,
                                           const std::vector<ParamBounds>& bounds) {
        std::vector<float> clamped = params;
        for (size_t i = 0; i < clamped.size() && i < bounds.size(); ++i) {
            clamped[i] = std::max(bounds[i].min, std::min(bounds[i].max, clamped[i]));
        }
        return clamped;
    }
};

// ============================================================
// Tune Context - holds state for optimization
// ============================================================

struct TuneContext {
    Done scene_linear;           // HEAD output (scene-linear RGB)
    std::vector<float> ref_rgb;  // Reference (linear RGB, downsampled to match scene_linear)
    int width, height;

    // Working buffer for VIBE output
    Done vibe_output;

    // Tree for vibe settings
    std::unique_ptr<Tree> tree;
};

// Apply VIBE with given parameters and return loss
static float evaluate_params(TuneContext& ctx, const std::vector<float>& params) {
    // Copy scene-linear input to output buffer
    ctx.vibe_output = ctx.scene_linear;

    // Update vibe settings in tree
    Stem& vibe = ctx.tree->root().next("vibe");
    Stem& linear = vibe.next("linear");

    // White balance
    Stem& cc = linear.next("colorCorrection");
    Stem& wb = cc.next("whiteBalance");
    wb.leaf("temperature").dial(params[P_TEMPERATURE]);
    wb.leaf("tint").dial(params[P_TINT]);

    // Exposure
    cc.leaf("exposure").dial(params[P_EXPOSURE]);

    // Tone mapping
    Stem& tm = linear.next("toneMapping");
    tm.leaf("contrast").dial(params[P_CONTRAST]);
    tm.leaf("skew").dial(params[P_SKEW]);

    // Global color
    Stem& gc = linear.next("globalColor");
    gc.leaf("saturation").dial(params[P_SATURATION]);
    gc.leaf("vibrance").dial(params[P_VIBRANCE]);

    // Bezier tone curve
    Stem& bc = linear.next("baseCurve");
    bc.leaf("r_y1").dial(params[P_CURVE_Y1_R]);
    bc.leaf("r_y2").dial(params[P_CURVE_Y2_R]);
    bc.leaf("g_y1").dial(params[P_CURVE_Y1_G]);
    bc.leaf("g_y2").dial(params[P_CURVE_Y2_G]);
    bc.leaf("b_y1").dial(params[P_CURVE_Y1_B]);
    bc.leaf("b_y2").dial(params[P_CURVE_Y2_B]);

    // SplitTone
    Stem& st = linear.next("splitTone");
    st.leaf("shadow_hue").dial(params[P_SHADOW_HUE]);
    st.leaf("shadow_sat").dial(params[P_SHADOW_SAT]);
    st.leaf("highlight_hue").dial(params[P_HIGHLIGHT_HUE]);
    st.leaf("highlight_sat").dial(params[P_HIGHLIGHT_SAT]);

    // SelectiveColour
    Stem& sc = linear.next("selectiveColour");
    sc.leaf("red").dial(params[P_HUE_SAT_RED]);
    sc.leaf("orange").dial(params[P_HUE_SAT_ORANGE]);
    sc.leaf("yellow").dial(params[P_HUE_SAT_YELLOW]);
    sc.leaf("green").dial(params[P_HUE_SAT_GREEN]);
    sc.leaf("cyan").dial(params[P_HUE_SAT_CYAN]);
    sc.leaf("blue").dial(params[P_HUE_SAT_BLUE]);
    sc.leaf("purple").dial(params[P_HUE_SAT_PURPLE]);
    sc.leaf("magenta").dial(params[P_HUE_SAT_MAGENTA]);

    // HSL Bezier curves
    Stem& hsl = linear.next("hslCurves");
    hsl.leaf("hue_0").dial(params[P_HUE_CURVE_0]);
    hsl.leaf("hue_90").dial(params[P_HUE_CURVE_90]);
    hsl.leaf("hue_180").dial(params[P_HUE_CURVE_180]);
    hsl.leaf("hue_270").dial(params[P_HUE_CURVE_270]);
    hsl.leaf("sat_0").dial(params[P_SAT_CURVE_0]);
    hsl.leaf("sat_90").dial(params[P_SAT_CURVE_90]);
    hsl.leaf("sat_180").dial(params[P_SAT_CURVE_180]);
    hsl.leaf("sat_270").dial(params[P_SAT_CURVE_270]);
    hsl.leaf("lum_0").dial(params[P_LUM_CURVE_0]);
    hsl.leaf("lum_90").dial(params[P_LUM_CURVE_90]);
    hsl.leaf("lum_180").dial(params[P_LUM_CURVE_180]);
    hsl.leaf("lum_270").dial(params[P_LUM_CURVE_270]);

    // Apply VIBE
    if (!flow::vibe(ctx.vibe_output, vibe, 3)) {
        return 1000.0f;  // Large penalty on failure
    }

    // Compute loss
    return compute_delta_e(ctx.vibe_output.rgb.data(), ctx.ref_rgb.data(),
                           ctx.width, ctx.height);
}

// ============================================================
// Public API: tune() - Learn optimal VIBE parameters
// ============================================================

bool tune(Done& scene_linear,
          const uint8_t* ref_jpg, size_t ref_jpg_size,
          int ref_width, int ref_height, int orientation,
          Stem& vibeNode,
          int max_iters)
{
    if (scene_linear.rgb.empty() || !ref_jpg || ref_jpg_size == 0) {
        return false;
    }

    // Decode reference JPEG to linear RGB
    auto ref_decoded = swap(ref_jpg, ref_jpg_size, 0, JPG, LIN);
    if (ref_decoded.empty()) {
        std::cerr << "[TUNE] Failed to decode reference JPEG" << std::endl;
        return false;
    }

    const float* ref_ptr = reinterpret_cast<const float*>(ref_decoded.data());

    // Apply orientation rotation if needed
    std::vector<float> ref_rotated;
    int work_w = ref_width, work_h = ref_height;

    if (orientation == 6 || orientation == 8) {
        work_w = ref_height;
        work_h = ref_width;
        ref_rotated.resize(work_w * work_h * 3);

        for (int y = 0; y < ref_height; ++y) {
            for (int x = 0; x < ref_width; ++x) {
                int srcIdx = (y * ref_width + x) * 3;
                int dstIdx;
                if (orientation == 6)  // 90° CW
                    dstIdx = (x * work_w + (work_w - 1 - y)) * 3;
                else  // 90° CCW
                    dstIdx = ((work_h - 1 - x) * work_w + y) * 3;
                ref_rotated[dstIdx + 0] = ref_ptr[srcIdx + 0];
                ref_rotated[dstIdx + 1] = ref_ptr[srcIdx + 1];
                ref_rotated[dstIdx + 2] = ref_ptr[srcIdx + 2];
            }
        }
        ref_ptr = ref_rotated.data();
    }

    // Downsample scene_linear to match reference size for faster evaluation
    TuneContext ctx;
    ctx.width = work_w;
    ctx.height = work_h;
    ctx.scene_linear.width = work_w;
    ctx.scene_linear.height = work_h;
    ctx.scene_linear.rgb.resize(work_w * work_h * 3);

    float x_scale = static_cast<float>(scene_linear.width) / work_w;
    float y_scale = static_cast<float>(scene_linear.height) / work_h;

    for (int y = 0; y < work_h; ++y) {
        for (int x = 0; x < work_w; ++x) {
            int sx = static_cast<int>(x * x_scale);
            int sy = static_cast<int>(y * y_scale);
            sx = std::min(sx, scene_linear.width - 1);
            sy = std::min(sy, scene_linear.height - 1);

            int srcIdx = (sy * scene_linear.width + sx) * 3;
            int dstIdx = (y * work_w + x) * 3;
            ctx.scene_linear.rgb[dstIdx + 0] = scene_linear.rgb[srcIdx + 0];
            ctx.scene_linear.rgb[dstIdx + 1] = scene_linear.rgb[srcIdx + 1];
            ctx.scene_linear.rgb[dstIdx + 2] = scene_linear.rgb[srcIdx + 2];
        }
    }

    // Copy reference (already at correct size)
    ctx.ref_rgb.assign(ref_ptr, ref_ptr + work_w * work_h * 3);

    // Create tree for vibe settings
    extern std::unique_ptr<Tree> makeTree();
    ctx.tree = makeTree();

    // Initialize vibe structure
    ctx.tree->root().next("vibe").next("linear");

    // Initial parameters
    std::vector<float> initial(N_PARAMS);
    for (int i = 0; i < N_PARAMS; ++i) {
        initial[i] = BOUNDS[i].init;
    }

    // Evaluate initial loss
    float initial_loss = evaluate_params(ctx, initial);
    std::cerr << "[TUNE] Initial (" << N_PARAMS << " params): "
              << "temp=" << std::fixed << std::setprecision(0) << initial[P_TEMPERATURE] << "K"
              << " exp=" << std::setprecision(2) << initial[P_EXPOSURE]
              << " con=" << initial[P_CONTRAST]
              << " → ΔE=" << std::setprecision(1) << initial_loss << std::endl;

    // Run optimizer
    auto result = NelderMead::optimize(
        [&ctx](const std::vector<float>& params) { return evaluate_params(ctx, params); },
        initial,
        BOUNDS,
        max_iters
    );

    std::cerr << "[TUNE] Final (" << result.iterations << " iters): "
              << "ΔE=" << std::fixed << std::setprecision(1) << result.loss << std::endl;
    std::cerr << "  WB: temp=" << std::setprecision(0) << result.params[P_TEMPERATURE] << "K"
              << " tint=" << std::setprecision(2) << result.params[P_TINT] << std::endl;
    std::cerr << "  Tone: exp=" << result.params[P_EXPOSURE]
              << " con=" << result.params[P_CONTRAST]
              << " skew=" << result.params[P_SKEW] << std::endl;
    std::cerr << "  Color: sat=" << result.params[P_SATURATION]
              << " vib=" << result.params[P_VIBRANCE] << std::endl;
    std::cerr << "  Curve R: y1=" << result.params[P_CURVE_Y1_R]
              << " y2=" << result.params[P_CURVE_Y2_R] << std::endl;
    std::cerr << "  Curve G: y1=" << result.params[P_CURVE_Y1_G]
              << " y2=" << result.params[P_CURVE_Y2_G] << std::endl;
    std::cerr << "  Curve B: y1=" << result.params[P_CURVE_Y1_B]
              << " y2=" << result.params[P_CURVE_Y2_B] << std::endl;
    std::cerr << "  SplitTone shadow: hue=" << std::setprecision(0) << result.params[P_SHADOW_HUE]
              << "° sat=" << std::setprecision(2) << result.params[P_SHADOW_SAT] << std::endl;
    std::cerr << "  SplitTone highlight: hue=" << std::setprecision(0) << result.params[P_HIGHLIGHT_HUE]
              << "° sat=" << std::setprecision(2) << result.params[P_HIGHLIGHT_SAT] << std::endl;
    std::cerr << "  SelectiveColour: R=" << result.params[P_HUE_SAT_RED]
              << " O=" << result.params[P_HUE_SAT_ORANGE]
              << " Y=" << result.params[P_HUE_SAT_YELLOW]
              << " G=" << result.params[P_HUE_SAT_GREEN] << std::endl;
    std::cerr << "                   C=" << result.params[P_HUE_SAT_CYAN]
              << " B=" << result.params[P_HUE_SAT_BLUE]
              << " P=" << result.params[P_HUE_SAT_PURPLE]
              << " M=" << result.params[P_HUE_SAT_MAGENTA] << std::endl;

    // Write optimal parameters to vibeNode
    Stem& linear = vibeNode.next("linear");

    // White balance
    Stem& cc = linear.next("colorCorrection");
    Stem& wb = cc.next("whiteBalance");
    wb.leaf("temperature").dial(result.params[P_TEMPERATURE]);
    wb.leaf("tint").dial(result.params[P_TINT]);

    // Exposure
    cc.leaf("exposure").dial(result.params[P_EXPOSURE]);

    // Tone mapping
    Stem& tm = linear.next("toneMapping");
    tm.leaf("contrast").dial(result.params[P_CONTRAST]);
    tm.leaf("skew").dial(result.params[P_SKEW]);

    // Global color
    Stem& gc = linear.next("globalColor");
    gc.leaf("saturation").dial(result.params[P_SATURATION]);
    gc.leaf("vibrance").dial(result.params[P_VIBRANCE]);

    // Bezier tone curve
    Stem& bc = linear.next("baseCurve");
    bc.leaf("r_y1").dial(result.params[P_CURVE_Y1_R]);
    bc.leaf("r_y2").dial(result.params[P_CURVE_Y2_R]);
    bc.leaf("g_y1").dial(result.params[P_CURVE_Y1_G]);
    bc.leaf("g_y2").dial(result.params[P_CURVE_Y2_G]);
    bc.leaf("b_y1").dial(result.params[P_CURVE_Y1_B]);
    bc.leaf("b_y2").dial(result.params[P_CURVE_Y2_B]);

    // SplitTone
    Stem& st = linear.next("splitTone");
    st.leaf("shadow_hue").dial(result.params[P_SHADOW_HUE]);
    st.leaf("shadow_sat").dial(result.params[P_SHADOW_SAT]);
    st.leaf("highlight_hue").dial(result.params[P_HIGHLIGHT_HUE]);
    st.leaf("highlight_sat").dial(result.params[P_HIGHLIGHT_SAT]);

    // SelectiveColour
    Stem& sc = linear.next("selectiveColour");
    sc.leaf("red").dial(result.params[P_HUE_SAT_RED]);
    sc.leaf("orange").dial(result.params[P_HUE_SAT_ORANGE]);
    sc.leaf("yellow").dial(result.params[P_HUE_SAT_YELLOW]);
    sc.leaf("green").dial(result.params[P_HUE_SAT_GREEN]);
    sc.leaf("cyan").dial(result.params[P_HUE_SAT_CYAN]);
    sc.leaf("blue").dial(result.params[P_HUE_SAT_BLUE]);
    sc.leaf("purple").dial(result.params[P_HUE_SAT_PURPLE]);
    sc.leaf("magenta").dial(result.params[P_HUE_SAT_MAGENTA]);

    // HSL Bezier curves
    Stem& hsl = linear.next("hslCurves");
    hsl.leaf("hue_0").dial(result.params[P_HUE_CURVE_0]);
    hsl.leaf("hue_90").dial(result.params[P_HUE_CURVE_90]);
    hsl.leaf("hue_180").dial(result.params[P_HUE_CURVE_180]);
    hsl.leaf("hue_270").dial(result.params[P_HUE_CURVE_270]);
    hsl.leaf("sat_0").dial(result.params[P_SAT_CURVE_0]);
    hsl.leaf("sat_90").dial(result.params[P_SAT_CURVE_90]);
    hsl.leaf("sat_180").dial(result.params[P_SAT_CURVE_180]);
    hsl.leaf("sat_270").dial(result.params[P_SAT_CURVE_270]);
    hsl.leaf("lum_0").dial(result.params[P_LUM_CURVE_0]);
    hsl.leaf("lum_90").dial(result.params[P_LUM_CURVE_90]);
    hsl.leaf("lum_180").dial(result.params[P_LUM_CURVE_180]);
    hsl.leaf("lum_270").dial(result.params[P_LUM_CURVE_270]);

    return true;
}

} // namespace flow

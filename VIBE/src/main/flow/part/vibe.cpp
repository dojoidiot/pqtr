// vibe.cpp - VIBE stage: Apply creative style to scene-linear RGB
//
// Pipeline: Exposure → Sigmoid ToneMap → GlobalColor → Linear→sRGB
//
// Reads settings from flow.json "vibe" node, applies via GPU compute.

#include "flow.hpp"

#include <dawn/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>
#include <dawn/dawn_proc.h>

#include <iostream>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace flow
{

// ============================================================
// Combined VIBE shader - all effects in one pass
// ============================================================

static const char* SHADER_VIBE = R"(
struct Uniforms {
    width: u32,
    height: u32,
    stages: u32,  // 1=exposure, 2=+tonemap, 3=+color

    // White balance (RGB multipliers, derived from temperature/tint)
    wb_r: f32,
    wb_g: f32,
    wb_b: f32,

    // Exposure (EV)
    exposure_ev: f32,

    // Tone mapping - log-logistic sigmoid (derived params from darktable)
    film_power: f32,      // controls contrast/slope at middle grey
    paper_power: f32,     // controls skew (pow(5, -skewness))
    film_fog: f32,        // black point offset
    paper_exp: f32,       // exposure parameter for curve shape
    white_target: f32,    // display white (typically 1.0)
    black_target: f32,    // display black (typically ~0.0)

    // Bezier tone curve control points (y values, x fixed at 0.25 and 0.75)
    // Curve: (0,0) -> (0.25, curve_y1_*) -> (0.75, curve_y2_*) -> (1,1)
    curve_y1_r: f32,      // shadows control for R
    curve_y2_r: f32,      // highlights control for R
    curve_y1_g: f32,      // shadows control for G
    curve_y2_g: f32,      // highlights control for G
    curve_y1_b: f32,      // shadows control for B
    curve_y2_b: f32,      // highlights control for B

    // SplitTone (shadow/highlight color grading)
    shadow_hue: f32,      // 0-360 degrees (0=red, 120=green, 240=blue)
    shadow_sat: f32,      // 0-1 strength
    highlight_hue: f32,   // 0-360 degrees
    highlight_sat: f32,   // 0-1 strength

    // SelectiveColour - per-hue saturation multipliers (8 sectors, 45° each)
    // Values are additive: 0 = no change, +1 = double sat, -1 = zero sat
    hue_sat_red: f32,     // 0° (red)
    hue_sat_orange: f32,  // 45° (orange)
    hue_sat_yellow: f32,  // 90° (yellow)
    hue_sat_green: f32,   // 135° (green)
    hue_sat_cyan: f32,    // 180° (cyan)
    hue_sat_blue: f32,    // 225° (blue)
    hue_sat_purple: f32,  // 270° (purple)
    hue_sat_magenta: f32, // 315° (magenta)

    // HSL Bezier curves - smooth adjustment based on input hue
    // 4 control points at 0°, 90°, 180°, 270° with cubic interpolation
    // HueCurve: hue shift in degrees (-30 to +30)
    hue_curve_0: f32,     // shift at red (0°)
    hue_curve_90: f32,    // shift at yellow (90°)
    hue_curve_180: f32,   // shift at cyan (180°)
    hue_curve_270: f32,   // shift at purple (270°)
    // SatCurve: saturation adjustment (-0.5 to +0.5)
    sat_curve_0: f32,     // adjust at red
    sat_curve_90: f32,    // adjust at yellow
    sat_curve_180: f32,   // adjust at cyan
    sat_curve_270: f32,   // adjust at purple
    // LumCurve: luminance adjustment (-0.2 to +0.2)
    lum_curve_0: f32,     // adjust at red
    lum_curve_90: f32,    // adjust at yellow
    lum_curve_180: f32,   // adjust at cyan
    lum_curve_270: f32,   // adjust at purple

    // Global color
    saturation: f32,
    vibrance: f32,
}

// Cubic Bezier evaluation: P0=(0,0), P1=(0.25,y1), P2=(0.75,y2), P3=(1,1)
fn bezier_curve(t: f32, y1: f32, y2: f32) -> f32 {
    let t2 = t * t;
    let t3 = t2 * t;
    let mt = 1.0 - t;
    let mt2 = mt * mt;
    let mt3 = mt2 * mt;

    // Bezier formula: B(t) = (1-t)³P0 + 3(1-t)²tP1 + 3(1-t)t²P2 + t³P3
    // For y-coordinate with P0.y=0, P1.y=y1, P2.y=y2, P3.y=1:
    let y = 3.0 * mt2 * t * y1 + 3.0 * mt * t2 * y2 + t3;
    return clamp(y, 0.0, 1.0);
}

// Find t for given x using Newton-Raphson (x coords: 0, 0.25, 0.75, 1)
fn bezier_solve_t(x: f32) -> f32 {
    // For x: P0.x=0, P1.x=0.25, P2.x=0.75, P3.x=1
    // B_x(t) = 3(1-t)²t(0.25) + 3(1-t)t²(0.75) + t³
    var t = x;  // initial guess
    for (var i = 0; i < 5; i++) {
        let t2 = t * t;
        let t3 = t2 * t;
        let mt = 1.0 - t;
        let mt2 = mt * mt;

        // B_x(t)
        let bx = 3.0 * mt2 * t * 0.25 + 3.0 * mt * t2 * 0.75 + t3;
        // B'_x(t) derivative
        let dbx = 3.0 * mt2 * 0.25 + 6.0 * mt * t * (0.75 - 0.25) + 3.0 * t2 * (1.0 - 0.75);

        if (abs(dbx) < 1e-6) { break; }
        t = t - (bx - x) / dbx;
        t = clamp(t, 0.0, 1.0);
    }
    return t;
}

// Apply Bezier tone curve to a single channel
fn apply_curve(val: f32, y1: f32, y2: f32) -> f32 {
    let x = clamp(val, 0.0, 1.0);
    let t = bezier_solve_t(x);
    return bezier_curve(t, y1, y2);
}

// Convert hue (0-360) to RGB color with unit saturation
fn hue_to_rgb(hue: f32) -> vec3<f32> {
    let h = hue / 60.0;
    let x = 1.0 - abs(h % 2.0 - 1.0);
    var rgb: vec3<f32>;
    if (h < 1.0) { rgb = vec3<f32>(1.0, x, 0.0); }
    else if (h < 2.0) { rgb = vec3<f32>(x, 1.0, 0.0); }
    else if (h < 3.0) { rgb = vec3<f32>(0.0, 1.0, x); }
    else if (h < 4.0) { rgb = vec3<f32>(0.0, x, 1.0); }
    else if (h < 5.0) { rgb = vec3<f32>(x, 0.0, 1.0); }
    else { rgb = vec3<f32>(1.0, 0.0, x); }
    return rgb;
}

// Apply split tone: tint shadows and highlights with different colors
fn apply_split_tone(rgb: vec3<f32>, sh_hue: f32, sh_sat: f32, hi_hue: f32, hi_sat: f32) -> vec3<f32> {
    // Luminance for shadow/highlight weighting
    let lum = 0.299 * rgb.r + 0.587 * rgb.g + 0.114 * rgb.b;

    // Shadow weight: stronger in dark areas (inverse sigmoid)
    let shadow_weight = 1.0 - smoothstep(0.0, 0.5, lum);
    // Highlight weight: stronger in bright areas
    let highlight_weight = smoothstep(0.5, 1.0, lum);

    // Get tint colors
    let shadow_tint = hue_to_rgb(sh_hue);
    let highlight_tint = hue_to_rgb(hi_hue);

    // Blend: add tint while preserving luminance
    var result = rgb;

    // Shadow tint (blend towards shadow color in dark areas)
    if (sh_sat > 0.001) {
        let shadow_blend = shadow_weight * sh_sat * 0.3;  // Max 30% blend
        result = mix(result, shadow_tint * lum * 2.0, shadow_blend);
    }

    // Highlight tint (blend towards highlight color in bright areas)
    if (hi_sat > 0.001) {
        let highlight_blend = highlight_weight * hi_sat * 0.3;
        result = mix(result, highlight_tint * lum, highlight_blend);
    }

    return result;
}

// RGB to HSL conversion (returns h in 0-360, s and l in 0-1)
fn rgb_to_hsl(rgb: vec3<f32>) -> vec3<f32> {
    let max_c = max(max(rgb.r, rgb.g), rgb.b);
    let min_c = min(min(rgb.r, rgb.g), rgb.b);
    let l = (max_c + min_c) / 2.0;

    if (max_c == min_c) {
        return vec3<f32>(0.0, 0.0, l);  // achromatic
    }

    let d = max_c - min_c;
    let s = select(d / (max_c + min_c), d / (2.0 - max_c - min_c), l > 0.5);

    var h: f32;
    if (max_c == rgb.r) {
        h = (rgb.g - rgb.b) / d + select(0.0, 6.0, rgb.g < rgb.b);
    } else if (max_c == rgb.g) {
        h = (rgb.b - rgb.r) / d + 2.0;
    } else {
        h = (rgb.r - rgb.g) / d + 4.0;
    }
    h = h * 60.0;

    return vec3<f32>(h, s, l);
}

// HSL to RGB conversion
fn hsl_to_rgb(hsl: vec3<f32>) -> vec3<f32> {
    let h = hsl.x;
    let s = hsl.y;
    let l = hsl.z;

    if (s < 0.001) {
        return vec3<f32>(l, l, l);  // achromatic
    }

    let q = select(l * (1.0 + s), l + s - l * s, l < 0.5);
    let p = 2.0 * l - q;

    let hk = h / 360.0;
    let tr = hk + 1.0/3.0;
    let tg = hk;
    let tb = hk - 1.0/3.0;

    // Compute each channel
    var r: f32; var g: f32; var b: f32;

    // Red channel
    var tc = select(tr, tr + 1.0, tr < 0.0);
    tc = select(tc, tc - 1.0, tc > 1.0);
    if (tc < 1.0/6.0) { r = p + (q - p) * 6.0 * tc; }
    else if (tc < 1.0/2.0) { r = q; }
    else if (tc < 2.0/3.0) { r = p + (q - p) * (2.0/3.0 - tc) * 6.0; }
    else { r = p; }

    // Green channel
    tc = select(tg, tg + 1.0, tg < 0.0);
    tc = select(tc, tc - 1.0, tc > 1.0);
    if (tc < 1.0/6.0) { g = p + (q - p) * 6.0 * tc; }
    else if (tc < 1.0/2.0) { g = q; }
    else if (tc < 2.0/3.0) { g = p + (q - p) * (2.0/3.0 - tc) * 6.0; }
    else { g = p; }

    // Blue channel
    tc = select(tb, tb + 1.0, tb < 0.0);
    tc = select(tc, tc - 1.0, tc > 1.0);
    if (tc < 1.0/6.0) { b = p + (q - p) * 6.0 * tc; }
    else if (tc < 1.0/2.0) { b = q; }
    else if (tc < 2.0/3.0) { b = p + (q - p) * (2.0/3.0 - tc) * 6.0; }
    else { b = p; }

    return vec3<f32>(r, g, b);
}

// Get per-hue saturation adjustment (8 sectors with smooth blending)
fn get_hue_sat_adjust(hue: f32, u: ptr<uniform, Uniforms>) -> f32 {
    // 8 sectors: 0, 45, 90, 135, 180, 225, 270, 315
    let hue_adj = array<f32, 8>(
        (*u).hue_sat_red,     // 0°
        (*u).hue_sat_orange,  // 45°
        (*u).hue_sat_yellow,  // 90°
        (*u).hue_sat_green,   // 135°
        (*u).hue_sat_cyan,    // 180°
        (*u).hue_sat_blue,    // 225°
        (*u).hue_sat_purple,  // 270°
        (*u).hue_sat_magenta  // 315°
    );

    // Find sector and interpolate
    let sector = hue / 45.0;
    let idx0 = i32(floor(sector)) % 8;
    let idx1 = (idx0 + 1) % 8;
    let t = fract(sector);

    // Smooth interpolation between adjacent sectors
    return mix(hue_adj[idx0], hue_adj[idx1], t);
}

// Evaluate HSL Bezier curve at given hue (0-360)
// Uses 4 control points at 0°, 90°, 180°, 270° with smooth Catmull-Rom interpolation
fn eval_hsl_curve(hue: f32, v0: f32, v90: f32, v180: f32, v270: f32) -> f32 {
    // Wrap hue to 0-360
    var h = hue;
    if (h < 0.0) { h = h + 360.0; }
    if (h >= 360.0) { h = h - 360.0; }

    // Find which quadrant we're in and compute interpolation parameter
    let quadrant = h / 90.0;
    let idx = i32(floor(quadrant));
    let t = fract(quadrant);

    // Get 4 values for Catmull-Rom: p0, p1, p2, p3 (circular)
    var p0: f32; var p1: f32; var p2: f32; var p3: f32;
    if (idx == 0) {
        p0 = v270; p1 = v0; p2 = v90; p3 = v180;
    } else if (idx == 1) {
        p0 = v0; p1 = v90; p2 = v180; p3 = v270;
    } else if (idx == 2) {
        p0 = v90; p1 = v180; p2 = v270; p3 = v0;
    } else {
        p0 = v180; p1 = v270; p2 = v0; p3 = v90;
    }

    // Catmull-Rom spline interpolation (smooth)
    let t2 = t * t;
    let t3 = t2 * t;
    let result = 0.5 * (
        (2.0 * p1) +
        (-p0 + p2) * t +
        (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
        (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3
    );
    return result;
}

// Apply HSL curves to adjust hue, saturation, and luminance based on input hue
fn apply_hsl_curves(hsl: vec3<f32>, u: ptr<uniform, Uniforms>) -> vec3<f32> {
    let h = hsl.x;
    let s = hsl.y;
    let l = hsl.z;

    // Evaluate curves at this hue
    let hue_shift = eval_hsl_curve(h, (*u).hue_curve_0, (*u).hue_curve_90,
                                   (*u).hue_curve_180, (*u).hue_curve_270);
    let sat_adj = eval_hsl_curve(h, (*u).sat_curve_0, (*u).sat_curve_90,
                                 (*u).sat_curve_180, (*u).sat_curve_270);
    let lum_adj = eval_hsl_curve(h, (*u).lum_curve_0, (*u).lum_curve_90,
                                 (*u).lum_curve_180, (*u).lum_curve_270);

    // Apply adjustments
    var new_h = h + hue_shift;
    if (new_h < 0.0) { new_h = new_h + 360.0; }
    if (new_h >= 360.0) { new_h = new_h - 360.0; }

    let new_s = clamp(s + sat_adj, 0.0, 1.0);
    let new_l = clamp(l + lum_adj, 0.0, 1.0);

    return vec3<f32>(new_h, new_s, new_l);
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> u: Uniforms;

const PI: f32 = 3.14159265359;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;
    if (x >= u.width || y >= u.height) { return; }

    let idx = (y * u.width + x) * 3u;

    // Load RGB (scene-linear from HEAD)
    var r = input[idx + 0u];
    var g = input[idx + 1u];
    var b = input[idx + 2u];

    // ========================================
    // 1. WHITE BALANCE (RGB multipliers)
    // ========================================
    r *= u.wb_r;
    g *= u.wb_g;
    b *= u.wb_b;

    // ========================================
    // 2. EXPOSURE (simple linear multiply)
    // ========================================
    let exp_mult = pow(2.0, u.exposure_ev);
    r *= exp_mult;
    g *= exp_mult;
    b *= exp_mult;

    // ========================================
    // 3. LOG-LOGISTIC SIGMOID TONE MAPPING (if stages >= 2)
    // Based on darktable's generalized log-logistic sigmoid
    // Models film + paper response curve
    // ========================================
    if (u.stages >= 2u) {

    // Desaturate negative values first (same as darktable)
    let avg = max((r + g + b) / 3.0, 0.0);
    let min_val = min(min(r, g), b);
    if (min_val < 0.0) {
        let sat_factor = avg / (avg - min_val + 1e-10);
        r = avg + sat_factor * (r - avg);
        g = avg + sat_factor * (g - avg);
        b = avg + sat_factor * (b - avg);
    }

    // Clamp to positive
    r = max(r, 0.0);
    g = max(g, 0.0);
    b = max(b, 0.0);

    // Apply sigmoid to luminance, preserve color ratios (RGB ratio method)
    let luma = (r + g + b) / 3.0;

    // Generalized log-logistic sigmoid (darktable formula):
    // film_response = pow(film_fog + value, film_power)
    // paper_response = magnitude * pow(film_response / (paper_exp + film_response), paper_power)
    let film_response = pow(u.film_fog + luma, u.film_power);
    let mapped_luma = u.white_target * pow(film_response / (u.paper_exp + film_response), u.paper_power);

    // Scale RGB by luminance ratio
    if (luma > 1e-9) {
        let scale = mapped_luma / luma;
        r *= scale;
        g *= scale;
        b *= scale;
    } else {
        r = mapped_luma;
        g = mapped_luma;
        b = mapped_luma;
    }

    } // end tone mapping

    // ========================================
    // 4. BEZIER TONE CURVE (per-channel adjustment) - if stages >= 3
    // Applies subtle curve after sigmoid to fine-tune response
    // ========================================
    if (u.stages >= 3u) {
        // Only apply if control points deviate from identity (y1=0.25, y2=0.75)
        let id_y1 = 0.25;
        let id_y2 = 0.75;
        let tol = 0.01;

        if (abs(u.curve_y1_r - id_y1) > tol || abs(u.curve_y2_r - id_y2) > tol) {
            r = apply_curve(r, u.curve_y1_r, u.curve_y2_r);
        }
        if (abs(u.curve_y1_g - id_y1) > tol || abs(u.curve_y2_g - id_y2) > tol) {
            g = apply_curve(g, u.curve_y1_g, u.curve_y2_g);
        }
        if (abs(u.curve_y1_b - id_y1) > tol || abs(u.curve_y2_b - id_y2) > tol) {
            b = apply_curve(b, u.curve_y1_b, u.curve_y2_b);
        }
    }

    // ========================================
    // 5. SPLIT TONE (shadow/highlight color grading) - if stages >= 3
    // ========================================
    if (u.stages >= 3u) {
        if (u.shadow_sat > 0.001 || u.highlight_sat > 0.001) {
            let st_result = apply_split_tone(vec3<f32>(r, g, b),
                u.shadow_hue, u.shadow_sat, u.highlight_hue, u.highlight_sat);
            r = st_result.r;
            g = st_result.g;
            b = st_result.b;
        }
    }

    // ========================================
    // 6. SELECTIVE COLOUR (per-hue saturation) - if stages >= 3
    // ========================================
    if (u.stages >= 3u) {
        // Convert to HSL, adjust saturation per hue, convert back
        let hsl = rgb_to_hsl(vec3<f32>(r, g, b));
        let hue_adjust = get_hue_sat_adjust(hsl.x, &u);

        if (abs(hue_adjust) > 0.001 && hsl.y > 0.01) {
            // Apply per-hue saturation adjustment
            let new_sat = clamp(hsl.y * (1.0 + hue_adjust), 0.0, 1.0);
            let adjusted = hsl_to_rgb(vec3<f32>(hsl.x, new_sat, hsl.z));
            r = adjusted.r;
            g = adjusted.g;
            b = adjusted.b;
        }
    }

    // ========================================
    // 7. HSL BEZIER CURVES (hue/sat/lum per hue) - if stages >= 3
    // ========================================
    if (u.stages >= 3u) {
        // Check if any curve has non-zero values
        let has_hue_curve = abs(u.hue_curve_0) > 0.001 || abs(u.hue_curve_90) > 0.001 ||
                            abs(u.hue_curve_180) > 0.001 || abs(u.hue_curve_270) > 0.001;
        let has_sat_curve = abs(u.sat_curve_0) > 0.001 || abs(u.sat_curve_90) > 0.001 ||
                            abs(u.sat_curve_180) > 0.001 || abs(u.sat_curve_270) > 0.001;
        let has_lum_curve = abs(u.lum_curve_0) > 0.001 || abs(u.lum_curve_90) > 0.001 ||
                            abs(u.lum_curve_180) > 0.001 || abs(u.lum_curve_270) > 0.001;

        if (has_hue_curve || has_sat_curve || has_lum_curve) {
            // Convert to HSL, apply curves, convert back
            let hsl = rgb_to_hsl(vec3<f32>(r, g, b));
            if (hsl.y > 0.01) {  // Only adjust chromatic colors
                let adjusted_hsl = apply_hsl_curves(hsl, &u);
                let adjusted = hsl_to_rgb(adjusted_hsl);
                r = adjusted.r;
                g = adjusted.g;
                b = adjusted.b;
            }
        }
    }

    // ========================================
    // 8. GLOBAL COLOR (saturation, vibrance) - if stages >= 3
    // ========================================
    if (u.stages >= 3u) {
        let sat_mult = 1.0 + u.saturation;  // saturation from flow.json is additive
        let vib = u.vibrance;  // vibrance is additive too

        if (abs(sat_mult - 1.0) > 0.01 || abs(vib) > 0.01) {
            // Simple HSL-style saturation
            let lum = 0.299 * r + 0.587 * g + 0.114 * b;

            // Vibrance: boost low-saturation colors more
            let max_c = max(max(r, g), b);
            let min_c = min(min(r, g), b);
            let chroma = max_c - min_c;
            let vib_weight = 1.0 - clamp(chroma * 2.0, 0.0, 1.0);
            let vib_boost = 1.0 + vib_weight * vib;

            // Combined saturation
            let total_sat = sat_mult * vib_boost;

            r = lum + (r - lum) * total_sat;
            g = lum + (g - lum) * total_sat;
            b = lum + (b - lum) * total_sat;
        }
    } // end color

    // ========================================
    // 5. OUTPUT (stay linear, swap() handles gamma)
    // ========================================
    // Clip to displayable range
    output[idx + 0u] = clamp(r, 0.0, 1.0);
    output[idx + 1u] = clamp(g, 0.0, 1.0);
    output[idx + 2u] = clamp(b, 0.0, 1.0);
}
)";

// ============================================================
// GPU context (reuse from process.cpp pattern)
// ============================================================

struct VibeGPU {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::ComputePipeline pipeline;
    bool ready = false;

    bool init() {
        DawnProcTable procs = dawn::native::GetProcs();
        dawnProcSetProcs(&procs);

        wgpu::InstanceDescriptor desc{};
        instance = wgpu::CreateInstance(&desc);
        if (!instance) return false;

        // Request adapter
        bool adapterDone = false;
        wgpu::RequestAdapterOptions adapterOpts{};
        adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;

        instance.RequestAdapter(&adapterOpts, wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView) {
                if (status == wgpu::RequestAdapterStatus::Success) adapter = a;
                adapterDone = true;
            });
        while (!adapterDone) instance.ProcessEvents();
        if (!adapter) return false;

        // Get adapter limits
        wgpu::Limits adapterLimits{};
        if (!adapter.GetLimits(&adapterLimits)) return false;

        // Request device with high buffer limits
        wgpu::Limits requiredLimits = adapterLimits;
        uint64_t desiredMaxBuffer = 2ULL * 1024 * 1024 * 1024;
        requiredLimits.maxBufferSize = std::min(desiredMaxBuffer, adapterLimits.maxBufferSize);
        requiredLimits.maxStorageBufferBindingSize = std::min(desiredMaxBuffer, adapterLimits.maxStorageBufferBindingSize);

        bool deviceDone = false;
        wgpu::DeviceDescriptor deviceDesc{};
        deviceDesc.requiredLimits = &requiredLimits;

        deviceDesc.SetUncapturedErrorCallback(
            [](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView msg) {
                std::cerr << "VIBE GPU error: " << std::string_view(msg.data, msg.length) << "\n";
            });

        adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView) {
                if (status == wgpu::RequestDeviceStatus::Success) device = d;
                deviceDone = true;
            });
        while (!deviceDone) instance.ProcessEvents();
        if (!device) return false;

        queue = device.GetQueue();

        // Create pipeline
        wgpu::ShaderSourceWGSL src{};
        src.code = SHADER_VIBE;
        wgpu::ShaderModuleDescriptor shaderDesc{};
        shaderDesc.nextInChain = &src;
        wgpu::ShaderModule shader = device.CreateShaderModule(&shaderDesc);

        wgpu::ComputePipelineDescriptor pipeDesc{};
        pipeDesc.compute.module = shader;
        pipeDesc.compute.entryPoint = "main";
        pipeline = device.CreateComputePipeline(&pipeDesc);

        ready = true;
        return true;
    }

    wgpu::Buffer createBuffer(size_t size, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor desc{};
        desc.size = size;
        desc.usage = usage;
        return device.CreateBuffer(&desc);
    }
};

static VibeGPU* g_vibe_gpu = nullptr;

static VibeGPU& vibeGpu() {
    if (!g_vibe_gpu) {
        g_vibe_gpu = new VibeGPU();
        if (!g_vibe_gpu->init()) {
            std::cerr << "Failed to initialize VIBE GPU\n";
        }
    }
    return *g_vibe_gpu;
}

// ============================================================
// Uniforms structure (must match shader)
// ============================================================

struct VibeUniforms {
    uint32_t width;
    uint32_t height;
    uint32_t stages;  // 1=exposure, 2=+tonemap, 3=+color+curve
    // White balance (RGB multipliers)
    float wb_r;
    float wb_g;
    float wb_b;
    // Exposure
    float exposure_ev;
    // Log-logistic sigmoid params (derived from contrast/skew)
    float film_power;
    float paper_power;
    float film_fog;
    float paper_exp;
    float white_target;
    float black_target;
    // Bezier tone curve control points (y values, x fixed at 0.25 and 0.75)
    // Identity curve: y1=0.25, y2=0.75
    float curve_y1_r;
    float curve_y2_r;
    float curve_y1_g;
    float curve_y2_g;
    float curve_y1_b;
    float curve_y2_b;
    // SplitTone (shadow/highlight color grading)
    float shadow_hue;      // 0-360 degrees
    float shadow_sat;      // 0-1 strength
    float highlight_hue;   // 0-360 degrees
    float highlight_sat;   // 0-1 strength
    // SelectiveColour - per-hue saturation (8 sectors)
    float hue_sat_red;
    float hue_sat_orange;
    float hue_sat_yellow;
    float hue_sat_green;
    float hue_sat_cyan;
    float hue_sat_blue;
    float hue_sat_purple;
    float hue_sat_magenta;
    // HSL Bezier curves - smooth adjustment based on input hue
    // 4 control points at 0°, 90°, 180°, 270° with Catmull-Rom interpolation
    float hue_curve_0;      // hue shift at red (0°)
    float hue_curve_90;     // hue shift at yellow (90°)
    float hue_curve_180;    // hue shift at cyan (180°)
    float hue_curve_270;    // hue shift at purple (270°)
    float sat_curve_0;      // saturation adjust at red
    float sat_curve_90;     // saturation adjust at yellow
    float sat_curve_180;    // saturation adjust at cyan
    float sat_curve_270;    // saturation adjust at purple
    float lum_curve_0;      // luminance adjust at red
    float lum_curve_90;     // luminance adjust at yellow
    float lum_curve_180;    // luminance adjust at cyan
    float lum_curve_270;    // luminance adjust at purple
    // Color
    float saturation;
    float vibrance;
};

// Middle grey constant (same as darktable)
static constexpr float MIDDLE_GREY = 0.1845f;

// Convert color temperature (Kelvin) and tint to RGB multipliers
// Based on approximation of Planckian locus
static void temperature_to_rgb(float temp_k, float tint, float& r, float& g, float& b)
{
    // Normalize temperature to 0-1 range (4000K-10000K)
    float t = (temp_k - 4000.0f) / 6000.0f;
    t = std::max(0.0f, std::min(1.0f, t));

    // Approximate daylight white point shift
    // Warm (low K) = more red, less blue
    // Cool (high K) = less red, more blue
    // These are relative multipliers around D65 (6500K)

    if (temp_k < 6500.0f) {
        // Warm: boost red, reduce blue
        float warmth = (6500.0f - temp_k) / 2500.0f;  // 0 at 6500K, 1 at 4000K
        r = 1.0f + warmth * 0.3f;
        g = 1.0f;
        b = 1.0f - warmth * 0.3f;
    } else {
        // Cool: reduce red, boost blue
        float coolness = (temp_k - 6500.0f) / 3500.0f;  // 0 at 6500K, 1 at 10000K
        r = 1.0f - coolness * 0.2f;
        g = 1.0f;
        b = 1.0f + coolness * 0.2f;
    }

    // Apply tint (green-magenta axis)
    // Positive tint = more magenta (less green)
    // Negative tint = more green (less magenta)
    g *= (1.0f - tint * 0.2f);

    // Normalize so green stays at 1.0 (standard practice)
    float norm = 1.0f / g;
    r *= norm;
    g = 1.0f;
    b *= norm;
}

// Calculate log-logistic sigmoid parameters from contrast/skew
// Based on darktable's commit_params() in sigmoid.c
static void calculate_sigmoid_params(float contrast, float skew,
                                     float display_white, float display_black,
                                     float& film_power, float& paper_power,
                                     float& film_fog, float& paper_exp,
                                     float& white_target, float& black_target)
{
    // Paper power from skew
    paper_power = std::pow(5.0f, -skew);

    // Reference slope calculation (no skew, normalized display)
    float ref_film_power = contrast;
    float ref_paper_power = 1.0f;
    float ref_magnitude = 1.0f;
    float ref_film_fog = 0.0f;
    float ref_paper_exp = std::pow(ref_film_fog + MIDDLE_GREY, ref_film_power)
                          * ((ref_magnitude / MIDDLE_GREY) - 1.0f);

    // Calculate reference slope at middle grey
    float delta = 1e-6f;
    auto loglogistic = [](float value, float mag, float p_exp, float f_fog, float f_pow, float p_pow) {
        float clamped = std::max(value, 0.0f);
        float film_resp = std::pow(f_fog + clamped, f_pow);
        return mag * std::pow(film_resp / (p_exp + film_resp), p_pow);
    };

    float ref_slope = (loglogistic(MIDDLE_GREY + delta, ref_magnitude, ref_paper_exp, ref_film_fog,
                                   ref_film_power, ref_paper_power)
                     - loglogistic(MIDDLE_GREY - delta, ref_magnitude, ref_paper_exp, ref_film_fog,
                                   ref_film_power, ref_paper_power))
                     / (2.0f * delta);

    // Slope at low film power with skew
    float temp_film_power = 1.0f;
    float temp_white_target = display_white;
    float temp_white_grey_relation = std::pow(temp_white_target / MIDDLE_GREY, 1.0f / paper_power) - 1.0f;
    float temp_paper_exp = std::pow(MIDDLE_GREY, temp_film_power) * temp_white_grey_relation;

    float temp_slope = (loglogistic(MIDDLE_GREY + delta, temp_white_target, temp_paper_exp,
                                    ref_film_fog, temp_film_power, paper_power)
                      - loglogistic(MIDDLE_GREY - delta, temp_white_target, temp_paper_exp,
                                    ref_film_fog, temp_film_power, paper_power))
                      / (2.0f * delta);

    // Film power to achieve target slope
    film_power = ref_slope / temp_slope;

    // Final parameter calculation
    white_target = display_white;
    black_target = display_black;

    float white_grey_relation = std::pow(white_target / MIDDLE_GREY, 1.0f / paper_power) - 1.0f;
    float white_black_relation = std::pow(black_target / white_target, -1.0f / paper_power) - 1.0f;

    film_fog = MIDDLE_GREY * std::pow(white_grey_relation, 1.0f / film_power)
               / (std::pow(white_black_relation, 1.0f / film_power)
                  - std::pow(white_grey_relation, 1.0f / film_power));

    paper_exp = std::pow(film_fog + MIDDLE_GREY, film_power) * white_grey_relation;
}

// ============================================================
// Public API: apply vibe settings to Done result
// ============================================================

bool vibe(Done& img, Stem& vibeNode, int stages)
{
    if (img.rgb.empty() || img.width <= 0 || img.height <= 0)
        return false;

    VibeGPU& gpu = vibeGpu();
    if (!gpu.ready) return false;

    int w = img.width;
    int h = img.height;
    size_t rgb_size = w * h * 3 * sizeof(float);

    // Parse vibe settings from tree
    VibeUniforms uniforms{};
    uniforms.width = static_cast<uint32_t>(w);
    uniforms.height = static_cast<uint32_t>(h);
    uniforms.stages = static_cast<uint32_t>(stages);

    // Defaults
    float temperature = 6500.0f;  // D65 neutral
    float tint = 0.0f;
    uniforms.wb_r = 1.0f;
    uniforms.wb_g = 1.0f;
    uniforms.wb_b = 1.0f;
    uniforms.exposure_ev = 0.0f;
    float contrast = 1.5f;
    float skew = 0.0f;
    float display_white = 1.0f;   // darktable default: 100 cd/m² → normalized to 1.0
    float display_black = 0.000152f;  // darktable default: 0.0152 cd/m² → normalized
    uniforms.saturation = 0.0f;
    uniforms.vibrance = 0.0f;
    // Curve defaults = identity (y1=0.25, y2=0.75 gives straight line)
    uniforms.curve_y1_r = 0.25f;
    uniforms.curve_y2_r = 0.75f;
    uniforms.curve_y1_g = 0.25f;
    uniforms.curve_y2_g = 0.75f;
    uniforms.curve_y1_b = 0.25f;
    uniforms.curve_y2_b = 0.75f;
    // SplitTone defaults = no tinting
    uniforms.shadow_hue = 0.0f;
    uniforms.shadow_sat = 0.0f;
    uniforms.highlight_hue = 0.0f;
    uniforms.highlight_sat = 0.0f;
    // SelectiveColour defaults = no per-hue adjustment
    uniforms.hue_sat_red = 0.0f;
    uniforms.hue_sat_orange = 0.0f;
    uniforms.hue_sat_yellow = 0.0f;
    uniforms.hue_sat_green = 0.0f;
    uniforms.hue_sat_cyan = 0.0f;
    uniforms.hue_sat_blue = 0.0f;
    uniforms.hue_sat_purple = 0.0f;
    uniforms.hue_sat_magenta = 0.0f;
    // HSL Bezier curves defaults = no adjustment
    uniforms.hue_curve_0 = 0.0f;
    uniforms.hue_curve_90 = 0.0f;
    uniforms.hue_curve_180 = 0.0f;
    uniforms.hue_curve_270 = 0.0f;
    uniforms.sat_curve_0 = 0.0f;
    uniforms.sat_curve_90 = 0.0f;
    uniforms.sat_curve_180 = 0.0f;
    uniforms.sat_curve_270 = 0.0f;
    uniforms.lum_curve_0 = 0.0f;
    uniforms.lum_curve_90 = 0.0f;
    uniforms.lum_curve_180 = 0.0f;
    uniforms.lum_curve_270 = 0.0f;

    // Read from tree if present
    if (vibeNode.test("linear")) {
        Stem& linear = vibeNode.next("linear");

        if (linear.test("colorCorrection")) {
            Stem& cc = linear.next("colorCorrection");
            if (cc.test("exposure")) {
                uniforms.exposure_ev = cc.leaf("exposure").dial();
            }
            if (cc.test("whiteBalance")) {
                Stem& wb = cc.next("whiteBalance");
                if (wb.test("temperature")) temperature = wb.leaf("temperature").dial();
                if (wb.test("tint")) tint = wb.leaf("tint").dial();
            }
        }

        if (linear.test("toneMapping")) {
            Stem& tm = linear.next("toneMapping");
            if (tm.test("contrast")) contrast = tm.leaf("contrast").dial();
            if (tm.test("skew")) skew = tm.leaf("skew").dial();
            // Note: greyPoint and clippingPoint from filmic are not used for sigmoid
            // sigmoid uses display_white/display_black targets instead
        }

        if (linear.test("globalColor")) {
            Stem& gc = linear.next("globalColor");
            if (gc.test("saturation")) uniforms.saturation = gc.leaf("saturation").dial();
            if (gc.test("vibrance")) uniforms.vibrance = gc.leaf("vibrance").dial();
        }

        if (linear.test("baseCurve")) {
            Stem& bc = linear.next("baseCurve");
            // Per-channel Bezier control points
            if (bc.test("r_y1")) uniforms.curve_y1_r = bc.leaf("r_y1").dial();
            if (bc.test("r_y2")) uniforms.curve_y2_r = bc.leaf("r_y2").dial();
            if (bc.test("g_y1")) uniforms.curve_y1_g = bc.leaf("g_y1").dial();
            if (bc.test("g_y2")) uniforms.curve_y2_g = bc.leaf("g_y2").dial();
            if (bc.test("b_y1")) uniforms.curve_y1_b = bc.leaf("b_y1").dial();
            if (bc.test("b_y2")) uniforms.curve_y2_b = bc.leaf("b_y2").dial();
        }

        if (linear.test("splitTone")) {
            Stem& st = linear.next("splitTone");
            if (st.test("shadow_hue")) uniforms.shadow_hue = st.leaf("shadow_hue").dial();
            if (st.test("shadow_sat")) uniforms.shadow_sat = st.leaf("shadow_sat").dial();
            if (st.test("highlight_hue")) uniforms.highlight_hue = st.leaf("highlight_hue").dial();
            if (st.test("highlight_sat")) uniforms.highlight_sat = st.leaf("highlight_sat").dial();
        }

        if (linear.test("selectiveColour")) {
            Stem& sc = linear.next("selectiveColour");
            if (sc.test("red")) uniforms.hue_sat_red = sc.leaf("red").dial();
            if (sc.test("orange")) uniforms.hue_sat_orange = sc.leaf("orange").dial();
            if (sc.test("yellow")) uniforms.hue_sat_yellow = sc.leaf("yellow").dial();
            if (sc.test("green")) uniforms.hue_sat_green = sc.leaf("green").dial();
            if (sc.test("cyan")) uniforms.hue_sat_cyan = sc.leaf("cyan").dial();
            if (sc.test("blue")) uniforms.hue_sat_blue = sc.leaf("blue").dial();
            if (sc.test("purple")) uniforms.hue_sat_purple = sc.leaf("purple").dial();
            if (sc.test("magenta")) uniforms.hue_sat_magenta = sc.leaf("magenta").dial();
        }

        if (linear.test("hslCurves")) {
            Stem& hsl = linear.next("hslCurves");
            // Hue curve (shift at each control point)
            if (hsl.test("hue_0")) uniforms.hue_curve_0 = hsl.leaf("hue_0").dial();
            if (hsl.test("hue_90")) uniforms.hue_curve_90 = hsl.leaf("hue_90").dial();
            if (hsl.test("hue_180")) uniforms.hue_curve_180 = hsl.leaf("hue_180").dial();
            if (hsl.test("hue_270")) uniforms.hue_curve_270 = hsl.leaf("hue_270").dial();
            // Saturation curve (adjust at each control point)
            if (hsl.test("sat_0")) uniforms.sat_curve_0 = hsl.leaf("sat_0").dial();
            if (hsl.test("sat_90")) uniforms.sat_curve_90 = hsl.leaf("sat_90").dial();
            if (hsl.test("sat_180")) uniforms.sat_curve_180 = hsl.leaf("sat_180").dial();
            if (hsl.test("sat_270")) uniforms.sat_curve_270 = hsl.leaf("sat_270").dial();
            // Luminance curve (adjust at each control point)
            if (hsl.test("lum_0")) uniforms.lum_curve_0 = hsl.leaf("lum_0").dial();
            if (hsl.test("lum_90")) uniforms.lum_curve_90 = hsl.leaf("lum_90").dial();
            if (hsl.test("lum_180")) uniforms.lum_curve_180 = hsl.leaf("lum_180").dial();
            if (hsl.test("lum_270")) uniforms.lum_curve_270 = hsl.leaf("lum_270").dial();
        }
    }

    // Convert temperature/tint to RGB multipliers
    temperature_to_rgb(temperature, tint, uniforms.wb_r, uniforms.wb_g, uniforms.wb_b);

    // Calculate derived log-logistic sigmoid parameters
    calculate_sigmoid_params(contrast, skew, display_white, display_black,
                             uniforms.film_power, uniforms.paper_power,
                             uniforms.film_fog, uniforms.paper_exp,
                             uniforms.white_target, uniforms.black_target);

    std::cerr << "[VIBE] wb=" << temperature << "K"
              << " exp=" << uniforms.exposure_ev
              << " contrast=" << contrast
              << " sat=" << uniforms.saturation << "\n";

    // Create GPU buffers
    auto bufIn = gpu.createBuffer(rgb_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    auto bufOut = gpu.createBuffer(rgb_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
    auto bufReadback = gpu.createBuffer(rgb_size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);
    auto bufUniform = gpu.createBuffer(sizeof(VibeUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);

    // Upload data
    gpu.queue.WriteBuffer(bufIn, 0, img.rgb.data(), rgb_size);
    gpu.queue.WriteBuffer(bufUniform, 0, &uniforms, sizeof(uniforms));

    // Bind group
    wgpu::BindGroupEntry entries[3] = {
        {nullptr, 0, bufIn, 0, rgb_size},
        {nullptr, 1, bufOut, 0, rgb_size},
        {nullptr, 2, bufUniform, 0, sizeof(VibeUniforms)}
    };
    wgpu::BindGroupDescriptor bgDesc{};
    bgDesc.layout = gpu.pipeline.GetBindGroupLayout(0);
    bgDesc.entryCount = 3;
    bgDesc.entries = entries;
    auto bg = gpu.device.CreateBindGroup(&bgDesc);

    // Dispatch
    auto encoder = gpu.device.CreateCommandEncoder();
    auto pass = encoder.BeginComputePass();
    pass.SetPipeline(gpu.pipeline);
    pass.SetBindGroup(0, bg);
    pass.DispatchWorkgroups((w + 7) / 8, (h + 7) / 8, 1);
    pass.End();
    encoder.CopyBufferToBuffer(bufOut, 0, bufReadback, 0, rgb_size);
    auto commands = encoder.Finish();
    gpu.queue.Submit(1, &commands);

    // Read back
    bool mapDone = false;
    bufReadback.MapAsync(wgpu::MapMode::Read, 0, rgb_size, wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::MapAsyncStatus, wgpu::StringView) { mapDone = true; });
    while (!mapDone) gpu.instance.ProcessEvents();

    const float* mapped = static_cast<const float*>(bufReadback.GetConstMappedRange(0, rgb_size));
    if (!mapped) return false;

    std::memcpy(img.rgb.data(), mapped, rgb_size);
    bufReadback.Unmap();

    return true;
}

} // namespace flow

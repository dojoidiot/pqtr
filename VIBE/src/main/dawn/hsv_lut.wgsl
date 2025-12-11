// hsv_lut.wgsl - VIBE HSV LUT Module
//
// 2D LUT indexed by hue and saturation.
// LUT contains delta (dH, dS, dV) per cell.
// Layout: h_bins * s_bins * 3 floats

struct Uniforms {
    width: u32,
    height: u32,
    h_bins: u32,
    s_bins: u32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;
@group(0) @binding(3) var<storage, read> lut: array<f32>;

const GAMMA: f32 = 2.2;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let base_idx = (y * uniforms.width + x) * 3u;
    let h_bins = uniforms.h_bins;
    let s_bins = uniforms.s_bins;

    // Load and convert to gamma
    let b_lin = clamp(input[base_idx + 0u], 0.0, 1.0);
    let g_lin = clamp(input[base_idx + 1u], 0.0, 1.0);
    let r_lin = clamp(input[base_idx + 2u], 0.0, 1.0);

    var r = pow(r_lin, 1.0 / GAMMA);
    var g = pow(g_lin, 1.0 / GAMMA);
    var b = pow(b_lin, 1.0 / GAMMA);

    // RGB to HSV (inline)
    let max_c = max(max(r, g), b);
    let min_c = min(min(r, g), b);
    var v = max_c;
    var s: f32 = 0.0;
    if (max_c > 0.001) {
        s = (max_c - min_c) / max_c;
    }
    var h: f32 = 0.0;

    if (max_c != min_c) {
        let d = max_c - min_c;
        if (max_c == r) {
            h = 60.0 * ((g - b) / d);
            if (h < 0.0) { h += 360.0; }
        } else if (max_c == g) {
            h = 60.0 * ((b - r) / d + 2.0);
        } else {
            h = 60.0 * ((r - g) / d + 4.0);
        }
    }

    // LUT lookup with bilinear interpolation
    let h_pos = (h / 360.0) * f32(h_bins);
    let s_pos = s * f32(s_bins - 1u);

    let h0 = u32(h_pos) % h_bins;
    let h1 = (h0 + 1u) % h_bins;
    let s0 = u32(clamp(s_pos, 0.0, f32(s_bins - 1u)));
    let s1 = min(s0 + 1u, s_bins - 1u);

    let hf = h_pos - floor(h_pos);
    let sf = s_pos - f32(s0);

    // Lookup indices
    let idx00 = (h0 * s_bins + s0) * 3u;
    let idx01 = (h0 * s_bins + s1) * 3u;
    let idx10 = (h1 * s_bins + s0) * 3u;
    let idx11 = (h1 * s_bins + s1) * 3u;

    // Bilinear interpolation for dH, dS, dV
    let w00 = (1.0 - hf) * (1.0 - sf);
    let w01 = (1.0 - hf) * sf;
    let w10 = hf * (1.0 - sf);
    let w11 = hf * sf;

    let dh = w00 * lut[idx00 + 0u] + w01 * lut[idx01 + 0u] + w10 * lut[idx10 + 0u] + w11 * lut[idx11 + 0u];
    let ds = w00 * lut[idx00 + 1u] + w01 * lut[idx01 + 1u] + w10 * lut[idx10 + 1u] + w11 * lut[idx11 + 1u];
    let dv = w00 * lut[idx00 + 2u] + w01 * lut[idx01 + 2u] + w10 * lut[idx10 + 2u] + w11 * lut[idx11 + 2u];

    // Apply deltas
    h = h + dh * 0.5;
    if (h < 0.0) { h += 360.0; }
    if (h >= 360.0) { h -= 360.0; }
    s = clamp(s + ds, 0.0, 1.0);
    v = clamp(v + dv, 0.0, 1.0);

    // HSV to RGB (inline)
    let c = v * s;
    let hp = h / 60.0;
    let x_val = c * (1.0 - abs(hp - 2.0 * floor(hp / 2.0) - 1.0));
    let m = v - c;

    if (hp < 1.0) { r = c; g = x_val; b = 0.0; }
    else if (hp < 2.0) { r = x_val; g = c; b = 0.0; }
    else if (hp < 3.0) { r = 0.0; g = c; b = x_val; }
    else if (hp < 4.0) { r = 0.0; g = x_val; b = c; }
    else if (hp < 5.0) { r = x_val; g = 0.0; b = c; }
    else { r = c; g = 0.0; b = x_val; }

    r = r + m;
    g = g + m;
    b = b + m;

    // Back to linear
    output[base_idx + 0u] = pow(clamp(b, 0.0, 1.0), GAMMA);
    output[base_idx + 1u] = pow(clamp(g, 0.0, 1.0), GAMMA);
    output[base_idx + 2u] = pow(clamp(r, 0.0, 1.0), GAMMA);
}

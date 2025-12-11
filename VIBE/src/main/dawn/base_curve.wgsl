// base_curve.wgsl - VIBE Base Curve Module
//
// Applies a 256-point tone curve in sRGB space.
// Curve data passed as separate storage buffer.

struct Uniforms {
    width: u32,
    height: u32,
    _pad0: f32,
    _pad1: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;
@group(0) @binding(3) var<storage, read> curve: array<f32>;  // 768 elements (256 per channel)

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let base_idx = (y * uniforms.width + x) * 3u;

    // Process each channel
    for (var c = 0u; c < 3u; c++) {
        let lin = input[base_idx + c];

        // Linear to sRGB
        let lin_clamped = clamp(lin, 0.0, 1.0);
        var srgb: f32;
        if (lin_clamped <= 0.0031308) {
            srgb = lin_clamped * 12.92;
        } else {
            srgb = 1.055 * pow(lin_clamped, 1.0 / 2.4) - 0.055;
        }

        // Apply curve with linear interpolation (per-channel: base = c * 256)
        let pos = srgb * 255.0;
        let idx0 = u32(clamp(pos, 0.0, 254.0));
        let idx1 = idx0 + 1u;
        let frac = pos - f32(idx0);
        let curve_base = c * 256u;

        let v0 = curve[curve_base + idx0];
        let v1 = curve[curve_base + idx1];
        let curved = v0 + frac * (v1 - v0);

        // sRGB to linear
        var result: f32;
        if (curved <= 0.04045) {
            result = curved / 12.92;
        } else {
            result = pow((curved + 0.055) / 1.055, 2.4);
        }

        output[base_idx + c] = result;
    }
}

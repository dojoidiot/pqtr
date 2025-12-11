// color_matrix.wgsl - VIBE Color Matrix Module
//
// 3x3 linear RGB transform.
// Used for color space conversions and color grading.

struct Uniforms {
    width: u32,
    height: u32,
    _pad0: f32,
    _pad1: f32,
    // Matrix stored row-major: [r0c0, r0c1, r0c2, r1c0, ...]
    m00: f32, m01: f32, m02: f32, _p0: f32,
    m10: f32, m11: f32, m12: f32, _p1: f32,
    m20: f32, m21: f32, m22: f32, _p2: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let base_idx = (y * uniforms.width + x) * 3u;

    // Input is BGR
    let b = input[base_idx + 0u];
    let g = input[base_idx + 1u];
    let r = input[base_idx + 2u];

    // Matrix multiply (RGB -> RGB)
    let r_out = r * uniforms.m00 + g * uniforms.m01 + b * uniforms.m02;
    let g_out = r * uniforms.m10 + g * uniforms.m11 + b * uniforms.m12;
    let b_out = r * uniforms.m20 + g * uniforms.m21 + b * uniforms.m22;

    // Output is BGR
    output[base_idx + 0u] = b_out;
    output[base_idx + 1u] = g_out;
    output[base_idx + 2u] = r_out;
}

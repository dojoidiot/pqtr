// cst.wgsl - Color Space Transform
// Applies 3x3 matrix: camera RGB → linear sRGB
// Input: RGB float32 buffer
// Output: RGB float32 buffer

struct Params {
    // Row-major 3x3 matrix
    m00: f32, m01: f32, m02: f32, _pad0: f32,
    m10: f32, m11: f32, m12: f32, _pad1: f32,
    m20: f32, m21: f32, m22: f32, _pad2: f32,
    width: u32,
    height: u32,
    _pad3: u32,
    _pad4: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<f32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let total = params.width * params.height;

    if (idx >= total) {
        return;
    }

    let base = idx * 3u;
    let r_in = input[base + 0u];
    let g_in = input[base + 1u];
    let b_in = input[base + 2u];

    // Matrix multiply
    let r_out = params.m00 * r_in + params.m01 * g_in + params.m02 * b_in;
    let g_out = params.m10 * r_in + params.m11 * g_in + params.m12 * b_in;
    let b_out = params.m20 * r_in + params.m21 * g_in + params.m22 * b_in;

    output[base + 0u] = r_out;
    output[base + 1u] = g_out;
    output[base + 2u] = b_out;
}

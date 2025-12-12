// color_matrix.wgsl - Camera Color Matrix
//
// Input: f32 RGB (linear, camera space)
// Output: f32 RGB (linear, working space)
//
// Applies 3x3 color matrix transform.
// Matrix is row-major: out_r = m00*r + m01*g + m02*b

struct Uniforms {
    width: u32,
    height: u32,
    _pad0: f32,
    _pad1: f32,
    // Row 0: R output
    m00: f32, m01: f32, m02: f32, _p0: f32,
    // Row 1: G output
    m10: f32, m11: f32, m12: f32, _p1: f32,
    // Row 2: B output
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

    let idx = (y * uniforms.width + x) * 3u;

    // Input is BGR
    let b = input[idx + 0u];
    let g = input[idx + 1u];
    let r = input[idx + 2u];

    // Apply matrix (RGB order for matrix, BGR for storage)
    let out_r = uniforms.m00 * r + uniforms.m01 * g + uniforms.m02 * b;
    let out_g = uniforms.m10 * r + uniforms.m11 * g + uniforms.m12 * b;
    let out_b = uniforms.m20 * r + uniforms.m21 * g + uniforms.m22 * b;

    // Output BGR
    output[idx + 0u] = out_b;
    output[idx + 1u] = out_g;
    output[idx + 2u] = out_r;
}

// lut_curve.wgsl - VIBE 1D LUT Curve Module
//
// Per-channel 1D LUT with 8-bit quantization.
// Expects 768 floats (256 per channel: R, G, B).

struct Uniforms {
    width: u32,
    height: u32,
    _pad0: f32,
    _pad1: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;
@group(0) @binding(3) var<storage, read> lut: array<f32>;  // 768 elements (256 per R,G,B)

const GAMMA: f32 = 2.2;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let base_idx = (y * uniforms.width + x) * 3u;

    // Load and convert to gamma
    let b_lin = clamp(input[base_idx + 0u], 0.0, 1.0);
    let g_lin = clamp(input[base_idx + 1u], 0.0, 1.0);
    let r_lin = clamp(input[base_idx + 2u], 0.0, 1.0);

    let r_gamma = pow(r_lin, 1.0 / GAMMA);
    let g_gamma = pow(g_lin, 1.0 / GAMMA);
    let b_gamma = pow(b_lin, 1.0 / GAMMA);

    // Quantize to 8-bit and look up (R channel at offset 0)
    let ri = u32(clamp(r_gamma * 255.0 + 0.5, 0.0, 255.0));
    let ro = lut[ri];

    // G channel at offset 256
    let gi = u32(clamp(g_gamma * 255.0 + 0.5, 0.0, 255.0));
    let go = lut[256u + gi];

    // B channel at offset 512
    let bi = u32(clamp(b_gamma * 255.0 + 0.5, 0.0, 255.0));
    let bo = lut[512u + bi];

    // Back to linear
    output[base_idx + 0u] = pow(bo, GAMMA);
    output[base_idx + 1u] = pow(go, GAMMA);
    output[base_idx + 2u] = pow(ro, GAMMA);
}

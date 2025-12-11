// exposure.wgsl - VIBE Exposure Module
//
// Theory: Linear scaling by 2^ev where ev = (dial - 0.5) * 8
// Dial 0.0 = -4 EV (16x darker)
// Dial 0.5 = 0 EV (neutral)
// Dial 1.0 = +4 EV (16x brighter)

struct Uniforms {
    width: u32,
    height: u32,
    dial: f32,
    _pad: f32,
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

    // BGR float32, row-major: index = (y * width + x) * 3 + channel
    let base_idx = (y * uniforms.width + x) * 3u;

    // Calculate multiplier: 2^ev where ev = (dial - 0.5) * 8
    let ev = (uniforms.dial - 0.5) * 8.0;
    let mult = pow(2.0, ev);

    // Apply to all channels
    output[base_idx + 0u] = input[base_idx + 0u] * mult;  // B
    output[base_idx + 1u] = input[base_idx + 1u] * mult;  // G
    output[base_idx + 2u] = input[base_idx + 2u] * mult;  // R
}

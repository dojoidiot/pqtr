// blc_bayer.wgsl - Black Level Correction on Bayer
//
// Input: u16 Bayer data (single channel)
// Output: f32 normalized [0,1+] Bayer
//
// Formula: output = max(0, (input - black) / (white - black))

struct Uniforms {
    width: u32,
    height: u32,
    black_level: f32,
    white_level: f32,
}

@group(0) @binding(0) var<storage, read> input: array<u32>;  // Packed u16 pairs
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= uniforms.width || y >= uniforms.height) {
        return;
    }

    let idx = y * uniforms.width + x;

    // Unpack u16 from u32 (two pixels per u32)
    let packed_idx = idx / 2u;
    let packed = input[packed_idx];
    var raw_value: f32;
    if (idx % 2u == 0u) {
        raw_value = f32(packed & 0xFFFFu);
    } else {
        raw_value = f32(packed >> 16u);
    }

    // Apply black level correction
    let black = uniforms.black_level;
    let white = uniforms.white_level;
    let scale = 1.0 / (white - black);

    let normalized = (raw_value - black) * scale;
    output[idx] = max(0.0, normalized);
}

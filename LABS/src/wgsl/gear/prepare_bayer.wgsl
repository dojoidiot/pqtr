// prepare_bayer.wgsl - Combined BLC + White Balance on Bayer
//
// Input: u16 Bayer data (packed as u32 pairs)
// Output: f32 normalized, white-balanced Bayer
//
// Combines blc_bayer and wb_bayer into single pass for efficiency.

struct Uniforms {
    width: u32,
    height: u32,
    black_level: f32,
    white_level: f32,
    wb_r: f32,
    wb_b: f32,
    pattern: u32,   // 0=RGGB, 1=GRBG, 2=BGGR, 3=GBRG
    _pad: f32,
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

    // Unpack u16 from u32
    let packed_idx = idx / 2u;
    let packed = input[packed_idx];
    var raw_value: f32;
    if (idx % 2u == 0u) {
        raw_value = f32(packed & 0xFFFFu);
    } else {
        raw_value = f32(packed >> 16u);
    }

    // Black level correction
    let black = uniforms.black_level;
    let white = uniforms.white_level;
    let scale = 1.0 / (white - black);
    let normalized = max(0.0, (raw_value - black) * scale);

    // Determine WB gain based on Bayer position
    let px = x % 2u;
    let py = y % 2u;
    let pos = py * 2u + px;

    var gain: f32 = 1.0;

    if (uniforms.pattern == 0u) {
        // RGGB: 0=R, 1=G, 2=G, 3=B
        if (pos == 0u) { gain = uniforms.wb_r; }
        else if (pos == 3u) { gain = uniforms.wb_b; }
    } else if (uniforms.pattern == 1u) {
        // GRBG: 0=G, 1=R, 2=B, 3=G
        if (pos == 1u) { gain = uniforms.wb_r; }
        else if (pos == 2u) { gain = uniforms.wb_b; }
    } else if (uniforms.pattern == 2u) {
        // BGGR: 0=B, 1=G, 2=G, 3=R
        if (pos == 0u) { gain = uniforms.wb_b; }
        else if (pos == 3u) { gain = uniforms.wb_r; }
    } else {
        // GBRG: 0=G, 1=B, 2=R, 3=G
        if (pos == 1u) { gain = uniforms.wb_b; }
        else if (pos == 2u) { gain = uniforms.wb_r; }
    }

    output[idx] = normalized * gain;
}

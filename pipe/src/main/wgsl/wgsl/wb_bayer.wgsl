// wb_bayer.wgsl - White Balance on Bayer
//
// Input: f32 Bayer (normalized)
// Output: f32 white-balanced Bayer
//
// Applies R/G/B gains based on 2x2 Bayer pattern position.
// Pattern codes: 0=RGGB, 1=GRBG, 2=BGGR, 3=GBRG

struct Uniforms {
    width: u32,
    height: u32,
    wb_r: f32,      // Red gain (normalized, G=1.0)
    wb_b: f32,      // Blue gain (normalized, G=1.0)
    pattern: u32,   // Bayer pattern code (0-3)
    _pad0: f32,
    _pad1: f32,
    _pad2: f32,
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

    let idx = y * uniforms.width + x;
    let value = input[idx];

    // Determine which color this pixel is based on Bayer pattern
    let px = x % 2u;
    let py = y % 2u;
    let pos = py * 2u + px;  // 0=TL, 1=TR, 2=BL, 3=BR

    // Pattern layouts (position -> color):
    // RGGB(0): 0=R, 1=G, 2=G, 3=B
    // GRBG(1): 0=G, 1=R, 2=B, 3=G
    // BGGR(2): 0=B, 1=G, 2=G, 3=R
    // GBRG(3): 0=G, 1=B, 2=R, 3=G

    var gain: f32 = 1.0;  // Default G gain

    if (uniforms.pattern == 0u) {
        // RGGB
        if (pos == 0u) { gain = uniforms.wb_r; }
        else if (pos == 3u) { gain = uniforms.wb_b; }
    } else if (uniforms.pattern == 1u) {
        // GRBG
        if (pos == 1u) { gain = uniforms.wb_r; }
        else if (pos == 2u) { gain = uniforms.wb_b; }
    } else if (uniforms.pattern == 2u) {
        // BGGR
        if (pos == 0u) { gain = uniforms.wb_b; }
        else if (pos == 3u) { gain = uniforms.wb_r; }
    } else {
        // GBRG
        if (pos == 1u) { gain = uniforms.wb_b; }
        else if (pos == 2u) { gain = uniforms.wb_r; }
    }

    output[idx] = value * gain;
}

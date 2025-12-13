// wb.wgsl - White Balance on Bayer
// Applies per-channel gains to RGGB Bayer pattern
// Input: Bayer float32 buffer (normalized [0,1])
// Output: Bayer float32 buffer (white balanced)

struct Params {
    gain_r: f32,    // Red gain (wb_rggb[0] / wb_rggb[1])
    gain_g: f32,    // Green gain (1.0)
    gain_b: f32,    // Blue gain (wb_rggb[2] / wb_rggb[1])
    _pad: f32,
    width: u32,
    height: u32,
    pattern: u32,   // 46=RGGB, 47=GRBG, 48=BGGR, 49=GBRG
    _pad2: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<f32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

// Get channel type for pixel at (x, y) given Bayer pattern
// Returns: 0=R, 1=G, 2=B
fn getChannel(x: u32, y: u32, pattern: u32) -> u32 {
    let px = x & 1u;
    let py = y & 1u;
    let pos = py * 2u + px;

    // RGGB (46): R G / G B -> positions 0,3=R/B, 1,2=G
    // GRBG (47): G R / B G -> positions 1,2=R/B, 0,3=G
    // BGGR (48): B G / G R -> positions 0,3=B/R, 1,2=G
    // GBRG (49): G B / R G -> positions 1,2=B/R, 0,3=G

    switch (pattern) {
        case 46u: { // RGGB
            if (pos == 0u) { return 0u; }      // R
            if (pos == 3u) { return 2u; }      // B
            return 1u;                          // G
        }
        case 47u: { // GRBG
            if (pos == 1u) { return 0u; }      // R
            if (pos == 2u) { return 2u; }      // B
            return 1u;                          // G
        }
        case 48u: { // BGGR
            if (pos == 0u) { return 2u; }      // B
            if (pos == 3u) { return 0u; }      // R
            return 1u;                          // G
        }
        case 49u: { // GBRG
            if (pos == 1u) { return 2u; }      // B
            if (pos == 2u) { return 0u; }      // R
            return 1u;                          // G
        }
        default: { // Default RGGB
            if (pos == 0u) { return 0u; }
            if (pos == 3u) { return 2u; }
            return 1u;
        }
    }
}

@compute @workgroup_size(16, 16)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= params.width || y >= params.height) {
        return;
    }

    let idx = y * params.width + x;
    let value = input[idx];

    // Get channel and apply appropriate gain
    let channel = getChannel(x, y, params.pattern);
    var gain: f32;
    switch (channel) {
        case 0u: { gain = params.gain_r; }
        case 1u: { gain = params.gain_g; }
        case 2u: { gain = params.gain_b; }
        default: { gain = 1.0; }
    }

    output[idx] = value * gain;
}

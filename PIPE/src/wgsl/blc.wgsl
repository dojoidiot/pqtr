// blc.wgsl - Black Level Correction
// Normalizes raw sensor values: output = (input - black) / (white - black)
// Input: Bayer uint16 buffer
// Output: Bayer float32 buffer [0, 1+]

struct Params {
    black_level: f32,
    white_level: f32,
    width: u32,
    height: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<u32>;      // uint16 packed as u32
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let total = params.width * params.height;

    if (idx >= total) {
        return;
    }

    // Read uint16 value (packed 2 per u32)
    let word_idx = idx / 2u;
    let word = input[word_idx];
    var raw: f32;
    if ((idx & 1u) == 0u) {
        raw = f32(word & 0xFFFFu);
    } else {
        raw = f32(word >> 16u);
    }

    // Normalize: (raw - black) / (white - black)
    let scale = 1.0 / (params.white_level - params.black_level);
    var normalized = (raw - params.black_level) * scale;

    // Clamp negative values to zero
    normalized = max(normalized, 0.0);

    output[idx] = normalized;
}

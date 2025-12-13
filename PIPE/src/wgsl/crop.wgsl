// crop.wgsl - Active Area Crop
// Extracts rectangular region from image
// Input: RGB float32 buffer (in_width x in_height)
// Output: RGB float32 buffer (crop_width x crop_height)

struct Params {
    in_width: u32,
    in_height: u32,
    crop_left: u32,
    crop_top: u32,
    crop_width: u32,
    crop_height: u32,
    _pad0: u32,
    _pad1: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<f32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

@compute @workgroup_size(16, 16)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= params.crop_width || y >= params.crop_height) {
        return;
    }

    // Source coordinates
    let src_x = x + params.crop_left;
    let src_y = y + params.crop_top;

    // Bounds check
    if (src_x >= params.in_width || src_y >= params.in_height) {
        return;
    }

    // Copy RGB values
    let src_idx = (src_y * params.in_width + src_x) * 3u;
    let dst_idx = (y * params.crop_width + x) * 3u;

    output[dst_idx + 0u] = input[src_idx + 0u];
    output[dst_idx + 1u] = input[src_idx + 1u];
    output[dst_idx + 2u] = input[src_idx + 2u];
}

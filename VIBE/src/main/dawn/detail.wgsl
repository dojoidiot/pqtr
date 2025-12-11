// detail.wgsl - VIBE Detail Module
//
// Sharpening via unsharp mask (blur + subtract).
// Note: Radius is fixed at 1 pixel for simplicity.

struct Uniforms {
    width: u32,
    height: u32,
    sharpen_amount: f32,
    sharpen_radius: f32,  // 0-1 dial, currently unused (fixed radius=1)
    denoise_amount: f32,  // unused in simplified version
    denoise_radius: f32,  // unused in simplified version
    _pad0: f32,
    _pad1: f32,
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

    let base_idx = (y * uniforms.width + x) * 3u;
    let w = uniforms.width;
    let h = uniforms.height;

    let amount = uniforms.sharpen_amount * 2.0;

    // If no sharpening, pass through
    if (abs(amount) < 0.01) {
        output[base_idx + 0u] = input[base_idx + 0u];
        output[base_idx + 1u] = input[base_idx + 1u];
        output[base_idx + 2u] = input[base_idx + 2u];
        return;
    }

    // 3x3 box blur (radius=1)
    // Clamp coordinates to image bounds
    let x0 = max(i32(x) - 1, 0);
    let x1 = i32(x);
    let x2 = min(i32(x) + 1, i32(w) - 1);
    let y0 = max(i32(y) - 1, 0);
    let y1 = i32(y);
    let y2 = min(i32(y) + 1, i32(h) - 1);

    // Process each channel
    for (var c = 0u; c < 3u; c++) {
        let center = input[base_idx + c];

        // Sum of 3x3 neighborhood
        var sum = 0.0;
        sum += input[(u32(y0) * w + u32(x0)) * 3u + c];
        sum += input[(u32(y0) * w + u32(x1)) * 3u + c];
        sum += input[(u32(y0) * w + u32(x2)) * 3u + c];
        sum += input[(u32(y1) * w + u32(x0)) * 3u + c];
        sum += input[(u32(y1) * w + u32(x1)) * 3u + c];
        sum += input[(u32(y1) * w + u32(x2)) * 3u + c];
        sum += input[(u32(y2) * w + u32(x0)) * 3u + c];
        sum += input[(u32(y2) * w + u32(x1)) * 3u + c];
        sum += input[(u32(y2) * w + u32(x2)) * 3u + c];

        let blur = sum / 9.0;

        // Unsharp mask: output = center + amount * (center - blur)
        let sharpened = center + amount * (center - blur);

        output[base_idx + c] = clamp(sharpened, 0.0, 1.0);
    }
}

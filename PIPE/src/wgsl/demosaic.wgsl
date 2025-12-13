// demosaic.wgsl - Bayer to RGB (Bilinear interpolation)
// Input: Bayer float32 buffer (WxH)
// Output: RGB float32 buffer (WxHx3)

struct Params {
    width: u32,
    height: u32,
    pattern: u32,   // 46=RGGB, 47=GRBG, 48=BGGR, 49=GBRG
    _pad: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<f32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

// Safe read with clamping
fn readBayer(x: i32, y: i32) -> f32 {
    let cx = clamp(x, 0, i32(params.width) - 1);
    let cy = clamp(y, 0, i32(params.height) - 1);
    return input[u32(cy) * params.width + u32(cx)];
}

// Get channel at (x,y): 0=R, 1=G, 2=B
fn getChannel(x: u32, y: u32) -> u32 {
    let px = x & 1u;
    let py = y & 1u;
    let pos = py * 2u + px;

    switch (params.pattern) {
        case 46u: { // RGGB
            if (pos == 0u) { return 0u; }
            if (pos == 3u) { return 2u; }
            return 1u;
        }
        case 47u: { // GRBG
            if (pos == 1u) { return 0u; }
            if (pos == 2u) { return 2u; }
            return 1u;
        }
        case 48u: { // BGGR
            if (pos == 0u) { return 2u; }
            if (pos == 3u) { return 0u; }
            return 1u;
        }
        case 49u: { // GBRG
            if (pos == 1u) { return 2u; }
            if (pos == 2u) { return 0u; }
            return 1u;
        }
        default: {
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

    let ix = i32(x);
    let iy = i32(y);
    let channel = getChannel(x, y);
    let center = readBayer(ix, iy);

    var r: f32;
    var g: f32;
    var b: f32;

    // Bilinear interpolation based on pixel position
    switch (channel) {
        case 0u: { // Red pixel
            r = center;
            // G: average of 4 neighbors
            g = (readBayer(ix-1, iy) + readBayer(ix+1, iy) +
                 readBayer(ix, iy-1) + readBayer(ix, iy+1)) * 0.25;
            // B: average of 4 diagonal neighbors
            b = (readBayer(ix-1, iy-1) + readBayer(ix+1, iy-1) +
                 readBayer(ix-1, iy+1) + readBayer(ix+1, iy+1)) * 0.25;
        }
        case 1u: { // Green pixel
            // Figure out if R is horizontal or vertical
            let isGreenRow = (y & 1u) == 0u;

            // For RGGB: row 0 = R G R G, row 1 = G B G B
            // Green in row 0: R neighbors horizontal, B neighbors vertical
            // Green in row 1: B neighbors horizontal, R neighbors vertical
            var r_h: bool;
            switch (params.pattern) {
                case 46u: { r_h = !isGreenRow; } // RGGB: R horizontal when y is odd
                case 47u: { r_h = isGreenRow; }  // GRBG: R horizontal when y is even
                case 48u: { r_h = isGreenRow; }  // BGGR: R horizontal when y is even
                case 49u: { r_h = !isGreenRow; } // GBRG: R horizontal when y is odd
                default: { r_h = !isGreenRow; }
            }

            g = center;
            if (r_h) {
                r = (readBayer(ix-1, iy) + readBayer(ix+1, iy)) * 0.5;
                b = (readBayer(ix, iy-1) + readBayer(ix, iy+1)) * 0.5;
            } else {
                r = (readBayer(ix, iy-1) + readBayer(ix, iy+1)) * 0.5;
                b = (readBayer(ix-1, iy) + readBayer(ix+1, iy)) * 0.5;
            }
        }
        case 2u: { // Blue pixel
            b = center;
            // G: average of 4 neighbors
            g = (readBayer(ix-1, iy) + readBayer(ix+1, iy) +
                 readBayer(ix, iy-1) + readBayer(ix, iy+1)) * 0.25;
            // R: average of 4 diagonal neighbors
            r = (readBayer(ix-1, iy-1) + readBayer(ix+1, iy-1) +
                 readBayer(ix-1, iy+1) + readBayer(ix+1, iy+1)) * 0.25;
        }
        default: {
            r = center;
            g = center;
            b = center;
        }
    }

    // Write RGB output
    let out_idx = (y * params.width + x) * 3u;
    output[out_idx + 0u] = r;
    output[out_idx + 1u] = g;
    output[out_idx + 2u] = b;
}

// demosaic.wgsl - Bayer Demosaicing
//
// Input: f32 Bayer (single channel, white-balanced)
// Output: f32 RGB (3 channels, BGR order)
//
// Algorithm: Bilinear interpolation
// Pattern codes: 0=RGGB, 1=GRBG, 2=BGGR, 3=GBRG
//
// NOTE: All pixel access inlined due to Dawn function call bug

struct Uniforms {
    width: u32,
    height: u32,
    pattern: u32,
    _pad: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = i32(gid.x);
    let y = i32(gid.y);
    let w = i32(uniforms.width);
    let h = i32(uniforms.height);

    if (x >= w || y >= h) {
        return;
    }

    let out_idx = u32(y * w + x) * 3u;

    // Current pixel value (inline)
    var c: f32;
    { let cx = clamp(x, 0, w - 1); let cy = clamp(y, 0, h - 1); c = input[u32(cy) * u32(w) + u32(cx)]; }

    // Neighbors (all inlined)
    var n: f32;  // North
    { let cx = clamp(x, 0, w - 1); let cy = clamp(y - 1, 0, h - 1); n = input[u32(cy) * u32(w) + u32(cx)]; }

    var s: f32;  // South
    { let cx = clamp(x, 0, w - 1); let cy = clamp(y + 1, 0, h - 1); s = input[u32(cy) * u32(w) + u32(cx)]; }

    var e: f32;  // East
    { let cx = clamp(x + 1, 0, w - 1); let cy = clamp(y, 0, h - 1); e = input[u32(cy) * u32(w) + u32(cx)]; }

    var ww: f32;  // West
    { let cx = clamp(x - 1, 0, w - 1); let cy = clamp(y, 0, h - 1); ww = input[u32(cy) * u32(w) + u32(cx)]; }

    var ne: f32;
    { let cx = clamp(x + 1, 0, w - 1); let cy = clamp(y - 1, 0, h - 1); ne = input[u32(cy) * u32(w) + u32(cx)]; }

    var nw: f32;
    { let cx = clamp(x - 1, 0, w - 1); let cy = clamp(y - 1, 0, h - 1); nw = input[u32(cy) * u32(w) + u32(cx)]; }

    var se: f32;
    { let cx = clamp(x + 1, 0, w - 1); let cy = clamp(y + 1, 0, h - 1); se = input[u32(cy) * u32(w) + u32(cx)]; }

    var sw: f32;
    { let cx = clamp(x - 1, 0, w - 1); let cy = clamp(y + 1, 0, h - 1); sw = input[u32(cy) * u32(w) + u32(cx)]; }

    // Determine pixel type based on pattern and position
    let px = u32(x) % 2u;
    let py = u32(y) % 2u;
    let pos = py * 2u + px;

    var r: f32 = 0.0;
    var g: f32 = 0.0;
    var b: f32 = 0.0;

    // RGGB pattern (0):
    //   R  Gr
    //   Gb B
    // pos: 0=R, 1=Gr, 2=Gb, 3=B

    if (uniforms.pattern == 0u) {
        if (pos == 0u) {
            // Red pixel
            r = c;
            g = (n + s + e + ww) * 0.25;
            b = (ne + nw + se + sw) * 0.25;
        } else if (pos == 1u) {
            // Green on red row
            r = (e + ww) * 0.5;
            g = c;
            b = (n + s) * 0.5;
        } else if (pos == 2u) {
            // Green on blue row
            r = (n + s) * 0.5;
            g = c;
            b = (e + ww) * 0.5;
        } else {
            // Blue pixel
            r = (ne + nw + se + sw) * 0.25;
            g = (n + s + e + ww) * 0.25;
            b = c;
        }
    } else if (uniforms.pattern == 1u) {
        // GRBG: 0=Gr, 1=R, 2=B, 3=Gb
        if (pos == 0u) {
            r = (e + ww) * 0.5;
            g = c;
            b = (n + s) * 0.5;
        } else if (pos == 1u) {
            r = c;
            g = (n + s + e + ww) * 0.25;
            b = (ne + nw + se + sw) * 0.25;
        } else if (pos == 2u) {
            r = (ne + nw + se + sw) * 0.25;
            g = (n + s + e + ww) * 0.25;
            b = c;
        } else {
            r = (n + s) * 0.5;
            g = c;
            b = (e + ww) * 0.5;
        }
    } else if (uniforms.pattern == 2u) {
        // BGGR: 0=B, 1=Gb, 2=Gr, 3=R
        if (pos == 0u) {
            r = (ne + nw + se + sw) * 0.25;
            g = (n + s + e + ww) * 0.25;
            b = c;
        } else if (pos == 1u) {
            r = (n + s) * 0.5;
            g = c;
            b = (e + ww) * 0.5;
        } else if (pos == 2u) {
            r = (e + ww) * 0.5;
            g = c;
            b = (n + s) * 0.5;
        } else {
            r = c;
            g = (n + s + e + ww) * 0.25;
            b = (ne + nw + se + sw) * 0.25;
        }
    } else {
        // GBRG: 0=Gb, 1=B, 2=R, 3=Gr
        if (pos == 0u) {
            r = (n + s) * 0.5;
            g = c;
            b = (e + ww) * 0.5;
        } else if (pos == 1u) {
            r = (ne + nw + se + sw) * 0.25;
            g = (n + s + e + ww) * 0.25;
            b = c;
        } else if (pos == 2u) {
            r = c;
            g = (n + s + e + ww) * 0.25;
            b = (ne + nw + se + sw) * 0.25;
        } else {
            r = (e + ww) * 0.5;
            g = c;
            b = (n + s) * 0.5;
        }
    }

    // Output BGR order
    output[out_idx + 0u] = b;
    output[out_idx + 1u] = g;
    output[out_idx + 2u] = r;
}

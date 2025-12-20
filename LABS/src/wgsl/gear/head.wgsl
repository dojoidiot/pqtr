// head.wgsl - Combined RAW to Linear RGB
//
// Single-pass: BLC + WB + Demosaic + Color Matrix
//
// Input: u16 Bayer (packed u32 pairs)
// Output: f32 RGB (linear, working space)
//
// This fused shader avoids intermediate buffer round-trips.

struct Uniforms {
    width: u32,
    height: u32,
    black_level: f32,
    white_level: f32,
    wb_r: f32,
    wb_b: f32,
    pattern: u32,   // 0=RGGB, 1=GRBG, 2=BGGR, 3=GBRG
    _pad0: f32,
    // Color matrix (row-major 3x3)
    m00: f32, m01: f32, m02: f32, _p0: f32,
    m10: f32, m11: f32, m12: f32, _p1: f32,
    m20: f32, m21: f32, m22: f32, _p2: f32,
}

@group(0) @binding(0) var<storage, read> bayer: array<u32>;  // Packed u16
@group(0) @binding(1) var<storage, read_write> output: array<f32>;  // RGB
@group(0) @binding(2) var<uniform> u: Uniforms;

// Workgroup shared memory for bayer tile (with halo for demosaic)
// Tile: 8x8 output, need 10x10 input (1 pixel halo each side)
var<workgroup> tile: array<f32, 100>;  // 10x10

fn get_bayer(x: i32, y: i32) -> f32 {
    let cx = clamp(x, 0, i32(u.width) - 1);
    let cy = clamp(y, 0, i32(u.height) - 1);
    let idx = u32(cy) * u.width + u32(cx);

    let packed_idx = idx / 2u;
    let packed = bayer[packed_idx];
    var raw: f32;
    if (idx % 2u == 0u) {
        raw = f32(packed & 0xFFFFu);
    } else {
        raw = f32(packed >> 16u);
    }

    // BLC
    let scale = 1.0 / (u.white_level - u.black_level);
    let normalized = max(0.0, (raw - u.black_level) * scale);

    // WB
    let px = u32(cx) % 2u;
    let py = u32(cy) % 2u;
    let pos = py * 2u + px;

    var gain: f32 = 1.0;
    if (u.pattern == 0u) {
        if (pos == 0u) { gain = u.wb_r; }
        else if (pos == 3u) { gain = u.wb_b; }
    } else if (u.pattern == 1u) {
        if (pos == 1u) { gain = u.wb_r; }
        else if (pos == 2u) { gain = u.wb_b; }
    } else if (u.pattern == 2u) {
        if (pos == 0u) { gain = u.wb_b; }
        else if (pos == 3u) { gain = u.wb_r; }
    } else {
        if (pos == 1u) { gain = u.wb_b; }
        else if (pos == 2u) { gain = u.wb_r; }
    }

    return normalized * gain;
}

@compute @workgroup_size(8, 8, 1)
fn main(
    @builtin(global_invocation_id) gid: vec3<u32>,
    @builtin(local_invocation_id) lid: vec3<u32>,
    @builtin(workgroup_id) wid: vec3<u32>
) {
    let x = i32(gid.x);
    let y = i32(gid.y);
    let lx = i32(lid.x);
    let ly = i32(lid.y);
    let w = i32(u.width);
    let h = i32(u.height);

    // Load tile with 1-pixel halo
    let tile_base_x = i32(wid.x) * 8 - 1;
    let tile_base_y = i32(wid.y) * 8 - 1;

    // Each thread loads multiple pixels to fill 10x10 tile
    for (var dy = ly; dy < 10; dy += 8) {
        for (var dx = lx; dx < 10; dx += 8) {
            let gx = tile_base_x + dx;
            let gy = tile_base_y + dy;
            tile[u32(dy) * 10u + u32(dx)] = get_bayer(gx, gy);
        }
    }

    workgroupBarrier();

    if (x >= w || y >= h) {
        return;
    }

    // Local tile access (offset by 1 for halo)
    let tx = lx + 1;
    let ty = ly + 1;

    let c = tile[u32(ty) * 10u + u32(tx)];
    let n = tile[u32(ty - 1) * 10u + u32(tx)];
    let s_val = tile[u32(ty + 1) * 10u + u32(tx)];
    let e = tile[u32(ty) * 10u + u32(tx + 1)];
    let ww = tile[u32(ty) * 10u + u32(tx - 1)];
    let ne = tile[u32(ty - 1) * 10u + u32(tx + 1)];
    let nw = tile[u32(ty - 1) * 10u + u32(tx - 1)];
    let se = tile[u32(ty + 1) * 10u + u32(tx + 1)];
    let sw = tile[u32(ty + 1) * 10u + u32(tx - 1)];

    // Demosaic
    let px = u32(x) % 2u;
    let py = u32(y) % 2u;
    let pos = py * 2u + px;

    var r: f32 = 0.0;
    var g: f32 = 0.0;
    var b: f32 = 0.0;

    if (u.pattern == 0u) {
        // RGGB
        if (pos == 0u) {
            r = c; g = (n + s_val + e + ww) * 0.25; b = (ne + nw + se + sw) * 0.25;
        } else if (pos == 1u) {
            r = (e + ww) * 0.5; g = c; b = (n + s_val) * 0.5;
        } else if (pos == 2u) {
            r = (n + s_val) * 0.5; g = c; b = (e + ww) * 0.5;
        } else {
            r = (ne + nw + se + sw) * 0.25; g = (n + s_val + e + ww) * 0.25; b = c;
        }
    } else if (u.pattern == 1u) {
        // GRBG
        if (pos == 0u) {
            r = (e + ww) * 0.5; g = c; b = (n + s_val) * 0.5;
        } else if (pos == 1u) {
            r = c; g = (n + s_val + e + ww) * 0.25; b = (ne + nw + se + sw) * 0.25;
        } else if (pos == 2u) {
            r = (ne + nw + se + sw) * 0.25; g = (n + s_val + e + ww) * 0.25; b = c;
        } else {
            r = (n + s_val) * 0.5; g = c; b = (e + ww) * 0.5;
        }
    } else if (u.pattern == 2u) {
        // BGGR
        if (pos == 0u) {
            r = (ne + nw + se + sw) * 0.25; g = (n + s_val + e + ww) * 0.25; b = c;
        } else if (pos == 1u) {
            r = (n + s_val) * 0.5; g = c; b = (e + ww) * 0.5;
        } else if (pos == 2u) {
            r = (e + ww) * 0.5; g = c; b = (n + s_val) * 0.5;
        } else {
            r = c; g = (n + s_val + e + ww) * 0.25; b = (ne + nw + se + sw) * 0.25;
        }
    } else {
        // GBRG
        if (pos == 0u) {
            r = (n + s_val) * 0.5; g = c; b = (e + ww) * 0.5;
        } else if (pos == 1u) {
            r = (ne + nw + se + sw) * 0.25; g = (n + s_val + e + ww) * 0.25; b = c;
        } else if (pos == 2u) {
            r = c; g = (n + s_val + e + ww) * 0.25; b = (ne + nw + se + sw) * 0.25;
        } else {
            r = (e + ww) * 0.5; g = c; b = (n + s_val) * 0.5;
        }
    }

    // Color matrix
    let out_r = u.m00 * r + u.m01 * g + u.m02 * b;
    let out_g = u.m10 * r + u.m11 * g + u.m12 * b;
    let out_b = u.m20 * r + u.m21 * g + u.m22 * b;

    // Output RGB (not BGR)
    let out_idx = (u32(y) * u.width + u32(x)) * 3u;
    output[out_idx + 0u] = out_r;
    output[out_idx + 1u] = out_g;
    output[out_idx + 2u] = out_b;
}

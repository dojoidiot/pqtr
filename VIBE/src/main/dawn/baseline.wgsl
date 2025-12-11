// baseline.wgsl - VIBE Baseline Module
//
// Highlight recovery + exposure adjustment.
// Recovers clipped channels by averaging unclipped ones.

struct Uniforms {
    width: u32,
    height: u32,
    ev: f32,
    clip_threshold: f32,
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

    var b = input[base_idx + 0u];
    var g = input[base_idx + 1u];
    var r = input[base_idx + 2u];

    let max_c = max(max(r, g), b);
    let clip = uniforms.clip_threshold;

    // Highlight recovery
    if (max_c > clip) {
        var clipped = 0u;
        if (r > clip) { clipped += 1u; }
        if (g > clip) { clipped += 1u; }
        if (b > clip) { clipped += 1u; }

        if (clipped < 3u) {
            var unclipped_sum = 0.0;
            var unclipped_count = 0u;

            if (r <= clip) { unclipped_sum += r; unclipped_count += 1u; }
            if (g <= clip) { unclipped_sum += g; unclipped_count += 1u; }
            if (b <= clip) { unclipped_sum += b; unclipped_count += 1u; }

            let avg = unclipped_sum / f32(unclipped_count);
            if (r > clip) { r = avg; }
            if (g > clip) { g = avg; }
            if (b > clip) { b = avg; }
        }
    }

    // Exposure
    let mult = pow(2.0, uniforms.ev);
    output[base_idx + 0u] = b * mult;
    output[base_idx + 1u] = g * mult;
    output[base_idx + 2u] = r * mult;
}

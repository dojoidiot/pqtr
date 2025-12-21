// head.cpp - GPU RAW processing
//
// Creates pipelines internally, runs head + warp passes

#include "flow.hpp"
#include "lute.hpp"
#include "drum.hpp"
#include <dawn/webgpu_cpp.h>
#include <cstring>
#include <vector>
#include <map>
#include <iostream>

namespace flow
{

    // Forward declarations from lute.cpp
    void luteLearn(const Done &head, const uint8_t *jpeg, size_t jpegSize,
                   const std::string &cameraKey);
    Done luteDiff(const Done &head, const uint8_t *jpeg, size_t jpegSize,
                  const std::string &cameraKey);

    // =========================================================================
    // Embedded shaders
    // =========================================================================

    static const char *HEAD_WGSL = R"(
struct Uniforms {
    width: u32, height: u32,
    black_level: f32, white_level: f32,
    wb_r: f32, wb_b: f32,
    pattern: u32, _pad0: f32,
    m00: f32, m01: f32, m02: f32, _p0: f32,
    m10: f32, m11: f32, m12: f32, _p1: f32,
    m20: f32, m21: f32, m22: f32, _p2: f32,
}

@group(0) @binding(0) var<storage, read> bayer: array<u32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> u: Uniforms;

var<workgroup> tile: array<f32, 100>;

fn get_bayer(x: i32, y: i32) -> f32 {
    let cx = clamp(x, 0, i32(u.width) - 1);
    let cy = clamp(y, 0, i32(u.height) - 1);
    let idx = u32(cy) * u.width + u32(cx);
    let packed = bayer[idx / 2u];
    var raw: f32;
    if (idx % 2u == 0u) { raw = f32(packed & 0xFFFFu); }
    else { raw = f32(packed >> 16u); }
    let scale = 1.0 / (u.white_level - u.black_level);
    let normalized = max(0.0, (raw - u.black_level) * scale);
    let px = u32(cx) % 2u; let py = u32(cy) % 2u;
    let pos = py * 2u + px;
    var gain: f32 = 1.0;
    if (u.pattern == 0u) { if (pos == 0u) { gain = u.wb_r; } else if (pos == 3u) { gain = u.wb_b; } }
    else if (u.pattern == 1u) { if (pos == 1u) { gain = u.wb_r; } else if (pos == 2u) { gain = u.wb_b; } }
    else if (u.pattern == 2u) { if (pos == 0u) { gain = u.wb_b; } else if (pos == 3u) { gain = u.wb_r; } }
    else { if (pos == 1u) { gain = u.wb_b; } else if (pos == 2u) { gain = u.wb_r; } }
    return normalized * gain;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>, @builtin(local_invocation_id) lid: vec3<u32>, @builtin(workgroup_id) wid: vec3<u32>) {
    let x = i32(gid.x); let y = i32(gid.y);
    let lx = i32(lid.x); let ly = i32(lid.y);
    let w = i32(u.width); let h = i32(u.height);
    let tile_base_x = i32(wid.x) * 8 - 1;
    let tile_base_y = i32(wid.y) * 8 - 1;
    for (var dy = ly; dy < 10; dy += 8) {
        for (var dx = lx; dx < 10; dx += 8) {
            tile[u32(dy) * 10u + u32(dx)] = get_bayer(tile_base_x + dx, tile_base_y + dy);
        }
    }
    workgroupBarrier();
    if (x >= w || y >= h) { return; }
    let tx = lx + 1; let ty = ly + 1;
    let c = tile[u32(ty) * 10u + u32(tx)];
    let n = tile[u32(ty - 1) * 10u + u32(tx)];
    let s_val = tile[u32(ty + 1) * 10u + u32(tx)];
    let e = tile[u32(ty) * 10u + u32(tx + 1)];
    let ww = tile[u32(ty) * 10u + u32(tx - 1)];
    let ne = tile[u32(ty - 1) * 10u + u32(tx + 1)];
    let nw = tile[u32(ty - 1) * 10u + u32(tx - 1)];
    let se = tile[u32(ty + 1) * 10u + u32(tx + 1)];
    let sw = tile[u32(ty + 1) * 10u + u32(tx - 1)];
    let px = u32(x) % 2u; let py = u32(y) % 2u;
    let pos = py * 2u + px;
    var r: f32 = 0.0; var g: f32 = 0.0; var b: f32 = 0.0;
    if (u.pattern == 0u) {
        if (pos == 0u) { r = c; g = (n + s_val + e + ww) * 0.25; b = (ne + nw + se + sw) * 0.25; }
        else if (pos == 1u) { r = (e + ww) * 0.5; g = c; b = (n + s_val) * 0.5; }
        else if (pos == 2u) { r = (n + s_val) * 0.5; g = c; b = (e + ww) * 0.5; }
        else { r = (ne + nw + se + sw) * 0.25; g = (n + s_val + e + ww) * 0.25; b = c; }
    } else if (u.pattern == 1u) {
        if (pos == 0u) { r = (e + ww) * 0.5; g = c; b = (n + s_val) * 0.5; }
        else if (pos == 1u) { r = c; g = (n + s_val + e + ww) * 0.25; b = (ne + nw + se + sw) * 0.25; }
        else if (pos == 2u) { r = (ne + nw + se + sw) * 0.25; g = (n + s_val + e + ww) * 0.25; b = c; }
        else { r = (n + s_val) * 0.5; g = c; b = (e + ww) * 0.5; }
    } else if (u.pattern == 2u) {
        if (pos == 0u) { r = (ne + nw + se + sw) * 0.25; g = (n + s_val + e + ww) * 0.25; b = c; }
        else if (pos == 1u) { r = (n + s_val) * 0.5; g = c; b = (e + ww) * 0.5; }
        else if (pos == 2u) { r = (e + ww) * 0.5; g = c; b = (n + s_val) * 0.5; }
        else { r = c; g = (n + s_val + e + ww) * 0.25; b = (ne + nw + se + sw) * 0.25; }
    } else {
        if (pos == 0u) { r = (n + s_val) * 0.5; g = c; b = (e + ww) * 0.5; }
        else if (pos == 1u) { r = (ne + nw + se + sw) * 0.25; g = (n + s_val + e + ww) * 0.25; b = c; }
        else if (pos == 2u) { r = c; g = (n + s_val + e + ww) * 0.25; b = (ne + nw + se + sw) * 0.25; }
        else { r = (e + ww) * 0.5; g = c; b = (n + s_val) * 0.5; }
    }
    let out_r = max(0.0, u.m00 * r + u.m01 * g + u.m02 * b);
    let out_g = max(0.0, u.m10 * r + u.m11 * g + u.m12 * b);
    let out_b = max(0.0, u.m20 * r + u.m21 * g + u.m22 * b);
    let out_idx = (u32(y) * u.width + u32(x)) * 3u;
    output[out_idx + 0u] = out_r;
    output[out_idx + 1u] = out_g;
    output[out_idx + 2u] = out_b;
}
)";

    static const char *WARP_WGSL = R"(
struct WarpUniforms {
    width: u32, height: u32, knot_count: u32, _pad: u32,
    knots: array<f32, 16>,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> u: WarpUniforms;

fn interpolate_spline(r: f32) -> f32 {
    let count = i32(u.knot_count);
    if (count <= 0) { return 0.0; }
    if (count == 1) { return u.knots[0]; }
    let r_clamped = clamp(r, 0.0, 1.0);
    let idx = r_clamped * f32(count - 1);
    let i0 = i32(idx);
    let i1 = min(i0 + 1, count - 1);
    let t = idx - f32(i0);
    return u.knots[i0] * (1.0 - t) + u.knots[i1] * t;
}

fn sample_rgb(x: f32, y: f32) -> vec3<f32> {
    let w = i32(u.width); let h = i32(u.height);
    let x0 = clamp(i32(x), 0, w - 1);
    let y0 = clamp(i32(y), 0, h - 1);
    let x1 = clamp(x0 + 1, 0, w - 1);
    let y1 = clamp(y0 + 1, 0, h - 1);
    let fx = max(0.0, x - f32(i32(x)));
    let fy = max(0.0, y - f32(i32(y)));
    let idx00 = (u32(y0) * u.width + u32(x0)) * 3u;
    let idx10 = (u32(y0) * u.width + u32(x1)) * 3u;
    let idx01 = (u32(y1) * u.width + u32(x0)) * 3u;
    let idx11 = (u32(y1) * u.width + u32(x1)) * 3u;
    var result: vec3<f32>;
    for (var c = 0u; c < 3u; c++) {
        let v00 = input[idx00 + c]; let v10 = input[idx10 + c];
        let v01 = input[idx01 + c]; let v11 = input[idx11 + c];
        let v0 = v00 * (1.0 - fx) + v10 * fx;
        let v1 = v01 * (1.0 - fx) + v11 * fx;
        result[c] = v0 * (1.0 - fy) + v1 * fy;
    }
    return result;
}

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = i32(gid.x); let y = i32(gid.y);
    let w = i32(u.width); let h = i32(u.height);
    if (x >= w || y >= h) { return; }
    if (u.knot_count == 0u) {
        let idx = (gid.y * u.width + gid.x) * 3u;
        output[idx + 0u] = input[idx + 0u];
        output[idx + 1u] = input[idx + 1u];
        output[idx + 2u] = input[idx + 2u];
        return;
    }
    let cx = f32(w) / 2.0; let cy = f32(h) / 2.0;
    let r_max = sqrt(cx * cx + cy * cy);
    let scale = 1.0 / 16384.0;
    var g_max: f32 = 1.0;
    for (var i = 0u; i < u.knot_count; i++) {
        let g = 1.0 + scale * u.knots[i];
        g_max = max(g_max, g);
    }
    let dx = f32(x) - cx; let dy = f32(y) - cy;
    let r = sqrt(dx * dx + dy * dy);
    var src_x: f32; var src_y: f32;
    if (r < 0.5) { src_x = f32(x); src_y = f32(y); }
    else {
        let r_norm = r / r_max;
        let spline_val = interpolate_spline(r_norm);
        let g = 1.0 + scale * spline_val;
        let g_normalized = g / g_max;
        src_x = cx + dx * g_normalized;
        src_y = cy + dy * g_normalized;
    }
    let rgb = sample_rgb(src_x, src_y);
    let out_idx = (gid.y * u.width + gid.x) * 3u;
    output[out_idx + 0u] = rgb.x;
    output[out_idx + 1u] = rgb.y;
    output[out_idx + 2u] = rgb.z;
}
)";

    // =========================================================================
    // Uniforms
    // =========================================================================

    struct HeadUniforms
    {
        uint32_t width, height;
        float black_level, white_level;
        float wb_r, wb_b;
        uint32_t pattern;
        float _pad0;
        float m00, m01, m02, _p0;
        float m10, m11, m12, _p1;
        float m20, m21, m22, _p2;
    };

    struct WarpUniforms
    {
        uint32_t width, height, knot_count, _pad;
        float knots[16];
    };

    // =========================================================================
    // Internal storage
    // =========================================================================

    struct TaskData
    {
        wgpu::Device device;
        wgpu::ComputePipeline head_pipe;
        wgpu::ComputePipeline warp_pipe;
        wgpu::Buffer bayer_buf;
        wgpu::Buffer rgb_buf;
        wgpu::Buffer rgb2_buf;
        wgpu::Buffer head_uniform;
        wgpu::Buffer warp_uniform;
        wgpu::Buffer readback_buf;
        wgpu::BindGroup head_bg;
        wgpu::BindGroup warp_bg;
        int w = 0, h = 0;
        size_t rgb_size = 0;
        bool has_warp = false;
        bool is_posted = false;

        // Tune mode
        bool is_tune = false;
        std::vector<uint8_t> view_data;  // embedded JPEG
        std::string camera_key;           // profile key

        // DRUM (local tone mapping)
        std::string dro_setting;          // DRO level from EXIF
    };

    static TaskData *g_task = nullptr;

    // =========================================================================
    // Task implementation
    // =========================================================================

    Task::~Task() {}

    void Task::post()
    {
        if (!g_task || g_task->is_posted)
            return;

        TaskData &t = *g_task;
        wgpu::CommandEncoder enc = t.device.CreateCommandEncoder();

        // Pass 1: head
        {
            wgpu::ComputePassEncoder pass = enc.BeginComputePass();
            pass.SetPipeline(t.head_pipe);
            pass.SetBindGroup(0, t.head_bg);
            pass.DispatchWorkgroups((t.w + 7) / 8, (t.h + 7) / 8, 1);
            pass.End();
        }

        // Pass 2: warp (if needed)
        if (t.has_warp)
        {
            wgpu::ComputePassEncoder pass = enc.BeginComputePass();
            pass.SetPipeline(t.warp_pipe);
            pass.SetBindGroup(0, t.warp_bg);
            pass.DispatchWorkgroups((t.w + 7) / 8, (t.h + 7) / 8, 1);
            pass.End();
            enc.CopyBufferToBuffer(t.rgb2_buf, 0, t.readback_buf, 0, t.rgb_size);
        }
        else
        {
            enc.CopyBufferToBuffer(t.rgb_buf, 0, t.readback_buf, 0, t.rgb_size);
        }

        wgpu::CommandBuffer cmd = enc.Finish();
        t.device.GetQueue().Submit(1, &cmd);
        t.is_posted = true;
    }

    void *Task::buff() const
    {
        if (!g_task)
            return nullptr;
        return const_cast<wgpu::Buffer *>(g_task->has_warp ? &g_task->rgb2_buf : &g_task->rgb_buf);
    }

    int Task::width() const { return g_task ? g_task->w : 0; }
    int Task::height() const { return g_task ? g_task->h : 0; }

    Done Task::done()
    {
        Done out;
        if (!g_task)
            return out;

        TaskData &t = *g_task;

        if (!t.is_posted)
        {
            wgpu::CommandEncoder enc = t.device.CreateCommandEncoder();

            {
                wgpu::ComputePassEncoder pass = enc.BeginComputePass();
                pass.SetPipeline(t.head_pipe);
                pass.SetBindGroup(0, t.head_bg);
                pass.DispatchWorkgroups((t.w + 7) / 8, (t.h + 7) / 8, 1);
                pass.End();
            }

            if (t.has_warp)
            {
                wgpu::ComputePassEncoder pass = enc.BeginComputePass();
                pass.SetPipeline(t.warp_pipe);
                pass.SetBindGroup(0, t.warp_bg);
                pass.DispatchWorkgroups((t.w + 7) / 8, (t.h + 7) / 8, 1);
                pass.End();
                enc.CopyBufferToBuffer(t.rgb2_buf, 0, t.readback_buf, 0, t.rgb_size);
            }
            else
            {
                enc.CopyBufferToBuffer(t.rgb_buf, 0, t.readback_buf, 0, t.rgb_size);
            }

            wgpu::CommandBuffer cmd = enc.Finish();
            t.device.GetQueue().Submit(1, &cmd);
        }

        bool mapped = false;
        t.readback_buf.MapAsync(
            wgpu::MapMode::Read, 0, t.rgb_size,
            wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::MapAsyncStatus, wgpu::StringView) { mapped = true; });

        wgpu::Instance instance = t.device.GetAdapter().GetInstance();
        while (!mapped)
            instance.ProcessEvents();

        out.width = t.w;
        out.height = t.h;
        out.rgb.resize(t.w * t.h * 3);

        const float *data = static_cast<const float *>(t.readback_buf.GetConstMappedRange());
        std::memcpy(out.rgb.data(), data, t.rgb_size);
        t.readback_buf.Unmap();

        // DRUM is now applied separately in the pipeline
        // (see flow test for staged application)

        // Tune mode: learn camera profile from flat + JPEG
        // (downsamples HEAD to JPEG size, learns resolution-independent LUT)
        if (t.is_tune && !t.view_data.empty() && !t.camera_key.empty())
        {
            luteLearn(out, t.view_data.data(), t.view_data.size(), t.camera_key);
        }

        delete g_task;
        g_task = nullptr;

        return out;
    }

    Done Task::diff()
    {
        Done out;
        if (!g_task)
            return out;

        TaskData &t = *g_task;

        // Need JPEG data for diff
        if (t.view_data.empty())
        {
            // No JPEG - just return empty
            delete g_task;
            g_task = nullptr;
            return out;
        }

        // Stash view data before done() clears task
        std::vector<uint8_t> view_copy = t.view_data;

        // Run GPU work and get HEAD result
        if (!t.is_posted)
        {
            wgpu::CommandEncoder enc = t.device.CreateCommandEncoder();

            {
                wgpu::ComputePassEncoder pass = enc.BeginComputePass();
                pass.SetPipeline(t.head_pipe);
                pass.SetBindGroup(0, t.head_bg);
                pass.DispatchWorkgroups((t.w + 7) / 8, (t.h + 7) / 8, 1);
                pass.End();
            }

            if (t.has_warp)
            {
                wgpu::ComputePassEncoder pass = enc.BeginComputePass();
                pass.SetPipeline(t.warp_pipe);
                pass.SetBindGroup(0, t.warp_bg);
                pass.DispatchWorkgroups((t.w + 7) / 8, (t.h + 7) / 8, 1);
                pass.End();
                enc.CopyBufferToBuffer(t.rgb2_buf, 0, t.readback_buf, 0, t.rgb_size);
            }
            else
            {
                enc.CopyBufferToBuffer(t.rgb_buf, 0, t.readback_buf, 0, t.rgb_size);
            }

            wgpu::CommandBuffer cmd = enc.Finish();
            t.device.GetQueue().Submit(1, &cmd);
        }

        // Read back HEAD result
        bool mapped = false;
        t.readback_buf.MapAsync(
            wgpu::MapMode::Read, 0, t.rgb_size,
            wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::MapAsyncStatus, wgpu::StringView) { mapped = true; });

        wgpu::Instance instance = t.device.GetAdapter().GetInstance();
        while (!mapped)
            instance.ProcessEvents();

        Done head;
        head.width = t.w;
        head.height = t.h;
        head.rgb.resize(t.w * t.h * 3);

        const float *data = static_cast<const float *>(t.readback_buf.GetConstMappedRange());
        std::memcpy(head.rgb.data(), data, t.rgb_size);
        t.readback_buf.Unmap();

        // Get camera key before deleting task
        std::string camera_key = t.is_tune ? t.camera_key : "";

        delete g_task;
        g_task = nullptr;

        // Compute spectral diff (apply LUT if camera_key provided and profile exists)
        return luteDiff(head, view_copy.data(), view_copy.size(), camera_key);
    }

    // =========================================================================
    // Helper: create pipeline
    // =========================================================================

    static wgpu::ComputePipeline createPipeline(wgpu::Device &device, const char *wgsl)
    {
        wgpu::ShaderSourceWGSL src;
        src.code = wgsl;
        wgpu::ShaderModuleDescriptor smDesc;
        smDesc.nextInChain = &src;
        wgpu::ShaderModule module = device.CreateShaderModule(&smDesc);

        wgpu::ComputePipelineDescriptor desc;
        desc.compute.module = module;
        return device.CreateComputePipeline(&desc);
    }

    // =========================================================================
    // headOpen
    // =========================================================================

    Task headOpen(Tree &info, uint16_t *data, void *device_ptr)
    {
        Task task;
        if (!data || !device_ptr)
            return task;

        wgpu::Device device = *static_cast<wgpu::Device *>(device_ptr);
        Stem &root = info.root();

        int w = static_cast<int>(root.leaf(WIDTH).dial());
        int h = static_cast<int>(root.leaf(HEIGHT).dial());
        if (w <= 0 || h <= 0)
            return task;

        // Build head uniforms
        HeadUniforms hu{};
        hu.width = w;
        hu.height = h;
        hu.black_level = root.leaf(BLACK).dial();
        hu.white_level = root.leaf(WHITE).dial();
        if (hu.white_level <= hu.black_level)
        {
            hu.black_level = 512;
            hu.white_level = 16383;
        }

        hu.wb_r = 1.0f;
        hu.wb_b = 1.0f;
        hu.m00 = 1; hu.m01 = 0; hu.m02 = 0;
        hu.m10 = 0; hu.m11 = 1; hu.m12 = 0;
        hu.m20 = 0; hu.m21 = 0; hu.m22 = 1;

        // Build warp uniforms
        WarpUniforms wu{};
        wu.width = w;
        wu.height = h;
        wu.knot_count = 0;

        // DRO setting for local tone mapping (extracted here, applied after GPU)
        std::string dro_setting;

        if (root.test("maker"))
        {
            Stem &maker = root.next("maker");

            if (maker.test("white_balance"))
            {
                Stem &wb = maker.next("white_balance");
                if (wb.test("r"))
                    hu.wb_r = wb.leaf("r").dial();
                if (wb.test("b"))
                    hu.wb_b = wb.leaf("b").dial();
            }

            if (maker.test("bayer_pattern"))
            {
                int pat = static_cast<int>(maker.leaf("bayer_pattern").dial());
                if (pat >= 46 && pat <= 49)
                    hu.pattern = static_cast<uint32_t>(pat - 46);
            }

            if (maker.test("dro"))
                dro_setting = maker.leaf("dro").text();

            if (maker.test("color_matrix"))
            {
                std::string matStr = maker.leaf("color_matrix").text();
                float m[9];
                size_t pos = 0, idx = 0;
                while (pos < matStr.size() && idx < 9)
                {
                    size_t end = matStr.find(',', pos);
                    if (end == std::string::npos)
                        end = matStr.size();
                    std::string num = matStr.substr(pos, end - pos);
                    size_t start = num.find_first_not_of(" \t");
                    if (start != std::string::npos)
                        m[idx++] = std::stof(num.substr(start));
                    pos = end + 1;
                }
                if (idx == 9)
                {
                    hu.m00 = m[0]; hu.m01 = m[1]; hu.m02 = m[2];
                    hu.m10 = m[3]; hu.m11 = m[4]; hu.m12 = m[5];
                    hu.m20 = m[6]; hu.m21 = m[7]; hu.m22 = m[8];
                }
            }

            if (maker.test("distortion"))
            {
                std::string distStr = maker.leaf("distortion").text();
                size_t pos = 0;
                while (pos < distStr.size() && wu.knot_count < 16)
                {
                    size_t end = distStr.find(',', pos);
                    if (end == std::string::npos)
                        end = distStr.size();
                    std::string num = distStr.substr(pos, end - pos);
                    size_t start = num.find_first_not_of(" \t");
                    if (start != std::string::npos)
                        wu.knots[wu.knot_count++] = std::stof(num.substr(start));
                    pos = end + 1;
                }
            }
        }

        // Create TaskData
        delete g_task;
        g_task = new TaskData();
        g_task->device = device;
        g_task->w = w;
        g_task->h = h;
        g_task->has_warp = (wu.knot_count > 0);
        g_task->dro_setting = dro_setting;

        // Create pipelines
        g_task->head_pipe = createPipeline(device, HEAD_WGSL);
        if (g_task->has_warp)
            g_task->warp_pipe = createPipeline(device, WARP_WGSL);

        size_t bayer_size = static_cast<size_t>(w) * h * sizeof(uint16_t);
        size_t rgb_size = static_cast<size_t>(w) * h * 3 * sizeof(float);
        g_task->rgb_size = rgb_size;

        wgpu::BufferDescriptor desc{};

        desc.size = bayer_size;
        desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        g_task->bayer_buf = device.CreateBuffer(&desc);

        desc.size = rgb_size;
        desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
        g_task->rgb_buf = device.CreateBuffer(&desc);

        if (g_task->has_warp)
        {
            desc.size = rgb_size;
            desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
            g_task->rgb2_buf = device.CreateBuffer(&desc);
        }

        desc.size = sizeof(HeadUniforms);
        desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
        g_task->head_uniform = device.CreateBuffer(&desc);

        if (g_task->has_warp)
        {
            desc.size = sizeof(WarpUniforms);
            desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
            g_task->warp_uniform = device.CreateBuffer(&desc);
        }

        desc.size = rgb_size;
        desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
        g_task->readback_buf = device.CreateBuffer(&desc);

        wgpu::Queue queue = device.GetQueue();
        queue.WriteBuffer(g_task->bayer_buf, 0, data, bayer_size);
        queue.WriteBuffer(g_task->head_uniform, 0, &hu, sizeof(hu));
        if (g_task->has_warp)
            queue.WriteBuffer(g_task->warp_uniform, 0, &wu, sizeof(wu));

        // Head bind group
        {
            wgpu::BindGroupLayout layout = g_task->head_pipe.GetBindGroupLayout(0);
            wgpu::BindGroupEntry entries[3]{};
            entries[0].binding = 0;
            entries[0].buffer = g_task->bayer_buf;
            entries[0].size = bayer_size;
            entries[1].binding = 1;
            entries[1].buffer = g_task->rgb_buf;
            entries[1].size = rgb_size;
            entries[2].binding = 2;
            entries[2].buffer = g_task->head_uniform;
            entries[2].size = sizeof(HeadUniforms);

            wgpu::BindGroupDescriptor bgDesc{};
            bgDesc.layout = layout;
            bgDesc.entryCount = 3;
            bgDesc.entries = entries;
            g_task->head_bg = device.CreateBindGroup(&bgDesc);
        }

        // Warp bind group
        if (g_task->has_warp)
        {
            wgpu::BindGroupLayout layout = g_task->warp_pipe.GetBindGroupLayout(0);
            wgpu::BindGroupEntry entries[3]{};
            entries[0].binding = 0;
            entries[0].buffer = g_task->rgb_buf;
            entries[0].size = rgb_size;
            entries[1].binding = 1;
            entries[1].buffer = g_task->rgb2_buf;
            entries[1].size = rgb_size;
            entries[2].binding = 2;
            entries[2].buffer = g_task->warp_uniform;
            entries[2].size = sizeof(WarpUniforms);

            wgpu::BindGroupDescriptor bgDesc{};
            bgDesc.layout = layout;
            bgDesc.entryCount = 3;
            bgDesc.entries = entries;
            g_task->warp_bg = device.CreateBindGroup(&bgDesc);
        }

        return task;
    }

    // =========================================================================
    // headTune - same as headOpen but with tune mode enabled
    // =========================================================================

    Task headTune(Tree &info, uint16_t *data, uint8_t *view, size_t viewSize, void *device_ptr)
    {
        // First, set up the task like headOpen
        Task task = headOpen(info, data, device_ptr);

        if (!g_task)
            return task;

        // Enable tune mode
        g_task->is_tune = true;

        // Copy view data (embedded JPEG)
        if (view && viewSize > 0)
        {
            g_task->view_data.assign(view, view + viewSize);
        }

        // Build camera key from metadata
        Stem &root = info.root();
        std::string make, model, style;

        if (root.test("maker"))
        {
            Stem &maker = root.next("maker");
            if (maker.test("make"))
                make = maker.leaf("make").text();
            if (maker.test("model"))
                model = maker.leaf("model").text();
            if (maker.test("creative_style"))
                style = maker.leaf("creative_style").text();
        }

        // Fallback defaults
        if (make.empty()) make = "Unknown";
        if (model.empty()) model = "Camera";
        if (style.empty()) style = "Standard";

        g_task->camera_key = make + "_" + model + "_" + style;

        return task;
    }

} // namespace flow


namespace flow
{
    // Stubs for forward-declared functions to resolve linker errors.
    // These appear to be part of an alternative learning path in head.cpp.
    void luteLearn(const Done &head, const uint8_t *jpeg, size_t jpegSize,
                   const std::string &cameraKey)
    {
        // TODO: Implement LUTE learning from HEAD task
    }

    Done luteDiff(const Done &head, const uint8_t *jpeg, size_t jpegSize,
                  const std::string &cameraKey)
    {
        // TODO: Implement LUTE diff from HEAD task
        return head; // Just return the input for now
    }
}

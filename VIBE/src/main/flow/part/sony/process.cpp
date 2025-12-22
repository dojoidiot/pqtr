// process.cpp
// Sony RAW processing pipeline using Dawn/WebGPU
// GPU-accelerated: BLC -> WB -> Demosaic -> ColorMatrix -> Crop

#include "../sony.h"

#include <dawn/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>
#include <dawn/dawn_proc.h>

#include <iostream>
#include <cstring>
#include <algorithm>

namespace sony
{

// ============================================================
// Embedded WGSL shaders
// ============================================================

static const char* SHADER_BLC_BAYER = R"(
struct Uniforms {
    width: u32,
    height: u32,
    black_level: f32,
    white_level: f32,
}

@group(0) @binding(0) var<storage, read> input: array<u32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;
    if (x >= uniforms.width || y >= uniforms.height) { return; }

    let idx = y * uniforms.width + x;
    let packed_idx = idx / 2u;
    let packed = input[packed_idx];
    var raw_value: f32;
    if (idx % 2u == 0u) {
        raw_value = f32(packed & 0xFFFFu);
    } else {
        raw_value = f32(packed >> 16u);
    }

    let scale = 1.0 / (uniforms.white_level - uniforms.black_level);
    let normalized = (raw_value - uniforms.black_level) * scale;
    output[idx] = max(0.0, normalized);
}
)";

static const char* SHADER_WB_BAYER = R"(
struct Uniforms {
    width: u32,
    height: u32,
    wb_r: f32,
    wb_b: f32,
    pattern: u32,
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
    if (x >= uniforms.width || y >= uniforms.height) { return; }

    let idx = y * uniforms.width + x;
    let value = input[idx];
    let px = x % 2u;
    let py = y % 2u;
    let pos = py * 2u + px;

    var gain: f32 = 1.0;
    if (uniforms.pattern == 0u) {
        if (pos == 0u) { gain = uniforms.wb_r; }
        else if (pos == 3u) { gain = uniforms.wb_b; }
    } else if (uniforms.pattern == 1u) {
        if (pos == 1u) { gain = uniforms.wb_r; }
        else if (pos == 2u) { gain = uniforms.wb_b; }
    } else if (uniforms.pattern == 2u) {
        if (pos == 0u) { gain = uniforms.wb_b; }
        else if (pos == 3u) { gain = uniforms.wb_r; }
    } else {
        if (pos == 1u) { gain = uniforms.wb_b; }
        else if (pos == 2u) { gain = uniforms.wb_r; }
    }
    output[idx] = value * gain;
}
)";

static const char* SHADER_DEMOSAIC = R"(
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
    if (x >= w || y >= h) { return; }

    let out_idx = u32(y * w + x) * 3u;

    // Helper inline pixel access with clamping
    var c: f32; { let cx = clamp(x, 0, w-1); let cy = clamp(y, 0, h-1); c = input[u32(cy)*u32(w)+u32(cx)]; }
    var n: f32; { let cx = clamp(x, 0, w-1); let cy = clamp(y-1, 0, h-1); n = input[u32(cy)*u32(w)+u32(cx)]; }
    var s: f32; { let cx = clamp(x, 0, w-1); let cy = clamp(y+1, 0, h-1); s = input[u32(cy)*u32(w)+u32(cx)]; }
    var e: f32; { let cx = clamp(x+1, 0, w-1); let cy = clamp(y, 0, h-1); e = input[u32(cy)*u32(w)+u32(cx)]; }
    var ww: f32; { let cx = clamp(x-1, 0, w-1); let cy = clamp(y, 0, h-1); ww = input[u32(cy)*u32(w)+u32(cx)]; }
    var ne: f32; { let cx = clamp(x+1, 0, w-1); let cy = clamp(y-1, 0, h-1); ne = input[u32(cy)*u32(w)+u32(cx)]; }
    var nw: f32; { let cx = clamp(x-1, 0, w-1); let cy = clamp(y-1, 0, h-1); nw = input[u32(cy)*u32(w)+u32(cx)]; }
    var se: f32; { let cx = clamp(x+1, 0, w-1); let cy = clamp(y+1, 0, h-1); se = input[u32(cy)*u32(w)+u32(cx)]; }
    var sw: f32; { let cx = clamp(x-1, 0, w-1); let cy = clamp(y+1, 0, h-1); sw = input[u32(cy)*u32(w)+u32(cx)]; }

    let px = u32(x) % 2u;
    let py = u32(y) % 2u;
    let pos = py * 2u + px;

    var r: f32 = 0.0;
    var g: f32 = 0.0;
    var b: f32 = 0.0;

    if (uniforms.pattern == 0u) {
        if (pos == 0u) { r = c; g = (n+s+e+ww)*0.25; b = (ne+nw+se+sw)*0.25; }
        else if (pos == 1u) { r = (e+ww)*0.5; g = c; b = (n+s)*0.5; }
        else if (pos == 2u) { r = (n+s)*0.5; g = c; b = (e+ww)*0.5; }
        else { r = (ne+nw+se+sw)*0.25; g = (n+s+e+ww)*0.25; b = c; }
    } else if (uniforms.pattern == 1u) {
        if (pos == 0u) { r = (e+ww)*0.5; g = c; b = (n+s)*0.5; }
        else if (pos == 1u) { r = c; g = (n+s+e+ww)*0.25; b = (ne+nw+se+sw)*0.25; }
        else if (pos == 2u) { r = (ne+nw+se+sw)*0.25; g = (n+s+e+ww)*0.25; b = c; }
        else { r = (n+s)*0.5; g = c; b = (e+ww)*0.5; }
    } else if (uniforms.pattern == 2u) {
        if (pos == 0u) { r = (ne+nw+se+sw)*0.25; g = (n+s+e+ww)*0.25; b = c; }
        else if (pos == 1u) { r = (n+s)*0.5; g = c; b = (e+ww)*0.5; }
        else if (pos == 2u) { r = (e+ww)*0.5; g = c; b = (n+s)*0.5; }
        else { r = c; g = (n+s+e+ww)*0.25; b = (ne+nw+se+sw)*0.25; }
    } else {
        if (pos == 0u) { r = (n+s)*0.5; g = c; b = (e+ww)*0.5; }
        else if (pos == 1u) { r = (ne+nw+se+sw)*0.25; g = (n+s+e+ww)*0.25; b = c; }
        else if (pos == 2u) { r = c; g = (n+s+e+ww)*0.25; b = (ne+nw+se+sw)*0.25; }
        else { r = (e+ww)*0.5; g = c; b = (n+s)*0.5; }
    }

    output[out_idx + 0u] = r;
    output[out_idx + 1u] = g;
    output[out_idx + 2u] = b;
}
)";

static const char* SHADER_COLOR_MATRIX = R"(
struct Uniforms {
    width: u32,
    height: u32,
    _pad0: f32,
    _pad1: f32,
    m00: f32, m01: f32, m02: f32, _p0: f32,
    m10: f32, m11: f32, m12: f32, _p1: f32,
    m20: f32, m21: f32, m22: f32, _p2: f32,
}

@group(0) @binding(0) var<storage, read> input: array<f32>;
@group(0) @binding(1) var<storage, read_write> output: array<f32>;
@group(0) @binding(2) var<uniform> uniforms: Uniforms;

@compute @workgroup_size(8, 8, 1)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;
    if (x >= uniforms.width || y >= uniforms.height) { return; }

    let idx = (y * uniforms.width + x) * 3u;
    let r = input[idx + 0u];
    let g = input[idx + 1u];
    let b = input[idx + 2u];

    let out_r = uniforms.m00*r + uniforms.m01*g + uniforms.m02*b;
    let out_g = uniforms.m10*r + uniforms.m11*g + uniforms.m12*b;
    let out_b = uniforms.m20*r + uniforms.m21*g + uniforms.m22*b;

    output[idx + 0u] = out_r;
    output[idx + 1u] = out_g;
    output[idx + 2u] = out_b;
}
)";

// ============================================================
// Dawn helpers
// ============================================================

struct GPU {
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;

    bool init() {
        DawnProcTable procs = dawn::native::GetProcs();
        dawnProcSetProcs(&procs);

        wgpu::InstanceDescriptor desc{};
        instance = wgpu::CreateInstance(&desc);
        if (!instance) return false;

        // Request adapter
        bool adapterDone = false;
        wgpu::RequestAdapterOptions adapterOpts{};
        adapterOpts.powerPreference = wgpu::PowerPreference::HighPerformance;

        instance.RequestAdapter(&adapterOpts, wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::RequestAdapterStatus status, wgpu::Adapter a, wgpu::StringView) {
                if (status == wgpu::RequestAdapterStatus::Success) adapter = a;
                adapterDone = true;
            });
        while (!adapterDone) instance.ProcessEvents();
        if (!adapter) return false;

        // Request device with higher limits for large images
        bool deviceDone = false;
        wgpu::DeviceDescriptor deviceDesc{};

        // Get adapter's supported limits first
        wgpu::Limits adapterLimits{};
        if (!adapter.GetLimits(&adapterLimits)) {
            std::cerr << "Failed to get adapter limits\n";
            return false;
        }

        std::cerr << "[GPU] Adapter max buffer: " << adapterLimits.maxBufferSize / (1024*1024) << " MB\n";

        // Request higher buffer limits based on what adapter supports
        // Default is 256MB, we need ~1GB for 60MP images (9600x6376x3xfloat = ~700MB)
        wgpu::Limits requiredLimits = adapterLimits;  // Start with adapter limits

        // Request up to 2GB for buffers (within adapter support)
        uint64_t desiredMaxBuffer = 2ULL * 1024 * 1024 * 1024;  // 2GB
        requiredLimits.maxBufferSize = std::min(desiredMaxBuffer, adapterLimits.maxBufferSize);
        requiredLimits.maxStorageBufferBindingSize = std::min(desiredMaxBuffer, adapterLimits.maxStorageBufferBindingSize);

        std::cerr << "[GPU] Requesting buffer: " << requiredLimits.maxBufferSize / (1024*1024) << " MB\n";

        deviceDesc.requiredLimits = &requiredLimits;

        deviceDesc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device&, wgpu::DeviceLostReason, wgpu::StringView msg) {
                std::cerr << "Device lost: " << std::string_view(msg.data, msg.length) << "\n";
            });
        deviceDesc.SetUncapturedErrorCallback(
            [](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView msg) {
                std::cerr << "GPU error: " << std::string_view(msg.data, msg.length) << "\n";
            });

        adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView) {
                if (status == wgpu::RequestDeviceStatus::Success) device = d;
                deviceDone = true;
            });
        while (!deviceDone) instance.ProcessEvents();
        if (!device) return false;

        // Verify actual limits granted
        wgpu::Limits deviceLimits{};
        if (device.GetLimits(&deviceLimits)) {
            std::cerr << "[GPU] Device max buffer: " << deviceLimits.maxBufferSize / (1024*1024) << " MB\n";
        }

        queue = device.GetQueue();
        return true;
    }

    wgpu::ComputePipeline createPipeline(const char* wgsl) {
        wgpu::ShaderSourceWGSL src{};
        src.code = wgsl;
        wgpu::ShaderModuleDescriptor shaderDesc{};
        shaderDesc.nextInChain = &src;
        wgpu::ShaderModule shader = device.CreateShaderModule(&shaderDesc);

        wgpu::ComputePipelineDescriptor pipeDesc{};
        pipeDesc.compute.module = shader;
        pipeDesc.compute.entryPoint = "main";
        return device.CreateComputePipeline(&pipeDesc);
    }

    wgpu::Buffer createBuffer(size_t size, wgpu::BufferUsage usage) {
        wgpu::BufferDescriptor desc{};
        desc.size = size;
        desc.usage = usage;
        return device.CreateBuffer(&desc);
    }
};

// Global GPU instance (initialized on first use)
static GPU* g_gpu = nullptr;

static GPU& gpu() {
    if (!g_gpu) {
        g_gpu = new GPU();
        if (!g_gpu->init()) {
            std::cerr << "Failed to initialize GPU\n";
        }
    }
    return *g_gpu;
}

// ============================================================
// Pipeline implementation
// ============================================================

bool Decoder::process_linear(const BayerU16& bayer, const RawMetadata& meta, ImageF32& rgb)
{
    GPU& g = gpu();
    if (!g.device) {
        std::cerr << "No GPU device\n";
        return false;
    }

    int w = meta.width;
    int h = meta.height;
    size_t bayer_size = w * h * sizeof(uint16_t);
    size_t float_size = w * h * sizeof(float);
    size_t rgb_size = w * h * 3 * sizeof(float);

    // Create pipelines
    auto pipelineBLC = g.createPipeline(SHADER_BLC_BAYER);
    auto pipelineWB = g.createPipeline(SHADER_WB_BAYER);
    auto pipelineDemosaic = g.createPipeline(SHADER_DEMOSAIC);
    auto pipelineColorMatrix = g.createPipeline(SHADER_COLOR_MATRIX);

    // Create buffers
    auto bufBayerU16 = g.createBuffer(bayer_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    auto bufBayerF32 = g.createBuffer(float_size, wgpu::BufferUsage::Storage);
    auto bufBayerWB = g.createBuffer(float_size, wgpu::BufferUsage::Storage);
    auto bufRGB = g.createBuffer(rgb_size, wgpu::BufferUsage::Storage);
    auto bufRGBOut = g.createBuffer(rgb_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
    auto bufReadback = g.createBuffer(rgb_size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);

    // Upload bayer data
    g.queue.WriteBuffer(bufBayerU16, 0, bayer.ptr(), bayer_size);

    // Uniform buffers
    struct BLCUniforms { uint32_t w, h; float black, white; };
    BLCUniforms blcU = { (uint32_t)w, (uint32_t)h, (float)meta.black_level, (float)meta.white_level };
    auto bufBlcUniform = g.createBuffer(sizeof(BLCUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    g.queue.WriteBuffer(bufBlcUniform, 0, &blcU, sizeof(blcU));

    float g_ref = meta.wb_rggb[1] > 0 ? (float)meta.wb_rggb[1] : 1024.0f;
    struct WBUniforms { uint32_t w, h; float wb_r, wb_b; uint32_t pattern; float _p[3]; };
    WBUniforms wbU = { (uint32_t)w, (uint32_t)h, (float)meta.wb_rggb[0]/g_ref, (float)meta.wb_rggb[2]/g_ref,
                       (uint32_t)meta.bayer_pattern, {0,0,0} };
    auto bufWbUniform = g.createBuffer(sizeof(WBUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    g.queue.WriteBuffer(bufWbUniform, 0, &wbU, sizeof(wbU));

    struct DemosaicUniforms { uint32_t w, h, pattern; float _p; };
    DemosaicUniforms demU = { (uint32_t)w, (uint32_t)h, (uint32_t)meta.bayer_pattern, 0 };
    auto bufDemUniform = g.createBuffer(sizeof(DemosaicUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    g.queue.WriteBuffer(bufDemUniform, 0, &demU, sizeof(demU));

    struct ColorMatrixUniforms { uint32_t w, h; float _p[2]; float m[12]; };
    ColorMatrixUniforms cmU = { (uint32_t)meta.crop_width, (uint32_t)meta.crop_height, {0,0},
        { meta.color_matrix[0], meta.color_matrix[1], meta.color_matrix[2], 0,
          meta.color_matrix[3], meta.color_matrix[4], meta.color_matrix[5], 0,
          meta.color_matrix[6], meta.color_matrix[7], meta.color_matrix[8], 0 } };
    auto bufCmUniform = g.createBuffer(sizeof(ColorMatrixUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    g.queue.WriteBuffer(bufCmUniform, 0, &cmU, sizeof(cmU));

    // Dispatch helper
    uint32_t wgX = (w + 7) / 8;
    uint32_t wgY = (h + 7) / 8;

    auto encoder = g.device.CreateCommandEncoder();

    // BLC pass
    {
        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufBayerU16, 0, bayer_size},
            {nullptr, 1, bufBayerF32, 0, float_size},
            {nullptr, 2, bufBlcUniform, 0, sizeof(BLCUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = pipelineBLC.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = g.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(pipelineBLC);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();
    }

    // WB pass
    {
        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufBayerF32, 0, float_size},
            {nullptr, 1, bufBayerWB, 0, float_size},
            {nullptr, 2, bufWbUniform, 0, sizeof(WBUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = pipelineWB.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = g.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(pipelineWB);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();
    }

    // Demosaic pass
    {
        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufBayerWB, 0, float_size},
            {nullptr, 1, bufRGB, 0, rgb_size},
            {nullptr, 2, bufDemUniform, 0, sizeof(DemosaicUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = pipelineDemosaic.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = g.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(pipelineDemosaic);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();
    }

    // ColorMatrix pass (on cropped region - TODO: implement crop shader)
    // For now, apply to full image
    {
        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufRGB, 0, rgb_size},
            {nullptr, 1, bufRGBOut, 0, rgb_size},
            {nullptr, 2, bufCmUniform, 0, sizeof(ColorMatrixUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = pipelineColorMatrix.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = g.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(pipelineColorMatrix);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();
    }

    // Copy to readback
    encoder.CopyBufferToBuffer(bufRGBOut, 0, bufReadback, 0, rgb_size);

    auto commands = encoder.Finish();
    g.queue.Submit(1, &commands);

    // Map and read back
    bool mapDone = false;
    bufReadback.MapAsync(wgpu::MapMode::Read, 0, rgb_size, wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::MapAsyncStatus status, wgpu::StringView) {
            mapDone = true;
        });
    while (!mapDone) g.instance.ProcessEvents();

    const float* mapped = static_cast<const float*>(bufReadback.GetConstMappedRange(0, rgb_size));
    if (!mapped) {
        std::cerr << "Failed to map readback buffer\n";
        return false;
    }

    // Copy to output (full size for now, TODO: crop)
    rgb.resize(w, h, 3);
    std::memcpy(rgb.ptr(), mapped, rgb_size);
    bufReadback.Unmap();

    return true;
}

bool Decoder::process(const BayerU16& bayer, const RawMetadata& meta, ImageF32& rgb)
{
    // For now, same as process_linear
    // TODO: add gamma shader
    return process_linear(bayer, meta, rgb);
}

} // namespace sony

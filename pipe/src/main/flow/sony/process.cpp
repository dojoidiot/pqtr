// process.cpp
// Sony RAW processing pipeline using Dawn/WebGPU
//
// ============================================================
// CLEAN-ROOM RAW DECODER - PIPELINE ORDER
// ============================================================
//
// The correct order for RAW processing (matching LibRaw/rawpy):
//
//   1. DECODE     - Decompress RAW (ARW2: 11-bit + 7-bit delta + tone curve)
//   2. BLC        - Black Level Correction (subtract black, normalize to 0-1)
//   3. DEMOSAIC   - Bayer interpolation (single-channel → RGB)
//   4. WB         - White Balance (MULTIPLY RGB by camera WB gains)
//   5. COLORMATRIX - Camera RGB → output colorspace (sRGB/etc)
//   6. GAMMA      - Linear → display (sRGB gamma or other)
//
// CRITICAL NOTES:
//
// - WB is applied AFTER demosaic, not before!
//   Applying WB to bayer data causes color artifacts.
//
// - WB is a MULTIPLY operation, not divide!
//   Camera WB values like [2436, 1024, 1604, 1024] mean:
//   R × 2.38, G × 1.0, B × 1.57 to achieve neutral white.
//
// - Sony ARW2 tone curve (tag 0x7010) must be applied during decode.
//   The curve expands 11-bit compressed values to 14-bit linear.
//   Without it, shadows are crushed and colors are wrong.
//
// ============================================================

// Use CPU RCD demosaic for dt comparison (set to 0 to use GPU bilinear)
#define USE_CPU_RCD_DEMOSAIC 1

// Skip WB in HEAD (apply via pqtr::apply instead for tuning)
#define SKIP_WB 0

// Skip ColorMatrix in HEAD (apply via pqtr::apply instead for tuning)
#define SKIP_COLOR_MATRIX 0

// Use dt-compatible colorin path (Lab roundtrip)
// Set to 1 to match darktable's baseline output
#define USE_DT_COLORIN 1

#include "../sony.h"

// CPU demosaic implementations (flow namespace)
namespace flow {
    void demosaic_rcd(const float* bayer, float* rgb, int w, int h, int pattern);
    void demosaic_bilinear(const float* bayer, float* rgb, int w, int h, int pattern);
}

#include <dawn/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>
#include <dawn/dawn_proc.h>

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

// WB applied AFTER demosaic to RGB (not to bayer)
static const char* SHADER_WB_RGB = R"(
struct Uniforms {
    width: u32,
    height: u32,
    wb_r: f32,
    wb_b: f32,
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

    // Multiply by WB gains (boost R and B relative to G)
    output[idx + 0u] = r * uniforms.wb_r;
    output[idx + 1u] = g;  // G stays at 1.0
    output[idx + 2u] = b * uniforms.wb_b;
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

    // Standard bayer pattern interpretation
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

        wgpu::Limits adapterLimits{};
        if (!adapter.GetLimits(&adapterLimits)) return false;

        // Request up to 2GB for buffers (for 60MP+ images)
        wgpu::Limits requiredLimits = adapterLimits;
        uint64_t desiredMaxBuffer = 2ULL * 1024 * 1024 * 1024;
        requiredLimits.maxBufferSize = std::min(desiredMaxBuffer, adapterLimits.maxBufferSize);
        requiredLimits.maxStorageBufferBindingSize = std::min(desiredMaxBuffer, adapterLimits.maxStorageBufferBindingSize);
        deviceDesc.requiredLimits = &requiredLimits;

        adapter.RequestDevice(&deviceDesc, wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::RequestDeviceStatus status, wgpu::Device d, wgpu::StringView) {
                if (status == wgpu::RequestDeviceStatus::Success) device = d;
                deviceDone = true;
            });
        while (!deviceDone) instance.ProcessEvents();
        if (!device) return false;

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

// GPU singleton with cached pipelines
struct GPUContext {
    GPU gpu;
    wgpu::ComputePipeline pipelineBLC;
    wgpu::ComputePipeline pipelineDemosaic;
    wgpu::ComputePipeline pipelineWB;
    wgpu::ComputePipeline pipelineColorMatrix;
    bool initialized = false;

    bool init() {
        if (initialized) return gpu.device != nullptr;
        initialized = true;
        if (!gpu.init()) return false;
        pipelineBLC = gpu.createPipeline(SHADER_BLC_BAYER);
        pipelineDemosaic = gpu.createPipeline(SHADER_DEMOSAIC);
        pipelineWB = gpu.createPipeline(SHADER_WB_RGB);
        pipelineColorMatrix = gpu.createPipeline(SHADER_COLOR_MATRIX);
        return true;
    }
};

static GPUContext& ctx() {
    static GPUContext instance;
    if (!instance.initialized) instance.init();
    return instance;
}

// ============================================================
// Pipeline implementation
// ============================================================

bool Decoder::process_linear(const BayerU16& bayer, const RawMetadata& meta, ImageF32& rgb)
{
    GPUContext& c = ctx();
    if (!c.gpu.device) return false;

    int w = meta.width;
    int h = meta.height;
    size_t bayer_size = w * h * sizeof(uint16_t);
    size_t float_size = w * h * sizeof(float);
    size_t rgb_size = w * h * 3 * sizeof(float);

    // Create buffers
    auto bufBayerU16 = c.gpu.createBuffer(bayer_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst);
    auto bufBayerF32 = c.gpu.createBuffer(float_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
    auto bufRGB = c.gpu.createBuffer(rgb_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc);
    auto bufRGBWB = c.gpu.createBuffer(rgb_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
    auto bufRGBOut = c.gpu.createBuffer(rgb_size, wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
    auto bufReadback = c.gpu.createBuffer(rgb_size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);

    c.gpu.queue.WriteBuffer(bufBayerU16, 0, bayer.ptr(), bayer_size);

    // BLC uniforms
    struct BLCUniforms { uint32_t w, h; float black, white; };
    BLCUniforms blcU = { (uint32_t)w, (uint32_t)h, (float)meta.black_level, (float)meta.white_level };
    auto bufBlcUniform = c.gpu.createBuffer(sizeof(BLCUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    c.gpu.queue.WriteBuffer(bufBlcUniform, 0, &blcU, sizeof(blcU));

    // WB uniforms (applied after demosaic by multiplying)
    float g_ref = meta.wb_rggb[1] > 0 ? (float)meta.wb_rggb[1] : 1024.0f;
    float as_shot_r = (float)meta.wb_rggb[0] / g_ref;
    float as_shot_b = (float)meta.wb_rggb[2] / g_ref;

    // Use as_shot WB (same as dt's temperature module)
    // dt's colorin applies D65/as_shot correction internally via the CAM matrix
    float wb_r = as_shot_r;
    float wb_b = as_shot_b;

    struct WBUniforms { uint32_t w, h; float wb_r, wb_b; };
    WBUniforms wbU = { (uint32_t)w, (uint32_t)h, wb_r, wb_b };
    auto bufWbUniform = c.gpu.createBuffer(sizeof(WBUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    c.gpu.queue.WriteBuffer(bufWbUniform, 0, &wbU, sizeof(wbU));

    // Demosaic uniforms (only used for GPU path)
    struct DemosaicUniforms { uint32_t w, h, pattern; float _p; };
    DemosaicUniforms demU = { (uint32_t)w, (uint32_t)h, (uint32_t)meta.bayer_pattern, 0 };
    auto bufDemUniform = c.gpu.createBuffer(sizeof(DemosaicUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    c.gpu.queue.WriteBuffer(bufDemUniform, 0, &demU, sizeof(demU));

    // ColorMatrix uniforms - camera RGB to sRGB
    // For USE_DT_COLORIN, we apply the full dt path in CPU after demosaic
    // For now, just use identity matrix as placeholder (actual transform in CPU)
#if USE_DT_COLORIN
    float cm[9] = {1,0,0, 0,1,0, 0,0,1};  // Identity - real transform done in CPU
#else
    // Sony sRGB matrix (from SR2SubIFD tag 0x7800)
    float cm[9];
    for (int i = 0; i < 9; i++) cm[i] = meta.color_matrix[i];
#endif

    struct ColorMatrixUniforms { uint32_t w, h; float _p[2]; float m[12]; };
    ColorMatrixUniforms cmU = { (uint32_t)w, (uint32_t)h, {0,0},
        { cm[0], cm[1], cm[2], 0,
          cm[3], cm[4], cm[5], 0,
          cm[6], cm[7], cm[8], 0 } };
    auto bufCmUniform = c.gpu.createBuffer(sizeof(ColorMatrixUniforms), wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst);
    c.gpu.queue.WriteBuffer(bufCmUniform, 0, &cmU, sizeof(cmU));

    uint32_t wgX = (w + 7) / 8;
    uint32_t wgY = (h + 7) / 8;

#if USE_CPU_RCD_DEMOSAIC
    // ================================================================
    // CPU RCD Path: GPU BLC → readback → CPU RCD → upload → GPU WB/CM
    // ================================================================

    // Phase 1: GPU BLC
    {
        auto encoder = c.gpu.device.CreateCommandEncoder();

        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufBayerU16, 0, bayer_size},
            {nullptr, 1, bufBayerF32, 0, float_size},
            {nullptr, 2, bufBlcUniform, 0, sizeof(BLCUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = c.pipelineBLC.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = c.gpu.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(c.pipelineBLC);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();

        // Copy to readback buffer
        auto bufBayerReadback = c.gpu.createBuffer(float_size, wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst);
        encoder.CopyBufferToBuffer(bufBayerF32, 0, bufBayerReadback, 0, float_size);

        auto commands = encoder.Finish();
        c.gpu.queue.Submit(1, &commands);

        // Read back normalized bayer
        bool mapDone = false;
        bufBayerReadback.MapAsync(wgpu::MapMode::Read, 0, float_size, wgpu::CallbackMode::AllowSpontaneous,
            [&](wgpu::MapAsyncStatus, wgpu::StringView) { mapDone = true; });
        while (!mapDone) c.gpu.instance.ProcessEvents();

        const float* bayerData = static_cast<const float*>(bufBayerReadback.GetConstMappedRange(0, float_size));
        if (!bayerData) return false;

        // Phase 2: CPU RCD demosaic
        std::vector<float> rgbCpu(w * h * 3);
        flow::demosaic_rcd(bayerData, rgbCpu.data(), w, h, meta.bayer_pattern);
        bufBayerReadback.Unmap();

#if USE_DT_COLORIN
        // ================================================================
        // Simplified path: WB + Sony sRGB matrix + gamma
        // ================================================================

        // Sony sRGB matrix from camera metadata
        const float* sony = meta.color_matrix;

        // sRGB gamma (from dt gamma.c)
        auto srgb_gamma = [](float x) -> float {
            return x <= 0.0031308f
                ? 12.92f * x
                : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
        };

        // D65 WB from dcraw matrix row sums
        const float dcraw_cam_xyz[9] = {
            0.4913f, -0.0541f, -0.0202f,
           -0.6130f,  1.3513f,  0.2906f,
           -0.1564f,  0.2151f,  0.7183f
        };
        float d65_r = 1.0f / (dcraw_cam_xyz[0] + dcraw_cam_xyz[1] + dcraw_cam_xyz[2]);
        float d65_g = 1.0f / (dcraw_cam_xyz[3] + dcraw_cam_xyz[4] + dcraw_cam_xyz[5]);
        float d65_b = 1.0f / (dcraw_cam_xyz[6] + dcraw_cam_xyz[7] + dcraw_cam_xyz[8]);
        d65_r /= d65_g; d65_b /= d65_g; d65_g = 1.0f;

        for (int i = 0; i < w * h; i++) {
            float r = rgbCpu[i*3+0];
            float g = rgbCpu[i*3+1];
            float b = rgbCpu[i*3+2];

            // Step 1: Apply D65 WB (like dt does)
            r *= d65_r;
            b *= d65_b;

            // Step 2: Camera RGB -> XYZ via dcraw matrix
            float X = dcraw_cam_xyz[0]*r + dcraw_cam_xyz[1]*g + dcraw_cam_xyz[2]*b;
            float Y = dcraw_cam_xyz[3]*r + dcraw_cam_xyz[4]*g + dcraw_cam_xyz[5]*b;
            float Z = dcraw_cam_xyz[6]*r + dcraw_cam_xyz[7]*g + dcraw_cam_xyz[8]*b;

            // Step 3: XYZ -> sRGB (D65)
            const float xyz_to_srgb[9] = {
                 3.2404542f, -1.5371385f, -0.4985314f,
                -0.9692660f,  1.8760108f,  0.0415560f,
                 0.0556434f, -0.2040259f,  1.0572252f
            };
            float r2 = xyz_to_srgb[0]*X + xyz_to_srgb[1]*Y + xyz_to_srgb[2]*Z;
            float g2 = xyz_to_srgb[3]*X + xyz_to_srgb[4]*Y + xyz_to_srgb[5]*Z;
            float b2 = xyz_to_srgb[6]*X + xyz_to_srgb[7]*Y + xyz_to_srgb[8]*Z;

            // Step 3: sRGB gamma
            r2 = srgb_gamma(std::max(0.0f, r2));
            g2 = srgb_gamma(std::max(0.0f, g2));
            b2 = srgb_gamma(std::max(0.0f, b2));

            rgbCpu[i*3+0] = r2;
            rgbCpu[i*3+1] = g2;
            rgbCpu[i*3+2] = b2;
        }
#endif

        // Upload demosaiced RGB to GPU
        c.gpu.queue.WriteBuffer(bufRGB, 0, rgbCpu.data(), rgb_size);
    }

    // Phase 3: GPU WB and ColorMatrix (or just copy if USE_DT_COLORIN)
    {
        auto encoder = c.gpu.device.CreateCommandEncoder();

#if USE_DT_COLORIN
        // WB and ColorMatrix already applied in CPU - just copy to readback
        encoder.CopyBufferToBuffer(bufRGB, 0, bufReadback, 0, rgb_size);
#elif !SKIP_WB
        // WB pass
        {
            wgpu::BindGroupEntry entries[3] = {
                {nullptr, 0, bufRGB, 0, rgb_size},
                {nullptr, 1, bufRGBWB, 0, rgb_size},
                {nullptr, 2, bufWbUniform, 0, sizeof(WBUniforms)}
            };
            wgpu::BindGroupDescriptor bgDesc{};
            bgDesc.layout = c.pipelineWB.GetBindGroupLayout(0);
            bgDesc.entryCount = 3;
            bgDesc.entries = entries;
            auto bg = c.gpu.device.CreateBindGroup(&bgDesc);

            auto pass = encoder.BeginComputePass();
            pass.SetPipeline(c.pipelineWB);
            pass.SetBindGroup(0, bg);
            pass.DispatchWorkgroups(wgX, wgY, 1);
            pass.End();
        }

#if !SKIP_COLOR_MATRIX
        // ColorMatrix pass
        {
            wgpu::BindGroupEntry entries[3] = {
                {nullptr, 0, bufRGBWB, 0, rgb_size},
                {nullptr, 1, bufRGBOut, 0, rgb_size},
                {nullptr, 2, bufCmUniform, 0, sizeof(ColorMatrixUniforms)}
            };
            wgpu::BindGroupDescriptor bgDesc{};
            bgDesc.layout = c.pipelineColorMatrix.GetBindGroupLayout(0);
            bgDesc.entryCount = 3;
            bgDesc.entries = entries;
            auto bg = c.gpu.device.CreateBindGroup(&bgDesc);

            auto pass = encoder.BeginComputePass();
            pass.SetPipeline(c.pipelineColorMatrix);
            pass.SetBindGroup(0, bg);
            pass.DispatchWorkgroups(wgX, wgY, 1);
            pass.End();
        }
        encoder.CopyBufferToBuffer(bufRGBOut, 0, bufReadback, 0, rgb_size);
#else
        // Skip ColorMatrix - output WB result directly
        encoder.CopyBufferToBuffer(bufRGBWB, 0, bufReadback, 0, rgb_size);
#endif

#else
        // Skip WB - output demosaic result directly
        encoder.CopyBufferToBuffer(bufRGB, 0, bufReadback, 0, rgb_size);
#endif
        auto commands = encoder.Finish();
        c.gpu.queue.Submit(1, &commands);
    }

#else
    // ================================================================
    // GPU Bilinear Path: All on GPU
    // ================================================================

    auto encoder = c.gpu.device.CreateCommandEncoder();

    // Pass 1: BLC
    {
        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufBayerU16, 0, bayer_size},
            {nullptr, 1, bufBayerF32, 0, float_size},
            {nullptr, 2, bufBlcUniform, 0, sizeof(BLCUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = c.pipelineBLC.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = c.gpu.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(c.pipelineBLC);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();
    }

    // Pass 2: GPU Demosaic (bilinear)
    {
        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufBayerF32, 0, float_size},
            {nullptr, 1, bufRGB, 0, rgb_size},
            {nullptr, 2, bufDemUniform, 0, sizeof(DemosaicUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = c.pipelineDemosaic.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = c.gpu.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(c.pipelineDemosaic);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();
    }

    // Pass 3: WB
    {
        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufRGB, 0, rgb_size},
            {nullptr, 1, bufRGBWB, 0, rgb_size},
            {nullptr, 2, bufWbUniform, 0, sizeof(WBUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = c.pipelineWB.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = c.gpu.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(c.pipelineWB);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();
    }

    // Pass 4: ColorMatrix
    {
        wgpu::BindGroupEntry entries[3] = {
            {nullptr, 0, bufRGBWB, 0, rgb_size},
            {nullptr, 1, bufRGBOut, 0, rgb_size},
            {nullptr, 2, bufCmUniform, 0, sizeof(ColorMatrixUniforms)}
        };
        wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = c.pipelineColorMatrix.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        auto bg = c.gpu.device.CreateBindGroup(&bgDesc);

        auto pass = encoder.BeginComputePass();
        pass.SetPipeline(c.pipelineColorMatrix);
        pass.SetBindGroup(0, bg);
        pass.DispatchWorkgroups(wgX, wgY, 1);
        pass.End();
    }

    encoder.CopyBufferToBuffer(bufRGBOut, 0, bufReadback, 0, rgb_size);
    auto commands = encoder.Finish();
    c.gpu.queue.Submit(1, &commands);
#endif

    // Final readback
    bool mapDone = false;
    bufReadback.MapAsync(wgpu::MapMode::Read, 0, rgb_size, wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::MapAsyncStatus, wgpu::StringView) { mapDone = true; });
    while (!mapDone) c.gpu.instance.ProcessEvents();

    const float* mapped = static_cast<const float*>(bufReadback.GetConstMappedRange(0, rgb_size));
    if (!mapped) return false;

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

// wb.cpp - White Balance Link
// Applies per-channel gains to Bayer data
// Reads wb_r, wb_g, wb_b, bayer_pattern from info

#include <pipe.hpp>
#include <wgpu.hpp>

static const char* WB_WGSL = R"(
struct Params {
    gain_r: f32,
    gain_g: f32,
    gain_b: f32,
    _pad: f32,
    width: u32,
    height: u32,
    pattern: u32,
    _pad2: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<f32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

fn getChannel(x: u32, y: u32, pattern: u32) -> u32 {
    let px = x & 1u;
    let py = y & 1u;
    let pos = py * 2u + px;

    switch (pattern) {
        case 46u: {
            if (pos == 0u) { return 0u; }
            if (pos == 3u) { return 2u; }
            return 1u;
        }
        case 47u: {
            if (pos == 1u) { return 0u; }
            if (pos == 2u) { return 2u; }
            return 1u;
        }
        case 48u: {
            if (pos == 0u) { return 2u; }
            if (pos == 3u) { return 0u; }
            return 1u;
        }
        case 49u: {
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

    let idx = y * params.width + x;
    let value = input[idx];

    let channel = getChannel(x, y, params.pattern);
    var gain: f32;
    switch (channel) {
        case 0u: { gain = params.gain_r; }
        case 1u: { gain = params.gain_g; }
        case 2u: { gain = params.gain_b; }
        default: { gain = 1.0; }
    }

    output[idx] = value * gain;
}
)";

namespace pipe {

class WbLink : public Link {
public:
    Name name() const override { return "wb"; }
    Name type() const override { return "core"; }

    Data flow(Data in) override {
        auto* ctx = static_cast<wgpu::Context*>(in.page);
        if (!ctx || !ctx->device) {
            in.info.text("error", "wb: no gpu context");
            return in;
        }

        // Read WB gains from info (normalized so G=1.0)
        float gain_r = in.info.dial("wb_r");
        float gain_g = in.info.dial("wb_g");
        float gain_b = in.info.dial("wb_b");
        int pattern = static_cast<int>(in.info.dial("bayer_pattern"));

        // Default gains if not set
        if (gain_g == 0.0f) gain_g = 1.0f;
        if (gain_r == 0.0f) gain_r = 1.0f;
        if (gain_b == 0.0f) gain_b = 1.0f;
        if (pattern == 0) pattern = 46;  // Default RGGB

        struct Params {
            float gain_r;
            float gain_g;
            float gain_b;
            float _pad;
            uint32_t width;
            uint32_t height;
            uint32_t pattern;
            uint32_t _pad2;
        } params;

        params.gain_r = gain_r;
        params.gain_g = gain_g;
        params.gain_b = gain_b;
        params._pad = 0.0f;
        params.width = static_cast<uint32_t>(ctx->width);
        params.height = static_cast<uint32_t>(ctx->height);
        params.pattern = static_cast<uint32_t>(pattern);
        params._pad2 = 0;

        size_t pixel_count = ctx->width * ctx->height;

        // Create uniform buffer
        ::wgpu::BufferDescriptor paramDesc{};
        paramDesc.size = sizeof(Params);
        paramDesc.usage = ::wgpu::BufferUsage::Uniform | ::wgpu::BufferUsage::CopyDst;
        ::wgpu::Buffer paramBuffer = ctx->device.CreateBuffer(&paramDesc);
        ctx->device.GetQueue().WriteBuffer(paramBuffer, 0, &params, sizeof(Params));

        // Create output buffer
        ::wgpu::BufferDescriptor outDesc{};
        outDesc.size = pixel_count * sizeof(float);
        outDesc.usage = ::wgpu::BufferUsage::Storage | ::wgpu::BufferUsage::CopySrc;
        ::wgpu::Buffer outBuffer = ctx->device.CreateBuffer(&outDesc);

        // Create compute pipeline
        ::wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
        wgslDesc.code = WB_WGSL;

        ::wgpu::ShaderModuleDescriptor shaderDesc{};
        shaderDesc.nextInChain = &wgslDesc;
        ::wgpu::ShaderModule shader = ctx->device.CreateShaderModule(&shaderDesc);

        ::wgpu::ComputePipelineDescriptor pipeDesc{};
        pipeDesc.compute.module = shader;
        pipeDesc.compute.entryPoint = "main";
        ::wgpu::ComputePipeline pipeline = ctx->device.CreateComputePipeline(&pipeDesc);

        // Create bind group
        ::wgpu::BindGroupEntry entries[3] = {};
        entries[0].binding = 0;
        entries[0].buffer = paramBuffer;
        entries[0].size = sizeof(Params);
        entries[1].binding = 1;
        entries[1].buffer = ctx->buffer;
        entries[1].size = pixel_count * sizeof(float);
        entries[2].binding = 2;
        entries[2].buffer = outBuffer;
        entries[2].size = pixel_count * sizeof(float);

        ::wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = pipeline.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        ::wgpu::BindGroup bindGroup = ctx->device.CreateBindGroup(&bgDesc);

        // Dispatch
        ::wgpu::CommandEncoder encoder = ctx->device.CreateCommandEncoder();
        ::wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);

        uint32_t wg_x = (params.width + 15) / 16;
        uint32_t wg_y = (params.height + 15) / 16;
        pass.DispatchWorkgroups(wg_x, wg_y);
        pass.End();

        ::wgpu::CommandBuffer commands = encoder.Finish();
        ctx->device.GetQueue().Submit(1, &commands);

        // Swap buffer
        ctx->buffer.Destroy();
        ctx->buffer = outBuffer;

        Data out;
        out.page = ctx;
        out.info = std::move(in.info);
        out.info.text("wb", "done");

        return out;
    }
};

Hold<Link> wb() {
    return Hold<Link>(new WbLink());
}

} // namespace pipe

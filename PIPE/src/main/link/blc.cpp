// blc.cpp - Black Level Correction Link
// Normalizes Bayer data: output = (input - black) / (white - black)
// Reads black_level, white_level from info

#include <pipe.hpp>
#include <wgpu.hpp>
#include <cstring>

// Embedded WGSL shader source
static const char* BLC_WGSL = R"(
struct Params {
    black_level: f32,
    white_level: f32,
    width: u32,
    height: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<u32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let total = params.width * params.height;

    if (idx >= total) {
        return;
    }

    let word_idx = idx / 2u;
    let word = input[word_idx];
    var raw: f32;
    if ((idx & 1u) == 0u) {
        raw = f32(word & 0xFFFFu);
    } else {
        raw = f32(word >> 16u);
    }

    let scale = 1.0 / (params.white_level - params.black_level);
    var normalized = (raw - params.black_level) * scale;
    normalized = max(normalized, 0.0);

    output[idx] = normalized;
}
)";

namespace pipe {

class BlcLink : public Link {
public:
    Name name() const override { return "blc"; }
    Name type() const override { return "core"; }

    Data flow(Data in) override {
        auto* ctx = static_cast<wgpu::Context*>(in.page);
        if (!ctx || !ctx->device) {
            in.info.text("error", "blc: no gpu context");
            return in;
        }

        // Read params from info
        float black = in.info.dial("black_level");
        float white = in.info.dial("white_level");

        if (white <= black) {
            in.info.text("error", "blc: invalid black/white levels");
            return in;
        }

        // Create params struct
        struct Params {
            float black_level;
            float white_level;
            uint32_t width;
            uint32_t height;
        } params;

        params.black_level = black;
        params.white_level = white;
        params.width = static_cast<uint32_t>(ctx->width);
        params.height = static_cast<uint32_t>(ctx->height);

        size_t pixel_count = ctx->width * ctx->height;

        // Create uniform buffer for params
        ::wgpu::BufferDescriptor paramDesc{};
        paramDesc.size = sizeof(Params);
        paramDesc.usage = ::wgpu::BufferUsage::Uniform | ::wgpu::BufferUsage::CopyDst;
        ::wgpu::Buffer paramBuffer = ctx->device.CreateBuffer(&paramDesc);
        ctx->device.GetQueue().WriteBuffer(paramBuffer, 0, &params, sizeof(Params));

        // Create output buffer (float32 per pixel)
        ::wgpu::BufferDescriptor outDesc{};
        outDesc.size = pixel_count * sizeof(float);
        outDesc.usage = ::wgpu::BufferUsage::Storage | ::wgpu::BufferUsage::CopySrc;
        ::wgpu::Buffer outBuffer = ctx->device.CreateBuffer(&outDesc);

        // Create compute pipeline
        ::wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
        wgslDesc.code = BLC_WGSL;

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
        entries[1].buffer = ctx->buffer;  // Input Bayer (uint16)
        entries[1].size = pixel_count * sizeof(uint16_t);
        entries[2].binding = 2;
        entries[2].buffer = outBuffer;
        entries[2].size = pixel_count * sizeof(float);

        ::wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = pipeline.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        ::wgpu::BindGroup bindGroup = ctx->device.CreateBindGroup(&bgDesc);

        // Dispatch compute
        ::wgpu::CommandEncoder encoder = ctx->device.CreateCommandEncoder();
        ::wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, bindGroup);

        uint32_t workgroups = (pixel_count + 255) / 256;
        pass.DispatchWorkgroups(workgroups);
        pass.End();

        ::wgpu::CommandBuffer commands = encoder.Finish();
        ctx->device.GetQueue().Submit(1, &commands);

        // Update context: swap buffer to float output
        ctx->buffer.Destroy();
        ctx->buffer = outBuffer;

        // Output
        Data out;
        out.page = ctx;
        out.info = std::move(in.info);
        out.info.text("blc", "done");

        return out;
    }
};

// Factory
Hold<Link> blc() {
    return Hold<Link>(new BlcLink());
}

} // namespace pipe

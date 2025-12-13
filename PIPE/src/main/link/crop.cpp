// crop.cpp - Crop Link
// Extracts active area from image
// Reads crop_left, crop_top, crop_width, crop_height from info

#include <pipe.hpp>
#include <wgpu.hpp>

static const char* CROP_WGSL = R"(
struct Params {
    in_width: u32,
    in_height: u32,
    crop_left: u32,
    crop_top: u32,
    crop_width: u32,
    crop_height: u32,
    _pad0: u32,
    _pad1: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<f32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

@compute @workgroup_size(16, 16)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let x = gid.x;
    let y = gid.y;

    if (x >= params.crop_width || y >= params.crop_height) {
        return;
    }

    let src_x = x + params.crop_left;
    let src_y = y + params.crop_top;

    if (src_x >= params.in_width || src_y >= params.in_height) {
        return;
    }

    let src_idx = (src_y * params.in_width + src_x) * 3u;
    let dst_idx = (y * params.crop_width + x) * 3u;

    output[dst_idx + 0u] = input[src_idx + 0u];
    output[dst_idx + 1u] = input[src_idx + 1u];
    output[dst_idx + 2u] = input[src_idx + 2u];
}
)";

namespace pipe {

class CropLink : public Link {
public:
    Name name() const override { return "crop"; }
    Name type() const override { return "core"; }

    Data flow(Data in) override {
        auto* ctx = static_cast<wgpu::Context*>(in.page);
        if (!ctx || !ctx->device) {
            in.info.text("error", "crop: no gpu context");
            return in;
        }

        // Read crop params from info
        int crop_left = static_cast<int>(in.info.dial("crop_left"));
        int crop_top = static_cast<int>(in.info.dial("crop_top"));
        int crop_width = static_cast<int>(in.info.dial("crop_width"));
        int crop_height = static_cast<int>(in.info.dial("crop_height"));

        // Default to full image if no crop specified
        if (crop_width <= 0) crop_width = ctx->width;
        if (crop_height <= 0) crop_height = ctx->height;

        // Validate bounds
        if (crop_left < 0) crop_left = 0;
        if (crop_top < 0) crop_top = 0;
        if (crop_left + crop_width > ctx->width) crop_width = ctx->width - crop_left;
        if (crop_top + crop_height > ctx->height) crop_height = ctx->height - crop_top;

        // Skip if no actual crop needed
        if (crop_left == 0 && crop_top == 0 &&
            crop_width == ctx->width && crop_height == ctx->height) {
            in.info.text("crop", "skipped");
            return in;
        }

        struct Params {
            uint32_t in_width;
            uint32_t in_height;
            uint32_t crop_left;
            uint32_t crop_top;
            uint32_t crop_width;
            uint32_t crop_height;
            uint32_t _pad0;
            uint32_t _pad1;
        } params;

        params.in_width = static_cast<uint32_t>(ctx->width);
        params.in_height = static_cast<uint32_t>(ctx->height);
        params.crop_left = static_cast<uint32_t>(crop_left);
        params.crop_top = static_cast<uint32_t>(crop_top);
        params.crop_width = static_cast<uint32_t>(crop_width);
        params.crop_height = static_cast<uint32_t>(crop_height);
        params._pad0 = params._pad1 = 0;

        size_t in_pixels = ctx->width * ctx->height;
        size_t out_pixels = crop_width * crop_height;

        // Uniform buffer
        ::wgpu::BufferDescriptor paramDesc{};
        paramDesc.size = sizeof(Params);
        paramDesc.usage = ::wgpu::BufferUsage::Uniform | ::wgpu::BufferUsage::CopyDst;
        ::wgpu::Buffer paramBuffer = ctx->device.CreateBuffer(&paramDesc);
        ctx->device.GetQueue().WriteBuffer(paramBuffer, 0, &params, sizeof(Params));

        // Output buffer (smaller)
        ::wgpu::BufferDescriptor outDesc{};
        outDesc.size = out_pixels * 3 * sizeof(float);
        outDesc.usage = ::wgpu::BufferUsage::Storage | ::wgpu::BufferUsage::CopySrc;
        ::wgpu::Buffer outBuffer = ctx->device.CreateBuffer(&outDesc);

        // Pipeline
        ::wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
        wgslDesc.code = CROP_WGSL;

        ::wgpu::ShaderModuleDescriptor shaderDesc{};
        shaderDesc.nextInChain = &wgslDesc;
        ::wgpu::ShaderModule shader = ctx->device.CreateShaderModule(&shaderDesc);

        ::wgpu::ComputePipelineDescriptor pipeDesc{};
        pipeDesc.compute.module = shader;
        pipeDesc.compute.entryPoint = "main";
        ::wgpu::ComputePipeline pipeline = ctx->device.CreateComputePipeline(&pipeDesc);

        // Bind group
        ::wgpu::BindGroupEntry entries[3] = {};
        entries[0].binding = 0;
        entries[0].buffer = paramBuffer;
        entries[0].size = sizeof(Params);
        entries[1].binding = 1;
        entries[1].buffer = ctx->buffer;
        entries[1].size = in_pixels * 3 * sizeof(float);
        entries[2].binding = 2;
        entries[2].buffer = outBuffer;
        entries[2].size = out_pixels * 3 * sizeof(float);

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

        uint32_t wg_x = (params.crop_width + 15) / 16;
        uint32_t wg_y = (params.crop_height + 15) / 16;
        pass.DispatchWorkgroups(wg_x, wg_y);
        pass.End();

        ::wgpu::CommandBuffer commands = encoder.Finish();
        ctx->device.GetQueue().Submit(1, &commands);

        // Swap buffer, update dimensions
        ctx->buffer.Destroy();
        ctx->buffer = outBuffer;
        ctx->width = crop_width;
        ctx->height = crop_height;

        Data out;
        out.page = ctx;
        out.info = std::move(in.info);
        out.info.text("crop", "done");

        return out;
    }
};

Hold<Link> crop() {
    return Hold<Link>(new CropLink());
}

} // namespace pipe

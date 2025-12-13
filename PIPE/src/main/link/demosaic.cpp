// demosaic.cpp - Demosaic Link
// Bayer → RGB via bilinear interpolation
// Reads bayer_pattern from info

#include <pipe.hpp>
#include <wgpu.hpp>

static const char* DEMOSAIC_WGSL = R"(
struct Params {
    width: u32,
    height: u32,
    pattern: u32,
    _pad: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<f32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

fn readBayer(x: i32, y: i32) -> f32 {
    let cx = clamp(x, 0, i32(params.width) - 1);
    let cy = clamp(y, 0, i32(params.height) - 1);
    return input[u32(cy) * params.width + u32(cx)];
}

fn getChannel(x: u32, y: u32) -> u32 {
    let px = x & 1u;
    let py = y & 1u;
    let pos = py * 2u + px;

    switch (params.pattern) {
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

    let ix = i32(x);
    let iy = i32(y);
    let channel = getChannel(x, y);
    let center = readBayer(ix, iy);

    var r: f32;
    var g: f32;
    var b: f32;

    switch (channel) {
        case 0u: {
            r = center;
            g = (readBayer(ix-1, iy) + readBayer(ix+1, iy) +
                 readBayer(ix, iy-1) + readBayer(ix, iy+1)) * 0.25;
            b = (readBayer(ix-1, iy-1) + readBayer(ix+1, iy-1) +
                 readBayer(ix-1, iy+1) + readBayer(ix+1, iy+1)) * 0.25;
        }
        case 1u: {
            let isGreenRow = (y & 1u) == 0u;
            var r_h: bool;
            switch (params.pattern) {
                case 46u: { r_h = !isGreenRow; }
                case 47u: { r_h = isGreenRow; }
                case 48u: { r_h = isGreenRow; }
                case 49u: { r_h = !isGreenRow; }
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
        case 2u: {
            b = center;
            g = (readBayer(ix-1, iy) + readBayer(ix+1, iy) +
                 readBayer(ix, iy-1) + readBayer(ix, iy+1)) * 0.25;
            r = (readBayer(ix-1, iy-1) + readBayer(ix+1, iy-1) +
                 readBayer(ix-1, iy+1) + readBayer(ix+1, iy+1)) * 0.25;
        }
        default: {
            r = center; g = center; b = center;
        }
    }

    let out_idx = (y * params.width + x) * 3u;
    output[out_idx + 0u] = r;
    output[out_idx + 1u] = g;
    output[out_idx + 2u] = b;
}
)";

namespace pipe {

class DemosaicLink : public Link {
public:
    Name name() const override { return "demosaic"; }
    Name type() const override { return "core"; }

    Data flow(Data in) override {
        auto* ctx = static_cast<wgpu::Context*>(in.page);
        if (!ctx || !ctx->device) {
            in.info.text("error", "demosaic: no gpu context");
            return in;
        }

        int pattern = static_cast<int>(in.info.dial("bayer_pattern"));
        if (pattern == 0) pattern = 46;

        struct Params {
            uint32_t width;
            uint32_t height;
            uint32_t pattern;
            uint32_t _pad;
        } params;

        params.width = static_cast<uint32_t>(ctx->width);
        params.height = static_cast<uint32_t>(ctx->height);
        params.pattern = static_cast<uint32_t>(pattern);
        params._pad = 0;

        size_t pixel_count = ctx->width * ctx->height;

        // Uniform buffer
        ::wgpu::BufferDescriptor paramDesc{};
        paramDesc.size = sizeof(Params);
        paramDesc.usage = ::wgpu::BufferUsage::Uniform | ::wgpu::BufferUsage::CopyDst;
        ::wgpu::Buffer paramBuffer = ctx->device.CreateBuffer(&paramDesc);
        ctx->device.GetQueue().WriteBuffer(paramBuffer, 0, &params, sizeof(Params));

        // Output: RGB float32 (3 floats per pixel)
        ::wgpu::BufferDescriptor outDesc{};
        outDesc.size = pixel_count * 3 * sizeof(float);
        outDesc.usage = ::wgpu::BufferUsage::Storage | ::wgpu::BufferUsage::CopySrc;
        ::wgpu::Buffer outBuffer = ctx->device.CreateBuffer(&outDesc);

        // Pipeline
        ::wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
        wgslDesc.code = DEMOSAIC_WGSL;

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
        entries[1].size = pixel_count * sizeof(float);
        entries[2].binding = 2;
        entries[2].buffer = outBuffer;
        entries[2].size = pixel_count * 3 * sizeof(float);

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

        // Swap buffer, update channels
        ctx->buffer.Destroy();
        ctx->buffer = outBuffer;
        ctx->channels = 3;

        Data out;
        out.page = ctx;
        out.info = std::move(in.info);
        out.info.text("demosaic", "done");

        return out;
    }
};

Hold<Link> demosaic() {
    return Hold<Link>(new DemosaicLink());
}

} // namespace pipe

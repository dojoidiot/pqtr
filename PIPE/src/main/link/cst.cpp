// cst.cpp - Color Space Transform Link
// Applies 3x3 matrix: camera RGB → linear sRGB
// Reads color_matrix[9] from info

#include <pipe.hpp>
#include <wgpu.hpp>

static const char* CST_WGSL = R"(
struct Params {
    m00: f32, m01: f32, m02: f32, _pad0: f32,
    m10: f32, m11: f32, m12: f32, _pad1: f32,
    m20: f32, m21: f32, m22: f32, _pad2: f32,
    width: u32,
    height: u32,
    _pad3: u32,
    _pad4: u32,
}

@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> input: array<f32>;
@group(0) @binding(2) var<storage, read_write> output: array<f32>;

@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) gid: vec3<u32>) {
    let idx = gid.x;
    let total = params.width * params.height;

    if (idx >= total) {
        return;
    }

    let base = idx * 3u;
    let r_in = input[base + 0u];
    let g_in = input[base + 1u];
    let b_in = input[base + 2u];

    let r_out = params.m00 * r_in + params.m01 * g_in + params.m02 * b_in;
    let g_out = params.m10 * r_in + params.m11 * g_in + params.m12 * b_in;
    let b_out = params.m20 * r_in + params.m21 * g_in + params.m22 * b_in;

    output[base + 0u] = r_out;
    output[base + 1u] = g_out;
    output[base + 2u] = b_out;
}
)";

namespace pipe {

class CstLink : public Link {
public:
    Name name() const override { return "cst"; }
    Name type() const override { return "core"; }

    Data flow(Data in) override {
        auto* ctx = static_cast<wgpu::Context*>(in.page);
        if (!ctx || !ctx->device) {
            in.info.text("error", "cst: no gpu context");
            return in;
        }

        // Read color matrix from info (9 floats)
        const float* matrix = in.info.data("color_matrix");

        struct Params {
            float m00, m01, m02, _pad0;
            float m10, m11, m12, _pad1;
            float m20, m21, m22, _pad2;
            uint32_t width;
            uint32_t height;
            uint32_t _pad3;
            uint32_t _pad4;
        } params;

        // Default to identity if no matrix provided
        if (matrix && in.info.size("color_matrix") >= 9) {
            params.m00 = matrix[0]; params.m01 = matrix[1]; params.m02 = matrix[2];
            params.m10 = matrix[3]; params.m11 = matrix[4]; params.m12 = matrix[5];
            params.m20 = matrix[6]; params.m21 = matrix[7]; params.m22 = matrix[8];
        } else {
            params.m00 = 1.0f; params.m01 = 0.0f; params.m02 = 0.0f;
            params.m10 = 0.0f; params.m11 = 1.0f; params.m12 = 0.0f;
            params.m20 = 0.0f; params.m21 = 0.0f; params.m22 = 1.0f;
        }

        params._pad0 = params._pad1 = params._pad2 = 0.0f;
        params.width = static_cast<uint32_t>(ctx->width);
        params.height = static_cast<uint32_t>(ctx->height);
        params._pad3 = params._pad4 = 0;

        size_t pixel_count = ctx->width * ctx->height;

        // Uniform buffer
        ::wgpu::BufferDescriptor paramDesc{};
        paramDesc.size = sizeof(Params);
        paramDesc.usage = ::wgpu::BufferUsage::Uniform | ::wgpu::BufferUsage::CopyDst;
        ::wgpu::Buffer paramBuffer = ctx->device.CreateBuffer(&paramDesc);
        ctx->device.GetQueue().WriteBuffer(paramBuffer, 0, &params, sizeof(Params));

        // Output buffer (same size as input)
        ::wgpu::BufferDescriptor outDesc{};
        outDesc.size = pixel_count * 3 * sizeof(float);
        outDesc.usage = ::wgpu::BufferUsage::Storage | ::wgpu::BufferUsage::CopySrc;
        ::wgpu::Buffer outBuffer = ctx->device.CreateBuffer(&outDesc);

        // Pipeline
        ::wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
        wgslDesc.code = CST_WGSL;

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
        entries[1].size = pixel_count * 3 * sizeof(float);
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

        uint32_t workgroups = (pixel_count + 255) / 256;
        pass.DispatchWorkgroups(workgroups);
        pass.End();

        ::wgpu::CommandBuffer commands = encoder.Finish();
        ctx->device.GetQueue().Submit(1, &commands);

        // Swap buffer
        ctx->buffer.Destroy();
        ctx->buffer = outBuffer;

        Data out;
        out.page = ctx;
        out.info = std::move(in.info);
        out.info.text("cst", "done");

        return out;
    }
};

Hold<Link> cst() {
    return Hold<Link>(new CstLink());
}

} // namespace pipe

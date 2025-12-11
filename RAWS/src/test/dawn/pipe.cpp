// pipe.cpp - DAWN Compute Pipeline Implementation

#include "pipe.hpp"
#include <cstring>
#include <cstdio>

namespace dawn_pipe
{

Pipe::Pipe()
    : dims_{0, 0}
    , input_size_(0)
    , output_size_(0)
    , uniform_size_(0)
    , data_size_(0)
    , output_channels_(3)
    , ready_(false)
{
}

Pipe::~Pipe() = default;

bool Pipe::init()
{
    instance_ = dawn::instance();
    if (!instance_)
    {
        fprintf(stderr, "[dawn_pipe] Failed to create instance\n");
        return false;
    }

    adapter_ = dawn::adapter(instance_);
    if (!adapter_)
    {
        fprintf(stderr, "[dawn_pipe] Failed to get adapter\n");
        return false;
    }

    device_ = dawn::device(adapter_);
    if (!device_)
    {
        fprintf(stderr, "[dawn_pipe] Failed to get device\n");
        return false;
    }

    return true;
}

bool Pipe::load(const char* wgsl)
{
    pipeline_ = dawn::pipeline(device_, wgsl);
    if (!pipeline_)
    {
        fprintf(stderr, "[dawn_pipe] Failed to create pipeline\n");
        return false;
    }
    return true;
}

void Pipe::set_input(const float* data, uint32_t width, uint32_t height)
{
    dims_ = {width, height};
    input_size_ = width * height * 3 * sizeof(float);
    output_size_ = width * height * output_channels_ * sizeof(float);

    // Create input buffer (storage, copy_dst)
    wgpu::BufferDescriptor input_desc{};
    input_desc.size = input_size_;
    input_desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    input_buffer_ = device_.CreateBuffer(&input_desc);

    // Create output buffer (storage, copy_src)
    wgpu::BufferDescriptor output_desc{};
    output_desc.size = output_size_;
    output_desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    output_buffer_ = device_.CreateBuffer(&output_desc);

    // Create staging buffer for readback
    wgpu::BufferDescriptor staging_desc{};
    staging_desc.size = output_size_;
    staging_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    staging_buffer_ = device_.CreateBuffer(&staging_desc);

    // Upload input data
    device_.GetQueue().WriteBuffer(input_buffer_, 0, data, input_size_);
}

void Pipe::set_input_bayer(const float* data, uint32_t width, uint32_t height)
{
    dims_ = {width, height};
    input_size_ = width * height * sizeof(float);  // 1 channel
    output_size_ = width * height * output_channels_ * sizeof(float);

    // Create input buffer
    wgpu::BufferDescriptor input_desc{};
    input_desc.size = input_size_;
    input_desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    input_buffer_ = device_.CreateBuffer(&input_desc);

    // Create output buffer
    wgpu::BufferDescriptor output_desc{};
    output_desc.size = output_size_;
    output_desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    output_buffer_ = device_.CreateBuffer(&output_desc);

    // Create staging buffer
    wgpu::BufferDescriptor staging_desc{};
    staging_desc.size = output_size_;
    staging_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    staging_buffer_ = device_.CreateBuffer(&staging_desc);

    // Upload input data
    device_.GetQueue().WriteBuffer(input_buffer_, 0, data, input_size_);
}

void Pipe::set_input_raw(const uint32_t* data, uint32_t width, uint32_t height)
{
    dims_ = {width, height};
    // u16 packed as u32 pairs: (width * height) u16 values = (width * height + 1) / 2 u32 values
    input_size_ = ((width * height + 1) / 2) * sizeof(uint32_t);
    output_size_ = width * height * output_channels_ * sizeof(float);

    // Create input buffer
    wgpu::BufferDescriptor input_desc{};
    input_desc.size = input_size_;
    input_desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    input_buffer_ = device_.CreateBuffer(&input_desc);

    // Create output buffer
    wgpu::BufferDescriptor output_desc{};
    output_desc.size = output_size_;
    output_desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc;
    output_buffer_ = device_.CreateBuffer(&output_desc);

    // Create staging buffer
    wgpu::BufferDescriptor staging_desc{};
    staging_desc.size = output_size_;
    staging_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    staging_buffer_ = device_.CreateBuffer(&staging_desc);

    // Upload input data
    device_.GetQueue().WriteBuffer(input_buffer_, 0, data, input_size_);
}

void Pipe::set_output_channels(int channels)
{
    output_channels_ = channels;
}

void Pipe::set_uniforms(const void* data, size_t size)
{
    uniform_size_ = size;

    // Align to 16 bytes for WebGPU
    size_t aligned_size = (size + 15) & ~15;

    wgpu::BufferDescriptor uniform_desc{};
    uniform_desc.size = aligned_size;
    uniform_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    uniform_buffer_ = device_.CreateBuffer(&uniform_desc);

    device_.GetQueue().WriteBuffer(uniform_buffer_, 0, data, size);
}

void Pipe::set_data(const void* data, size_t size)
{
    data_size_ = size;

    wgpu::BufferDescriptor data_desc{};
    data_desc.size = size;
    data_desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    data_buffer_ = device_.CreateBuffer(&data_desc);

    device_.GetQueue().WriteBuffer(data_buffer_, 0, data, size);
}

bool Pipe::dispatch()
{
    if (!pipeline_ || !input_buffer_ || !output_buffer_)
    {
        fprintf(stderr, "[dawn_pipe] Pipeline or buffers not ready\n");
        return false;
    }

    // Create bind group
    wgpu::BindGroupLayout layout = pipeline_.GetBindGroupLayout(0);

    std::vector<wgpu::BindGroupEntry> entries;

    // @binding(0) input
    wgpu::BindGroupEntry input_entry{};
    input_entry.binding = 0;
    input_entry.buffer = input_buffer_;
    input_entry.size = input_size_;
    entries.push_back(input_entry);

    // @binding(1) output
    wgpu::BindGroupEntry output_entry{};
    output_entry.binding = 1;
    output_entry.buffer = output_buffer_;
    output_entry.size = output_size_;
    entries.push_back(output_entry);

    // @binding(2) uniforms (if set)
    if (uniform_buffer_)
    {
        wgpu::BindGroupEntry uniform_entry{};
        uniform_entry.binding = 2;
        uniform_entry.buffer = uniform_buffer_;
        uniform_entry.size = (uniform_size_ + 15) & ~15;
        entries.push_back(uniform_entry);
    }

    // @binding(3) data buffer (curves, LUTs, etc)
    if (data_buffer_)
    {
        wgpu::BindGroupEntry data_entry{};
        data_entry.binding = 3;
        data_entry.buffer = data_buffer_;
        data_entry.size = data_size_;
        entries.push_back(data_entry);
    }

    wgpu::BindGroupDescriptor bg_desc{};
    bg_desc.layout = layout;
    bg_desc.entryCount = entries.size();
    bg_desc.entries = entries.data();
    bind_group_ = device_.CreateBindGroup(&bg_desc);

    // Dispatch compute
    wgpu::CommandEncoderDescriptor enc_desc{};
    wgpu::CommandEncoder encoder = device_.CreateCommandEncoder(&enc_desc);

    wgpu::ComputePassDescriptor pass_desc{};
    wgpu::ComputePassEncoder pass = encoder.BeginComputePass(&pass_desc);
    pass.SetPipeline(pipeline_);
    pass.SetBindGroup(0, bind_group_);

    // Workgroup size 8x8, dispatch enough groups
    uint32_t groups_x = (dims_.width + 7) / 8;
    uint32_t groups_y = (dims_.height + 7) / 8;
    pass.DispatchWorkgroups(groups_x, groups_y, 1);
    pass.End();

    // Copy output to staging
    encoder.CopyBufferToBuffer(output_buffer_, 0, staging_buffer_, 0, output_size_);

    wgpu::CommandBuffer commands = encoder.Finish();
    device_.GetQueue().Submit(1, &commands);

    ready_ = true;
    return true;
}

std::vector<float> Pipe::get_output()
{
    std::vector<float> result;
    if (!ready_ || !staging_buffer_)
        return result;

    result.resize(dims_.width * dims_.height * output_channels_);

    bool done = false;
    staging_buffer_.MapAsync(
        wgpu::MapMode::Read, 0, output_size_,
        wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::MapAsyncStatus status, wgpu::StringView) {
            if (status == wgpu::MapAsyncStatus::Success)
            {
                const void* mapped = staging_buffer_.GetConstMappedRange(0, output_size_);
                memcpy(result.data(), mapped, output_size_);
                staging_buffer_.Unmap();
            }
            done = true;
        });

    while (!done)
    {
        instance_.ProcessEvents();
    }

    return result;
}

} // namespace dawn_pipe

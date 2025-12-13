// wgpu.hpp - PQTR WebGPU interface
//
// Link API:
//   wgpu::link() - CPU buffer → GPU buffer transfer
//   Input:  Page = BayerBuffer* (from GEAR)
//   Output: Page = WgpuContext* (GPU device + buffer)
//
// Direct API (dawn namespace):
//   instance(), adapter(), device(), pipeline()

#pragma once

#include <pipe.hpp>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

namespace wgpu {

    // ============================================================
    // GPU context passed through pipe
    // ============================================================

    struct Context {
        ::wgpu::Device device;
        ::wgpu::Buffer buffer;  // Current data buffer
        int width;
        int height;
        int channels;           // 1=Bayer, 3=RGB, 4=RGBA
    };

    // ============================================================
    // Link contributors for pipe
    // ============================================================

    // open(): CPU → GPU (after GEAR)
    //   Input:  Page = BayerBuffer* (CPU)
    //   Output: Page = Context* (GPU)
    pipe::Hold<pipe::Link> open();

    // shut(): GPU → CPU (end of pipe)
    //   Input:  Page = Context* (GPU)
    //   Output: Page = output buffer (CPU), frees Context
    pipe::Hold<pipe::Link> shut();

} // namespace wgpu

namespace dawn
{
    // Re-export wgpu types
    using Instance = wgpu::Instance;
    using Adapter = wgpu::Adapter;
    using Device = wgpu::Device;
    using ComputePipeline = wgpu::ComputePipeline;
    using Buffer = wgpu::Buffer;
    using BindGroup = wgpu::BindGroup;
    using CommandEncoder = wgpu::CommandEncoder;
    using ComputePassEncoder = wgpu::ComputePassEncoder;

    // Initialize WebGPU instance
    Instance instance();

    // Request adapter (GPU device handle)
    Adapter adapter(Instance instance);

    // Request device (logical GPU connection)
    Device device(Adapter adapter);

    // Create compute pipeline from WGSL shader
    ComputePipeline pipeline(Device device, const char* wgsl);

} // namespace dawn

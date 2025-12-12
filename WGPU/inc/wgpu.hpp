// dawn.hpp - PQTR WebGPU interface
//
// Single header for WebGPU access via Dawn.
// Use for GPU compute in VIBE mods.
// This should be BLAT! 

#pragma once

#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

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

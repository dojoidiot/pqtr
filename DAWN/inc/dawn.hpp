// dawn.hpp - PQTR WebGPU interface
//
// Single header for WebGPU access via Dawn.
// Use for GPU compute in VIBE mods.

#pragma once

#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

namespace dawn
{
    // Initialize WebGPU instance
    // Returns nullptr on failure
    wgpu::Instance instance();

    // Request adapter (GPU device handle)
    // Blocks until adapter is ready
    wgpu::Adapter adapter(wgpu::Instance instance);

    // Request device (logical GPU connection)
    // Blocks until device is ready
    wgpu::Device device(wgpu::Adapter adapter);

    // Create compute pipeline from WGSL shader
    wgpu::ComputePipeline pipeline(wgpu::Device device, const char* wgsl);

} // namespace dawn

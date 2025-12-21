// dawn.cpp - PQTR WebGPU implementation
//
// Provides simple C++ wrappers around Dawn WebGPU.

#include <dawn/webgpu_cpp.h>
#include <dawn/native/DawnNative.h>
#include <dawn/dawn_proc.h>

#include <cstdio>

namespace dawn
{

wgpu::Instance instance()
{
    // Set up Dawn proc table
    DawnProcTable procs = dawn::native::GetProcs();
    dawnProcSetProcs(&procs);

    // Create instance with default descriptor
    wgpu::InstanceDescriptor desc{};
    return wgpu::CreateInstance(&desc);
}

wgpu::Adapter adapter(wgpu::Instance instance)
{
    wgpu::Adapter result;
    bool done = false;

    wgpu::RequestAdapterOptions options{};
    options.powerPreference = wgpu::PowerPreference::HighPerformance;

    instance.RequestAdapter(
        &options,
        wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
            if (status == wgpu::RequestAdapterStatus::Success) {
                result = adapter;
            } else {
                fprintf(stderr, "Failed to get adapter: %.*s\n",
                    static_cast<int>(message.length), message.data);
            }
            done = true;
        });

    // Process events until callback fires
    while (!done) {
        instance.ProcessEvents();
    }

    return result;
}

wgpu::Device device(wgpu::Adapter adapter)
{
    wgpu::Device result;
    bool done = false;

    wgpu::DeviceDescriptor desc{};
    desc.SetDeviceLostCallback(
        wgpu::CallbackMode::AllowSpontaneous,
        [](const wgpu::Device&, wgpu::DeviceLostReason, wgpu::StringView message) {
            fprintf(stderr, "Device lost: %.*s\n",
                static_cast<int>(message.length), message.data);
        });
    desc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType, wgpu::StringView message) {
            fprintf(stderr, "WebGPU error: %.*s\n",
                static_cast<int>(message.length), message.data);
        });

    adapter.RequestDevice(
        &desc,
        wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
            if (status == wgpu::RequestDeviceStatus::Success) {
                result = device;
            } else {
                fprintf(stderr, "Failed to get device: %.*s\n",
                    static_cast<int>(message.length), message.data);
            }
            done = true;
        });

    // Process events until callback fires
    wgpu::Instance inst = adapter.GetInstance();
    while (!done) {
        inst.ProcessEvents();
    }

    return result;
}

wgpu::ComputePipeline pipeline(wgpu::Device device, const char* wgsl)
{
    // Create shader module from WGSL
    wgpu::ShaderSourceWGSL wgslSource{};
    wgslSource.code = wgsl;

    wgpu::ShaderModuleDescriptor shaderDesc{};
    shaderDesc.nextInChain = &wgslSource;

    wgpu::ShaderModule shader = device.CreateShaderModule(&shaderDesc);

    // Create compute pipeline
    wgpu::ComputePipelineDescriptor pipelineDesc{};
    pipelineDesc.compute.module = shader;
    pipelineDesc.compute.entryPoint = "main";

    return device.CreateComputePipeline(&pipelineDesc);
}

} // namespace dawn

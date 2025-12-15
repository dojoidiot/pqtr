#pragma once

#include <dawn/webgpu.h>

namespace dawn {

wgpu::Instance instance();
wgpu::Adapter adapter(wgpu::Instance instance);
wgpu::Device device(wgpu::Adapter adapter);
wgpu::ComputePipeline pipeline(wgpu::Device device, const char* wgsl);

} // namespace dawn

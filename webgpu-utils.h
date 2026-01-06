#pragma once
#include <webgpu/webgpu.hpp>

void inspectAdapter(wgpu::Adapter adapter);

void inspectDevice(wgpu::Device device);

uint32_t ceilToNexMultiple(uint32_t structSize, uint32_t limitMinUniformBufferOffset);
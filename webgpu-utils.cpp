#include "webgpu-utils.h"

#include <iostream>
#include <vector>
#include <cassert>

// 返回dynamic uniform buffer的中前面（n -1）个dynamic uniform的每个uniform的存储空间大小。第n个uniform为strcutSize本身。
// @param structSize : C++层的struct的空间大小
// @param limitMinUniformBufferOffset : 当前device所要求的最小uniform buffer 偏移字节数
uint32_t ceilToNexMultiple(uint32_t structSize, uint32_t limitMinUniformBufferOffset) {
    // structSize是否为 limitMinUniformBufferOffset 的整数倍？
    // 是：返回 structSize原值（即 count * limitMinUniformBufferOffset)
    // 否：返回 [(整数倍 + 1) * limitMinUniformBufferOffset]
    // 向上(ceil)取 limitMinUniformBufferOffset 的整数倍
    uint32_t ceilCount = structSize / limitMinUniformBufferOffset + (structSize % limitMinUniformBufferOffset == 0 ? 0 : 1);
	return ceilCount * limitMinUniformBufferOffset;
}

void inspectAdapter(wgpu::Adapter adapter) {
    wgpu::SupportedLimits supportedLimits = {};
    supportedLimits.nextInChain = nullptr;
    bool success = adapter.getLimits(&supportedLimits);
    if (success) {
        std::cout << "Adapter limits:" << std::endl;
        std::cout << "  maxVertexAttributes: " << supportedLimits.limits.maxVertexAttributes << std::endl;
        std::cout << "  maxVertexBuffers: " << supportedLimits.limits.maxVertexBuffers << std::endl;
        std::cout << "  maxBufferSize: " << supportedLimits.limits.maxBufferSize << std::endl;
        std::cout << "  maxTextureDimension1D: " << supportedLimits.limits.maxTextureDimension1D << std::endl;
        std::cout << "  maxTextureDimension2D: " << supportedLimits.limits.maxTextureDimension2D << std::endl;
        std::cout << "  maxTextureDimension3D: " << supportedLimits.limits.maxTextureDimension3D << std::endl;
        std::cout << "  maxTextureArrayLayers: " << supportedLimits.limits.maxTextureArrayLayers << std::endl;
        std::cout << "  maxBindGroups: " << supportedLimits.limits.maxBindGroups << std::endl;
    }


    size_t featureCount = adapter.enumerateFeatures(nullptr);
    std::vector<wgpu::FeatureName> features;
    // 使用 vector 的构造函数分配相同大小的元素
    for (size_t i = 0; i < featureCount; ++i) {
        features.push_back(wgpu::FeatureName(wgpu::FeatureName::Undefined));
    }
    adapter.enumerateFeatures(features.data());
    std::cout << "Device features(" << featureCount << ") (C++ wrapper):" << std::endl;
    std::cout << std::hex;
    for (wgpu::FeatureName f : features) {
        std::cout << " - 0x" << static_cast<WGPUFeatureName>(f) << std::endl;
    }
    std::cout << std::dec;




    wgpu::AdapterProperties properties = {};
    adapter.getProperties(&properties);
    std::cout << "Adapter properties:" << std::endl;
    std::cout << "  vendorID: " << properties.vendorID << std::endl;
    std::cout << "  vendorName: " << (properties.vendorName ? properties.vendorName : "N/A") << std::endl;
    std::cout << "  architecture: " << (properties.architecture ? properties.architecture : "N/A") << std::endl;
    std::cout << "  deviceID: " << properties.deviceID << std::endl;
    std::cout << "  name: " << (properties.name ? properties.name : "N/A") << std::endl;
    std::cout << "  driverDescription: " << (properties.driverDescription ? properties.driverDescription : "N/A") << std::endl;
    
    std::cout << std::hex;
    std::cout << "  adapterType: " << properties.adapterType << std::endl;
    std::cout << "  backendType: " << properties.backendType << std::endl;
    std::cout << std::dec;
}



void inspectDevice(wgpu::Device device) {
    size_t featureCount = device.enumerateFeatures(nullptr);
    std::vector<wgpu::FeatureName> features;
    // 使用 vector 的构造函数分配相同大小的元素
    for (size_t i = 0; i < featureCount; ++i) {
        features.push_back(wgpu::FeatureName(wgpu::FeatureName::Undefined));
    }
    device.enumerateFeatures(features.data());
    std::cout << "Device features(" << featureCount << ") (C++ wrapper):" << std::endl;
    std::cout << std::hex;
    for (wgpu::FeatureName f : features) {
        std::cout << " - 0x" << static_cast<WGPUFeatureName>(f) << std::endl;
    }
    std::cout << std::dec;

    wgpu::SupportedLimits limits = {};
    limits.nextInChain = nullptr;
    bool success = device.getLimits(&limits);
    if (success) {
		std::cout << "Device limits:" << std::endl;
		std::cout << " - maxTextureDimension1D: " << limits.limits.maxTextureDimension1D << std::endl;
		std::cout << " - maxTextureDimension2D: " << limits.limits.maxTextureDimension2D << std::endl;
		std::cout << " - maxTextureDimension3D: " << limits.limits.maxTextureDimension3D << std::endl;
		std::cout << " - maxTextureArrayLayers: " << limits.limits.maxTextureArrayLayers << std::endl;
		std::cout << " - maxBindGroups: " << limits.limits.maxBindGroups << std::endl;
		std::cout << " - maxDynamicUniformBuffersPerPipelineLayout: " << limits.limits.maxDynamicUniformBuffersPerPipelineLayout << std::endl;
		std::cout << " - maxDynamicStorageBuffersPerPipelineLayout: " << limits.limits.maxDynamicStorageBuffersPerPipelineLayout << std::endl;
		std::cout << " - maxSampledTexturesPerShaderStage: " << limits.limits.maxSampledTexturesPerShaderStage << std::endl;
		std::cout << " - maxSamplersPerShaderStage: " << limits.limits.maxSamplersPerShaderStage << std::endl;
		std::cout << " - maxStorageBuffersPerShaderStage: " << limits.limits.maxStorageBuffersPerShaderStage << std::endl;
		std::cout << " - maxStorageTexturesPerShaderStage: " << limits.limits.maxStorageTexturesPerShaderStage << std::endl;
		std::cout << " - maxUniformBuffersPerShaderStage: " << limits.limits.maxUniformBuffersPerShaderStage << std::endl;
		std::cout << " - maxUniformBufferBindingSize: " << limits.limits.maxUniformBufferBindingSize << std::endl;
		std::cout << " - maxStorageBufferBindingSize: " << limits.limits.maxStorageBufferBindingSize << std::endl;
		std::cout << " - minUniformBufferOffsetAlignment: " << limits.limits.minUniformBufferOffsetAlignment << std::endl;
		std::cout << " - minStorageBufferOffsetAlignment: " << limits.limits.minStorageBufferOffsetAlignment << std::endl;
		std::cout << " - maxVertexBuffers: " << limits.limits.maxVertexBuffers << std::endl;
		std::cout << " - maxVertexAttributes: " << limits.limits.maxVertexAttributes << std::endl;
		std::cout << " - maxVertexBufferArrayStride: " << limits.limits.maxVertexBufferArrayStride << std::endl;
		std::cout << " - maxInterStageShaderComponents: " << limits.limits.maxInterStageShaderComponents << std::endl;
		std::cout << " - maxComputeWorkgroupStorageSize: " << limits.limits.maxComputeWorkgroupStorageSize << std::endl;
		std::cout << " - maxComputeInvocationsPerWorkgroup: " << limits.limits.maxComputeInvocationsPerWorkgroup << std::endl;
		std::cout << " - maxComputeWorkgroupSizeX: " << limits.limits.maxComputeWorkgroupSizeX << std::endl;
		std::cout << " - maxComputeWorkgroupSizeY: " << limits.limits.maxComputeWorkgroupSizeY << std::endl;
		std::cout << " - maxComputeWorkgroupSizeZ: " << limits.limits.maxComputeWorkgroupSizeZ << std::endl;
		std::cout << " - maxComputeWorkgroupsPerDimension: " << limits.limits.maxComputeWorkgroupsPerDimension << std::endl;
	}
}
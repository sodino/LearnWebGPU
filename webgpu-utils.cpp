#include "webgpu-utils.h"

#include <iostream>
#include <vector>
#include <cassert>

WGPUAdapter requestAdapterSync(WGPUInstance instance, WGPURequestAdapterOptions const * options) {
    struct UserData {
        WGPUAdapter adapter = nullptr;
        bool requestEnded = false;
    };

    UserData userData;


    auto onAdapterRequestEnded = [](WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * pUserData) {
        UserData& userData = *reinterpret_cast<UserData*>(pUserData);
        if (status == WGPURequestAdapterStatus_Success) {
            userData.adapter = adapter;
        } else {
            std::cout << "Could not get WebGPU adapter: " << message <<std::endl;
        }

        userData.requestEnded = true;
    };

    wgpuInstanceRequestAdapter(
        instance,
        options,
        onAdapterRequestEnded,
        (void*)&userData
    );

    assert(userData.requestEnded);
    return userData.adapter;
}

void inspectAdapter(WGPUAdapter adapter) {
    WGPUSupportedLimits supportedLimits = {};
    supportedLimits.nextInChain = nullptr;
    bool success = wgpuAdapterGetLimits(adapter, &supportedLimits);
    if (success) {
        std::cout << "Adapter limits:" << std::endl;
        std::cout << "  maxTextureDimension1D: " << supportedLimits.limits.maxTextureDimension1D << std::endl;
        std::cout << "  maxTextureDimension2D: " << supportedLimits.limits.maxTextureDimension2D << std::endl;
        std::cout << "  maxTextureDimension3D: " << supportedLimits.limits.maxTextureDimension3D << std::endl;
        std::cout << "  maxTextureArrayLayers: " << supportedLimits.limits.maxTextureArrayLayers << std::endl;
        std::cout << "  maxBindGroups: " << supportedLimits.limits.maxBindGroups << std::endl;
    }


    std::vector<WGPUFeatureName> features;
    // 先取:特性数量
    size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
    features.resize(featureCount);
    // 再取:特性列表
    wgpuAdapterEnumerateFeatures(adapter, features.data());
    std::cout << "Adapter features:" << std::endl;
    std::cout << std::hex;
    for (WGPUFeatureName f : features) {
        std::cout << " - 0x" << f << std::endl;
    }
    std::cout << std::dec;

    WGPUAdapterProperties properties = {};
    wgpuAdapterGetProperties(adapter, &properties);
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

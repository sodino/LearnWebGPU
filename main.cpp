#include "webgpu-utils.h"
#include <webgpu/webgpu.h>

#include <iostream>
#include <vector>
#include <cassert>


class Application {
public:
    Application();

    ~Application();


    bool Initialize();
    void Terminate();
    void MainLoop();
    bool IsRunning();
private:
    WGPUDevice device;
};

int main() {
    Application app;
    if (!app.Initialize()) {
        std::cout << "Failed to initialize application." << std::endl;
        return 1;
    }
    return 1;
}


Application::Application() {
}
Application::~Application() {
}
bool Application::Initialize() {
    WGPUInstance instance = wgpuCreateInstance(nullptr);
    std::cout << "-> Created WebGPU instance: " << instance << std::endl;

    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);
    if (adapter == nullptr) {
        std::cout << "-> Failed to get WebGPU adapter." << std::endl;
        return false;
    }
    std::cout << "-> Got WebGPU adapter: " << adapter << std::endl;
    inspectAdapter(adapter);

    wgpuInstanceRelease(instance); // 不再需要了,释放WGPUInstance

    std::cout << "Requesting device ..." << std::endl;
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My WebGPU Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "Default Queue";
    deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const * message, void * ) {
        std::cout << "WebGPU Device lost! Reason: " << reason << ", message: " << message << std::endl;
    };
    
    device = requestDeviceSync(adapter, &deviceDesc);
    if (device == nullptr) {
        std::cout << "-> Failed to get WebGPU device." << std::endl;
        return false;
    }
    std::cout << "-> Got WebGPU device: " << device << std::endl;
    inspectDevice(device);

    auto onDeviceError = [](WGPUErrorType type, char const * message, void * ) {
        std::cout << "WebGPU Device Error! Type: " << type << ", message: " << message << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);
    

    return true;
}
void Application::Terminate() {
}
void Application::MainLoop() {
}
bool Application::IsRunning() {
    return false;
}
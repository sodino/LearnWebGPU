#include "webgpu-utils.h"
#include <webgpu/webgpu.h>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

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
    GLFWwindow* window = nullptr;
    WGPUSurface surface = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
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
    // Init glfw Window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(800, 600, "Learn WebGPU", nullptr, nullptr);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window." << std::endl;
        return false;
    }





    WGPUInstance instance = wgpuCreateInstance(nullptr);
    if (instance == nullptr) {
        std::cout << "Failed to create WebGPU instance." << std::endl;
        return false;
    }

    std::cout << "-> Created WebGPU instance: " << instance << std::endl;

    surface = glfwGetWGPUSurface(instance, window);
    if (surface == nullptr) {
        std::cout << "Failed to create WebGPU surface from GLFW window." << std::endl;
        return false;
    }
    std::cout << "-> Created WebGPU surface: " << surface << std::endl;




    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = surface;    // 让适配器使用这个surface
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


    queue = wgpuDeviceGetQueue(device);
    std::cout << "-> Got WebGPU queue: " << queue << std::endl;

    WGPUSurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.device = device;
    config.width = 640;
    config.height = 480;
    config.usage = WGPUTextureUsage_RenderAttachment;
    WGPUTextureFormat surfaceFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
    config.format = surfaceFormat;

    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(surface, &config);
    std::cout << "-> Configured WebGPU surface." << std::endl;


    wgpuAdapterRelease(adapter); // 不再需要了,释放WGPUAdapter
    return true;
}
void Application::Terminate() {
    if (queue) {
        wgpuQueueRelease(queue);
        queue = nullptr;
    }
    if (device) {
        wgpuDeviceRelease(device);
        device = nullptr;
    }
    if (surface) {
        wgpuSurfaceUnconfigure(surface);
        wgpuSurfaceRelease(surface);
        surface = nullptr;
    }
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
}
void Application::MainLoop() {
}
bool Application::IsRunning() {
    return false;
}
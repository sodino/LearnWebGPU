#include "webgpu-utils.h"
#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
    #include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

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
    WGPUTextureView GetNextSurfaceTextureView();

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

    while (app.IsRunning()) {
        app.MainLoop();
    }

    app.Terminate();
    return 0;
}


Application::Application() {
}
Application::~Application() {
}
WGPUTextureView Application::GetNextSurfaceTextureView() {
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        std::cout << "Failed to get current surface texture. Status: " << surfaceTexture.status << std::endl;
        return nullptr;
    }


    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.nextInChain = nullptr;
    viewDesc.label = "Surface texture view";
    viewDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_All;
    WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);
    // wgpuTextureRelease(surfaceTexture.texture); // 释放纹理对象引用, 但wgpu-native不能手动释放，所以注释掉
    return targetView;
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
    config.width = 800;
    config.height = 600;
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
	glfwPollEvents();

	// Get the next target texture view
	WGPUTextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;

	// Create a command encoder for the draw call
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = "My command encoder";
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

	// Create the render pass that clears the screen with our color
	WGPURenderPassDescriptor renderPassDesc = {};
	renderPassDesc.nextInChain = nullptr;

	// The attachment part of the render pass descriptor describes the target texture of the pass
	WGPURenderPassColorAttachment renderPassColorAttachment = {};
	renderPassColorAttachment.view = targetView;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	renderPassColorAttachment.clearValue = WGPUColor{ 1.0, 0.0, 1.0, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	// Create the render pass and end it immediately (we only clear the screen but do not draw anything)
	WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
	wgpuRenderPassEncoderEnd(renderPass);
	wgpuRenderPassEncoderRelease(renderPass);

	// Finally encode and submit the render pass
	WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.nextInChain = nullptr;
	cmdBufferDescriptor.label = "Command buffer";
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
	wgpuCommandEncoderRelease(encoder);

	std::cout << "Submitting command..." << std::endl;
	wgpuQueueSubmit(queue, 1, &command);
	wgpuCommandBufferRelease(command);
	std::cout << "Command submitted." << std::endl;

	// At the end of the frame
	wgpuTextureViewRelease(targetView);
#ifndef __EMSCRIPTEN__
	wgpuSurfacePresent(surface);
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
	wgpuDevicePoll(device, false, nullptr);
#endif
}

bool Application::IsRunning() {
    if (window == nullptr) {
        return false;
    }
    bool b = glfwWindowShouldClose(window) == GLFW_FALSE;
    return b;
}
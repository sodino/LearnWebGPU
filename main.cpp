#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include "webgpu-utils.h"
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
    wgpu::TextureView GetNextSurfaceTextureView();

private:
    GLFWwindow* window = nullptr;
    wgpu::Surface surface = nullptr;
    wgpu::Device device = nullptr;
    wgpu::Queue queue = nullptr;
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


Application::Application() { }
Application::~Application() { }

wgpu::TextureView Application::GetNextSurfaceTextureView() {
    wgpu::SurfaceTexture surfaceTexture;
    surface.getCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::Success) {
        std::cout << "Failed to get current surface texture. Status: " << surfaceTexture.status << std::endl;
        return nullptr;
    }
    wgpu::Texture texture = surfaceTexture.texture;

    wgpu::TextureViewDescriptor tvDesc = {};
    tvDesc.nextInChain = nullptr;
    tvDesc.label = "Surface texture view";
    // tvDesc.format = wgpuTextureGetFormat(surfaceTexture.texture);
    tvDesc.format = texture.getFormat();
    // tvDesc.dimension = WGPUTextureViewDimension_2D;
    tvDesc.dimension = wgpu::TextureViewDimension::_2D;
    tvDesc.baseMipLevel = 0;
    tvDesc.mipLevelCount = 1;
    tvDesc.baseArrayLayer = 0;
    tvDesc.arrayLayerCount = 1;
    tvDesc.aspect = WGPUTextureAspect_All;
    // WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &tvDesc);
    wgpu::TextureView targetView = texture.createView(tvDesc);
    // wgpuTextureRelease(surfaceTexture.texture); // 释放纹理对象引用, 但wgpu-native不能手动释放，所以注释掉
    return targetView;
}

// 初始化WebGPU和GLFW
// 1. 初始化 glfw，输出 ： window (留存, IsRunning 判断)
// 2. 实现化WGPUInstance : instance (销毁)
// 3. window + instance 输出 WGPUSurface / surface (留存， 每次绘制时将渲染结果提交到屏幕显示)
// 4. instance 请求出 WGPUAdapter / adapter (销毁)
// 5. adapter 请求出 WGPUDevice / device (留存，创建CommandEncoder & 每次绘制时触发后端执行各种事件/回调)
// 6. device 取出 WGPUQueue / queue (留存, 将渲染命令提交到GPU执行队列)
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




    wgpu::InstanceDescriptor desc = {};
    wgpu::Instance instance = wgpu::createInstance(desc);    // wgpuInstanceRelease
    if (instance == nullptr) {
        std::cout << "Failed to create WebGPU instance." << std::endl;
        return false;
    }

    std::cout << "-> Created WebGPU instance: " << instance << std::endl;

    surface = glfwGetWGPUSurface(instance, window);         // wgpuSurfaceRelease
    if (surface == nullptr) {
        std::cout << "Failed to create WebGPU surface from GLFW window." << std::endl;
        return false;
    }
    std::cout << "-> Created WebGPU surface: " << surface << std::endl;




    wgpu::RequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    adapterOpts.compatibleSurface = surface;    // 让适配器使用这个surface
    wgpu::Adapter adapter = instance.requestAdapter(adapterOpts);   // wgpuAdapterRelease
    if (adapter == nullptr) {
        std::cout << "-> Failed to get WebGPU adapter." << std::endl;
        return false;
    }
    std::cout << "-> Got WebGPU adapter: " << adapter << std::endl;
    inspectAdapter(adapter);

    // wgpuInstanceRelease(instance); // 不再需要了,释放WGPUInstance
    instance.release();

    std::cout << "Requesting device ..." << std::endl;
    wgpu::DeviceDescriptor deviceDesc = {};
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My WebGPU Device";
    deviceDesc.requiredFeatureCount = 0;
    deviceDesc.requiredLimits = nullptr;
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "Default Queue";
    deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const * message, void * ) {
        std::cout << "WebGPU Device lost! Reason: " << reason << ", message: " << message << std::endl;
    };
    
    device = adapter.requestDevice(deviceDesc);       // wgpuDeviceRelease
    if (device == nullptr) {
        std::cout << "-> Failed to get WebGPU device." << std::endl;
        return false;
    }
    std::cout << "-> Got WebGPU device: " << device << std::endl;
    inspectDevice(device);

    auto onDeviceError = [](wgpu::ErrorType type, char const * message) {
        std::cout << "WebGPU Device Error! Type: " << type << ", message: " << message << std::endl;
    };
    // wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);
    auto errorCallbackHandle = device.setUncapturedErrorCallback(onDeviceError);


    // queue = wgpuDeviceGetQueue(device);     // wgpuQueueRelease
    queue = device.getQueue();     // wgpuQueueRelease
    std::cout << "-> Got WebGPU queue: " << queue << std::endl;

    wgpu::SurfaceConfiguration cfgSurface = {};
    cfgSurface.nextInChain = nullptr;
    cfgSurface.device = device;
    cfgSurface.width = 800;
    cfgSurface.height = 600;
    // cfgSurface.usage = WGPUTextureUsage_RenderAttachment;
    cfgSurface.usage = wgpu::TextureUsage::RenderAttachment;
    // WGPUTextureFormat textureFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
    wgpu::TextureFormat textureFormat = surface.getPreferredFormat(adapter);
    cfgSurface.format = textureFormat;

    cfgSurface.viewFormatCount = 0;
    cfgSurface.viewFormats = nullptr;
    cfgSurface.presentMode = WGPUPresentMode_Fifo;
    cfgSurface.alphaMode = WGPUCompositeAlphaMode_Auto;
    // wgpuSurfaceConfigure(surface, &cfgSurface);         // wgpuSurfaceUnconfigure
    surface.configure(cfgSurface);         // wgpuSurfaceUnconfigure
    std::cout << "-> Configured WebGPU surface." << std::endl;


    // wgpuAdapterRelease(adapter); // 不再需要了,释放WGPUAdapter
    adapter.release(); // 不再需要了,释放WGPUAdapter
    return true;
}
void Application::Terminate() {
    if (queue) {
        // wgpuQueueRelease(queue);
        queue.release();
        queue = nullptr;
    }
    if (device) {
        // wgpuDeviceRelease(device);
        device.release();
        device = nullptr;
    }
    if (surface) {
        // wgpuSurfaceUnconfigure(surface);
        // wgpuSurfaceRelease(surface);
        surface.unconfigure();
        surface.release();
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
	wgpu::TextureView targetView = GetNextSurfaceTextureView();
	if (!targetView) return;

	// Create a command encoder for the draw call
	// WGPUCommandEncoderDescriptor encoderDesc = {};
	wgpu::CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = "My command encoder";
	// WGPUCommandEncoder cmdEncoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);   // wgpuCommandEncoderRelease
	wgpu::CommandEncoder cmdEncoder = device.createCommandEncoder(encoderDesc);   // wgpuCommandEncoderRelease

	// Create the render pass that clears the screen with our color
	// WGPURenderPassDescriptor renderPassDesc = {};
    wgpu::RenderPassDescriptor renderPassDesc = {};
	renderPassDesc.nextInChain = nullptr;

	// The attachment part of the render pass descriptor describes the target texture of the pass
	// WGPURenderPassColorAttachment colorAttachment = {};
    wgpu::RenderPassColorAttachment colorAttachment = {};
	colorAttachment.view = targetView;
	colorAttachment.resolveTarget = nullptr;
	colorAttachment.loadOp = WGPULoadOp_Clear;
	colorAttachment.storeOp = WGPUStoreOp_Store;
	// colorAttachment.clearValue = WGPUColor{ 1.0, 0.0, 1.0, 1.0 };
	colorAttachment.clearValue = wgpu::Color{ 1.0, 0.0, 1.0, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &colorAttachment;
	renderPassDesc.depthStencilAttachment = nullptr;
	renderPassDesc.timestampWrites = nullptr;

	// Create the render pass and end it immediately (we only clear the screen but do not draw anything)
	// WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);  // wgpuRenderPassEncoderRelease
    // wgpuRenderPassEncoderEnd(renderPass);
	// wgpuRenderPassEncoderRelease(renderPass);

	wgpu::RenderPassEncoder renderPass = cmdEncoder.beginRenderPass(renderPassDesc);  // wgpuRenderPassEncoderRelease
	renderPass.end();
	renderPass.release();

	// Finally encode and submit the render pass
	wgpu::CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.nextInChain = nullptr;
	cmdBufferDescriptor.label = "Command buffer";
	// WGPUCommandBuffer cmdBuffer = wgpuCommandEncoderFinish(cmdEncoder, &cmdBufferDescriptor); // wgpuCommandBufferRelease
	// wgpuCommandEncoderRelease(cmdEncoder);
    wgpu::CommandBuffer cmdBuffer = cmdEncoder.finish(cmdBufferDescriptor); // wgpuCommandBufferRelease
	cmdEncoder.release();

	std::cout << "Submitting command..." << std::endl;
	// wgpuQueueSubmit(queue, 1, &cmdBuffer);
	// wgpuCommandBufferRelease(cmdBuffer);
    queue.submit(1, &cmdBuffer);
	cmdBuffer.release();
	std::cout << "Command submitted." << std::endl;

	// At the end of the frame
	// wgpuTextureViewRelease(targetView);
    targetView.release();
#ifndef __EMSCRIPTEN__
	// wgpuSurfacePresent(surface);
	surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
	// wgpuDeviceTick(device);
    device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
	wgpuDevicePoll(device, false, nullptr);
	device.poll(false);
#endif
}

bool Application::IsRunning() {
    if (window == nullptr) {
        return false;
    }
    bool b = glfwWindowShouldClose(window) == GLFW_FALSE;
    return b;
}
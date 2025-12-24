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




const char* shaderSource = R"(
// 位置+颜色 的顶点属性结构，作为顶点着色器的输入参数
struct VertexInput {
    @location(0) position : vec2f,
    @location(1) color : vec3f,
};

// 顶点着色器的输出 & 片段着色器的输入
struct VertexOutput {
    @builtin(position) position : vec4f,
    @location(0) color : vec3f,
};

@vertex 
fn vs_main(in: VertexInput) -> VertexOutput {
    var out : VertexOutput; // 输入和输出都使用自定义结构
    out.position = vec4f(in.position, 0.0, 1.0);
    out.color = in.color; // 向片段着色器转发 颜色值
    return out;
}

@fragment
fn fs_main(in : VertexOutput) -> @location(0) vec4f {
    return vec4f(in.color, 1.0);
}
)";


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
    void InitializePipeline(wgpu::TextureFormat format);

    // 实现：创建 、 写入 、 复制 、 读取/映射 、 释放这些操作。
    void PlayingWithBuffers();

    // 因为要传入vertex positon,需要使用vertexBuffer，需要提前申请maxVertexBuffer
    wgpu::RequiredLimits GetRequiredLimits(wgpu::Adapter adapter) const;
    void InitializeBuffers();

private:
    GLFWwindow* window = nullptr;
    wgpu::Surface surface = nullptr;
    wgpu::Device device = nullptr;
    wgpu::Queue queue = nullptr;

    std::unique_ptr<wgpu::ErrorCallback> uncapturedErrorCallback;

    wgpu::RenderPipeline pipeline;

    wgpu::Buffer bufPoint;
    wgpu::Buffer bufIndex;
    uint32_t indexCount;
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


wgpu::RequiredLimits Application::GetRequiredLimits(wgpu::Adapter adapter) const {
    wgpu::SupportedLimits supportedLimits;
    adapter.getLimits(&supportedLimits);

    wgpu::RequiredLimits requiredLimits = wgpu::Default;
    requiredLimits.limits.maxVertexAttributes = 2;   // position + color : 要两种vertex attribute了
    requiredLimits.limits.maxVertexBuffers = 1;      //  6组{顶点 + color}直接填入一个VertexBuffer，仍然填1
    requiredLimits.limits.maxBufferSize = 6 * 5 * sizeof(float); // 6个顶点，每个顶点一对(x,y) + rgb共5个值，每个值都是float
    requiredLimits.limits.maxVertexBufferArrayStride = 5 * sizeof(float); // 步长为2:每个顶点需5个float，即一组(x,y) + 一组rgb

    requiredLimits.limits.maxInterStageShaderComponents = 3; // 从顶点着色器转发到片段着色器的数据最多为3个float，即rgb。
    requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    return requiredLimits;
}




void Application::InitializeBuffers() {
    // 定义由两个三角形拼成的正方形的 点数据
    std::vector<float> pointData = {
        // x0, y0,        r0,  g0,  b0
        -0.5, -0.5,      1.0, 0.0, 0.0, // 左下 0
        +0.5, -0.5,      0.0, 1.0, 0.0, // 右下 1
        +0.5, +0.5,      0.0, 0.0, 1.0, // 右上 2
        -0.5, +0.5,      1.0, 1.0, 0.0  // 左上 3
    };

    // 定义索引，规则 点数据 如何组成三角形
    std::vector<uint16_t> indexData = {
        0, 1, 2, // 右下的三角形
        0, 2, 3  // 左上的三角形
    };

    indexCount = static_cast<uint32_t>(indexData.size()); // 索引才有真实 : 点数据个数

    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.mappedAtCreation = false;
    // 创建点数据 buffer
    bufferDesc.size = pointData.size() * sizeof(float);  // float是4bytes，整个size必定是4的倍数了
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
    bufPoint = device.createBuffer(bufferDesc);
    queue.writeBuffer(bufPoint, 0, pointData.data(), bufferDesc.size);

    // 创建索引 buffer
    uint16_t idxSize = indexData.size() * sizeof(uint16_t);
    bufferDesc.size = (idxSize +3) & ~3; // 向上取值到4的倍数
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index; // 变更为 Index 索引缓冲区
    bufIndex = device.createBuffer(bufferDesc);
    queue.writeBuffer(bufIndex, 0, indexData.data(), bufferDesc.size);
}


void Application::InitializePipeline(wgpu::TextureFormat format) {
    wgpu::ShaderModuleDescriptor shaderDesc;
    #ifdef WEBGPU_BACKEND_WGPU
        shaderDesc.hintCount = 0;
        shaderDesc.hints = nullptr;
    #endif

    wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
    shaderCodeDesc.chain.next = nullptr;
    shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    shaderCodeDesc.code = shaderSource;

    shaderDesc.nextInChain = &shaderCodeDesc.chain;

    wgpu::ShaderModule shaderModule = device.createShaderModule(shaderDesc);

    wgpu::RenderPipelineDescriptor pipelineDesc;

    wgpu::VertexBufferLayout vertexBufferLayout;
    // position + rgb ，需要两种 VertexAttribute
    std::vector<wgpu::VertexAttribute> vertexAttribs;

    wgpu::VertexAttribute positionAttrib;
    positionAttrib.shaderLocation = 0; // @location(0)
    positionAttrib.format = wgpu::VertexFormat::Float32x2;
    positionAttrib.offset = 0;
    vertexAttribs.push_back(positionAttrib);

    wgpu::VertexAttribute rgbAttrib;
    rgbAttrib.shaderLocation = 1;     // @location(1)
    rgbAttrib.format = wgpu::VertexFormat::Float32x3;
    rgbAttrib.offset = 2 * sizeof(float); // 前面每一组position的长度是2个float
    vertexAttribs.push_back(rgbAttrib);

    vertexBufferLayout.attributeCount = vertexAttribs.size();    // 1个position Attrib + 1个rgb Attrib
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = 5 * sizeof(float);          // 顶点数据 步长为 5 float
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;

    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
    pipelineDesc.primitive.cullMode = wgpu::CullMode::None;



    wgpu::FragmentState fragmentState;
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = "fs_main";
    fragmentState.constantCount = 0;
    fragmentState.constants = nullptr;

    wgpu::BlendState blendState;
    blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = wgpu::BlendOperation::Add;
    blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blendState.alpha.dstFactor = wgpu::BlendFactor::One;
    blendState.alpha.operation = wgpu::BlendOperation::Add;


    wgpu::ColorTargetState colorState;
    colorState.format = format;
    colorState.blend = &blendState;
    colorState.writeMask = wgpu::ColorWriteMask::All;


    fragmentState.targetCount = 1;  // 因为count是1，对应 @location(0) 中的 0即为这里的colorState
    fragmentState.targets = &colorState;
    pipelineDesc.fragment = &fragmentState;

    pipelineDesc.depthStencil = nullptr;
    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;
    pipelineDesc.layout = nullptr;
    pipeline = device.createRenderPipeline(pipelineDesc);

    shaderModule.release();
}

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
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "Default Queue";
    deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const * message, void * ) {
        std::cout << "WebGPU Device lost! Reason: " << reason << ", message: " << message << std::endl;
    };
    wgpu::RequiredLimits requiredLimits = GetRequiredLimits(adapter);
    deviceDesc.requiredLimits = &requiredLimits;
    
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
    uncapturedErrorCallback = device.setUncapturedErrorCallback(onDeviceError);


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
    wgpu::TextureFormat textureFormat = surface.getPreferredFormat(adapter);// Store the chosen surface format so pipeline creation can use it
    cfgSurface.format = textureFormat;

    cfgSurface.viewFormatCount = 0;
    cfgSurface.viewFormats = nullptr;
    cfgSurface.presentMode = WGPUPresentMode_Fifo;
    cfgSurface.alphaMode = WGPUCompositeAlphaMode_Auto;
    // wgpuSurfaceConfigure(surface, &cfgSurface);         // wgpuSurfaceUnconfigure2
    surface.configure(cfgSurface);         // wgpuSurfaceUnconfigure
    std::cout << "-> Configured WebGPU surface." << std::endl;


    // wgpuAdapterRelease(adapter); // 不再需要了,释放WGPUAdapter
    adapter.release(); // 不再需要了,释放WGPUAdapter


    InitializePipeline(textureFormat);
    InitializeBuffers();

    // PlayingWithBuffers();
    return true;
}

void Application::PlayingWithBuffers() {
    const int LENGTH = 16;
    // 预备cpu数据，准备写入到gpu
    std::vector<uint8_t> numbers(LENGTH);
    for (uint8_t i = 0; i < LENGTH; i ++) {
        numbers[i] = i;
    }


    wgpu::BufferDescriptor bufferDesc;
    bufferDesc.label = "Some GPU-side data buffer";
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::CopySrc;
    bufferDesc.size = LENGTH;
    bufferDesc.mappedAtCreation = false;
    // 1. 创建
    wgpu::Buffer buffer1 = device.createBuffer(bufferDesc);

    bufferDesc.label = "Output buffer";
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead;
    wgpu::Buffer buffer2 = device.createBuffer(bufferDesc);
    // 2. 写入
    queue.writeBuffer(buffer1, 0, numbers.data(), numbers.size());

    // 3. 复制
    wgpu::CommandEncoder encoder = device.createCommandEncoder(wgpu::Default);
    encoder.copyBufferToBuffer(buffer1, 0, buffer2, 0, LENGTH);
    wgpu::CommandBuffer command = encoder.finish(wgpu::Default);
    encoder.release();
    queue.submit(1, &command);
    command.release();

    // 4. 映射与读取（C++ 风格）
    bool ready = false;

    // C++ 风格的回调 lambda
    auto asyncCallback = [&ready](wgpu::BufferMapAsyncStatus status) {
        if (status != wgpu::BufferMapAsyncStatus::Success) {
            std::cout << "buffer2 mapped failed, status=" << (int)status << std::endl;
        }
        ready = true;
    };

    // 使用 C++ 风格的 mapAsync()
    // 这个 返回值，必须持有。一旦不持有，那智能指针所持有的对象就会被回收，mapAsync内部异步处理时，找不到对象，会直接崩溃。那 asyncCallback 再也收不到回调了
    std::unique_ptr<wgpu::BufferMapCallback> _ = buffer2.mapAsync(wgpu::MapMode::Read, 0, LENGTH, asyncCallback);

    // 轮询设备直到映射完成
    while (!ready) {
        // poll : 尝试推动 GPU -> CPU 回调。
        // GPU是并行异步的，不能保证 poll 第一次时就会有结果，只好一直while了
        device.poll(true);
    }

    // 读取映射的数据
    uint8_t* bufferData = (uint8_t*)buffer2.getConstMappedRange(0, LENGTH);
    std::cout << "bufferData = [";
    for (int i = 0; i < LENGTH; i++) {
        std::cout << (int)bufferData[i] << " ";
    }
    std::cout << "]" << std::endl;

    buffer2.unmap(); // 结束 CPU 对 Buffer 的映射访问，把 Buffer 重新交还给 GPU 使用

    // 5. 回收
    buffer1.release();
    buffer2.release();
}

void Application::Terminate() {
    if (bufPoint != nullptr) {
        bufPoint.release();
        bufPoint = nullptr;
    }
    if (bufIndex != nullptr) {
        bufIndex.release();
        bufIndex = nullptr;
    }
    if (pipeline != nullptr) {
        pipeline.release();
        pipeline = nullptr;
    }
    if (queue != nullptr) {
        // wgpuQueueRelease(queue);
        queue.release();
        queue = nullptr;
    }
    if (device != nullptr) {
        // wgpuDeviceRelease(device);
        device.release();
        device = nullptr;
    }
    if (surface != nullptr) {
        // wgpuSurfaceUnconfigure(surface);
        // wgpuSurfaceRelease(surface);
        surface.unconfigure();
        surface.release();
        surface = nullptr;
    }
    if (window != nullptr) {
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

    renderPass.setPipeline(pipeline);
    renderPass.setVertexBuffer(0, bufPoint, 0, bufPoint.getSize());
    renderPass.setIndexBuffer(bufIndex, wgpu::IndexFormat::Uint16, 0, bufIndex.getSize());
    // renderPass.draw(indexCount, 1, 0, 0);
    renderPass.drawIndexed(indexCount, 1, 0, 0, 0);

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
	// wgpuDevicePoll(device, false, nullptr);
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
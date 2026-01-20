#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include "webgpu-utils.h"
#ifdef WEBGPU_BACKEND_WGPU
    #include <webgpu/wgpu.h>
#endif // WEBGPU_BACKEND_WGPU

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>


#define GLM_FORCE_DEPTH_ZERO_TO_ONE // GLM 默认使用 -1 到 1 的深度范围，而 WebGPU 使用 0 到 1。宏GLM_FORCE_DEPTH_ZERO_TO_ONE：将深度范围设为 0 到 1（WebGPU 默认）
#define GLM_FORCE_LEFT_HANDED // GLM 默认使用右手坐标系，而 WebGPU 使用左手坐标系。宏GLM_FORCE_LEFT_HANDED：将坐标系设为左手坐标系（WebGPU 默认）
#include <glm/glm.hpp> // all types inspired from GLSL
#include <glm/ext.hpp>
#include <iostream>
#include <vector>
#include <cassert>


#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

const char* shaderSource = R"(
// 位置+颜色 的顶点属性结构，作为顶点着色器的输入参数
struct VertexInput {
    @location(0) position : vec3f,
};

// 顶点着色器的输出 & 片段着色器的输入
struct VertexOutput {
    @builtin(position) position : vec4f,
};

struct MyUniforms {
    matModel   : mat4x4f,
    matView    : mat4x4f,    
    matProjection : mat4x4f,
};
@group(0) @binding(0)
var<uniform> data : MyUniforms;

@group(0) @binding(1)
var gradientTexture : texture_2d<f32>;


@vertex 
fn vs_main(in: VertexInput) -> VertexOutput {
    var out : VertexOutput; // 输入和输出都使用自定义结构
	
    // 矩阵乘法是右结合的，最右边的矩阵，最先作用在向量上。
    // 使用 Model / View / Projection 矩阵，将顶点坐标从 局部空间 → 世界空间 → 摄像机空间 → 裁剪空间（用于后续透视除法） 的逐步转换。
    out.position = data.matProjection * data.matView * data.matModel * vec4f(in.position, 1.0);
    return out;
}

@fragment
fn fs_main(in : VertexOutput) -> @location(0) vec4f {
    // 动态获取纹理宽高
    let _size : vec2<u32> = textureDimensions(gradientTexture);
    // 注意 textureDimensions的返回 是 u32，后续我们要和 i32进行对比，所以这里提前转化一下
    let textureSize : vec2<i32> = vec2<i32>(_size);
    // let textureSize : vec2<i32> = vec2<i32>(textureDimensions(gradientTexture)); // 并成一句话



    // 获取屏幕空间坐标，如：
	//   - 屏幕左上角：in.position.xy = (0.0, 0.0)
	//   - 屏幕中心：in.position.xy = (320.0, 240.0)  [屏幕大小 640x480]
	//   - 屏幕右下角：in.position.xy = (639.0, 479.0)
	let screenPosition = in.position.xy;
    // 从float转化到int32, 截断小数部分 : 如 : screenPosition = (50.9, 75.1) -> texCoord = (50, 75)  
    let texCoord = vec2<i32>(screenPosition);
    // 从纹理中读取图片指定坐标的颜色值
    // imgColor 默认值为 红色（非法坐标的范围）
    var imgColor = vec4f(0.0, 0.0, 0.0, 0.0);   // 0.0 : 透出原本的洋青色背景
    // texCoord的xy小于纹理宽高
    if (texCoord.x >= 0 && texCoord.x < textureSize.x 
        && texCoord.y >= 0 && texCoord.y < textureSize.y
    ) {
        imgColor = textureLoad(gradientTexture, texCoord, 0);   // 0 : 指定读取的 mip 级别，表示原图分辨率/最高分辨率。
    }

    // 最终输出颜色
    return imgColor;
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
    void InitializeBindGroups();
    void InitializeDepthTexture();

    void InitializeImageTexture();

private:
    struct MyUniforms {
        glm::mat4x4 matModel;
        glm::mat4x4 matView;
        glm::mat4x4 matProjection;
    };
    static_assert(sizeof(MyUniforms) % 16 == 0); // GPU一次性读取 16bytes，所以这里检查 16 bytes对齐。
    // time : 传0时表示初始状态
    void UpdateMyUniforms(MyUniforms& my, float time = 0.0f);
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

    wgpu::BindGroup bindGroup;
    wgpu::Buffer bufUniform;
    wgpu::BindGroupLayout layoutBindGroup;
    wgpu::PipelineLayout layoutPipeline;

    // 深度缓冲相关
    wgpu::Texture texDepth;
    wgpu::TextureView texViewDepth;


    // 图片纹理
    wgpu::Texture texImage;
    wgpu::TextureView texViewImage;
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



void Application::InitializeImageTexture() {
    // 在CPU侧自己创建一个 256x256 的 RGBA 图片数据
    const uint32_t texWidth = 256;
    const uint32_t texHeight = 256;

    // 创建 图片Texture 
    wgpu::TextureDescriptor descTexture;
    descTexture.dimension = wgpu::TextureDimension::_2D;
    descTexture.size = {texWidth, texHeight, 1};
    descTexture.mipLevelCount = 1;
    descTexture.sampleCount = 1;
    descTexture.format = wgpu::TextureFormat::RGBA8Unorm;
    descTexture.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    descTexture.viewFormatCount = 0;
    descTexture.viewFormats = nullptr;
    texImage = device.createTexture(descTexture);

    wgpu::TextureViewDescriptor descTextureView;
    // 纹理是“用来显示图片 / 作为颜色纹理”——👉 aspect = wgpu::TextureAspect::All（也是唯一正确、通用的选择）
    descTextureView.aspect = wgpu::TextureAspect::All;  // 使用全部的纹理数据（这些数据都用来表示RGBA了）
    descTextureView.baseArrayLayer = 0;
    descTextureView.arrayLayerCount = 1;
    descTextureView.baseMipLevel = 0;
    descTextureView.mipLevelCount = 1;
    descTextureView.dimension = wgpu::TextureViewDimension::_2D;
    descTextureView.format = descTexture.format;
    texViewImage = texImage.createView(descTextureView);

    // CPU rgba数据
    std::vector<uint8_t> imageData(texWidth * texHeight * 4); // 4 bytes per pixel (RGBA)

    // 填充图片数据，这里简单地创建一个渐变图像
    // for (uint32_t y = 0; y < texHeight; ++y) {
    //     for (uint32_t x = 0; x < texWidth; ++x) {
    //         size_t index = (y * texWidth + x) * 4;
    //         imageData[index + 0] = static_cast<uint8_t>((x / static_cast<float>(texWidth)) * 255); // R
    //         imageData[index + 1] = static_cast<uint8_t>((y / static_cast<float>(texHeight)) * 255); // G
    //         imageData[index + 2] = 128; // B
    //         imageData[index + 3] = 255; // A
    //     }
    // }
    // 填充图片数据，这里简单地创建 马赛克 效果
    for (uint32_t i = 0; i < texWidth; ++i) {
        for (uint32_t j = 0; j < texHeight; ++j) {
            uint8_t* p = &imageData[4 * (j * texWidth + i)];
            p[0] = (i / 16) % 2 == (j / 16) % 2 ? 255 : 0; // r
            p[1] = ((i - j) / 16) % 2 == 0 ? 255 : 0; // g
            p[2] = ((i + j) / 16) % 2 == 0 ? 255 : 0; // b
            p[3] = 255; // a
        }
    }

    // GPU 纹理中，要被写入的那一块区域在哪里 : imageCopyTexture.texture = texImage;
    wgpu::ImageCopyTexture imageCopyTexture;
    imageCopyTexture.texture = texImage;
    imageCopyTexture.mipLevel = 0;
    imageCopyTexture.origin = {0, 0, 0};

    wgpu::TextureDataLayout dataLayout; // 描述 : CPU 内存是怎么排布的
    dataLayout.offset = 0;
    dataLayout.bytesPerRow = texWidth * 4; // 4 bytes per pixel
    dataLayout.rowsPerImage = texHeight;

    queue.writeTexture(
        imageCopyTexture, // 要写入哪一块 GPU 纹理
        imageData.data(), imageData.size(), // CPU 侧待写入数据
        dataLayout,                         // 'CPU 侧待写入数据'的排布描述
        descTexture.size                    // 要写入的区域大小
    );

}

void Application::InitializeDepthTexture() {
    // Texture : 本质上是显存，创建时约定了“内存 + 存储规则”。在当前示例中，这块GPU 内存在这里解读为“这块 2D 表格” : data[x][y] = depthValue 的形式存储深度信息。
    // TextureView : 如何“解读/使用”这块显存的“视角/方式”。
    // GPU 永远不直接用 Texture，只通过 TextureView 来用。因为 现代GPU的核心设计思想中有：同一块内存，可能要被“多种方式使用”。

    wgpu::TextureDescriptor texDesc;
    texDesc.label = "Depth texture";
    // 深度纹理用于存储每个像素的深度值，所以设置为宽高与屏幕分辨率一致的 _2D纹理即可。
    // （二维的索引，存储一个深度值 : data[x][y] = depthValue)。
    // GPU 的所有光栅化都是 2D 的，每个fragment都天然对应一个屏幕上的像素位置(x,y)，用2D texture 方便零映射成本。
    texDesc.dimension = wgpu::TextureDimension::_2D; 
    texDesc.size = {WINDOW_WIDTH, WINDOW_HEIGHT, 1};    // 1 : 当前2D纹理只需要一个深度值。
    texDesc.format = wgpu::TextureFormat::Depth24Plus;  // Plus : 至少24位精度的深度缓冲来做标准的Z-buffer尝试测试。具体使用哪个由当前设备硬件决定。
    texDesc.usage = wgpu::TextureUsage::RenderAttachment;
    texDesc.sampleCount = 1;
    texDesc.mipLevelCount = 1;
    texDepth = device.createTexture(texDesc);

    // 创建深度纹理视图
    wgpu::TextureViewDescriptor texViewDesc;
    texViewDesc.label = "Depth texture view";
    texViewDesc.dimension = wgpu::TextureViewDimension::_2D;
    texViewDesc.aspect = wgpu::TextureAspect::DepthOnly;    // 本例中只需要深度值
    // 上述texDesc中 depthOrArrayLayers 配置了这张texture一共有 1 层
    texViewDesc.baseArrayLayer = 0;    // 所以当前textureView使用这共1层中的 第0层(只有1层)。
    texViewDesc.arrayLayerCount = 1;   // 所以配置textureView以baseArrayLayer为起始的共1层，有且只有填 1。
    // MipLevel 指的是纹理的 mipmap 层级。
    // 一个纹理可以有多级分辨率，从原始大小（Level 0）到更小的缩小版（Level 1、Level 2……）。
    texViewDesc.baseMipLevel = 0;      // 深度纹理视图是从 第 0 层 mipmap 开始的，也就是原始大小的那一层。
    // 总共包含多少个mipmap层级，1 就表示只有原始大小level=0这一层。
    texViewDesc.mipLevelCount = 1;     // 所以配置textureView以baseMipLevel为起始的共1层，有且只有填 1。
    texViewDesc.format = wgpu::TextureFormat::Depth24Plus;
    texViewDepth = texDepth.createView(texViewDesc);
}

wgpu::RequiredLimits Application::GetRequiredLimits(wgpu::Adapter adapter) const {
    wgpu::SupportedLimits supportedLimits;
    adapter.getLimits(&supportedLimits);

    wgpu::RequiredLimits requiredLimits = wgpu::Default;
    requiredLimits.limits.maxVertexAttributes = 2;   // position + color : 要两种vertex attribute了
    requiredLimits.limits.maxVertexBuffers = 1;      //  6组{顶点 + color}直接填入一个VertexBuffer，仍然填1
    requiredLimits.limits.maxBufferSize = sizeof(MyUniforms); // 至少要覆盖MyUniforms的大小。
    requiredLimits.limits.maxVertexBufferArrayStride = 6 * sizeof(float); // 步长为2:每个顶点需6个float，即一组(x,y,z) + 一组rgb

    requiredLimits.limits.maxInterStageShaderComponents = 3; // 从顶点着色器转发到片段着色器的数据最多为3个float，即rgb。
    requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;

    // 为uniform 配置limits
    requiredLimits.limits.maxBindGroups = 1;
    requiredLimits.limits.maxUniformBuffersPerShaderStage = 1;
    requiredLimits.limits.maxSampledTexturesPerShaderStage = 1; // 目前片段着色器阶段 只用到 1 个采样纹理 gradientTexture
    requiredLimits.limits.maxUniformBufferBindingSize = sizeof(MyUniforms); // 目前只传递MyUniforms结构体，则maxUniformBufferBindingSize至少要覆盖MyUniforms的大小。
    return requiredLimits;
}


void Application::InitializeBindGroups() {
    std::vector<wgpu::BindGroupEntry> entries(2, wgpu::Default);
    {
        // 对应 @binding(0)，这里不再是解释，而是直接赋值 bufUniform 的作用。
        wgpu::BindGroupEntry& entry = entries[0];
        entry.binding = 0; 
        entry.buffer = bufUniform;
        entry.offset = 0;
        entry.size = sizeof(MyUniforms);
    }
    {
        // 对应 @binding(1)，赋值为 texViewImage 纹理视图
        wgpu::BindGroupEntry& entry = entries[1];
        entry.binding = 1; 
        entry.textureView = texViewImage;
    }

    wgpu::BindGroupDescriptor descBindGroup{};
    descBindGroup.layout = layoutBindGroup;
    descBindGroup.entryCount = 2;
    descBindGroup.entries = entries.data();
    bindGroup = device.createBindGroup(descBindGroup);
}

void Application::InitializeBuffers() {
    // 定义由4个顶点拼成正方形
    std::vector<float> pointData = {
            // x0,  y0
            -1.0, -1.0,
            +1.0, -1.0,
            +1.0, +1.0,
            -1.0, +1.0
    };

    // 定义索引，规则 点数据 如何组成金字塔
    std::vector<uint16_t> indexData = {
            // 确定正方形的面 : 要符合frontFace = wgpu::FrontFace::CCW 逆时针的顶点顺序，才是正面。
            0, 1, 2,  // 右下的三角形
            0, 2, 3   // 左上的三角形
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

    // 创建Uniform buffer
    bufferDesc.size = sizeof(MyUniforms); // uniform buffer的size必须是16 bytes的倍数（这是硬件行为要求的，虽然当前例子只使用一个f32的uniform，会导致余留出空着的多个f32）
    bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bufUniform = device.createBuffer(bufferDesc);
    MyUniforms my; // 先写入一个默认值吧...
    UpdateMyUniforms(my);
    queue.writeBuffer(bufUniform, 0, &my, sizeof(MyUniforms));
}



void Application::UpdateMyUniforms(MyUniforms& my, float ) {
    // 矩阵运算的顺序 与 最终实现的效果顺序是 相反的。
    {// Model
        glm::mat4x4 m(1.0f);
        my.matModel = m;
    }

    {// View : 原则 : 摄像机不动，世界在动；所以实现运算时，数据要取反。
        glm::mat4x4 v(1.0f);
        my.matView = v;
        // my.matView = glm::lookAt(glm::vec3(-0.5f, -2.5f, 2.0f), glm::vec3(0.0f), glm::vec3(0, 0, 1)); // the last argument indicates our Up direction convention
    }
    {// Projection 
        glm::mat4x4 p(1.0f);
        my.matProjection = p;
        // my.matProjection = glm::perspective(45 * PI / 180, 640.0f / 480.0f, 0.01f, 100.0f);
    }
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

    vertexBufferLayout.attributeCount = vertexAttribs.size();    // 1个position Attrib + 1个rgb Attrib
    vertexBufferLayout.attributes = vertexAttribs.data();
    vertexBufferLayout.arrayStride = 2 * sizeof(float);          // 顶点数据 步长为 (xyz) + rgb 共 6 float
    vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = "vs_main";
    pipelineDesc.vertex.constantCount = 0;
    pipelineDesc.vertex.constants = nullptr;

    pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;

    pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;    // 顶点+Indexed 绘制时，顶点顺序要按逆时针方向，才是正面才能被画出来。
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

    // 配置深度测试相关
    // Z-Buffer algorithm : 深度测试算法，是GPU自带硬件/固定功能硬件管线实现的，不是涉及着色器脚本。
    // 所以在pipeline配置中，只需也只能配置以下3个参数即可启用。
    wgpu::DepthStencilState depthState = wgpu::Default;
    // depthWriteEnabled & depthCompare 必须同时配置，才算启用深度测试。
    depthState.depthWriteEnabled = true; // 允许深度缓冲写入深度值
    depthState.depthCompare = wgpu::CompareFunction::Less;  // 越接近Camera，才绘制。因为depth buffer 是 0~1 的值，越小越接近Camera。
    depthState.format = wgpu::TextureFormat::Depth24Plus;
    pipelineDesc.depthStencil = &depthState;

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    // 创建 BindGroupLayoutEntry 
    std::vector<wgpu::BindGroupLayoutEntry> groupEntries(2, wgpu::Default);
    {
        // 对应wgsl中的 @binding(0)，这里最终是解释 layout 的作用
        wgpu::BindGroupLayoutEntry& groupEntry = groupEntries[0];
        groupEntry.binding = 0;
        groupEntry.visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment; // 在顶点着色器阶段能访问这个资源
        groupEntry.buffer.type = wgpu::BufferBindingType::Uniform; // 当前@binding(0)是 Uniform 类型
        groupEntry.buffer.minBindingSize = sizeof(MyUniforms); // 真实的MyUniforms这个struct的size，已经符合：buffer 最小对齐要求：16 byte的倍数
    }
    {
        // 对应wgsl中的 @binding(1), 即gradientTexture
        wgpu::BindGroupLayoutEntry& groupEntry = groupEntries[1];
        groupEntry.binding = 1; 
        groupEntry.visibility = wgpu::ShaderStage::Fragment; // 在片段着色器阶段能访问这个资源
        groupEntry.texture.sampleType = wgpu::TextureSampleType::Float; // 纹理采样类型为 float 类型
        groupEntry.texture.viewDimension = wgpu::TextureViewDimension::_2D; // 2D 纹理
    }

    // 创建 BindGroupLayout ，并带上上述的BindGroupLayoutEntry
    wgpu::BindGroupLayoutDescriptor descGroupLayout{};
    descGroupLayout.entryCount = 2; // 目前 1.有一个 uniform 变量 2.gradientTexture 纹理
    descGroupLayout.entries = groupEntries.data();
    layoutBindGroup = device.createBindGroupLayout(descGroupLayout);

    // 创建 PipelineLayout
    wgpu::PipelineLayoutDescriptor descPipelineLayout{};
    descPipelineLayout.bindGroupLayoutCount = 1;
    descPipelineLayout.bindGroupLayouts = (WGPUBindGroupLayout*)&layoutBindGroup; // 转成C的结构??!!
    layoutPipeline = device.createPipelineLayout(descPipelineLayout);


    pipelineDesc.layout = layoutPipeline;
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
    tvDesc.aspect = wgpu::TextureAspect::All;
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
    cfgSurface.width = WINDOW_WIDTH;
    cfgSurface.height = WINDOW_HEIGHT;
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
    InitializeDepthTexture();
    InitializeImageTexture();
    InitializeBindGroups();

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
    if (bindGroup != nullptr) {
        bindGroup.release();
        bindGroup = nullptr;
    }
    if (layoutPipeline != nullptr) {
        layoutPipeline.release();
        layoutPipeline = nullptr;
    }
    if (layoutBindGroup != nullptr) {
        layoutBindGroup.release();
        layoutBindGroup = nullptr;
    }
    if (bufUniform != nullptr) {
        bufUniform.release();
        bufUniform = nullptr;
    }
    if (bufPoint != nullptr) {
        bufPoint.release();
        bufPoint = nullptr;
    }
    if (bufIndex != nullptr) {
        bufIndex.release();
        bufIndex = nullptr;
    }
    if (texImage != nullptr) {
        texImage.release();
        texImage = nullptr;
    }
    if (texViewImage != nullptr) {
        texViewImage.release();
        texViewImage = nullptr;
    }
    if (texDepth != nullptr) {
        texDepth.release();
        texDepth = nullptr;
    }
    if (texViewDepth != nullptr) {
        texViewDepth.release();
        texViewDepth = nullptr;
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

    // 将时间写入到 uniform buffer 中
    float t = static_cast<float>(glfwGetTime());
    MyUniforms my;
    UpdateMyUniforms(my, t);
    queue.writeBuffer(bufUniform, 0, &my, sizeof(MyUniforms));

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
	colorAttachment.clearValue = wgpu::Color{ 1.0, 0.0, 0.0, 1.0 };
#ifndef WEBGPU_BACKEND_WGPU
	renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif // NOT WEBGPU_BACKEND_WGPU

	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &colorAttachment;

    // 配置深度缓冲附件 : 让渲染通道/renderPass在textureView上操作深度值。
    wgpu::RenderPassDepthStencilAttachment attachDepth = wgpu::Default;
    attachDepth.view = texViewDepth;                // 这次渲染要用这个深度缓冲进行深度测试和写入
    attachDepth.depthLoadOp = wgpu::LoadOp::Clear;  // 渲染开始时，清空深度缓冲，使用depthClearValue进行填充。
    attachDepth.depthClearValue = 1.0f;             // 深度缓冲的初始值，这里设置为1.0f，表示最远距离。
    attachDepth.depthStoreOp = wgpu::StoreOp::Store; // 渲染结束时，将深度缓冲写入到深度纹理中。
    attachDepth.depthReadOnly = false;
    attachDepth.stencilLoadOp = wgpu::LoadOp::Undefined;
    attachDepth.stencilStoreOp = wgpu::StoreOp::Undefined;
    attachDepth.stencilClearValue = 0.0f;
    attachDepth.stencilReadOnly = true;
	renderPassDesc.depthStencilAttachment = &attachDepth;
	renderPassDesc.timestampWrites = nullptr;

	// Create the render pass and end it immediately (we only clear the screen but do not draw anything)
	// WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);  // wgpuRenderPassEncoderRelease
    // wgpuRenderPassEncoderEnd(renderPass);
	// wgpuRenderPassEncoderRelease(renderPass);

	wgpu::RenderPassEncoder renderPass = cmdEncoder.beginRenderPass(renderPassDesc);  // wgpuRenderPassEncoderRelease

    renderPass.setPipeline(pipeline);
    renderPass.setVertexBuffer(0, bufPoint, 0, bufPoint.getSize());
    renderPass.setIndexBuffer(bufIndex, wgpu::IndexFormat::Uint16, 0, bufIndex.getSize());
    renderPass.setBindGroup(0, bindGroup, 0, nullptr); // unfirom buffer 与 bind Group绑定&更新
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
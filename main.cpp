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
    return true;
}
void Application::Terminate() {
}
void Application::MainLoop() {
}
bool Application::IsRunning() {
    return false;
}
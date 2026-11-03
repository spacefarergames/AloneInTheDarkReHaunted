#include "../Native/unityGpuBridge.h"
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

static void require(bool condition, const char* message) { if(!condition)throw std::runtime_error(message); }
int main() {
    std::atomic<bool> returned{false},firstReturned{false};
    uint32_t readbackValue=0;
    std::thread engine([&] {
        bgfx::Init init; init.resolution.width=320;init.resolution.height=200;
        require(bgfx::init(init),"adapter initialization failed");
        auto uniform=bgfx::createUniform("u_shared",bgfx::UniformType::Vec4);
        auto alias=bgfx::createUniform("u_shared",bgfx::UniformType::Vec4);
        require(uniform.idx==alias.idx,"same-name uniforms must share one handle");
        bgfx::destroy(alias);float value[4]={1,2,3,4};bgfx::setUniform(uniform,value);
        auto vs=bgfx::createEmbeddedShader(nullptr,bgfx::RendererType::Direct3D11,"flat_vs");
        auto fs=bgfx::createEmbeddedShader(nullptr,bgfx::RendererType::Direct3D11,"flat_ps");
        auto program=bgfx::createProgram(vs,fs,true);
        bgfx::VertexLayout layout;layout.begin().add(bgfx::Attrib::Position,3,bgfx::AttribType::Float).end();
        bgfx::TransientVertexBuffer buffer;bgfx::allocTransientVertexBuffer(&buffer,3,layout);
        const float vertices[]={0,0,0,320,0,0,0,200,0};std::memcpy(buffer.data,vertices,sizeof(vertices));
        bgfx::setVertexBuffer(0,&buffer);bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::submit(0,program);
        auto ready=bgfx::readTexture(bgfx::TextureHandle{0},&readbackValue);
        require(bgfx::frame()>=ready,"readback fence did not complete with Unity execution");
        require(readbackValue==42,"readback was read before acknowledgement");firstReturned=true;
        bgfx::touch(0);require(bgfx::frame()>=ready,"readback fence did not advance");
        require(readbackValue==42,"readback storage was used before the acknowledgement");returned=true;
    });
    std::vector<uint8_t> packet;
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(std::chrono::steady_clock::now()<deadline) {
        int size=FitdUnity_CopyGpuPacket(nullptr,0);
        if(size<0){packet.resize(size_t(-size));require(FitdUnity_CopyGpuPacket(packet.data(),int(packet.size()))>0,"packet copy failed");break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if(packet.size()<8){FitdUnity_CancelGpuWait();engine.join();throw std::runtime_error("no packet");}
    uint32_t magic,frame;std::memcpy(&magic,packet.data(),4);std::memcpy(&frame,packet.data()+4,4);
    require(magic==0x32475546 && frame==1,"wrong protocol header");
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    require(!firstReturned && !returned,"producer must not run a frame ahead of Unity");
    readbackValue=42;FitdUnity_AcknowledgeGpuPacket(frame);
    packet.clear();
    while(std::chrono::steady_clock::now()<deadline){
        int size=FitdUnity_CopyGpuPacket(nullptr,0);
        if(size<0){packet.resize(size_t(-size));FitdUnity_CopyGpuPacket(packet.data(),int(packet.size()));break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    require(packet.size()>=8 && firstReturned && !returned,"next frame must wait for Unity acknowledgement");
    std::memcpy(&frame,packet.data()+4,4);FitdUnity_AcknowledgeGpuPacket(frame);
    engine.join();require(returned,"engine did not resume after Unity acknowledgement");
    std::thread cancelledProducer([]{bgfx::touch(0);bgfx::frame();});
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while(FitdUnity_CopyGpuPacket(nullptr,0)==0 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    require(FitdUnity_CopyGpuPacket(nullptr,0)<0,"cancellation test did not queue a packet");
    FitdUnity_CancelGpuWait();cancelledProducer.join();
    require(FitdUnity_CopyGpuPacket(nullptr,0)==0,"cancelled packets must never be replayed");
    std::puts("FITD_GPU_BRIDGE_PASS: mesh packet, named shader program, frame acknowledgement, and no GPU device");
}

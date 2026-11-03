// CPU-side adapter for the renderer's bgfx front-end. This translation unit
// replaces the bgfx graphics backend: it never creates a device or a window.
#include "unityGpuBridge.h"
#include "generatedShaderUniforms.h"
#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bgfx/platform.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef FITD_UNITY
void unityCheckStop();
#endif

namespace {
using Bytes = std::vector<uint8_t>;
enum Op : uint16_t { Texture = 1, UpdateTexture, Program, Uniform, FrameBuffer, View, Draw, Blit, DestroyTexture, DestroyFrameBuffer, ReadTexture, ResizeBackbuffer };
template<class T> void put(Bytes& bytes, T value) {
    const auto* start = reinterpret_cast<const uint8_t*>(&value); bytes.insert(bytes.end(), start, start + sizeof(T));
}
void blob(Bytes& bytes, const void* data, size_t size) {
    if (size) { const auto* start = static_cast<const uint8_t*>(data); bytes.insert(bytes.end(), start, start + size); }
}
void string(Bytes& bytes, const std::string& value) {
    if (value.size() > UINT16_MAX) throw std::length_error("GPU bridge string too long");
    put<uint16_t>(bytes, uint16_t(value.size())); blob(bytes, value.data(), value.size());
}
void record(Bytes& stream, Op op, const Bytes& payload) {
    put<uint16_t>(stream, op); put<uint32_t>(stream, uint32_t(payload.size())); blob(stream, payload.data(), payload.size());
}
struct Buffer { Bytes bytes; bgfx::VertexLayout layout; };
struct UniformData { std::string name; bgfx::UniformType::Enum type; uint16_t count; Bytes value; uint32_t references=1; };
struct TextureData { uint16_t w, h; bgfx::TextureFormat::Enum format; };
struct TextureBinding { uint16_t sampler = UINT16_MAX, texture = UINT16_MAX; uint32_t flags = UINT32_MAX; };
struct ViewData {
    uint16_t framebuffer = UINT16_MAX, x = 0, y = 0, w = 960, h = 600, clear = 0;
    uint32_t rgba = 0x000000ff; float depth = 1; uint8_t stencil = 0;
    std::array<float, 16> view{}, projection{};
    ViewData() { for (int i = 0; i < 4; ++i) view[i*5] = projection[i*5] = 1; }
};
std::map<uint16_t, Buffer> vertexBuffers;
std::map<uint16_t, bgfx::VertexLayout> layouts;
std::map<uint16_t, UniformData> uniforms;
std::map<uint16_t, TextureData> textures;
std::map<uint16_t, std::string> shaders;
std::map<uint16_t,const std::unordered_set<std::string>*> programUniforms;
std::map<uint16_t, ViewData> views;
std::map<uint16_t, Bytes> draws;
std::vector<std::unique_ptr<uint8_t[]>> transientStorage;
std::vector<uint16_t> transientHandles;
Bytes resources, releases;
uint16_t nextVertex = 0, nextLayout = 0, nextShader = 0, nextProgram = 0, nextTexture = 0, nextUniform = 0, nextFrameBuffer = 0;
const uint8_t* currentVertices = nullptr;
bgfx::VertexLayout currentLayout;
uint32_t currentVertexCount = 0;
const uint8_t* currentIndices = nullptr;
uint32_t currentIndexCount = 0;
bool index16 = true;
uint64_t currentState = BGFX_STATE_DEFAULT;
uint32_t stencilFront = 0, stencilBack = 0;
std::array<uint16_t, 4> scissor = {0, 0, UINT16_MAX, UINT16_MAX};
std::array<TextureBinding, 16> bindings;
bgfx::Caps capabilities{};
uint32_t frameId = 0;
std::mutex packetMutex;
std::condition_variable packetReady;
std::deque<Bytes> packets;
std::atomic<bool> cancelled{false};
uint32_t acknowledgedFrame = 0;
using Clock=std::chrono::steady_clock;
Clock::time_point previousAck{};
double timingFrames=0,timingWork=0,timingWait=0,timingEncode=0;
double milliseconds(Clock::duration duration){return std::chrono::duration<double,std::milli>(duration).count();}
uint16_t id(uint16_t& counter) {
    if (counter == UINT16_MAX) throw std::runtime_error("Unity renderer resource handle limit reached");
    return counter++;
}
void releaseMemory(const bgfx::Memory* memory) {
    if (memory) { delete[] memory->data; std::free(const_cast<bgfx::Memory*>(memory)); }
}
int components(bgfx::UniformType::Enum type) {
    return type == bgfx::UniformType::Mat4 ? 16 : type == bgfx::UniformType::Mat3 ? 9 : 4;
}
float half(uint16_t value) {
    uint32_t sign = (uint32_t(value & 0x8000)) << 16;
    int exp = (value >> 10) & 31; uint32_t fraction = value & 1023, result;
    if (exp == 0) {
        if (!fraction) result = sign;
        else { exp = -14; while (!(fraction & 1024)) { fraction <<= 1; --exp; } result = sign | uint32_t(exp + 127) << 23 | (fraction & 1023) << 13; }
    } else result = sign | uint32_t(exp == 31 ? 255 : exp + 112) << 23 | fraction << 13;
    float output; std::memcpy(&output, &result, 4); return output;
}
void attribute(float* output, const uint8_t* vertex, bgfx::Attrib::Enum attrib) {
    if (!currentLayout.has(attrib)) return;
    uint8_t count; bgfx::AttribType::Enum type; bool normalized, asInt;
    currentLayout.decode(attrib, count, type, normalized, asInt);
    const uint8_t* data = vertex + currentLayout.getOffset(attrib);
    for (uint8_t i = 0; i < count; ++i) {
        if (type == bgfx::AttribType::Float) std::memcpy(output + i, data + i*4, 4);
        else if (type == bgfx::AttribType::Uint8) output[i] = normalized ? data[i] / 255.f : float(data[i]);
        else if (type == bgfx::AttribType::Int16) { int16_t value; std::memcpy(&value, data + i*2, 2); output[i] = normalized ? std::max(-1.f, value / 32767.f) : float(value); }
        else if (type == bgfx::AttribType::Half) { uint16_t value; std::memcpy(&value, data + i*2, 2); output[i] = half(value); }
        else throw std::runtime_error("Unsupported Unity renderer vertex attribute type");
    }
}
}

FITD_UNITY_API int FitdUnity_CopyGpuPacket(void* destination, int capacity) {
    std::lock_guard<std::mutex> lock(packetMutex);
    if (cancelled.load() || packets.empty()) return 0;
    if (!destination || capacity < int(packets.front().size())) return -int(packets.front().size());
    int size = int(packets.front().size()); std::memcpy(destination, packets.front().data(), size); packets.pop_front();
    packetReady.notify_all(); return size;
}
FITD_UNITY_API void FitdUnity_CancelGpuWait() {
    { std::lock_guard<std::mutex> lock(packetMutex); cancelled = true; packets.clear(); }
    packetReady.notify_all();
}
FITD_UNITY_API void FitdUnity_AcknowledgeGpuPacket(uint32_t frame) {
    { std::lock_guard<std::mutex> lock(packetMutex); acknowledgedFrame = std::max(acknowledgedFrame, frame); }
    packetReady.notify_all();
}

FITD_UNITY_API void FitdUnity_GetGpuTimings(double* output,int count) {
    if(!output || count!=4)return;
    std::lock_guard<std::mutex> lock(packetMutex);output[0]=timingFrames;output[1]=timingWork;output[2]=timingWait;output[3]=timingEncode;
}

namespace bgfx {
PlatformData::PlatformData() : ndt(nullptr), nwh(nullptr), context(nullptr), backBuffer(nullptr), backBufferDS(nullptr), type(NativeWindowHandleType::Default) {}
Resolution::Resolution() : format(TextureFormat::BGRA8), width(960), height(600), reset(0), numBackBuffers(2), maxFrameLatency(1), debugTextScale(1) {}
Init::Limits::Limits() : maxEncoders(1), minResourceCbSize(64*1024), transientVbSize(6<<20), transientIbSize(2<<20) {}
Init::Init() : type(RendererType::Noop), vendorId(0), deviceId(0), capabilities(UINT64_MAX), debug(false), profile(false), callback(nullptr), allocator(nullptr) {}
VertexLayout::VertexLayout() { begin(); }
VertexLayout& VertexLayout::begin(RendererType::Enum) {
    m_stride = 0; m_hash = 0; std::fill(std::begin(m_attributes), std::end(m_attributes), UINT16_MAX); std::fill(std::begin(m_offset), std::end(m_offset), 0); return *this;
}
VertexLayout& VertexLayout::add(Attrib::Enum attrib, uint8_t count, AttribType::Enum type, bool normalized, bool asInt) {
    m_offset[attrib] = m_stride;
    m_attributes[attrib] = uint16_t(count | int(type) << 3 | int(normalized) << 7 | int(asInt) << 8);
    m_stride += count * (type == AttribType::Float ? 4 : type == AttribType::Uint8 ? 1 : 2); return *this;
}
VertexLayout& VertexLayout::skip(uint8_t count) { m_stride += count; return *this; }
void VertexLayout::end() { m_hash = m_stride; for (uint16_t a : m_attributes) m_hash = m_hash * 33 + a; }
void VertexLayout::decode(Attrib::Enum attrib, uint8_t& count, AttribType::Enum& type, bool& normalized, bool& asInt) const {
    uint16_t value = m_attributes[attrib]; count = value & 7; type = AttribType::Enum((value >> 3) & 15); normalized = (value & 128) != 0; asInt = (value & 256) != 0;
}
bool init(const Init& value) {
    capabilities.rendererType = RendererType::Direct3D11;
    capabilities.supported = BGFX_CAPS_TEXTURE_BLIT | BGFX_CAPS_TEXTURE_READ_BACK;
    capabilities.originBottomLeft = false; capabilities.homogeneousDepth = false;
    capabilities.limits.maxViews = 256; capabilities.limits.maxTextureSize = 8192;
    for (auto& format : capabilities.formats) format = BGFX_CAPS_FORMAT_TEXTURE_2D | BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER;
    views[0].w = uint16_t(value.resolution.width); views[0].h = uint16_t(value.resolution.height);
    return true;
}
void shutdown() {}
const Caps* getCaps() { return &capabilities; }
RendererType::Enum getRendererType() { return RendererType::Direct3D11; }
const char* getRendererName(RendererType::Enum) { return "Unity command renderer"; }
void reset(uint32_t w, uint32_t h, uint32_t, TextureFormat::Enum) { views[0].w = uint16_t(w); views[0].h = uint16_t(h); Bytes size; put(size,uint16_t(w)); put(size,uint16_t(h)); record(resources,ResizeBackbuffer,size); }
RenderFrame::Enum renderFrame(int32_t) { return RenderFrame::Render; }
const Memory* alloc(uint32_t size) {
    auto* memory = static_cast<Memory*>(std::malloc(sizeof(Memory)));
    if (!memory) throw std::bad_alloc();
    memory->data = new uint8_t[size]; memory->size = size; return memory;
}
const Memory* copy(const void* data, uint32_t size) { const Memory* memory = alloc(size); if (size) std::memcpy(memory->data, data, size); return memory; }
uint32_t getAvailTransientVertexBuffer(uint32_t count, const VertexLayout&) { return count; }
uint32_t getAvailTransientIndexBuffer(uint32_t count, bool) { return count; }
void allocTransientVertexBuffer(TransientVertexBuffer* buffer, uint32_t count, const VertexLayout& layout) {
    size_t size = size_t(count) * layout.getStride();
    if (size > 64*1024*1024) throw std::length_error("Transient mesh exceeds bridge limit");
    auto storage = std::make_unique<uint8_t[]>(size); buffer->data = storage.get(); buffer->size = uint32_t(size);
    buffer->startVertex = 0; buffer->stride = layout.getStride(); buffer->handle = {UINT16_MAX};
    buffer->layoutHandle = {id(nextLayout)}; layouts[buffer->layoutHandle.idx] = layout;
    transientStorage.push_back(std::move(storage)); transientHandles.push_back(buffer->layoutHandle.idx);
}
void allocTransientIndexBuffer(TransientIndexBuffer* buffer, uint32_t count, bool use32) {
    auto storage = std::make_unique<uint8_t[]>(size_t(count) * (use32 ? 4 : 2));
    buffer->data = storage.get(); buffer->size = count * (use32 ? 4 : 2); buffer->startIndex = 0; buffer->handle = {0}; buffer->isIndex16 = !use32;
    transientStorage.push_back(std::move(storage));
}
VertexBufferHandle createVertexBuffer(const Memory* memory, const VertexLayout& layout, uint16_t) {
    auto handle = VertexBufferHandle{id(nextVertex)};
    vertexBuffers[handle.idx] = {Bytes(memory->data, memory->data + memory->size), layout}; releaseMemory(memory); return handle;
}
TextureHandle createTexture2D(uint16_t w, uint16_t h, bool, uint16_t layers, TextureFormat::Enum format, uint64_t flags, const Memory* memory) {
    if (layers != 1) throw std::runtime_error("Texture arrays are not supported by the FITD Unity adapter");
    auto handle = TextureHandle{id(nextTexture)}; textures[handle.idx] = {w,h,format};
    uint16_t kind;
    switch (format) { case TextureFormat::RGBA8: kind=0; break; case TextureFormat::BGRA8: kind=1; break; case TextureFormat::R8: kind=2; break; case TextureFormat::R8U: kind=3; break; case TextureFormat::D24S8: kind=4; break; default: throw std::runtime_error("Unsupported bridge texture format"); }
    Bytes data; put(data,handle.idx); put(data,w); put(data,h); put(data,kind); put(data,flags);
    put<uint32_t>(data, memory ? memory->size : 0); if (memory) blob(data,memory->data,memory->size);
    record(resources,Texture,data); releaseMemory(memory); return handle;
}
void updateTexture2D(TextureHandle handle, uint16_t, uint8_t, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const Memory* memory, uint16_t pitch) {
    Bytes data; put(data,handle.idx); put(data,x); put(data,y); put(data,w); put(data,h); put(data,pitch); put(data,memory->size); blob(data,memory->data,memory->size);
    record(resources,UpdateTexture,data); releaseMemory(memory);
}
FrameBufferHandle createFrameBuffer(uint8_t count, const TextureHandle* handles, bool) {
    auto handle = FrameBufferHandle{id(nextFrameBuffer)}; Bytes data; put(data,handle.idx); put(data,count);
    for (uint8_t i=0;i<count;++i) put(data,handles[i].idx); record(resources,FrameBuffer,data); return handle;
}
FrameBufferHandle createFrameBuffer(uint8_t count, const Attachment* attachments, bool destroyTextures) {
    std::vector<TextureHandle> handles; for (uint8_t i=0;i<count;++i) handles.push_back(attachments[i].handle); return createFrameBuffer(count,handles.data(),destroyTextures);
}
FrameBufferHandle createFrameBuffer(uint16_t w, uint16_t h, TextureFormat::Enum format, uint64_t flags) {
    TextureHandle texture = createTexture2D(w,h,false,1,format,flags | BGFX_TEXTURE_RT,nullptr); return createFrameBuffer(1,&texture,true);
}
ShaderHandle createShader(const Memory* memory) { auto handle = ShaderHandle{id(nextShader)}; shaders[handle.idx] = "unknown"; releaseMemory(memory); return handle; }
ShaderHandle createEmbeddedShader(const EmbeddedShader*, RendererType::Enum, const char* name) { auto handle=ShaderHandle{id(nextShader)}; shaders[handle.idx]=name; return handle; }
ProgramHandle createProgram(ShaderHandle vs, ShaderHandle fs, bool) {
    auto handle=ProgramHandle{id(nextProgram)};programUniforms[handle.idx]=fitdShaderUniforms(shaders.at(vs.idx),shaders.at(fs.idx)); Bytes data; put(data,handle.idx); string(data,shaders.at(vs.idx)); string(data,shaders.at(fs.idx)); record(resources,Program,data); return handle;
}
UniformHandle createUniform(const char* name, UniformType::Enum type, uint16_t count) {
    for(auto& item:uniforms)if(item.second.name==name) {
        if(item.second.type!=type)throw std::runtime_error("Uniform type mismatch");
        ++item.second.references;item.second.count=std::max(item.second.count,count);return UniformHandle{item.first};
    }
    auto handle=UniformHandle{id(nextUniform)}; uniforms[handle.idx]={name,type,count,{}};
    Bytes data; put(data,handle.idx); string(data,name); put<uint8_t>(data,uint8_t(type)); put(data,count); record(resources,Uniform,data); return handle;
}
void setUniform(UniformHandle handle,const void* value,uint16_t count) {
    auto& uniform=uniforms.at(handle.idx); size_t size=size_t(count)*components(uniform.type)*sizeof(float);
    const auto* data=static_cast<const uint8_t*>(value); uniform.value.assign(data,data+size);
}
void setTexture(uint8_t stage,UniformHandle sampler,TextureHandle texture,uint32_t flags) {
    if (stage >= bindings.size()) throw std::out_of_range("Texture stage"); bindings[stage]={sampler.idx,texture.idx,flags};
}
void setVertexBuffer(uint8_t, const TransientVertexBuffer* buffer,uint32_t first,uint32_t count,VertexLayoutHandle) {
    currentLayout=layouts.at(buffer->layoutHandle.idx); uint32_t available=buffer->size/buffer->stride;
    if (first>available) throw std::out_of_range("Vertex offset");
    currentVertices=buffer->data+size_t(first)*buffer->stride; currentVertexCount=std::min(count,available-first);
}
void setVertexBuffer(uint8_t stream,const TransientVertexBuffer* buffer) { setVertexBuffer(stream,buffer,0,UINT32_MAX,BGFX_INVALID_HANDLE); }
void setVertexBuffer(uint8_t,VertexBufferHandle handle,uint32_t first,uint32_t count,VertexLayoutHandle) {
    const auto& buffer=vertexBuffers.at(handle.idx); currentLayout=buffer.layout; uint32_t available=uint32_t(buffer.bytes.size()/currentLayout.getStride());
    if (first>available) throw std::out_of_range("Vertex offset");
    currentVertices=buffer.bytes.data()+size_t(first)*currentLayout.getStride(); currentVertexCount=std::min(count,available-first);
}
void setVertexBuffer(uint8_t stream,VertexBufferHandle handle) { setVertexBuffer(stream,handle,0,UINT32_MAX,BGFX_INVALID_HANDLE); }
void setIndexBuffer(const TransientIndexBuffer* buffer,uint32_t first,uint32_t count) {
    uint32_t stride=buffer->isIndex16?2:4,available=buffer->size/stride;
    if (first>available) throw std::out_of_range("Index offset");
    currentIndices=buffer->data+size_t(first)*stride; currentIndexCount=std::min(count,available-first); index16=buffer->isIndex16;
}
void setIndexBuffer(const TransientIndexBuffer* buffer) { setIndexBuffer(buffer,0,UINT32_MAX); }
void setState(uint64_t state,uint32_t) { currentState=state; }
void setStencil(uint32_t front,uint32_t back) { stencilFront=front; stencilBack=back; }
uint16_t setScissor(uint16_t x,uint16_t y,uint16_t w,uint16_t h) { scissor={x,y,w,h}; return 0; }
void setScissor(uint16_t) { scissor={0,0,UINT16_MAX,UINT16_MAX}; }
void setViewName(ViewId,const char*,int32_t) {}
void setViewMode(ViewId,ViewMode::Enum) {}
void setViewFrameBuffer(ViewId view,FrameBufferHandle framebuffer) { views[view].framebuffer=framebuffer.idx; }
void setViewRect(ViewId view,uint16_t x,uint16_t y,uint16_t w,uint16_t h) { auto& item=views[view]; item.x=x;item.y=y;item.w=w;item.h=h; }
void setViewClear(ViewId view,uint16_t flags,uint32_t rgba,float depth,uint8_t stencil) { auto& item=views[view];item.clear=flags;item.rgba=rgba;item.depth=depth;item.stencil=stencil; }
void setViewTransform(ViewId view,const void* model,const void* projection) {
    auto& item=views[view]; ViewData identity;
    if (model) std::memcpy(item.view.data(),model,64); else item.view=identity.view;
    if (projection) std::memcpy(item.projection.data(),projection,64); else item.projection=identity.projection;
}
void touch(ViewId view) { draws[view]; }
void submit(ViewId view,ProgramHandle program,uint32_t,uint8_t flags) {
    if (!currentVertices || !currentVertexCount) throw std::runtime_error("Draw has no mesh");
    Bytes data;data.reserve(30+size_t(currentVertexCount)*80+size_t(currentIndexCount)*4+1024);
    const auto* used=programUniforms[program.idx];
    auto visible=[used](const UniformData& uniform){return !uniform.value.empty() && (!used || used->count(uniform.name)!=0);};
    put(data,program.idx);put(data,currentState);put(data,stencilFront);put(data,stencilBack);
    for(auto value:scissor) put(data,value);
    put(data,currentVertexCount);
    for(uint32_t i=0;i<currentVertexCount;++i) {
        float values[20]={0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,1,0};
        const uint8_t* vertex=currentVertices+size_t(i)*currentLayout.getStride();
        attribute(values,vertex,Attrib::Position);attribute(values+4,vertex,Attrib::Color0);attribute(values+8,vertex,Attrib::TexCoord0);
        attribute(values+12,vertex,Attrib::TexCoord1);attribute(values+16,vertex,Attrib::Color1); blob(data,values,sizeof(values));
    }
    put(data,currentIndexCount);
    for(uint32_t i=0;i<currentIndexCount;++i) { uint32_t value=0;if(index16){uint16_t index;std::memcpy(&index,currentIndices+i*2,2);value=index;}else std::memcpy(&value,currentIndices+i*4,4);if(value>=currentVertexCount)throw std::out_of_range("Mesh index exceeds vertex count");put(data,value); }
    uint16_t count=0;for(const auto& uniform:uniforms) if(visible(uniform.second))++count;put(data,count);
    for(const auto& uniform:uniforms) if(visible(uniform.second)) { put(data,uniform.first);put<uint32_t>(data,uint32_t(uniform.second.value.size()));blob(data,uniform.second.value.data(),uniform.second.value.size()); }
    put<uint8_t>(data,uint8_t(bindings.size()));for(const auto& binding:bindings) { put(data,binding.sampler);put(data,binding.texture);put(data,binding.flags); }
    record(draws[view],Draw,data);
    if(flags & BGFX_DISCARD_BINDINGS)bindings.fill(TextureBinding{});
    if(flags & BGFX_DISCARD_INDEX_BUFFER) {currentIndices=nullptr;currentIndexCount=0;}
    if(flags & BGFX_DISCARD_VERTEX_STREAMS) {currentVertices=nullptr;currentVertexCount=0;}
    if(flags & BGFX_DISCARD_STATE) {currentState=BGFX_STATE_DEFAULT;stencilFront=stencilBack=0;scissor={0,0,UINT16_MAX,UINT16_MAX};}
}
void blit(ViewId view,TextureHandle destination,uint16_t dx,uint16_t dy,TextureHandle source,uint16_t sx,uint16_t sy,uint16_t w,uint16_t h) {
    Bytes data;put(data,destination.idx);put(data,source.idx);put(data,dx);put(data,dy);put(data,sx);put(data,sy);put(data,w);put(data,h);record(draws[view],Blit,data);
}
uint32_t readTexture(TextureHandle texture,void* destination,uint8_t) {
    Bytes data;put(data,texture.idx);put<uint64_t>(data,uint64_t(reinterpret_cast<uintptr_t>(destination)));record(releases,ReadTexture,data);return frameId+1;
}
uint32_t frame(bool) {
    auto started=Clock::now();double work=previousAck==Clock::time_point{}?0:milliseconds(started-previousAck);
    Bytes packet;put<uint32_t>(packet,0x32475546);put(packet,++frameId);blob(packet,resources.data(),resources.size());resources.clear();
    for(const auto& view:draws) {
        const auto& item=views[view.first]; Bytes data;put(data,view.first);put(data,item.framebuffer);put(data,item.x);put(data,item.y);put(data,item.w);put(data,item.h);
        put(data,item.clear);put(data,item.rgba);put(data,item.depth);put(data,item.stencil);blob(data,item.view.data(),64);blob(data,item.projection.data(),64);record(packet,View,data);
        blob(packet,view.second.data(),view.second.size());
    }
    blob(packet,releases.data(),releases.size());releases.clear();draws.clear();transientStorage.clear();
    for(auto handle:transientHandles) layouts.erase(handle);transientHandles.clear();
    // Transient handles are only packet-local. Persistent mesh handles remain unique.
    nextLayout=0;
    auto queued=Clock::now();
    std::unique_lock<std::mutex> lock(packetMutex);packetReady.wait(lock,[]{return packets.size()<2 || cancelled.load();});
    if(!cancelled) {
        packets.push_back(std::move(packet));
        // Complete this packet before sampling input for the next gameplay frame.
        // No additional frame of stale input is kept ahead of Unity presentation.
        packetReady.wait(lock,[]{return acknowledgedFrame >= frameId || cancelled.load();});
    }
#ifdef FITD_UNITY
    // Cancellation must unwind gameplay, never report an unfinished readback as ready.
    if(cancelled.load()){lock.unlock();unityCheckStop();}
#endif
    previousAck=Clock::now();++timingFrames;timingWork+=work;timingWait+=milliseconds(previousAck-queued);timingEncode+=milliseconds(queued-started);
    return frameId;
}
void requestScreenShot(FrameBufferHandle,const char*) {}
void destroy(TextureHandle handle) {Bytes data;put(data,handle.idx);record(releases,DestroyTexture,data);textures.erase(handle.idx);}
void destroy(FrameBufferHandle handle) {Bytes data;put(data,handle.idx);record(releases,DestroyFrameBuffer,data);}
void destroy(UniformHandle handle) {auto found=uniforms.find(handle.idx);if(found!=uniforms.end() && --found->second.references==0)uniforms.erase(found); }
void destroy(VertexBufferHandle handle) { vertexBuffers.erase(handle.idx); }
void destroy(ShaderHandle handle) { shaders.erase(handle.idx); }
void destroy(ProgramHandle handle) {programUniforms.erase(handle.idx);}
void setName(ShaderHandle handle,const char* name,int32_t) {shaders[handle.idx]=name;}
void setName(TextureHandle,const char*,int32_t) {}
void setName(FrameBufferHandle,const char*,int32_t) {}
void setName(VertexBufferHandle,const char*,int32_t) {}
}

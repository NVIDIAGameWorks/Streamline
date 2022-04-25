/*
* Copyright (c) 2022 NVIDIA CORPORATION. All rights reserved
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "include/sl.h"
#include "source/core/sl.extra/extra.h"

namespace sl
{

namespace param
{
struct IParameters;
}

namespace chi
{
typedef uint32_t NativeFormat;

using Device = void*;
using Resource = void*;
using ResourceView = void*;
using Kernel = size_t;
using CommandList = void*;
using CommandQueue = void*;
using CommandAllocator = void*;
using PipelineState = void*;
using SwapChain = void*;
using Handle = void*;
using Output = void*;

constexpr uint32_t kAllSubResources = 0xffffffff;

#define SL_SAFE_RELEASE(a) if(a) { a->Release(); a = nullptr;}
#define MAX_NUM_NODES 2

// Do not enable this in production
#ifdef SL_PRODUCTION
#define SL_ENABLE_PERF_TIMING 0
#else
#define SL_ENABLE_PERF_TIMING 1
#endif

#define SL_READBACK_QUEUE_SIZE 3

constexpr uint32_t NativeFormatUnknown = 0; // matching DXGI_FORMAT_UNKNOWN

enum Format
{
    eFormatINVALID,
    eFormatRGBA32F,
    eFormatRGBA16F,
    eFormatRGB32F,
    eFormatRGB16F,
    eFormatRG16F,
    eFormatR16F,
    eFormatRG32F,
    eFormatR32F,
    eFormatR8UN,
    eFormatRG8UN,
    eFormatRGB11F,
    eFormatRGBA8UN,
    eFormatSRGBA8UN,
    eFormatBGRA8UN,
    eFormatSBGRA8UN,
    eFormatRG16UI,
    eFormatRG16SI,
    eFormatE5M3,
    eFormatRGB10A2UN,
    eFormatR8UI,
    eFormatR16UI,
    eFormatRG16UN,
    eFormatR32UI,
    eFormatRG32UI,
    eFormatD32S32,
    eFormatD24S8,
    eFormatCOUNT,
};

enum Sampler
{
    eSamplerLinearClamp,
    eSamplerLinearMirror,
    eSamplerAnisoClamp,
    eSamplerPointClamp,
    eSamplerPointMirror,
    eSamplerCount
};

enum PlatformType
{
    ePlatformTypeD3D11,
    ePlatformTypeD3D12,
    ePlatformTypeVK,
    ePlatformTypeCUDA,
    ePlatformTypeUMD_D3D11, // driver back-end for dx11
    ePlatformTypeUMD_D3D12, // driver back-end for dx12
};

enum HeapType
{
    eHeapTypeDefault = 1,
    eHeapTypeUpload = 2,
    eHeapTypeReadback = 3
};

enum BarrierType
{
    eBarrierTypeUAV
};

enum class CommandQueueType
{
    eGraphics,
    eCompute,
    eCopy
};

enum class ResourceState : uint32_t
{
    eUnknown = 0,
    eGeneral = 1 << 0,
    eVertexBuffer = 1 << 1,
    eIndexBuffer = 1 << 2,
    eConstantBuffer = 1 << 3,
    eArgumentBuffer = 1 << 4,
    eTextureRead = 1 << 5,
    eStorageRead = 1 << 6,
    eStorageWrite = 1 << 7,
    eStorageRW = eStorageRead | eStorageWrite,
    eColorAttachmentRead = 1 << 8,
    eColorAttachmentWrite = 1 << 9,
    eColorAttachmentRW = eColorAttachmentRead | eColorAttachmentWrite,
    eDepthStencilAttachmentWrite = 1 << 10,
    eDepthStencilAttachmentRead = 1 << 11,
    eCopySource = 1 << 12,
    eCopyDestination = 1 << 13,
    eAccelStructRead = 1 << 14,
    eAccelStructWrite = 1 << 15,
    eResolveSource = 1 << 16,
    eResolveDestination = 1 << 17,
    ePresent = 1 << 18,
    eGenericRead = 1 << 19
};

inline ResourceState operator|(ResourceState a, ResourceState b)
{
    return (ResourceState)((uint32_t)a | (uint32_t)b);
}

inline ResourceState operator|=(ResourceState& a, ResourceState b)
{
    a = (ResourceState)((uint32_t)a | (uint32_t)b);
    return a;
}

inline ResourceState operator&=(ResourceState& a, ResourceState b)
{
    a = (ResourceState)((uint32_t)a & (uint32_t)b);
    return a;
}

inline bool operator&(ResourceState a, ResourceState b)
{
    return ((uint32_t)a & (uint32_t)b) != 0;
}

inline ResourceState operator~(ResourceState a)
{
    return (ResourceState)~((uint32_t)a);
}

enum class ResourceFlags : uint32_t
{
    eNone = 0,
    // Texture specific
    eRowMajorLayout = 1 << 0,
    eShaderResource = 1 << 1,
    eShaderResourceStorage = 1 << 2,
    eColorAttachment = 1 << 3,
    eDepthStencilAttachment = 1 << 4,
    // Buffer specific
    eRawOrStructuredBuffer = 1 << 5,
    eVertexBuffer = 1 << 6,
    eIndexBuffer = 1 << 7,
    eConstantBuffer = 1 << 8,
    eArgumentBuffer = 1 << 9,
    eAccelStruct = 1 << 10,
    eShaderBindingTable = 1 << 11,
    eCount
};

inline bool operator&(ResourceFlags a, ResourceFlags b)
{
    return ((uint32_t)a & (uint32_t)b) != 0;
}

inline ResourceFlags operator&=(ResourceFlags& a, ResourceFlags b)
{
    a = (ResourceFlags)((uint32_t)a & (uint32_t)b);
    return a;
}

inline ResourceFlags operator|(ResourceFlags a, ResourceFlags b)
{
    return (ResourceFlags)((uint32_t)a | (uint32_t)b);
}

inline ResourceFlags& operator |= (ResourceFlags& lhs, ResourceFlags rhs)
{
    lhs = (ResourceFlags)((int)lhs | (int)rhs);
    return lhs;
}

inline ResourceFlags operator~(ResourceFlags a)
{
    return (ResourceFlags)~((uint32_t)a);
}

struct CommonThreadContext
{
    
};

struct ResourceTransition
{
    ResourceTransition() {};
    ResourceTransition(Resource r, ResourceState t, ResourceState f = ResourceState::eUnknown, uint32_t sr = kAllSubResources) : resource(r), to(t), from(f), subresource(sr) {};
    Resource resource = {};
    ResourceState to = {};
    ResourceState from = ResourceState::eGeneral; // figured out internally
    uint32_t subresource = kAllSubResources;
    inline bool operator==(const ResourceTransition& rhs) const { return resource == rhs.resource && to == rhs.to && from == rhs.from && subresource == rhs.subresource; }
};

struct ResourceDescription
{
    ResourceDescription() {};
    ResourceDescription(uint32_t w, uint32_t h, uint32_t f, uint32_t mps, ResourceState s = ResourceState::eUnknown) : 
        width(w), height(h), nativeFormat(f), mips(mps), state(s) 
    {
        updateStateAndFlags();        
    };
    ResourceDescription(uint32_t w, uint32_t h, uint32_t f, HeapType ht = eHeapTypeDefault, ResourceState s = ResourceState::eUnknown, ResourceFlags fl = ResourceFlags::eShaderResourceStorage) :
        width(w), height(h), nativeFormat(f), heapType(ht), state(s), flags(fl)
    {
        updateStateAndFlags();
    };
    ResourceDescription(uint32_t w, uint32_t h, Format f, HeapType ht = eHeapTypeDefault, ResourceState s = ResourceState::eUnknown, ResourceFlags fl = ResourceFlags::eShaderResourceStorage) : 
        width(w), height(h), format(f), heapType(ht), state(s), flags(fl)
    {
        updateStateAndFlags();
    };

    inline void updateStateAndFlags()
    {
        if (state == ResourceState::eUnknown)
        {
            switch (heapType)
            {
                case eHeapTypeUpload:
                    state = ResourceState::eGenericRead;
                    break;
                case eHeapTypeReadback:
                    state = ResourceState::eCopyDestination;
                    break;
                case eHeapTypeDefault:
                    state = ResourceState::eCopyDestination;
            }
        }
    }

    inline bool operator==(const ResourceDescription& rhs) const { return !memcmp(this, &rhs, sizeof(ResourceDescription)); }
    inline bool operator!=(const ResourceDescription& rhs) const { return !operator==(rhs); }

    uint32_t width;
    uint32_t height;
    uint32_t nativeFormat = NativeFormatUnknown;
    Format format = eFormatINVALID;
    uint32_t mips = 1;
    HeapType heapType = eHeapTypeDefault;
    uint32_t creationMask = 1;
    uint32_t visibilityMask = 0;
    ResourceState state = ResourceState::eUnknown;
    uint64_t gpuVirtualAddress = 0;
    ResourceFlags flags = ResourceFlags::eNone;
};

struct ResourceInfo
{
    ResourceInfo() {};
    ResourceInfo(const ResourceInfo& other) { operator=(other); }
    inline ResourceInfo& operator=(const ResourceInfo& rhs)
    {
        desc = rhs.desc;
        memory = rhs.memory;
        return *this;
    }

    ResourceDescription desc{};
    void* memory{};
};

struct Coordinates
{
    int x;
    int y;

    template<typename T> Coordinates(T X, T Y) : x(X), y(Y) {}
};

struct Dimensions
{
    int width;
    int height;

    template<typename T> Dimensions(T w, T h) : width(w), height(h) {}
};

struct ResourceArea
{
    Resource    resource;
    Coordinates base;
    Dimensions  dimensions;
};

enum FlushType
{
    eDefault,
    ePrevious,
    eCurrent,
    eCount
};

struct ICommandListContext
{
    virtual uint32_t getBufferCount() = 0;
    virtual uint32_t getCurrentBufferIndex() = 0;
    virtual uint32_t acquireNextBufferIndex(SwapChain chain) = 0;
    virtual bool beginCommandList() = 0;
    virtual void executeCommandList() = 0;
    virtual bool flushAll() = 0;
    virtual void waitOnGPUForTheOtherQueue(const ICommandListContext* other) = 0;
    virtual bool waitForCommandList(FlushType ft = FlushType::eDefault) = 0;
    virtual bool didFrameFinish(uint32_t index) = 0;
    virtual CommandList getCmdList() = 0;
    virtual CommandQueue getCmdQueue() = 0;
    virtual CommandAllocator getCmdAllocator() = 0;
    virtual Handle getFenceEvent() = 0;
    virtual bool present(SwapChain chain, uint32_t sync, uint32_t flags) = 0;
};

// Common functions like begin/end event or anything else that just needs command list
using PFunCommonCmdList = void(CommandList pCmdList);
using PFunGetThreadContext = CommonThreadContext*(void);

// padding are bytes not covered by 2D resource (appears due to block-linear memory structure)
enum CLEAR_TYPE { CLEAR_UNDEFINED, CLEAR_ZBC_WITH_PADDING, CLEAR_ZBC_WITHOUT_PADDING, CLEAR_NON_ZBC };

enum ComputeStatus
{
    eComputeStatusOk,
    eComputeStatusError,
    eComputeStatusNoImplementation,
    eComputeStatusInvalidArgument,
    eComputeStatusInvalidPointer,
    eComputeStatusNotSupported,
    eComputeStatusInvalidCall,
    eComputeStatusCount,
};

#define CHI_VALIDATE(f) {auto r = f; if(r != sl::chi::eComputeStatusOk) { SL_LOG_ERROR("%s failed", #f); } };
#define CHI_CHECK(f) {auto _r = f;if(_r != sl::chi::eComputeStatusOk) { SL_LOG_ERROR("%s failed",#f); return _r;}};
#define CHI_CHECK_RF(f) {auto _r = f;if(_r != sl::chi::eComputeStatusOk) { SL_LOG_ERROR("%s failed",#f); return false;}};
#define CHI_CHECK_RV(f) {auto _r = f;if(_r != sl::chi::eComputeStatusOk) { SL_LOG_ERROR("%s failed",#f); return;}};
#define NVAPI_CHECK(f) {auto r = f; if(r != NVAPI_OK) { SL_LOG_ERROR("%s failed error %d", #f, r); return sl::chi::eComputeStatusError;} };

class ICompute
{
public:
    
    virtual ComputeStatus init(Device device, param::IParameters* params) = 0;
    virtual ComputeStatus shutdown() = 0;

    virtual ComputeStatus getPlatformType(PlatformType &type) = 0;

    //! To trigger immediate resource release call pass UINT_MAX
    virtual ComputeStatus collectGarbage(uint32_t finishedFrame) = 0;

    virtual ComputeStatus getFinishedFrameIndex(uint32_t& index) = 0;

    virtual ComputeStatus getNativeResourceState(ResourceState state, uint32_t& nativeState) = 0;
    virtual ComputeStatus getResourceState(uint32_t nativeState, ResourceState& state) = 0;

    virtual ComputeStatus createKernel(void *blob, uint32_t blobSize, const char* fileName, const char* entryPoint, Kernel &OutKernel) = 0;
    virtual ComputeStatus createBuffer(const ResourceDescription &createResourceDesc, Resource &outResource, const char friendlyName[] = "") = 0;
    virtual ComputeStatus createTexture2D(const ResourceDescription &createResourceDesc, Resource &outResource, const char friendlyName[] = "") = 0;

    virtual ComputeStatus setCallbacks(pfunResourceAllocateCallback allocate, pfunResourceReleaseCallback release, PFunGetThreadContext getThreadContext) = 0;

    //! NOTE: All destroy methods are delayed by few frames
    //! 
    //! To trigger immediate resource release call collectGarbage(UINT_MAX) 
    virtual ComputeStatus destroyKernel(Kernel& kernel) = 0;
    virtual ComputeStatus destroyResource(Resource& resource) = 0;
    virtual ComputeStatus destroy(std::function<void(void)> task) = 0;

    virtual ComputeStatus createCommandQueue(CommandQueueType type, CommandQueue& queue, const char friendlyName[] = "", uint32_t index = 0) = 0;
    virtual ComputeStatus destroyCommandQueue(CommandQueue& queue) = 0;

    virtual ComputeStatus createCommandListContext(CommandQueue queue, uint32_t count, ICommandListContext*& ctx, const char friendlyName[] = "") = 0;
    virtual ComputeStatus destroyCommandListContext(ICommandListContext* ctx) = 0;

    virtual ComputeStatus pushState(CommandList cmdList) = 0;
    virtual ComputeStatus popState(CommandList cmdList) = 0;

    virtual ComputeStatus getNativeFormat(Format format, NativeFormat& native) = 0;
    virtual ComputeStatus getFormat(NativeFormat native, Format& format) = 0;

    //! NOTE: SL compute interface uses implicit dispatch for simplicity.
    //! 
    //! Root signatures, constant updates, pipeline states etc. are all
    //! managed automatically for convenience.
    //!
    virtual ComputeStatus bindSharedState(CommandList cmdList, uint32_t node = 0) = 0;
    virtual ComputeStatus bindKernel(const Kernel kernel) = 0;
    virtual ComputeStatus bindSampler(uint32_t binding, uint32_t reg, Sampler sampler) = 0;
    virtual ComputeStatus bindConsts(uint32_t binding, uint32_t reg, void *data, size_t dataSize, uint32_t instances) = 0;
    virtual ComputeStatus bindTexture(uint32_t binding, uint32_t reg, Resource resource, uint32_t mipOffset = 0, uint32_t mipLevels = 0) = 0;
    virtual ComputeStatus bindRWTexture(uint32_t binding, uint32_t reg, Resource resource, uint32_t mipOffset = 0) = 0;
    virtual ComputeStatus bindRawBuffer(uint32_t binding, uint32_t reg, Resource resource) = 0;
    virtual ComputeStatus dispatch(uint32_t blockX, uint32_t blockY, uint32_t blockZ = 1) = 0;
    
    // Hooks up back to the SL command list to restore its state
    virtual ComputeStatus restorePipeline(CommandList cmdList) = 0;

    virtual ComputeStatus insertGPUBarrier(CommandList cmdList, Resource resource, BarrierType barrierType = eBarrierTypeUAV) = 0;
    virtual ComputeStatus insertGPUBarrierList(CommandList cmdList, const Resource* resources, uint32_t resourceCount, BarrierType barrierType = eBarrierTypeUAV) = 0;
    virtual ComputeStatus transitionResources(CommandList cmdList, const ResourceTransition* transitions, uint32_t count, extra::ScopedTasks* tasks = nullptr) = 0;
    
    virtual ComputeStatus setResourceState(Resource resource, ResourceState state, uint32_t subresource = kAllSubResources) = 0;
    virtual ComputeStatus getResourceState(Resource resource, ResourceState& state) = 0;

    virtual ComputeStatus onHostResourceCreated(Resource resource, const ResourceInfo& info) = 0;
    virtual ComputeStatus onHostResourceDestroyed(Resource resource) = 0;

    virtual ComputeStatus copyResource(CommandList cmdList, Resource dstResource, Resource srcResource) = 0;
    virtual ComputeStatus cloneResource(Resource resource, Resource &outResource, const char friendlyName[] = "", ResourceState initialState = ResourceState::eCopyDestination, uint32_t creationMask = 0, uint32_t visibilityMask = 0) = 0;
    virtual ComputeStatus copyBufferToReadbackBuffer(CommandList cmdList, Resource source, Resource destination, uint32_t bytesToCopy) = 0;
    virtual ComputeStatus copyHostToDeviceBuffer(CommandList cmdList, uint64_t size, const void *data, Resource uploadResource, Resource targetResource, unsigned long long uploadOffset = 0, unsigned long long dstOffset = 0) = 0;
    virtual ComputeStatus copyHostToDeviceTexture(CommandList cmdList, uint64_t size, uint64_t rowPitch, const void* data, Resource targetResource, Resource& uploadResource) = 0;

    virtual ComputeStatus getResourceDescription(Resource resource, ResourceDescription &OutDesc) = 0;

    virtual ComputeStatus clearView(CommandList cmdList, Resource resource, const float4 Color, const RECT * pRect, uint32_t NumRects, CLEAR_TYPE &outType) = 0;    
    virtual ComputeStatus renderText(CommandList cmdList, int x, int y, const char *text, const ResourceArea &area, const float4& color = { 1.0f, 1.0f, 1.0f, 0.0f }, int reverseX = 0, int reverseY = 0) = 0;
    
    virtual ComputeStatus getAllocatedBytes(unsigned long long &bytes) const = 0;

    virtual ComputeStatus dumpResource(CommandList cmdList, Resource src, const char *path) = 0;

    virtual ComputeStatus setDebugName(Resource res, const char friendlyName[]) = 0;
    virtual ComputeStatus getDebugName(Resource res, std::wstring& name) = 0;

    virtual ComputeStatus setFullscreenState(SwapChain chain, bool fullscreen, Output out = nullptr) = 0;
    virtual ComputeStatus getRefreshRate(SwapChain chain, float& refreshRate) = 0;
    virtual ComputeStatus getSwapChainBuffer(SwapChain chain, uint32_t index, Resource& buffer) = 0;
        
    virtual ComputeStatus beginPerfSection(CommandList cmdList, const char *section, uint32_t node = 0, bool reset = false) = 0;
    virtual ComputeStatus endPerfSection(CommandList cmdList, const char *section, float &avgTimeMS, uint32_t node = 0) = 0;
#if SL_ENABLE_PERF_TIMING
    virtual ComputeStatus beginProfiling(CommandList cmdList, uint32_t metadata, const void *data, uint32_t size) = 0;
    virtual ComputeStatus endProfiling(CommandList cmdList) = 0;
#endif
};

ICompute* getD3D11();
ICompute *getD3D12();

}
}

/*
* Copyright (c) 2022-2023 NVIDIA CORPORATION. All rights reserved
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
#include "include/sl_reflex.h"
#include "source/core/sl.extra/extra.h"

#if defined(SL_DEBUG)
#define ASSERT_ONLY_CODE 1
#endif

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
using PhysicalDevice = void*;
using Instance = void*;
using Resource = sl::Resource*;
using SubresourceRange = sl::SubresourceRange*;
using ResourceView = void*;
using Kernel = size_t;
using CommandList = void*;
using CommandQueue = void*;
using CommandAllocator = void*;
using PipelineState = void*;
using SwapChain = void*;
using Fence = void*;
using Handle = void*;
using Output = void*;

constexpr uint32_t kAllSubResources = 0xffffffff;
constexpr uint64_t kBinarySemaphoreValue = 0xcafec0de;
constexpr const char* kGlobalVRAMSegment = "global";

#define SL_SAFE_RELEASE(a) if(a) { ((IUnknown*)a)->Release(); a = nullptr;}
#define MAX_NUM_NODES 2

#define SL_READBACK_QUEUE_SIZE 3

enum OutOfBandCommandQueueType
{
    eOutOfBandRender = 0,
    eOutOfBandPresent = 1,
};

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
    eFormatD32S8U,
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
    eCopy,
    eOpticalFlow
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
    eDepthStencilAttachmentRead = 1 << 11,
    eDepthStencilAttachmentWrite = 1 << 10,
    eDepthStencilAttachmentRW = eDepthStencilAttachmentRead | eDepthStencilAttachmentWrite,
    eCopySource = 1 << 12,
    eCopyDestination = 1 << 13,
    eAccelStructRead = 1 << 14,
    eAccelStructWrite = 1 << 15,
    eResolveSource = 1 << 16,
    eResolveDestination = 1 << 17,
    ePresent = 1 << 18,
    eGenericRead = 1 << 19,
    eUndefined = 1 << 20
};

SL_ENUM_OPERATORS_32(ResourceState)

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
    // Misc
    eSharedResource = 1 << 12,
    eCount
};

enum CmdListPipeType
{
    eCmdListGraphics = 0,
    eCmdListCompute = 1
};

SL_ENUM_OPERATORS_32(ResourceFlags)

struct CommonThreadContext
{
    
};

struct ResourceTransition
{
    ResourceTransition() {};
    ResourceTransition(Resource r, ResourceState t, ResourceState f = ResourceState::eUnknown, uint32_t sr = kAllSubResources) : resource(r), to(t), from(f), subresource(sr) {};
    ResourceTransition(Resource r, ResourceState t, uint32_t f, uint32_t sr = kAllSubResources) : resource(r), to(t), fromNativeState(f), subresource(sr) {};
    Resource resource{};
    ResourceState to{};
    ResourceState from = ResourceState::eUnknown; // figured out internally
    uint32_t fromNativeState{};
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

    uint32_t width{};
    uint32_t height{};
    uint32_t nativeFormat = NativeFormatUnknown;
    Format format = eFormatINVALID;
    uint32_t mips = 1;
    uint32_t depth = 1;
    HeapType heapType = eHeapTypeDefault;
    uint32_t creationMask = 1;
    uint32_t visibilityMask = 0;
    ResourceState state = ResourceState::eUnknown;
    uint64_t gpuVirtualAddress = 0;
    ResourceFlags flags = ResourceFlags::eNone;
    std::string sName;
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

struct ResourceFootprint
{
    Format format{};
    uint32_t width{};
    uint32_t height{};
    uint32_t depth{};
    uint32_t rowPitch{};
    uint64_t offset{};
    uint32_t numRows{};
    uint64_t rowSizeInBytes{};
    uint64_t totalBytes{};
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
    eCurrent
};

struct GPUSyncInfo
{
    GPUSyncInfo() {};
    GPUSyncInfo(const GPUSyncInfo&) = delete;
    std::vector<Fence> waitSemaphores{};
    std::vector<uint64_t> waitValues{};
    std::vector<Fence> signalSemaphores{};
    std::vector<uint64_t> signalValues{};
    chi::Fence fence{};
    bool signalPresentSemaphore{};
    bool useEmptyCmdBuffer = true;
};

struct TranslatedResource
{
    TranslatedResource() {};
    TranslatedResource(chi::Resource r) : source(r), translated(r), handle(nullptr), clone(nullptr) {};

    inline operator bool() const { return source && source->native != nullptr; }
    inline operator bool() { return source && source->native != nullptr; }
    inline operator chi::Resource () { return translated; };

    chi::Resource source{};     // incoming resource
    chi::Resource translated{}; // resource to use (could be the same as source or shared)
    chi::Handle handle{};       // NT handle
    chi::Resource clone{};      // null or clone if format cannot be shared as NT handle so copy is needed
};

struct TranslatedFence : TranslatedResource
{
    TranslatedFence() {};
    TranslatedFence(chi::Fence fence) { sourceFence = { ResourceType::eFence, fence }; translated = &sourceFence; }

    inline operator chi::Fence() { return translated->native; };

    sl::Resource sourceFence{};     // incoming fence
};

struct SyncPoint
{
    Fence semaphore{};
    uint64_t value{};
};

enum class WaitStatus
{
    eNoTimeout,
    eTimeout,
    eError,
};

struct DebugInfo
{
    DebugInfo(const char* sFile, uint32_t uLine) : m_sFile(sFile), m_uLine(uLine) { }
    DebugInfo() : m_sFile("NO_FILE") { }
    const char* m_sFile = nullptr;
    uint32_t m_uLine = 0;
};

struct ICommandListContext
{
    virtual RenderAPI getType() = 0;
    virtual uint32_t getPrevCommandListIndex() = 0;
    virtual uint32_t getCurrentCommandListIndex() = 0;
    virtual uint64_t getSyncValueAtIndex(uint32_t idx) = 0;
    virtual SyncPoint getSyncPointAtIndex(uint32_t idx) = 0;
    virtual Fence getNextVkAcquireFence() = 0;
    virtual int acquireNextBufferIndex(SwapChain chain, uint32_t& index, Fence* waitSemaphore = nullptr) = 0;
    virtual bool isCommandListRecording() = 0;
    virtual bool beginCommandList() = 0;
    virtual bool executeCommandList(const GPUSyncInfo* info = nullptr) = 0;
    virtual WaitStatus flushAll() = 0;
    virtual void syncGPU(const GPUSyncInfo* info) = 0;
    virtual void waitOnGPUForTheOtherQueue(const ICommandListContext* other, uint32_t clIndex,
        uint64_t syncValue, const DebugInfo &debugInfo) = 0;
    virtual WaitStatus waitCPUFence(Fence fence, uint64_t syncValue) = 0;
    virtual void waitGPUFence(Fence fence, uint64_t syncValue, const DebugInfo &debugInfo) = 0;
    virtual bool signalGPUFence(Fence fence, uint64_t syncValue) = 0;
    virtual bool signalGPUFenceAt(uint32_t index) = 0;
    virtual WaitStatus waitForCommandList(FlushType ft = FlushType::eDefault) = 0;
    virtual uint64_t getCompletedValue(Fence fence) = 0;
    virtual bool didCommandListFinish(uint32_t index) = 0;
    virtual WaitStatus waitForCommandListToFinish(uint32_t index) = 0;
    virtual CommandList getCmdList() = 0;
    virtual CommandQueue getCmdQueue() = 0;
    virtual CommandAllocator getCmdAllocator() = 0;
    virtual Handle getFenceEvent() = 0;
    virtual Fence getFence(uint32_t index) = 0;
    virtual int present(SwapChain chain, uint32_t sync, uint32_t flags, void* params = nullptr) = 0;
    virtual void getFrameStats(SwapChain chain, void* frameStats) = 0;
    virtual void getLastPresentID(SwapChain chain, uint32_t& id) = 0;
    virtual void waitForVblank(SwapChain chain) = 0;
};

// HashedResource uses std::shared_ptr<> to keep track of references to the underlying
// HashedResourceData object. As soon as nobody references the HashedResourceData, the
// destructor for it would be called, which will use the cached m_pCompute pointer to
// release the underlying chi::Resource
struct HashedResourceData
{
    HashedResourceData(sl::Resource* pResource, class ICompute* pCompute, bool bOwnResource);
    ~HashedResourceData();
    uint64_t hash{};
    ResourceState state{};
    Resource resource{};
private:
    HashedResourceData(const HashedResourceData&) = delete;
    void operator=(const HashedResourceData&) = delete;
    ICompute* m_pCompute{};
    bool m_bOwnResource = false; // if true, we will call destroyResource() in destructor
    IUnknown* m_pNative = nullptr;
};
struct HashedResource
{
    HashedResource(uint64_t hash, ResourceState state, Resource resource, ICompute* pCompute, bool bOwnResource)
    {
        if (resource)
        {
		    assert(resource->native);
            m_p = std::make_shared<HashedResourceData>(resource, pCompute, bOwnResource);
            m_p->hash = hash;
            m_p->state = state;
        }
    }
    HashedResource(uint64_t hash, Resource resource, ICompute* pCompute, bool bOwnResource);
    HashedResource() { }

    inline operator bool() const { return m_p != nullptr; }
    inline bool operator ==(const HashedResource& other) const { return m_p == other.m_p; }
    inline operator Resource() const { return m_p ? m_p->resource : nullptr; }
    ResourceState &accessState() { return m_p ? m_p->state : s_invalidState; }
    ResourceState getState() const { return m_p ? m_p->state : ResourceState::eUnknown; }
    uint64_t& accessHash() { return m_p ? m_p->hash : s_invalidHash; }
    void* getNative() const { return m_p ? m_p->resource->native : nullptr; }
#if ASSERT_ONLY_CODE
    bool dbgIsCorrupted() const
    {
        // The value 0xDD is used by the Visual C++ debugger to fill memory that has been freed or
        // released. This is done to help identify memory leaks and other memory - related errors.
        return m_p ? (uint64_t &)m_p->resource->native == 0xdddddddddddddddd : false;
    }
#endif

private:
    std::shared_ptr<HashedResourceData> m_p;
    static ResourceState s_invalidState;
    static uint64_t s_invalidHash;
};

struct IResourcePool
{
    virtual void setMaxQueueSize(size_t maxSize) = 0;
    virtual HashedResource allocate(Resource source, const char* debugName, ResourceState initialState = ResourceState::eCopyDestination) = 0;
    virtual void recycle(HashedResource res) = 0;
    virtual void clear() = 0;
    virtual void collectGarbage(float deltaMs = 10000.0f) = 0;
};

// Common functions
using PFun_GetThreadContext = CommonThreadContext*(void);

// padding are bytes not covered by 2D resource (appears due to block-linear memory structure)
enum CLEAR_TYPE { CLEAR_UNDEFINED, CLEAR_ZBC_WITH_PADDING, CLEAR_ZBC_WITHOUT_PADDING, CLEAR_NON_ZBC };

enum ComputeStatus
{
    eOk,
    eError,
    eNoImplementation,
    eInvalidArgument,
    eInvalidPointer,
    eNotSupported,
    eInvalidCall,
    eNotReady,
    eCount,
};

enum class VendorId : uint32_t
{
    eMS = 0x1414, // Software Render Adapter
    eNVDA = 0x10DE,
    eAMD = 0x1002,
    eIntel = 0x8086,
};

enum FenceFlags : uint32_t
{
    eFenceFlagsNone = 0,
    eFenceFlagsShared = 0x1,
    eFenceFlagsSharedAcrossAdapter = 0x2,
    eFenceFlagsNonMonitored = 0x4
};

SL_ENUM_OPERATORS_32(FenceFlags)

inline const char* getComputeStatusAsStr(ComputeStatus status)
{
    switch (status)
    {
        case ComputeStatus::eOk: return "Ok";
        case ComputeStatus::eError: return "Error";
        case ComputeStatus::eNoImplementation: return "NoImplementation";
        case ComputeStatus::eInvalidArgument: return "InvalidArgument";
        case ComputeStatus::eInvalidPointer: return "InvalidPointer";
        case ComputeStatus::eNotSupported: return "NotSupported";
        case ComputeStatus::eInvalidCall: return "InvalidCall";
        case ComputeStatus::eNotReady: return "NotReady";
        default: return "Unknown";
    }
}

#define CHI_VALIDATE(f) {auto r = f; if(r != sl::chi::ComputeStatus::eOk) { SL_LOG_ERROR("%s failed %d (%s)", #f, r, getComputeStatusAsStr(r)); } };
#define CHI_CHECK(f) {auto _r = f;if(_r != sl::chi::ComputeStatus::eOk) { SL_LOG_ERROR("%s failed %d (%s)", #f, _r, getComputeStatusAsStr(_r)); return _r;}};
#define CHI_CHECK_RF(f) {auto _r = f;if(_r != sl::chi::ComputeStatus::eOk) { SL_LOG_ERROR("%s failed %d (%s)", #f, _r, getComputeStatusAsStr(_r)); return false;}};
#define CHI_CHECK_RV(f) {auto _r = f;if(_r != sl::chi::ComputeStatus::eOk) { SL_LOG_ERROR("%s failed %d (%s)", #f, _r, getComputeStatusAsStr(_r)); return;}};
#define CHI_CHECK_RR(f) {auto _r = f;if(_r != sl::chi::ComputeStatus::eOk) { SL_LOG_ERROR("%s failed %d (%s)", #f, _r, getComputeStatusAsStr(_r)); return sl::Result::eErrorComputeFailed;}};
#define NVAPI_CHECK(f) {auto r = f; if(r != NVAPI_OK) { SL_LOG_ERROR_ONCE( "%s failed error %d", #f, r); return sl::chi::ComputeStatus::eError;} };

class ICompute
{
public:
    
    virtual ComputeStatus init(Device device, param::IParameters* params) = 0;
    virtual ComputeStatus shutdown() = 0;

    virtual ComputeStatus getDevice(Device& device) = 0;
    virtual ComputeStatus getInstance(Instance& instance) = 0;
    virtual ComputeStatus getPhysicalDevice(PhysicalDevice& device) = 0;
    virtual ComputeStatus getHostQueueInfo(chi::CommandQueue queue, void* pQueueInfo) = 0;

    virtual ComputeStatus waitForIdle(Device device) = 0;

    virtual ComputeStatus clearCache() = 0;

    virtual ComputeStatus getVendorId(VendorId& id) = 0;

    virtual ComputeStatus getRenderAPI(RenderAPI &type) = 0;

    //! To trigger immediate resource release call pass UINT_MAX
    virtual ComputeStatus collectGarbage(uint32_t finishedFrame) = 0;

    virtual ComputeStatus getFinishedFrameIndex(uint32_t& index) = 0;

    virtual ComputeStatus getNativeResourceState(ResourceState state, uint32_t& nativeState) = 0;
    virtual ComputeStatus getResourceState(uint32_t nativeState, ResourceState& state) = 0;
    virtual ComputeStatus getBarrierResourceState(uint32_t barrierType, ResourceState& state) = 0;

    virtual ComputeStatus createKernel(void *blob, uint32_t blobSize, const char* fileName, const char* entryPoint, Kernel &OutKernel) = 0;
    virtual ComputeStatus createBuffer(const ResourceDescription &createResourceDesc, Resource &outResource, const char friendlyName[] = "") = 0;
    virtual ComputeStatus createTexture2D(const ResourceDescription &createResourceDesc, Resource &outResource, const char friendlyName[] = "") = 0;
    virtual ComputeStatus createFence(FenceFlags flags, uint64_t initialValue, Fence& outFence, const char friendlyName[] = "") = 0;

    virtual ComputeStatus setCallbacks(PFun_ResourceAllocateCallback allocate, PFun_ResourceReleaseCallback release, PFun_GetThreadContext getThreadContext) = 0;

    virtual ComputeStatus destroyKernel(Kernel& kernel) = 0;
    virtual ComputeStatus destroyFence(Fence& fence) = 0;
    //! NOTE: Resource destroy methods by default are delayed by 3 frames
    //! 
    //! To trigger immediate resource release set frameDelay to 0
    //! To trigger immediate release of ALL resources call collectGarbage(UINT_MAX) 
    virtual ComputeStatus destroyResource(Resource resource, uint32_t frameDelay = 3) = 0;
    virtual ComputeStatus destroy(std::function<void(void)> task, uint32_t frameDelay = 3) = 0;

    virtual ComputeStatus createCommandQueue(CommandQueueType type, CommandQueue& queue, const char friendlyName[] = "", uint32_t index = 0) = 0;
    virtual ComputeStatus destroyCommandQueue(CommandQueue& queue) = 0;

    virtual ComputeStatus createCommandListContext(CommandQueue queue, uint32_t count, ICommandListContext*& ctx, const char friendlyName[] = "") = 0;
    virtual ComputeStatus destroyCommandListContext(ICommandListContext* ctx) = 0;

    virtual ComputeStatus pushState(CommandList cmdList) = 0;
    virtual ComputeStatus popState(CommandList cmdList) = 0;

    virtual ComputeStatus getNativeFormat(Format format, NativeFormat& native) = 0;
    virtual ComputeStatus getFormat(NativeFormat native, Format& format) = 0;
    virtual ComputeStatus getFormatAsString(const Format format, std::string& name) = 0;
    virtual ComputeStatus getBytesPerPixel(Format format, size_t& size) = 0;

    //! NOTE: SL compute interface uses implicit dispatch for simplicity.
    //! 
    //! Root signatures, constant updates, pipeline states etc. are all
    //! managed automatically for convenience.
    //!
    //! IMPORTANT: Constants are stored in a circular buffer of 'instances' size 
    //! so make sure to allocate enough space to avoid cpu/gpu race conditions.
    //! 
    //! Easiest approach is to use triple-buffering so num_dispatch_calls_per_frame * 3 (or in other words num_viewports * 3)
    //!
    virtual ComputeStatus bindSharedState(CommandList cmdList, uint32_t node = 0) = 0;
    virtual ComputeStatus bindKernel(const Kernel kernel) = 0;
    virtual ComputeStatus bindSampler(uint32_t binding, uint32_t reg, Sampler sampler) = 0;
    virtual ComputeStatus bindConsts(uint32_t binding, uint32_t reg, void *data, size_t dataSize, uint32_t instances) = 0;
    virtual ComputeStatus bindTexture(uint32_t binding, uint32_t reg, Resource resource, uint32_t mipOffset = 0, uint32_t mipLevels = 0) = 0;
    virtual ComputeStatus bindRWTexture(uint32_t binding, uint32_t reg, Resource resource, uint32_t mipOffset = 0) = 0;
    virtual ComputeStatus bindRawBuffer(uint32_t binding, uint32_t reg, Resource resource) = 0;
    virtual ComputeStatus dispatch(uint32_t blockX, uint32_t blockY, uint32_t blockZ = 1) = 0;
    
    virtual ComputeStatus startTrackingResource(uint64_t uid, Resource resource) = 0;
    virtual ComputeStatus stopTrackingResource(uint64_t uid, Resource dbgResource) = 0;

    // Hooks up back to the SL command list to restore its state
    virtual ComputeStatus restorePipeline(CommandList cmdList) = 0;

    virtual ComputeStatus insertGPUBarrier(CommandList cmdList, Resource resource, BarrierType barrierType = eBarrierTypeUAV) = 0;
    virtual ComputeStatus insertGPUBarrierList(CommandList cmdList, const Resource* resources, uint32_t resourceCount, BarrierType barrierType = eBarrierTypeUAV) = 0;
    virtual ComputeStatus transitionResources(CommandList cmdList, const ResourceTransition* transitions, uint32_t count, extra::ScopedTasks* tasks = nullptr) = 0;
    
    virtual ComputeStatus getResourceState(Resource resource, ResourceState& state) = 0;

    virtual ComputeStatus copyResource(CommandList cmdList, Resource dstResource, Resource srcResource) = 0;
    virtual ComputeStatus cloneResource(Resource resource, Resource &outResource, const char friendlyName[] = "", ResourceState initialState = ResourceState::eCopyDestination, uint32_t creationMask = 0, uint32_t visibilityMask = 0) = 0;
    virtual ComputeStatus copyBufferToReadbackBuffer(CommandList cmdList, Resource source, Resource destination, uint32_t bytesToCopy) = 0;
    virtual ComputeStatus copyHostToDeviceBuffer(CommandList cmdList, uint64_t size, const void *data, Resource uploadResource, Resource targetResource, uint64_t uploadOffset = 0, uint64_t dstOffset = 0) = 0;
    virtual ComputeStatus copyHostToDeviceTexture(CommandList cmdList, uint64_t size, uint64_t rowPitch, const void* data, Resource targetResource, Resource& uploadResource) = 0;
    virtual ComputeStatus copyDeviceTextureToDeviceBuffer(CommandList cmdList, Resource srcTexture, Resource dstBuffer) = 0;

    virtual ComputeStatus mapResource(CommandList cmdList, Resource resource, void*& data, uint32_t subResource = 0, uint64_t offset = 0, uint64_t totalBytes = UINT64_MAX) = 0;
    virtual ComputeStatus unmapResource(CommandList cmdList, Resource resource, uint32_t subResource = 0) = 0;

    virtual ComputeStatus getResourceDescription(Resource resource, ResourceDescription &OutDesc) = 0;
    virtual ComputeStatus getResourceFootprint(Resource resoruce, ResourceFootprint& footprint) = 0;

    virtual ComputeStatus clearView(CommandList cmdList, Resource resource, const float4 Color, const RECT * pRect, uint32_t NumRects, CLEAR_TYPE &outType) = 0;    

    virtual ComputeStatus beginVRAMSegment(const char* name) = 0;
    virtual ComputeStatus endVRAMSegment() = 0;
    virtual ComputeStatus getAllocatedBytes(uint64_t& bytes, const char* name = kGlobalVRAMSegment) = 0;
    virtual ComputeStatus setVRAMBudget(uint64_t currentUsageBytes, uint64_t budgetBytes) = 0;
    virtual ComputeStatus getVRAMBudget(uint64_t& availableBytes) = 0;

    virtual ComputeStatus setDebugName(Resource res, const char friendlyName[]) = 0;
    virtual ComputeStatus getDebugName(Resource res, std::wstring& name) = 0;

    virtual ComputeStatus getFullscreenState(SwapChain chain, bool& fullscreen) = 0;
    virtual ComputeStatus setFullscreenState(SwapChain chain, bool fullscreen, Output out = nullptr) = 0;
    virtual ComputeStatus getRefreshRate(SwapChain chain, float& refreshRate) = 0;
    virtual ComputeStatus getSwapChainBuffer(SwapChain chain, uint32_t index, Resource& buffer) = 0;
        
    virtual ComputeStatus beginPerfSection(CommandList cmdList, const char *section, uint32_t node = 0, bool reset = false) = 0;
    virtual ComputeStatus endPerfSection(CommandList cmdList, const char *section, float &avgTimeMS, uint32_t node = 0) = 0;
    virtual ComputeStatus beginProfiling(CommandList cmdList, uint32_t metadata, const char* marker) = 0;
    virtual ComputeStatus endProfiling(CommandList cmdList) = 0;
    virtual ComputeStatus beginProfilingQueue(CommandQueue cmdQueue, uint32_t metadata, const char* marker) = 0;
    virtual ComputeStatus endProfilingQueue(CommandQueue cmdQueue) = 0;

    // Latency API
    virtual ComputeStatus setSleepMode(const ReflexOptions& consts) = 0;
    virtual ComputeStatus getSleepStatus(ReflexState& settings) = 0;
    virtual ComputeStatus getLatencyReport(ReflexState& settings) = 0;
    virtual ComputeStatus sleep() = 0;
    virtual ComputeStatus setReflexMarker(PCLMarker marker, uint64_t frameId) = 0;
    virtual ComputeStatus notifyOutOfBandCommandQueue(CommandQueue queue, OutOfBandCommandQueueType type) = 0;
    virtual ComputeStatus setAsyncFrameMarker(CommandQueue queue, PCLMarker marker, uint64_t frameId) = 0;
    
    // Sharing API
    virtual ComputeStatus fetchTranslatedResourceFromCache(ICompute* otherAPI, ResourceType type, Resource res, TranslatedResource& shared, const char friendlyName[] = "") = 0;
    virtual ComputeStatus prepareTranslatedResources(CommandList cmdList, const std::vector<std::pair<chi::TranslatedResource, chi::ResourceDescription>>& resourceList) = 0;

    // Resource pool
    virtual ComputeStatus createResourcePool(IResourcePool** pool, const char* vramSegment) = 0;
    virtual ComputeStatus destroyResourcePool(IResourcePool* pool) = 0;

    // OFA
    virtual ComputeStatus isNativeOpticalFlowSupported() = 0;
};

ICompute* getD3D11();
ICompute *getD3D12();
ICompute *getVulkan();

inline HashedResourceData::HashedResourceData(sl::Resource *pResource, ICompute* pCompute, bool bOwnResource) :
    resource(pResource),
    m_pCompute(pCompute),
    m_bOwnResource(bOwnResource)
{
    assert(m_pCompute && resource && resource->native);
    RenderAPI api = RenderAPI::eD3D12;
    m_pCompute->getRenderAPI(api);
    if (api != RenderAPI::eVulkan)
    {
        // if we don't own the resource pointer - someone may delete it from under us. so
        // cache the pointer to native IUnknown here. that way we will be able to Release()
        // it in ~HashedResourceData()
        m_pNative = ((IUnknown*)resource->native);
        m_pNative->AddRef();
    }
}
inline HashedResourceData::~HashedResourceData()
{
    if (m_pNative)
    {
        m_pNative->Release();
    }
    if (m_bOwnResource)
    {
        assert(m_pCompute && resource && resource->native);
        m_pCompute->destroyResource(resource, 0);
    }
}
inline HashedResource::HashedResource(uint64_t hash, Resource resource, ICompute* pCompute, bool bOwnResource)
{
    if (resource)
    {
        assert(resource->native && pCompute);
        m_p = std::make_shared<HashedResourceData>(resource, pCompute, bOwnResource);
        m_p->hash = hash;
        pCompute->getResourceState(resource, m_p->state);
    }
}


}
}

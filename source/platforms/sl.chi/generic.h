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

#include <chrono>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>

#include "source/platforms/sl.chi/compute.h"

typedef LUID    NVSDK_NGX_LUID;

#include <atomic>
#include <utility>
#include <cassert>

namespace sl
{

namespace chi
{

template <class T>
inline void hash_combine(std::size_t & s, const T & v)
{
    std::hash<T> h;
    s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

struct KernelDataBase
{
    size_t hash = {};
    std::string name = {};
    std::string entryPoint = {};
    std::vector<uint8_t> kernelBlob = {};
    std::atomic<uint32_t> refcounter = 0;
};

struct TimestampedResource
{
    //! Need to compare the native pointers here, not the resource itself which encapsulates extra info and could
    //! be different while still pointing to the same underlying native interface.
    inline bool operator==(const TimestampedResource& rhs) const { return resource->native == rhs.resource->native; }
    Resource resource;
    uint32_t frame;
    uint32_t frameDelay;
};

struct TimestampedLambda
{
    TimestampedLambda() {};
    TimestampedLambda(std::function<void(void)> t, uint32_t f, uint32_t fd) : task(t), frame(f), frameDelay(fd) {};
    TimestampedLambda(const TimestampedLambda& rhs) { operator=(rhs); }

    TimestampedLambda& operator=(const TimestampedLambda& rhs)
    {
        task = rhs.task;
        frame = rhs.frame;
        frameDelay = rhs.frameDelay;
        return *this;
    }

    std::function<void(void)> task;
    uint32_t frame;
    uint32_t frameDelay;
};

enum class VRAMOperation
{
    eAlloc,
    eFree,

    eCount
};

void* nsightSecureLoadLibraryCallback(const wchar_t* libName);

class Generic : public ICompute
{
protected:
    using KernelMap = std::map<Kernel, KernelDataBase*>;
    KernelMap m_kernels = {};

    bool m_nsightInitialized{ false };

    std::atomic<uint32_t> m_finishedFrame = 0;

    Device m_typelessDevice{};

    RenderAPI m_platform{};

    param::IParameters* m_parameters = {};

    using ResourceList = std::vector<Resource>;
    using TimestampedResourceList = std::vector<TimestampedResource>;
    using TimestampedLambdaList = std::vector<TimestampedLambda>;

    TimestampedResourceList m_resourcesToDestroy = {};
    TimestampedLambdaList m_destroyWithLambdas = {};

    std::mutex m_mutexKernel;
    std::mutex m_mutexProfiler;
    std::mutex m_mutexResource;
    std::mutex m_mutexDynamicText;
    std::mutex m_mutexResourceTrack;
    std::mutex m_mutexVRAM;

    std::atomic<uint64_t> m_vramBudgetBytes{};
    std::atomic<uint64_t> m_vramUsageBytes{};

    using ResourceTrackingMap = std::map<uint64_t, IUnknown*>;
    ResourceTrackingMap m_resourceTrackMap{};
    // frame-aware tracking of resources tagged using frame-based resource tagging APIs
    std::map<uint32_t, ResourceTrackingMap> m_frameResourceTrackingMap{};

    PFun_ResourceAllocateCallback* m_allocateCallback = {};
    PFun_ResourceReleaseCallback* m_releaseCallback = {};
    PFun_GetThreadContext* m_getThreadContext = {};

    bool m_bFastUAVClearSupported = false;
    PreferenceFlags m_preferenceFlags{};

    struct VRAMSegment
    {
        uint64_t allocCount{};
        uint64_t totalAllocatedSize{};
    };
    std::map<std::string, VRAMSegment> m_vramSegments{};
    std::map<std::thread::id, std::string> m_currentVRAMSegment{};

    std::map<void*, TranslatedResource> m_sharedResourceMap{};

    virtual int destroyResourceDeferredImpl(const Resource InResource) = 0;
    virtual ComputeStatus createBufferResourceImpl(ResourceDescription &InOutResourceDesc, Resource &OutResource, ResourceState InitialState, const char InFriendlyName[]) = 0;
    virtual ComputeStatus createTexture2DResourceSharedImpl(ResourceDescription &InOutResourceDesc, Resource &OutResource, bool UseNativeFormat, ResourceState InitialState, const char InFriendlyName[]) = 0;
    virtual ComputeStatus insertGPUBarrierList(CommandList cmdList, const Resource* InResources, unsigned int InResourceCount, BarrierType InBarrierType = eBarrierTypeUAV) override;
    virtual ComputeStatus transitionResourceImpl(CommandList cmdList, const ResourceTransition *transisitions, uint32_t count) = 0;

    virtual ComputeStatus beginVRAMSegment(const char* name) override final;
    virtual ComputeStatus endVRAMSegment() override final;
    virtual ComputeStatus getAllocatedBytes(uint64_t& bytes, const char* name = kGlobalVRAMSegment) override;

    virtual std::wstring getDebugName(Resource res) = 0;

    ComputeStatus createTexture2DResourceShared(const ResourceDescription& CreateResourceDesc, Resource& OutResource, bool UseNativeFormat, const char InFriendlyName[]);
    ComputeStatus genericPostInit();

    bool savePFM(const std::string &path, const char* srcBuffer, const int width, const int height);
    uint64_t getResourceSize(Resource res);

    void setResourceTracked(chi::Resource resource, uint64_t tracked);
    bool isResourceTracked(chi::Resource resource);

    VRAMSegment manageVRAM(Resource res, VRAMOperation op);

#if SL_ENABLE_PROFILING
    ComputeStatus beginProfilingImpl(CommandList cmdList, const char* marker, uint8_t r, uint8_t g, uint8_t b) override { return ComputeStatus::eNoImplementation; }
    ComputeStatus endProfilingImpl(CommandList cmdList) override { return ComputeStatus::eNoImplementation; }
    ComputeStatus beginProfilingQueueImpl(CommandQueue cmdQueue, const char* marker, uint8_t r, uint8_t g, uint8_t b) override { return ComputeStatus::eNoImplementation; }
    ComputeStatus endProfilingQueueImpl(CommandQueue cmdQueue) override { return ComputeStatus::eNoImplementation; }
#endif

public:

    virtual ComputeStatus init(Device InDevice, param::IParameters* params);
    virtual ComputeStatus shutdown();

    virtual ComputeStatus getDevice(Device& device) override { device = m_typelessDevice; return ComputeStatus::eOk; }

    //! The following methods are VK specific so by default no implementation
    virtual ComputeStatus getInstance(Instance& instance)  override { return ComputeStatus::eNoImplementation; };
    virtual ComputeStatus getPhysicalDevice(PhysicalDevice& device)  override { return ComputeStatus::eNoImplementation; };
    virtual ComputeStatus getHostQueueInfo(chi::CommandQueue queue, void* pQueueInfo) override { return ComputeStatus::eOk; }

    virtual ComputeStatus waitForIdle(Device device) override
    {
        // This is a Vulkan-only operation. It must not be exersiced in other code paths. Even in Vulkan
        // it's a dubious operation. We have it only because Vulkan spec has it and the games may call
        // it. Why it's dubious? See - we have multiple threads. Those threads keep submitting work.
        // If the game calls https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkDeviceWaitIdle.html
        // that function has to be aware of all threads, and has to have a way to flush their work.
        // So it's quite finicky to make that function work correctly. Let's limit its use as much as possible
        assert(false);
        return ComputeStatus::eNoImplementation;
    };

    virtual ComputeStatus getVendorId(VendorId& id) override;
    virtual ComputeStatus clearCache() override;

    virtual ComputeStatus getNativeResourceState(ResourceState state, uint32_t& nativeState) override { return ComputeStatus::eNoImplementation; };
    virtual ComputeStatus getResourceState(uint32_t nativeState, ResourceState& state) override { return ComputeStatus::eNoImplementation; };
    virtual ComputeStatus getBarrierResourceState(uint32_t barrierType, ResourceState& state)  override { return ComputeStatus::eNoImplementation; };
    virtual ComputeStatus getResourceFootprint(Resource resoruce, ResourceFootprint& footprint) override;

    virtual ComputeStatus getFinishedFrameIndex(uint32_t& index) final override { index = m_finishedFrame; return ComputeStatus::eOk; };

    virtual ComputeStatus createCommandListContext(ChiCommandQueue* queue,
                                                   uint32_t count,
                                                   ICommandListContext*& ctx,
                                                   const char friendlyName[]) override
    {
        return ComputeStatus::eNoImplementation;
    }
    virtual ComputeStatus destroyCommandListContext(ICommandListContext* ctx) override { return ComputeStatus::eNoImplementation; }

    virtual uint64_t getCompletedValue(Fence fence) override { /* not implemented */ assert(false); return 0; }
    virtual WaitStatus waitCPUFence(Fence fence, uint64_t syncValue) override { /* not implemented */ assert(false); return WaitStatus::eError; }

    virtual ComputeStatus createCommandQueue(CommandQueueType type,
                                             ChiCommandQueue*& queue,
                                             const char friendlyName[],
                                             uint32_t index) override
    {
        return ComputeStatus::eNoImplementation;
    }

    virtual ComputeStatus destroyCommandQueue(ChiCommandQueue* queue) override
    {
        return ComputeStatus::eNoImplementation;
    }
    virtual ComputeStatus createFence(FenceFlags flags, uint64_t initialValue, Fence& outFence, const char friendlyName[] = "")  override { return ComputeStatus::eNoImplementation; }
    virtual ComputeStatus destroyFence(Fence& fence) override { SL_SAFE_RELEASE(fence); return ComputeStatus::eOk; }
    virtual ComputeStatus getDebugName(Resource res, std::wstring& name) { name = getDebugName(res); return ComputeStatus::eOk; }
    virtual ComputeStatus setDebugName(Resource res, const char friendlyName[]) override { return ComputeStatus::eNoImplementation; }
        
    virtual ComputeStatus getRefreshRate(WindowHandle window, float& refreshRate) override;
    virtual ComputeStatus getDisplayId(WindowHandle window, uint32_t& displayId) override;
    virtual ComputeStatus getSwapChainBuffer(SwapChain chain, uint32_t index, Resource& buffer) override { return ComputeStatus::eNoImplementation; }

    virtual ComputeStatus pushState(CommandList cmdList) override { return ComputeStatus::eOk; }
    virtual ComputeStatus popState(CommandList cmdList) override { return ComputeStatus::eOk; }

    virtual ComputeStatus getNativeFormat(Format format, NativeFormat& native) override;
    virtual ComputeStatus getFormat(NativeFormat native, Format& format) override;
    virtual ComputeStatus getFormatAsString(const Format format, std::string& name) override final;

    virtual ComputeStatus getBytesPerPixel(Format format, size_t& size) override;
    virtual ComputeStatus destroyResource(Resource InResource, uint32_t frameDelay = 3) override final;
    virtual ComputeStatus destroy(std::function<void(void)> task, uint32_t frameDelay = 3) override final;
        
    virtual ComputeStatus setVRAMBudget(uint64_t currentUsageBytes, uint64_t budgetBytes) override final { m_vramBudgetBytes.store(budgetBytes); m_vramUsageBytes.store(currentUsageBytes); return ComputeStatus::eOk; }
    virtual ComputeStatus getVRAMBudget(uint64_t& totalBytes)  override final 
    { 
        if(m_vramBudgetBytes.load() == 0) return ComputeStatus::eNotReady;
        totalBytes = m_vramBudgetBytes.load() > m_vramUsageBytes.load() ? m_vramBudgetBytes.load() - m_vramUsageBytes.load() : 0; 
        return ComputeStatus::eOk; 
    }

    virtual ComputeStatus collectGarbage(uint32_t frame);

    ComputeStatus createBuffer(const ResourceDescription &CreateResourceDesc, Resource &OutResource, const char InFriendlyName[]) override final;
    ComputeStatus createTexture2D(const ResourceDescription &CreateResourceDesc, Resource &OutResource, const char InFriendlyName[]) override final;
        

    ComputeStatus setCallbacks(PFun_ResourceAllocateCallback allocate, PFun_ResourceReleaseCallback release, PFun_GetThreadContext getThreadContext) override final
    {
        m_allocateCallback = allocate;
        m_releaseCallback = release;
        m_getThreadContext = getThreadContext;
        return ComputeStatus::eOk;
    }

    virtual ComputeStatus startTrackingResource(uint64_t uid, Resource resource) override;
    virtual ComputeStatus startTrackingResource(uint32_t frameId, uint64_t uid, Resource resource) override;
    virtual ComputeStatus stopTrackingResource(uint64_t uid, Resource dbgResource) override;
    virtual ComputeStatus stopTrackingResource(uint32_t frameId, uint64_t uid, Resource dbgResource) override;

    ComputeStatus restorePipeline(CommandList cmdList)  override { return ComputeStatus::eOk; }

    ComputeStatus transitionResources(CommandList cmdList, const ResourceTransition* transitions, uint32_t count, extra::ScopedTasks* tasks = nullptr) override;
    ComputeStatus getResourceState(Resource resource, ResourceState& state) override;
    ComputeStatus copyResource(CommandList cmdList, Resource dstResource, Resource srcResource) override { return ComputeStatus::eNoImplementation; }
    ComputeStatus cloneResource(Resource inResource, Resource &outResource, const char friendlyName[], ResourceState initialState, unsigned int creationMask, unsigned int visibilityMask) override { return ComputeStatus::eNoImplementation; }
    ComputeStatus copyDeviceTextureToDeviceBuffer(CommandList cmdList, Resource srcTexture, Resource dstBuffer) override { return ComputeStatus::eNoImplementation; }

    ComputeStatus getResourceDescription(Resource InResource, ResourceDescription &OutDesc) override { return ComputeStatus::eNoImplementation; }
    
    ComputeStatus getFullscreenState(SwapChain chain, bool& fullscreen) override { return ComputeStatus::eNoImplementation; };
    ComputeStatus setFullscreenState(SwapChain chain, bool fullscreen, Output out = nullptr) override { return ComputeStatus::eNoImplementation; }

    virtual bool signalCPUFence(Fence fence, uint64_t syncValue) = 0;

    virtual ComputeStatus setSleepMode(const ReflexOptions& consts) override;
    virtual ComputeStatus getSleepStatus(ReflexState& settings) override;
    virtual ComputeStatus getLatencyReport(ReflexState& settings) override;
    virtual ComputeStatus sleep() override;
    virtual ComputeStatus setReflexMarker(PCLMarker marker, uint64_t frameId) override;
    

    // Sharing API
    virtual ComputeStatus fetchTranslatedResourceFromCache(ICompute* otherAPI, ResourceType type, Resource res, TranslatedResource& shared, const char friendlyName[]) override;
    virtual ComputeStatus prepareTranslatedResources(CommandList cmdList, const std::vector<std::pair<chi::TranslatedResource, chi::ResourceDescription>>& resourceList) override { return ComputeStatus::eNoImplementation; }
    virtual ComputeStatus createSharedHandle(Resource res, Handle& handle) { return ComputeStatus::eNoImplementation; }
    virtual ComputeStatus destroySharedHandle(Handle& handle)  { return ComputeStatus::eNoImplementation; }
    virtual ComputeStatus getResourceFromSharedHandle(ResourceType type, Handle handle, Resource& res)  { return ComputeStatus::eNoImplementation; }

    // Resource pool
    virtual ComputeStatus createResourcePool(IResourcePool** pool, const char* vramSegment) override final;
    virtual ComputeStatus destroyResourcePool(IResourcePool* pool) override final;

    // OFA
    virtual ComputeStatus isNativeOpticalFlowSupported() override { return ComputeStatus::eNoImplementation; }

    virtual ComputeStatus isDeviceExtensionSupported(const char* extension, uint32_t version) override { return ComputeStatus::eNoImplementation; }

    ComputeStatus setSwapChainPrivateData(void* nativeSwapChain, void* data) override { return ComputeStatus::eNoImplementation; }
    ComputeStatus getSwapChainPrivateData(void* nativeSwapChain, void** data) override { return ComputeStatus::eNoImplementation; }
};

}
}

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

#include <chrono>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>

#include "source/platforms/sl.chi/compute.h"

#if !defined(SL_WINDOWS)
typedef struct GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;
#else
typedef LUID    NVSDK_NGX_LUID;
#endif

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
};

struct TimestampedResource
{
    inline bool operator==(const TimestampedResource& rhs) const { return resource == rhs.resource; }
    Resource resource;
    uint32_t frame;
};

struct TimestampedLambda
{
    std::function<void(void)> task;
    uint32_t frame;
};

class Generic : public ICompute
{
protected:
    using KernelMap = std::map<Kernel, KernelDataBase*>;
    KernelMap m_kernels = {};

    std::atomic<uint32_t> m_finishedFrame = 0;

    Device m_typelessDevice{};

    param::IParameters* m_parameters = {};

    using ResourceList = std::vector<Resource>;
    using TimestampedResourceList = std::vector<TimestampedResource>;
    using TimestampedLambdaList = std::vector<TimestampedLambda>;

    TimestampedResourceList m_resourcesToDestroy = {};
    TimestampedLambdaList m_destroyWithLambdas = {};

    std::mutex m_mutex;
    std::mutex m_mutexDynamicText;

    using ResourceStateMap = std::map<Resource, ResourceInfo>;
    ResourceStateMap m_resourceStateMap = {};

    Resource m_font[MAX_NUM_NODES] = {};
    Resource m_dynamicText[MAX_NUM_NODES] = {};
    Resource m_dynamicTextUpload[MAX_NUM_NODES] = {};
    Kernel m_kernelFont = {};
    uint32_t m_textIndex[MAX_NUM_NODES] = {};

    pfunResourceAllocateCallback* m_allocateCallback = {};
    pfunResourceReleaseCallback* m_releaseCallback = {};
    PFunGetThreadContext* m_getThreadContext = {};

    bool m_bFastUAVClearSupported = false;
    uint32_t m_preferenceFlags = 0;

    std::atomic<uint32_t> m_allocCount = 0; // total number of resource allocations done by this class
    std::atomic<unsigned long long> m_totalAllocatedSize = 0; // total size of all resources allocated by this instance of the class

    virtual void destroyResourceDeferredImpl(const Resource InResource) = 0;
    virtual ComputeStatus createBufferResourceImpl(ResourceDescription &InOutResourceDesc, Resource &OutResource, ResourceState InitialState) = 0;
    virtual ComputeStatus createTexture2DResourceSharedImpl(ResourceDescription &InOutResourceDesc, Resource &OutResource, bool UseNativeFormat, ResourceState InitialState) = 0;
    virtual ComputeStatus insertGPUBarrierList(CommandList cmdList, const Resource* InResources, unsigned int InResourceCount, BarrierType InBarrierType = eBarrierTypeUAV) override;
    virtual ComputeStatus transitionResourceImpl(CommandList cmdList, const ResourceTransition *transisitions, uint32_t count) = 0;
    virtual ComputeStatus getAllocatedBytes(unsigned long long &bytes) const override  { bytes = m_totalAllocatedSize; return eComputeStatusOk; }
    virtual std::wstring getDebugName(Resource res) = 0;

    ComputeStatus createTexture2DResourceShared(const ResourceDescription& CreateResourceDesc, Resource& OutResource, bool UseNativeFormat, const char InFriendlyName[]);
    ComputeStatus genericPostInit();

    bool savePFM(const std::string &path, const char* srcBuffer, const int width, const int height);
    uint64_t getResourceSize(Resource res);

public:

    virtual ComputeStatus init(Device InDevice, param::IParameters* params);
    virtual ComputeStatus shutdown();

    virtual ComputeStatus clearCache() override { return eComputeStatusNoImplementation; };

    virtual ComputeStatus getNativeResourceState(ResourceState state, uint32_t& nativeState) override { return eComputeStatusNoImplementation; };
    virtual ComputeStatus getResourceState(uint32_t nativeState, ResourceState& state) override { return eComputeStatusNoImplementation; };
    virtual ComputeStatus getResourceFootprint(Resource resoruce, ResourceFootprint& footprint) override { return eComputeStatusNoImplementation; };

    virtual ComputeStatus getFinishedFrameIndex(uint32_t& index) final override { index = m_finishedFrame; return eComputeStatusOk; };

    virtual ComputeStatus createCommandListContext(CommandQueue queue, uint32_t count, ICommandListContext*& ctx, const char friendlyName[])  override { return eComputeStatusNoImplementation; }
    virtual ComputeStatus destroyCommandListContext(ICommandListContext* ctx) override { return eComputeStatusNoImplementation; }

    virtual ComputeStatus createCommandQueue(CommandQueueType type, CommandQueue& queue, const char friendlyName[], uint32_t index) override { return eComputeStatusNoImplementation; }
    virtual ComputeStatus destroyCommandQueue(CommandQueue& queue) override { return eComputeStatusNoImplementation; }

    virtual ComputeStatus getDebugName(Resource res, std::wstring& name) { name = getDebugName(res); return eComputeStatusOk; }
    virtual ComputeStatus setDebugName(Resource res, const char friendlyName[]) override { return eComputeStatusNoImplementation; }
        
    virtual ComputeStatus getRefreshRate(SwapChain chain, float& refreshRate) override { return eComputeStatusNoImplementation; }
    virtual ComputeStatus getSwapChainBuffer(SwapChain chain, uint32_t index, Resource& buffer) override { return eComputeStatusNoImplementation; }

    virtual ComputeStatus pushState(CommandList cmdList) override { return eComputeStatusOk; }
    virtual ComputeStatus popState(CommandList cmdList) override { return eComputeStatusOk; }

    virtual ComputeStatus getNativeFormat(Format format, NativeFormat& native) override;
    virtual ComputeStatus getFormat(NativeFormat native, Format& format) override;
    virtual ComputeStatus getBytesPerPixel(Format format, size_t& size) override;
    virtual ComputeStatus destroyResource(Resource& InResource) override final;
    virtual ComputeStatus destroy(std::function<void(void)> task) override final;
        
    virtual ComputeStatus collectGarbage(uint32_t frame);

    ComputeStatus renderText(CommandList cmdList, int x, int y, const char *text, const ResourceArea &area, const float4& color = { 1.0f, 1.0f, 1.0f, 0.0f }, int reverseX = 0, int reverseY = 0) override;

    ComputeStatus createBuffer(const ResourceDescription &CreateResourceDesc, Resource &OutResource, const char InFriendlyName[]) override final;
    ComputeStatus createTexture2D(const ResourceDescription &CreateResourceDesc, Resource &OutResource, const char InFriendlyName[]) override final;
        

    ComputeStatus setCallbacks(pfunResourceAllocateCallback allocate, pfunResourceReleaseCallback release, PFunGetThreadContext getThreadContext) override final
    {
        m_allocateCallback = allocate;
        m_releaseCallback = release;
        m_getThreadContext = getThreadContext;
        return eComputeStatusOk;
    }

    ComputeStatus restorePipeline(CommandList cmdList)  override { return eComputeStatusOk; }

    ComputeStatus onHostResourceCreated(Resource resource, const ResourceInfo& info) override;
    ComputeStatus onHostResourceDestroyed(Resource resource) override { return eComputeStatusOk; }

    ComputeStatus transitionResources(CommandList cmdList, const ResourceTransition* transitions, uint32_t count, extra::ScopedTasks* tasks = nullptr) override;
    ComputeStatus setResourceState(Resource resource, ResourceState state, uint32_t subresource = kAllSubResources)  override;
    ComputeStatus getResourceState(Resource resource, ResourceState& state) override;
    ComputeStatus copyResource(CommandList cmdList, Resource dstResource, Resource srcResource) override { return eComputeStatusNoImplementation; }
    ComputeStatus cloneResource(Resource inResource, Resource &outResource, const char friendlyName[], ResourceState initialState, unsigned int creationMask, unsigned int visibilityMask) override { return eComputeStatusNoImplementation; }
    ComputeStatus copyDeviceTextureToDeviceBuffer(CommandList cmdList, Resource srcTexture, Resource dstBuffer) override { return eComputeStatusNoImplementation; }

    ComputeStatus getResourceDescription(Resource InResource, ResourceDescription &OutDesc) override { return eComputeStatusNoImplementation; }
    
    ComputeStatus setFullscreenState(SwapChain chain, bool fullscreen, Output out = nullptr) override { return eComputeStatusNoImplementation; }

    ComputeStatus beginProfiling(CommandList cmdList, unsigned int Metadata, const char* marker) override { return eComputeStatusOk;  }
    ComputeStatus endProfiling(CommandList cmdList)  override { return eComputeStatusOk; }
    ComputeStatus beginProfilingQueue(CommandQueue cmdList, uint32_t metadata, const char* marker)  override { return eComputeStatusOk; }
    ComputeStatus endProfilingQueue(CommandQueue cmdList)  override { return eComputeStatusOk; }

    virtual ComputeStatus setSleepMode(const ReflexConstants& consts) override;
    virtual ComputeStatus getSleepStatus(ReflexSettings& settings) override;
    virtual ComputeStatus getLatencyReport(ReflexSettings& settings) override;
    virtual ComputeStatus sleep() override;
    virtual ComputeStatus setReflexMarker(ReflexMarker marker, uint64_t frameId) override;
};

}
}
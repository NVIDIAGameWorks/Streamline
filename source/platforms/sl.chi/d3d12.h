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

#include <d3d12.h>
#include <future>
#include <array>

#include "source/core/sl.thread/thread.h"
#include "source/platforms/sl.chi/generic.h"
#include "source/platforms/sl.chi/d3dx12.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandList.h"
#include "source/core/sl.extra/extra.h"

namespace sl
{
namespace chi
{

constexpr int kHeapCount = 4;

struct D3D12ThreadContext : public CommonThreadContext
{
    interposer::D3D12GraphicsCommandList* cmdList = {};
};

constexpr unsigned int SL_MAX_D3D12_DESCRIPTORS          = 1024;
constexpr unsigned int SL_DESCRIPTOR_WRAPAROUND_CAPACITY = 2;

class GpuUploadBuffer
{
public:
    ID3D12Resource *getResource() { return m_resource; }
    virtual void release() { m_resource->Release(); }
    UINT64 size() { return m_resource ? m_resource->GetDesc().Width : 0; }

protected:
    ID3D12Resource *m_resource = {};

    GpuUploadBuffer() {}
    ~GpuUploadBuffer()
    {
        if (m_resource)
        {
            m_resource->Unmap(0, nullptr);
            release();
            m_resource = {};
        }
    }

    void allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
    {
        auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        if (FAILED(device->CreateCommittedResource(
            &uploadHeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_resource))))
        {
            SL_LOG_ERROR( "Failed to create GPU upload buffer");
        }
        if (resourceName)
        {
            m_resource->SetName(resourceName);
        }
    }

    uint8_t* mapCpuWriteOnly()
    {
        uint8_t* mappedData;
        // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        if (FAILED(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData))))
        {
            SL_LOG_ERROR( "Failed to map GPU upload buffer");
        }
        return mappedData;
    }
};

class ConstantBuffer : public GpuUploadBuffer
{
    uint8_t* m_mappedConstantData;
    UINT m_alignedInstanceSize;
    UINT m_numInstances;
    UINT m_size;
    UINT m_index = 0;

    inline constexpr uint32_t align(uint32_t size, uint32_t alignment)
    {
        return (size + (alignment - 1)) & ~(alignment - 1);
    }

public:
    ConstantBuffer() : m_alignedInstanceSize(0), m_numInstances(0), m_mappedConstantData(nullptr) {}

    void create(ID3D12Device* device, UINT size, UINT numInstances = 1, LPCWSTR resourceName = nullptr)
    {
        m_size = size;
        m_numInstances = numInstances;
        m_index = 0;
        m_alignedInstanceSize = align(size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
        UINT bufferSize = numInstances * m_alignedInstanceSize;
        allocate(device, bufferSize, resourceName);
        m_mappedConstantData = mapCpuWriteOnly();
    }

    UINT getIndex() const { return m_index; }
    void advanceIndex() { m_index = (m_index + 1) % m_numInstances; }

    void copyStagingToGpu(void *staging, UINT instanceIndex = 0)
    {
        memcpy(m_mappedConstantData + instanceIndex * m_alignedInstanceSize, staging, m_size);
    }

    UINT getNumInstances() { return m_numInstances; }
    D3D12_GPU_VIRTUAL_ADDRESS getGpuVirtualAddress(UINT instanceIndex = 0)
    {
        return m_resource->GetGPUVirtualAddress() + instanceIndex * m_alignedInstanceSize;
    }
};

struct KernelDispatchData
{
    KernelDispatchData() {};
    KernelDispatchData(const KernelDispatchData& rhs) { operator=(rhs); }

    uint32_t slot = 0;
    uint32_t numSamplers = 0;
    std::vector<UINT64> handles = {};
    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters = {};
    CD3DX12_DESCRIPTOR_RANGE rootRanges[32];
    std::vector<ConstantBuffer*> cb = {};
    CD3DX12_STATIC_SAMPLER_DESC samplers[8] = {};

    ID3D12RootSignature* rootSignature = {};
    ID3D12PipelineState* pso = {};

    inline KernelDispatchData& operator=(const KernelDispatchData& rhs)
    {
        slot = rhs.slot;
        numSamplers = rhs.numSamplers;
        handles = rhs.handles;
        rootParameters = rhs.rootParameters;
        memcpy(rootRanges, rhs.rootRanges, 32 * sizeof(CD3DX12_DESCRIPTOR_RANGE));
        cb = rhs.cb;
        memcpy(samplers, rhs.samplers, 8 * sizeof(CD3DX12_STATIC_SAMPLER_DESC));
        rootSignature = rhs.rootSignature;
        pso = rhs.pso;
        return *this;
    }

    inline bool addSlot(uint32_t index)
    {
        if (index >= (uint32_t)handles.size())
        {
            handles.resize(index + 1);
            handles[index] = 0;
            rootParameters.resize(index + 1);
            cb.resize(index + 1);
            return true;
        }
        return false;
    }

    inline void validate(uint32_t index, D3D12_DESCRIPTOR_RANGE_TYPE rangeType, UINT numDescriptors, UINT baseShaderRegister)
    {
        if (rootRanges[index].RangeType != rangeType || rootRanges[index].NumDescriptors != numDescriptors || rootRanges[index].BaseShaderRegister != baseShaderRegister)
        {
            SL_LOG_ERROR( "Incorrect root parameter setup!");
        }
    }
};

using KernelDispatchDataMap = std::map< Kernel, KernelDispatchData>;

struct DispatchDataD3D12
{
    DispatchDataD3D12() {};
    ~DispatchDataD3D12()
    {
        if (kddMap)
        {
            for (auto& [kernel, kdd] : *kddMap)
            {
                for (auto& cb : kdd.cb)
                {
                    delete cb;
                }
            }
            delete kddMap;
            kddMap = {};
        }
    }

    KernelDataBase* kernel = {};
    KernelDispatchDataMap* kddMap = {};
    ID3D12GraphicsCommandList* cmdList = {};
    uint32_t node = 0;
};

struct HeapInfo
{
    ID3D12DescriptorHeap *descriptorHeap[MAX_NUM_NODES] = {};
    ID3D12DescriptorHeap *descriptorHeapCPU[MAX_NUM_NODES] = {};

    UINT descIndex[MAX_NUM_NODES] = {0,0};

    // Number of time we've wrapped around our Descriptor heap
    unsigned int wrapAroundCount = 0;
};

struct ResourceDriverData
{
    uint32_t handle = 0;
    uint64_t virtualAddress = 0;
    uint64_t size = 0;
    uint32_t descIndex = 0;
    bool bZBCSupported = false;
    HeapInfo *heap = {};
};

class D3D12 : public Generic
{
    struct PerfData
    {
        UINT8* pStagingPtr = nullptr;
        ID3D12QueryHeap *queryHeap[SL_READBACK_QUEUE_SIZE] = {};
        ID3D12Resource  *queryBufferReadback[SL_READBACK_QUEUE_SIZE] = {};
        UINT queryIdx = 0;
        extra::AverageValueMeter meter{};
        bool reset[SL_READBACK_QUEUE_SIZE] = {};
    };
    using MapSectionPerf = std::map<std::string, PerfData>;
    MapSectionPerf m_sectionPerfMap[MAX_NUM_NODES] = {};

    ID3D12Device         *m_device     = nullptr;

    Kernel m_copyKernel = {};

    bool m_dbgSupportRs2RelaxedConversionRules = false;

    UINT m_descriptorSize = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_descHandleSamplerCPU[MAX_NUM_NODES][eSamplerCount] = {};

    HeapInfo* m_heap = nullptr;
    
    UINT m_visibleNodeMask = 0;

    std::map<void*, std::map<uint32_t,ResourceDriverData>> m_resourceData;
    std::map<size_t, ID3D12PipelineState*> m_psoMap = {};
    std::map<size_t, ID3D12RootSignature*> m_rootSignatureMap = {};
    thread::ThreadContext<DispatchDataD3D12> m_dispatchContext;

    size_t hashRootSignature(const CD3DX12_ROOT_SIGNATURE_DESC& desc);

    virtual std::wstring getDebugName(Resource res) override final;
    virtual int destroyResourceDeferredImpl(const Resource InResource) override final;

    ComputeStatus getLUIDFromDevice(NVSDK_NGX_LUID *OutId);
    ComputeStatus getSurfaceDriverData(Resource resource, ResourceDriverData &data, uint32_t mipOffset = 0);
    ComputeStatus getTextureDriverData(Resource resource, ResourceDriverData &data, uint32_t mipOffset = 0, uint32_t mipLevels = 0, Sampler sampler = Sampler::eSamplerPointClamp);
    ComputeStatus transitionResourceImpl(CommandList InCmdList, const ResourceTransition* transisitions, uint32_t count) override final;
    ComputeStatus createTexture2DResourceSharedImpl(ResourceDescription& InOutResourceDesc, Resource& OutResource, bool UseNativeFormat, ResourceState InitialState) override final;
    ComputeStatus createBufferResourceImpl(ResourceDescription& InOutResourceDesc, Resource& OutResource, ResourceState InitialState) override final;

    bool dx11On12 = false;
    bool isSupportedFormat(DXGI_FORMAT format, int flag1, int flag2);
    DXGI_FORMAT getCorrectFormat(DXGI_FORMAT Format);
    UINT getNewAndIncreaseDescIndex();

    inline D3D12_RESOURCE_STATES toD3D12States(ResourceState state)
    {
        uint32_t res;
        getNativeResourceState(state, res);
        return (D3D12_RESOURCE_STATES)res;
    }

public:

    virtual ComputeStatus init(Device InDevice, param::IParameters* params) override final;
    virtual ComputeStatus shutdown() override final;

    virtual ComputeStatus clearCache() override final;

    virtual ComputeStatus getRenderAPI(RenderAPI &OutType) override final;

    virtual ComputeStatus restorePipeline(CommandList cmdList)  override final;

    virtual ComputeStatus getNativeResourceState(ResourceState state, uint32_t& nativeState) override final;
    virtual ComputeStatus getResourceState(uint32_t nativeState, ResourceState& state) override final;
    virtual ComputeStatus getBarrierResourceState(uint32_t barrierType, ResourceState& state)  override final;

    virtual ComputeStatus createKernel(void *InCubinBlob, unsigned int InCubinBlobSize, const char* fileName, const char *EntryPoint, Kernel &OutKernel) override final;
    virtual ComputeStatus destroyKernel(Kernel& kernel) override final;

    virtual ComputeStatus createFence(FenceFlags flags, uint64_t initialValue, Fence& outFence, const char friendlyName[])  override final;

    virtual ComputeStatus createCommandListContext(CommandQueue queue, uint32_t count, ICommandListContext*& ctx, const char friendlyName[]) override final;
    virtual ComputeStatus destroyCommandListContext(ICommandListContext* ctx) override final;

    virtual ComputeStatus createCommandQueue(CommandQueueType type, CommandQueue& queue, const char friendlyName[], uint32_t index) override final;
    virtual ComputeStatus destroyCommandQueue(CommandQueue& queue) override final;

    virtual ComputeStatus getFullscreenState(SwapChain chain, bool& fullscreen) override final;
    virtual ComputeStatus setFullscreenState(SwapChain chain, bool fullscreen, Output out) override final;
    virtual ComputeStatus getRefreshRate(SwapChain chain, float& refreshRate) override final;
    virtual ComputeStatus getSwapChainBuffer(SwapChain chain, uint32_t index, Resource& buffer) override final;

    virtual ComputeStatus bindKernel(const Kernel InKernel) override final;
    virtual ComputeStatus bindSharedState(CommandList cmdList, UINT node) override final;
    virtual ComputeStatus bindSampler(uint32_t binding, uint32_t reg, Sampler sampler) override;
    virtual ComputeStatus bindConsts(uint32_t binding, uint32_t reg, void* data, size_t dataSize, uint32_t instances) override;
    virtual ComputeStatus bindTexture(uint32_t binding, uint32_t reg, Resource resource, uint32_t mipOffset = 0, uint32_t mipLevels = 0) override;
    virtual ComputeStatus bindRWTexture(uint32_t binding, uint32_t reg, Resource resource, uint32_t mipOffset = 0) override;
    virtual ComputeStatus bindRawBuffer(uint32_t binding, uint32_t reg, Resource resource) override;
    virtual ComputeStatus dispatch(unsigned int blockX, unsigned int blockY, unsigned int blockZ = 1) override final;

    virtual ComputeStatus clearView(CommandList cmdList, Resource InResource, const float4 Color, const RECT * pRect, unsigned int NumRects, CLEAR_TYPE &outType) override final;

    virtual ComputeStatus insertGPUBarrierList(CommandList cmdList, const Resource* InResources, unsigned int InResourceCount, BarrierType InBarrierType = eBarrierTypeUAV) override final;
    virtual ComputeStatus insertGPUBarrier(CommandList cmdList, Resource InResource, BarrierType InBarrierType) override final;
    virtual ComputeStatus copyResource(CommandList cmdList, Resource InDstResource, Resource InSrcResource) override final;
    virtual ComputeStatus cloneResource(Resource InResource, Resource &OutResource, const char friendlyName[], ResourceState InitialState, unsigned int InCreationMask, unsigned int InVisibilityMask) override final;
    virtual ComputeStatus copyBufferToReadbackBuffer(CommandList cmdList, Resource InResource, Resource OutResource, unsigned int InBytesToCopy) override final;
    virtual ComputeStatus copyDeviceTextureToDeviceBuffer(CommandList cmdList, Resource srcTexture, Resource dstBuffer) override final;
    virtual ComputeStatus copyHostToDeviceBuffer(CommandList InCmdList, uint64_t InSize, const void* InData, Resource InUploadResource, Resource InTargetResource, unsigned long long InUploadOffset, unsigned long long InDstOffset) override final;
    virtual ComputeStatus copyHostToDeviceTexture(CommandList InCmdList, uint64_t InSize, uint64_t RowPitch, const void* InData, Resource InTargetResource, Resource& InUploadResource) override final;

    virtual ComputeStatus mapResource(CommandList cmdList, Resource resource, void*& data, uint32_t subResource = 0, uint64_t offset = 0, uint64_t totalBytes = UINT64_MAX) override final;
    virtual ComputeStatus unmapResource(CommandList cmdList, Resource resource, uint32_t subResource) override final;

    virtual ComputeStatus getResourceState(Resource resource, ResourceState& state) override final;
    virtual ComputeStatus getResourceDescription(Resource InResource, ResourceDescription& OutDesc) override final;
    virtual ComputeStatus getResourceFootprint(Resource resource, ResourceFootprint& footprint) override final;

    virtual ComputeStatus setDebugName(Resource res, const char friendlyName[]) override final;

    ComputeStatus beginPerfSection(CommandList cmdList, const char *key, unsigned int node, bool InReset = false) override final;
    ComputeStatus endPerfSection(CommandList cmdList, const char *key, float &OutAvgTimeMS, unsigned int node) override final;
    ComputeStatus beginProfiling(CommandList cmdList, UINT Metadata, const char* marker) override final;
    ComputeStatus endProfiling(CommandList cmdList) override final;
    ComputeStatus beginProfilingQueue(CommandQueue cmdQueue, UINT Metadata, const char* marker) override final;
    ComputeStatus endProfilingQueue(CommandQueue cmdQueue) override final;

    virtual ComputeStatus notifyOutOfBandCommandQueue(CommandQueue queue, OutOfBandCommandQueueType type) override final;
    virtual ComputeStatus setAsyncFrameMarker(CommandQueue queue, ReflexMarker marker, uint64_t frameId) override final;

    virtual ComputeStatus createSharedHandle(Resource res, Handle& handle)  override final;
    virtual ComputeStatus destroySharedHandle(Handle& handle)  override final;
    virtual ComputeStatus getResourceFromSharedHandle(ResourceType type, Handle handle, Resource& res)  override final;
};

}
}

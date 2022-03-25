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

#include <dxgi1_4.h>
#include <algorithm>
#include <cmath>
#include <d3dcompiler.h>
#include <future>

#include "source/core/sl.log/log.h"
#include "source/platforms/sl.chi/d3d12.h"
#include "shaders/copy_to_buffer_cs.h"

namespace sl
{
namespace chi
{

// helper function for texture subresource calculations
// https://msdn.microsoft.com/en-us/library/windows/desktop/dn705766(v=vs.85).aspx
inline static uint32_t calcSubresource(uint32_t MipSlice, uint32_t ArraySlice, uint32_t PlaneSlice, uint32_t MipLevels, uint32_t ArraySize)
{
    return MipSlice + (ArraySlice * MipLevels) + (PlaneSlice * MipLevels * ArraySize);
}

D3D12 s_d3d12;
ICompute *getD3D12()
{
    return &s_d3d12;
}

std::wstring D3D12::getDebugName(Resource res)
{
    auto unknown = (IUnknown*)res;
    ID3D12Pageable* pageable;
    IDXGIObject* dxgi;
    unknown->QueryInterface(&pageable);
    unknown->QueryInterface(&dxgi);
    wchar_t name[128] = {};
    std::wstring wname = L"Unknown";
    if (pageable)
    {
        UINT size = sizeof(name);
        if (FAILED(pageable->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name)))
        {
            char sname[128] = {};
            size = sizeof(sname);
            if (SUCCEEDED(pageable->GetPrivateData(WKPDID_D3DDebugObjectName, &size, sname)))
            {
                std::string tmp(sname);
                wname = std::wstring(tmp.begin(), tmp.end());
            }
        }
        else
        {
            wname = name;
        }
        pageable->Release();
    }
    else if(dxgi)
    {
        UINT size = sizeof(name);
        if (FAILED(dxgi->GetPrivateData(WKPDID_D3DDebugObjectNameW, &size, name)))
        {
            char sname[128] = {};
            size = sizeof(sname);
            if (SUCCEEDED(dxgi->GetPrivateData(WKPDID_D3DDebugObjectName, &size, sname)))
            {
                std::string tmp(sname);
                wname = std::wstring(tmp.begin(), tmp.end());
            }
        }
        else
        {
            wname = name;
        }
        dxgi->Release();
    }
    return wname;
}

const char * getDXGIFormatStr(uint32_t format)
{
    static constexpr const char *GDXGI_FORMAT_STR[] = {
        "DXGI_FORMAT_UNKNOWN",
        "DXGI_FORMAT_R32G32B32A32_TYPELESS",
        "DXGI_FORMAT_R32G32B32A32_FLOAT",
        "DXGI_FORMAT_R32G32B32A32_UINT",
        "DXGI_FORMAT_R32G32B32A32_SINT",
        "DXGI_FORMAT_R32G32B32_TYPELESS",
        "DXGI_FORMAT_R32G32B32_FLOAT",
        "DXGI_FORMAT_R32G32B32_UINT",
        "DXGI_FORMAT_R32G32B32_SINT",
        "DXGI_FORMAT_R16G16B16A16_TYPELESS",
        "DXGI_FORMAT_R16G16B16A16_FLOAT",
        "DXGI_FORMAT_R16G16B16A16_UNORM",
        "DXGI_FORMAT_R16G16B16A16_UINT",
        "DXGI_FORMAT_R16G16B16A16_SNORM",
        "DXGI_FORMAT_R16G16B16A16_SINT",
        "DXGI_FORMAT_R32G32_TYPELESS",
        "DXGI_FORMAT_R32G32_FLOAT",
        "DXGI_FORMAT_R32G32_UINT",
        "DXGI_FORMAT_R32G32_SINT",
        "DXGI_FORMAT_R32G8X24_TYPELESS",
        "DXGI_FORMAT_D32_FLOAT_S8X24_UINT",
        "DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS",
        "DXGI_FORMAT_X32_TYPELESS_G8X24_UINT",
        "DXGI_FORMAT_R10G10B10A2_TYPELESS",
        "DXGI_FORMAT_R10G10B10A2_UNORM",
        "DXGI_FORMAT_R10G10B10A2_UINT",
        "DXGI_FORMAT_R11G11B10_FLOAT",
        "DXGI_FORMAT_R8G8B8A8_TYPELESS",
        "DXGI_FORMAT_R8G8B8A8_UNORM",
        "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB",
        "DXGI_FORMAT_R8G8B8A8_UINT",
        "DXGI_FORMAT_R8G8B8A8_SNORM",
        "DXGI_FORMAT_R8G8B8A8_SINT",
        "DXGI_FORMAT_R16G16_TYPELESS",
        "DXGI_FORMAT_R16G16_FLOAT",
        "DXGI_FORMAT_R16G16_UNORM",
        "DXGI_FORMAT_R16G16_UINT",
        "DXGI_FORMAT_R16G16_SNORM",
        "DXGI_FORMAT_R16G16_SINT",
        "DXGI_FORMAT_R32_TYPELESS",
        "DXGI_FORMAT_D32_FLOAT",
        "DXGI_FORMAT_R32_FLOAT",
        "DXGI_FORMAT_R32_UINT",
        "DXGI_FORMAT_R32_SINT",
        "DXGI_FORMAT_R24G8_TYPELESS",
        "DXGI_FORMAT_D24_UNORM_S8_UINT",
        "DXGI_FORMAT_R24_UNORM_X8_TYPELESS",
        "DXGI_FORMAT_X24_TYPELESS_G8_UINT",
        "DXGI_FORMAT_R8G8_TYPELESS",
        "DXGI_FORMAT_R8G8_UNORM",
        "DXGI_FORMAT_R8G8_UINT",
        "DXGI_FORMAT_R8G8_SNORM",
        "DXGI_FORMAT_R8G8_SINT",
        "DXGI_FORMAT_R16_TYPELESS",
        "DXGI_FORMAT_R16_FLOAT",
        "DXGI_FORMAT_D16_UNORM",
        "DXGI_FORMAT_R16_UNORM",
        "DXGI_FORMAT_R16_UINT",
        "DXGI_FORMAT_R16_SNORM",
        "DXGI_FORMAT_R16_SINT",
        "DXGI_FORMAT_R8_TYPELESS",
        "DXGI_FORMAT_R8_UNORM",
        "DXGI_FORMAT_R8_UINT",
        "DXGI_FORMAT_R8_SNORM",
        "DXGI_FORMAT_R8_SINT",
        "DXGI_FORMAT_A8_UNORM",
        "DXGI_FORMAT_R1_UNORM",
        "DXGI_FORMAT_R9G9B9E5_SHAREDEXP",
        "DXGI_FORMAT_R8G8_B8G8_UNORM",
        "DXGI_FORMAT_G8R8_G8B8_UNORM",
        "DXGI_FORMAT_BC1_TYPELESS",
        "DXGI_FORMAT_BC1_UNORM",
        "DXGI_FORMAT_BC1_UNORM_SRGB",
        "DXGI_FORMAT_BC2_TYPELESS",
        "DXGI_FORMAT_BC2_UNORM",
        "DXGI_FORMAT_BC2_UNORM_SRGB",
        "DXGI_FORMAT_BC3_TYPELESS",
        "DXGI_FORMAT_BC3_UNORM",
        "DXGI_FORMAT_BC3_UNORM_SRGB",
        "DXGI_FORMAT_BC4_TYPELESS",
        "DXGI_FORMAT_BC4_UNORM",
        "DXGI_FORMAT_BC4_SNORM",
        "DXGI_FORMAT_BC5_TYPELESS",
        "DXGI_FORMAT_BC5_UNORM",
        "DXGI_FORMAT_BC5_SNORM",
        "DXGI_FORMAT_B5G6R5_UNORM",
        "DXGI_FORMAT_B5G5R5A1_UNORM",
        "DXGI_FORMAT_B8G8R8A8_UNORM",
        "DXGI_FORMAT_B8G8R8X8_UNORM",
        "DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM",
        "DXGI_FORMAT_B8G8R8A8_TYPELESS",
        "DXGI_FORMAT_B8G8R8A8_UNORM_SRGB",
        "DXGI_FORMAT_B8G8R8X8_TYPELESS",
        "DXGI_FORMAT_B8G8R8X8_UNORM_SRGB",
        "DXGI_FORMAT_BC6H_TYPELESS",
        "DXGI_FORMAT_BC6H_UF16",
        "DXGI_FORMAT_BC6H_SF16",
        "DXGI_FORMAT_BC7_TYPELESS",
        "DXGI_FORMAT_BC7_UNORM",
        "DXGI_FORMAT_BC7_UNORM_SRGB",
        "DXGI_FORMAT_AYUV",
        "DXGI_FORMAT_Y410",
        "DXGI_FORMAT_Y416",
        "DXGI_FORMAT_NV12",
        "DXGI_FORMAT_P010",
        "DXGI_FORMAT_P016",
        "DXGI_FORMAT_420_OPAQUE",
        "DXGI_FORMAT_YUY2",
        "DXGI_FORMAT_Y210",
        "DXGI_FORMAT_Y216",
        "DXGI_FORMAT_NV11",
        "DXGI_FORMAT_AI44",
        "DXGI_FORMAT_IA44",
        "DXGI_FORMAT_P8",
        "DXGI_FORMAT_A8P8",
        "DXGI_FORMAT_B4G4R4A4_UNORM",
        "DXGI_FORMAT_P208",
        "DXGI_FORMAT_V208",
        "DXGI_FORMAT_V408",
    };
    if (format < countof(GDXGI_FORMAT_STR))
    {
        return GDXGI_FORMAT_STR[format];
    }
        
    return "DXGI_INVALID_FORMAT";
}

struct CommandListContext : public ICommandListContext
{
    ID3D12CommandQueue* cmdQueue;
    ID3D12GraphicsCommandList* cmdList;
    std::vector<ID3D12CommandAllocator*> allocator;
    std::vector <ID3D12Fence*> fence;
    HANDLE fenceEvent;
    std::vector<UINT64> fenceValue = {};
    bool cmdListIsRecording = false;
    std::atomic<uint32_t> index = 0;
    std::atomic<uint32_t> lastIndex = UINT_MAX;
    uint32_t bufferCount = 0;
    std::wstring name;

    void init(const char* debugName, ID3D12Device* device, ID3D12CommandQueue* queue, uint32_t count)
    {
        std::string tmp = debugName;
        name = std::wstring(tmp.begin(), tmp.end());
        cmdQueue = queue;
        auto cmdQueueDesc = cmdQueue->GetDesc();
        bufferCount = count;
        allocator.resize(count);
        fence.resize(count);
        fenceValue.resize(count);
        for (uint32_t i = 0; i < count; i++)
        {
            device->CreateCommandAllocator(cmdQueueDesc.Type, IID_PPV_ARGS(&allocator[i]));
            fenceValue[i] = 0;
            device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
        }

        device->CreateCommandList(0, cmdQueueDesc.Type, allocator[0], nullptr, IID_PPV_ARGS(&cmdList));

        cmdList->Close(); // Immediately close since it will be reset on first use
        cmdList->SetName((name + L" command list").c_str());

        fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    }

    void shutdown()
    {
        SL_SAFE_RELEASE(cmdList);
        for (uint32_t i = 0; i < bufferCount; i++)
        {
            SL_SAFE_RELEASE(allocator[i]);
            SL_SAFE_RELEASE(fence[i]);
        }
        CloseHandle(fenceEvent);
        allocator.clear();
        fence.clear();
    }

    CommandList getCmdList()
    {
        return cmdList;
    }

    CommandQueue getCmdQueue()
    {
        return cmdQueue;
    }

    CommandAllocator getCmdAllocator()
    {
        return allocator[index];
    }

    Handle getFenceEvent()
    {
        return fenceEvent;
    }

    bool beginCommandList()
    {
        if (cmdListIsRecording)
        {
            return true;
        }

        // Only rest allocator if we are done with the work
        if (fence[index]->GetCompletedValue() >= fenceValue[index])
        {
            allocator[index]->Reset();
        }

        cmdListIsRecording = SUCCEEDED(cmdList->Reset(allocator[index], nullptr));
        if (!cmdListIsRecording)
        {
            SL_LOG_ERROR("%S command buffer - cannot reset command list", name.c_str());
        }
        return cmdListIsRecording;
    }

    void executeCommandList()
    {
        assert(cmdListIsRecording);
        cmdListIsRecording = false;

        if (FAILED(cmdList->Close()))
            return;

        ID3D12CommandList* const cmd_lists[] = { cmdList };
        cmdQueue->ExecuteCommandLists(ARRAYSIZE(cmd_lists), cmd_lists);
    }

    bool flushAll()
    {
        for (uint32_t i = 0; i < bufferCount; i++)
        {
            const UINT64 syncValue = fenceValue[i] + 1;
            if (FAILED(cmdQueue->Signal(fence[i], syncValue)))
                return false;
            if (SUCCEEDED(fence[i]->SetEventOnCompletion(syncValue, fenceEvent)))
            {
                WaitForSingleObject(fenceEvent, 500); // don't wait INFINITE
            }
            else
            {
                return false;
            }
        }
        return true;
    }
    
    uint32_t getBufferCount()
    {
        return bufferCount;
    }

    uint32_t getCurrentBufferIndex()
    {
        return index;
    }

    uint32_t acquireNextBufferIndex(SwapChain chain)
    {
        // VK specific, nothing to do here
        return index;
    }

    bool didFrameFinish(uint32_t index)
    {
        if (index >= bufferCount)
        {
            SL_LOG_ERROR("Invalid index");
            return false;
        }
        return fence[index]->GetCompletedValue() >= fenceValue[index];
    }

    void waitOnGPUForTheOtherQueue(const ICommandListContext* other)
    {
        auto tmp = (const CommandListContext*)other;
        if (tmp->cmdQueue != cmdQueue && tmp->lastIndex != UINT_MAX)
        {
            if (FAILED(cmdQueue->Wait(tmp->fence[tmp->lastIndex], tmp->fenceValue[tmp->lastIndex])))
            {
                SL_LOG_ERROR("Failed to wait on the command queue");
            }
        }
    }

    bool waitForCommandList(FlushType ft)
    {
        // Flush command list, to avoid it still referencing resources that may be destroyed after this call
        if (cmdListIsRecording)
            executeCommandList();
        // Increment fence value to ensure it has not been signaled before
        const UINT64 syncValue = fenceValue[index] + 1;
        if (FAILED(cmdQueue->Signal(fence[index], syncValue)))
            return false; // Cannot wait on fence if signaling was not successful
        if (ft == eCurrent)
        {
            // Flushing so wait for current sync value
            if (SUCCEEDED(fence[index]->SetEventOnCompletion(syncValue, fenceEvent)))
                WaitForSingleObject(fenceEvent, 500); // don't wait INFINITE
        }
        else if (ft == ePrevious && lastIndex != UINT_MAX)
        {
            // If previous frame did not finish let's wait for it
            if (fence[lastIndex]->GetCompletedValue() < fenceValue[lastIndex])
            {
                if (SUCCEEDED(fence[lastIndex]->SetEventOnCompletion(fenceValue[lastIndex], fenceEvent)))
                    WaitForSingleObject(fenceEvent, 500); // don't wait INFINITE
            }
        }
        else
        {
            // If old frame (bufferCount ago) at this index did not finish let's wait for it
            if (fence[index]->GetCompletedValue() < fenceValue[index])
            {
                if (SUCCEEDED(fence[index]->SetEventOnCompletion(fenceValue[index], fenceEvent)))
                    WaitForSingleObject(fenceEvent, 500); // don't wait INFINITE
            }
        }
        // Update CPU side fence value now that it is guaranteed to have come through
        fenceValue[index] = syncValue;

        lastIndex.store(index.load());

        index = (index + 1) % bufferCount;
        return true;
    }

    bool present(SwapChain chain, uint32_t sync, uint32_t flags)
    {
        if (FAILED(((IDXGISwapChain*)chain)->Present(sync, flags)))
        {
            SL_LOG_ERROR("Present failed");
            return false;
        }
        return true;
    }

};

ComputeStatus D3D12::init(Device InDevice, param::IParameters* params)
{
    Generic::init(InDevice, params);

    m_device = (ID3D12Device*)InDevice;

    UINT NodeCount = m_device->GetNodeCount();
    m_visibleNodeMask = (1 << NodeCount) - 1;

    if (NodeCount > MAX_NUM_NODES)
    {
        SL_LOG_ERROR(" too many GPU nodes");
        return eComputeStatusError;
    }

    HRESULT hr = S_OK;

    // The ability to cast one fully typed resource to a compatible fully typed cast resource
    // (instead of creating the resource typeless)
    // should be supported by all our GPUs with drivers going back as far as RS2 which is way before the introduction of DLSS
    // There are some restrictions though see :
    // https://microsoft.github.io/DirectX-Specs/d3d/RelaxedCasting.html
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 FeatureOptions3;
    hr = m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &FeatureOptions3, sizeof(FeatureOptions3));
    if (FAILED(hr) || !FeatureOptions3.CastingFullyTypedFormatSupported)
    {
        SL_LOG_ERROR(" CheckFeatureSupport() call did not succeed or the driver did not report CastingFullyTypedFormatSupported. Windows 10 RS2 or higher was expected. hr=%d", hr);
        m_dbgSupportRs2RelaxedConversionRules = false;
    }
    else
    {
        m_dbgSupportRs2RelaxedConversionRules = true;
    }

    SL_LOG_INFO("GPU nodes %u - visible node mask %u", NodeCount, m_visibleNodeMask);

    m_heap = new HeapInfo;

    for(UINT Node = 0; Node < NodeCount; Node++)
    {
        // create desc heaps for SRV/UAV/CBV
        {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            // Each wraparound we target a different part of the heap to prevent direct reuse
            heapDesc.NumDescriptors = SL_MAX_D3D12_DESCRIPTORS * SL_DESCRIPTOR_WRAPAROUND_CAPACITY;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            heapDesc.NodeMask = (1 << Node);
            hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap->descriptorHeap[Node]));
            if (FAILED(hr)) { SL_LOG_ERROR(" failed to create descriptor heap, hr=%d", hr); return eComputeStatusError; }
            m_heap->descriptorHeap[Node]->SetName(L"sl.chi.heapGPU");
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            hr = m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap->descriptorHeapCPU[Node]));
            if (FAILED(hr)) { SL_LOG_ERROR(" failed to create descriptor heap, hr=%d", hr); return eComputeStatusError; }
            m_heap->descriptorHeapCPU[Node]->SetName(L"sl.chi.heapCPU");
        }
        
        m_descriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    m_bFastUAVClearSupported = true;

    genericPostInit();

    CHI_CHECK(createKernel((void*)copy_to_buffer_cs, copy_to_buffer_cs_len, "copy_to_buffer.cs", "main", m_copyKernel));

    m_workCompleteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!m_workCompleteEvent) 
    { 
        SL_LOG_ERROR("error: failed to create event"); 
        return eComputeStatusError; 
    }
    hr = m_device->CreateFence(0ull, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&m_fence);
    if (FAILED(hr)) 
    { 
        SL_LOG_ERROR("error: failed to create fence, hr=%d", hr); 
        return eComputeStatusError; 
    }

    return eComputeStatusOk;
}

ComputeStatus D3D12::shutdown()
{
    SL_SAFE_RELEASE(m_fence);

    CHI_CHECK(destroyKernel(m_copyKernel));
    m_copyKernel = {};

    for (auto& rb : m_readbackMap)
    {
        CHI_CHECK(destroyResource(rb.second.target));
        for (int i = 0; i < SL_READBACK_QUEUE_SIZE; i++)
        {
            CHI_CHECK(destroyResource(rb.second.readback[i]));
        }
    }
    for (UINT node = 0; node < MAX_NUM_NODES; node++)
    {
#if SL_ENABLE_PERF_TIMING
        for (auto section : m_sectionPerfMap[node])
        {
            for (int i = 0; i < SL_READBACK_QUEUE_SIZE; i++)
            {
                SL_SAFE_RELEASE(section.second.queryHeap[i]);
                SL_SAFE_RELEASE(section.second.queryBufferReadback[i]);
            }
        }
        m_sectionPerfMap[node].clear();
#endif
        SL_SAFE_RELEASE(m_heap->descriptorHeap[node]);
        SL_SAFE_RELEASE(m_heap->descriptorHeapCPU[node]);
    }

    delete m_heap;
    m_heap = {};

    for (auto& v : m_psoMap)
    {
        SL_LOG_VERBOSE("Destroying pipeline state 0x%llx", v.second);
        SL_SAFE_RELEASE(v.second);
    }
    for (auto& v : m_rootSignatureMap)
    {
        SL_LOG_VERBOSE("Destroying root signature 0x%llx", v.second);
        SL_SAFE_RELEASE(v.second);
    }
    m_rootSignatureMap.clear();
    m_psoMap.clear();

    ComputeStatus Res = eComputeStatusOk;
    for (auto& k : m_kernels)
    {
        auto kernel = (KernelDataBase*)k.second;
        SL_LOG_VERBOSE("Destroying kernel %s", kernel->name.c_str());
        delete kernel;
    }
    m_kernels.clear();

    return Generic::shutdown();
}

ComputeStatus D3D12::getPlatformType(PlatformType &OutType)
{
    OutType = ePlatformTypeD3D12;
    return eComputeStatusOk;
}

ComputeStatus D3D12::restorePipeline(CommandList pCmdList)
{
    D3D12ThreadContext* thread = (D3D12ThreadContext*)m_getThreadContext();
    auto cmdList = ((ID3D12GraphicsCommandList*)pCmdList);
    
    assert(thread->cmdList->m_base == pCmdList);

    if (thread->cmdList->m_numHeaps > 0)
    {
        cmdList->SetDescriptorHeaps(thread->cmdList->m_numHeaps, thread->cmdList->m_heaps);
    }
    if (thread->cmdList->m_rootSignature)
    {
        cmdList->SetComputeRootSignature(thread->cmdList->m_rootSignature);
        for (auto& pair : thread->cmdList->m_mapHandles)
        {
            cmdList->SetComputeRootDescriptorTable(pair.first, pair.second);
        }
        for (auto& pair : thread->cmdList->m_mapCBV)
        {
            cmdList->SetComputeRootConstantBufferView(pair.first, pair.second);
        }
        for (auto& pair : thread->cmdList->m_mapSRV)
        {
            cmdList->SetComputeRootShaderResourceView(pair.first, pair.second);
        }
        for (auto& pair : thread->cmdList->m_mapUAV)
        {
            cmdList->SetComputeRootUnorderedAccessView(pair.first, pair.second);
        }
        for (auto& pair : thread->cmdList->m_mapConstants)
        {
            cmdList->SetComputeRoot32BitConstants(pair.first, pair.second.Num32BitValuesToSet, pair.second.SrcData, pair.second.DestOffsetIn32BitValues);
        }
    }
    if (thread->cmdList->m_pso)
    {
        cmdList->SetPipelineState(thread->cmdList->m_pso);
    }
    if (thread->cmdList->m_so)
    {
        static_cast<ID3D12GraphicsCommandList4*>(cmdList)->SetPipelineState1(thread->cmdList->m_so);
    }
    return eComputeStatusOk;
}

ComputeStatus D3D12::getResourceState(uint32_t states, ResourceState& resourceStates)
{
    resourceStates = ResourceState::ePresent;

    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        resourceStates = resourceStates | (ResourceState::eConstantBuffer | ResourceState::eVertexBuffer);
    if (states & (D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))
        resourceStates = resourceStates | (ResourceState::eTextureRead);
    if (states & (D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
        resourceStates = resourceStates | (ResourceState::eStorageRead);
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDEX_BUFFER)
        resourceStates = resourceStates | ResourceState::eIndexBuffer;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
        resourceStates = resourceStates | ResourceState::eArgumentBuffer;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        resourceStates = resourceStates | ResourceState::eStorageRW;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET)
        resourceStates = resourceStates | ResourceState::eColorAttachmentWrite;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_READ)
        resourceStates = resourceStates | ResourceState::eDepthStencilAttachmentRead;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE)
        resourceStates = resourceStates | ResourceState::eDepthStencilAttachmentWrite;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE)
        resourceStates = resourceStates | ResourceState::eCopySource;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST)
        resourceStates = resourceStates | ResourceState::eCopyDestination;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)
        resourceStates = resourceStates | (ResourceState::eAccelStructRead | ResourceState::eAccelStructWrite);
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RESOLVE_DEST)
        resourceStates = resourceStates | ResourceState::eResolveDestination;
    if (states & D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
        resourceStates = resourceStates | ResourceState::eResolveSource;

    return eComputeStatusOk;
}

ComputeStatus D3D12::getNativeResourceState(ResourceState states, uint32_t& resourceStates)
{
    resourceStates = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

    if (states & (ResourceState::eConstantBuffer | ResourceState::eVertexBuffer))
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (states & (ResourceState::eTextureRead))
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (states & (ResourceState::eGenericRead))
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_GENERIC_READ;
    if (states & (ResourceState::eStorageRead))
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (states & ResourceState::eIndexBuffer)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if (states & ResourceState::eArgumentBuffer)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    if ((states & ResourceState::eStorageWrite) && (states & ResourceState::eStorageRead))
    {
        // Clear out incompatible state if we want read/write access
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        resourceStates &= ~(D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
            D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    }
    if (states & ResourceState::eColorAttachmentWrite)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (states & ResourceState::eDepthStencilAttachmentRead)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_READ;
    if (states & ResourceState::eDepthStencilAttachmentWrite)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if (states & ResourceState::eCopySource)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (states & ResourceState::eCopyDestination)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COPY_DEST;
    if (states & (ResourceState::eAccelStructRead | ResourceState::eAccelStructWrite))
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    if (states & ResourceState::eResolveDestination)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RESOLVE_DEST;
    if (states & ResourceState::eResolveSource)
        resourceStates |= D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

    return eComputeStatusOk;
}

ComputeStatus D3D12::createKernel(void *blobData, unsigned int blobSize, const char* fileName, const char *entryPoint, Kernel &kernel)
{
    if (!blobData) return eComputeStatusInvalidArgument;

    size_t hash = 0;
    auto i = blobSize;
    while (i--)
    {
        hash_combine(hash, ((char*)blobData)[i]);
    }

    ComputeStatus Res = eComputeStatusOk;
    KernelDataBase*data = {};
    bool missing = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_kernels.find(hash);
        missing = it == m_kernels.end();
        if (missing)
        {
            data = new KernelDataBase;
            data->hash = hash;
            m_kernels[hash] = data;
        }
        else
        {
            data = (KernelDataBase*)(*it).second;
        }
    }
    if (missing)
    {
        data->name = fileName;
        data->entryPoint = entryPoint;
        const char* blob = (const char*)blobData;
        if (blob[0] == 'D' && blob[1] == 'X' && blob[2] == 'B' && blob[3] == 'C')
        {
            data->kernelBlob.resize(blobSize);
            memcpy(data->kernelBlob.data(), blob, blobSize);
            SL_LOG_VERBOSE("Creating DXBC kernel %s:%s hash %llu", fileName, entryPoint, hash);
        }
        else
        {
            SL_LOG_ERROR("Unsupported kernel blob");
            return eComputeStatusInvalidArgument;
        }
    }
    else
    {
        if (data->entryPoint != entryPoint || data->name != fileName)
        {
            SL_LOG_ERROR("Shader %s:%s has overlapping hash with shader %s:%s", data->name.c_str(), data->entryPoint.c_str(), fileName, entryPoint);
            return eComputeStatusError;
        }
        SL_LOG_WARN("Kernel %s:%s with hash 0x%llx already created!", fileName, entryPoint, hash);
    }
    kernel = hash;
    return Res;
}

ComputeStatus D3D12::destroyKernel(Kernel& InKernel)
{
    if (!InKernel) return eComputeStatusOk; // fine to destroy null kernels
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_kernels.find(InKernel);
    if (it == m_kernels.end())
    {
        return eComputeStatusInvalidCall;
    }
    auto data = (KernelDataBase*)(it->second);
    SL_LOG_VERBOSE("Destroying kernel %s", data->name.c_str());
    delete it->second;
    m_kernels.erase(it);
    InKernel = {};
    return eComputeStatusOk;
}

ComputeStatus D3D12::createCommandListContext(CommandQueue queue, uint32_t count, ICommandListContext*& ctx, const char friendlyName[])
{
    auto tmp = new CommandListContext();
    tmp->init(friendlyName, m_device, (ID3D12CommandQueue*)queue, count);
    ctx = tmp;
    return eComputeStatusOk;
}

ComputeStatus D3D12::destroyCommandListContext(ICommandListContext* ctx)
{
    auto tmp = (CommandListContext*)ctx;
    tmp->shutdown();
    delete tmp;
    return eComputeStatusOk;
}

ComputeStatus D3D12::createCommandQueue(CommandQueueType type, CommandQueue& queue, const char friendlyName[])
{
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type == CommandQueueType::eGraphics ? D3D12_COMMAND_LIST_TYPE_DIRECT : D3D12_COMMAND_LIST_TYPE_COMPUTE;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_HIGH;
    ID3D12CommandQueue* tmp;
    if (FAILED(m_device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), (void**)&tmp)))
    {
        SL_LOG_ERROR("Failed to create command queue %s", friendlyName);
        return eComputeStatusError;
    }
    queue = tmp;
    setDebugName(queue, friendlyName);
    return eComputeStatusOk;
}

ComputeStatus D3D12::destroyCommandQueue(CommandQueue& queue)
{
    if (queue)
    {
        auto tmp = (ID3D12CommandQueue*)queue;
        SL_SAFE_RELEASE(tmp);
    }    
    return eComputeStatusOk;
}

ComputeStatus D3D12::setFullscreenState(SwapChain chain, bool fullscreen, Output out)
{
    if (!chain) return eComputeStatusInvalidArgument;
    IDXGISwapChain* swapChain = (IDXGISwapChain*)chain;
    if (FAILED(swapChain->SetFullscreenState(fullscreen, (IDXGIOutput*)out)))
    {
        SL_LOG_ERROR("Failed to set fullscreen state");
    }
    return eComputeStatusOk;
}

ComputeStatus D3D12::getRefreshRate(SwapChain chain, float& refreshRate)
{
    if (!chain) return eComputeStatusInvalidArgument;
    IDXGISwapChain* swapChain = (IDXGISwapChain*)chain;
    IDXGIOutput* dxgiOutput;
    HRESULT hr = swapChain->GetContainingOutput(&dxgiOutput);
    // if swap chain get failed to get DXGIoutput then follow the below link get the details from remarks section
    //https://docs.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-getcontainingoutput
    if (SUCCEEDED(hr))
    {
        // get the descriptor for current output
        // from which associated mornitor will be fetched
        DXGI_OUTPUT_DESC outputDes{};
        hr = dxgiOutput->GetDesc(&outputDes);
        dxgiOutput->Release();
        if (SUCCEEDED(hr))
        {
            MONITORINFOEXW info;
            info.cbSize = sizeof(info);
            // get the associated monitor info
            if (GetMonitorInfoW(outputDes.Monitor, &info) != 0)
            {
                // using the CCD get the associated path and display configuration
                UINT32 requiredPaths, requiredModes;
                if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes) == ERROR_SUCCESS)
                {
                    std::vector<DISPLAYCONFIG_PATH_INFO> paths(requiredPaths);
                    std::vector<DISPLAYCONFIG_MODE_INFO> modes2(requiredModes);
                    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes2.data(), nullptr) == ERROR_SUCCESS)
                    {
                        // iterate through all the paths until find the exact source to match
                        for (auto& p : paths) {
                            DISPLAYCONFIG_SOURCE_DEVICE_NAME sourceName;
                            sourceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                            sourceName.header.size = sizeof(sourceName);
                            sourceName.header.adapterId = p.sourceInfo.adapterId;
                            sourceName.header.id = p.sourceInfo.id;
                            if (DisplayConfigGetDeviceInfo(&sourceName.header) == ERROR_SUCCESS)
                            {
                                // find the matched device which is associated with current device 
                                // there may be the possibility that display may be duplicated and windows may be one of them in such scenario
                                // there may be two callback because source is same target will be different
                                // as window is on both the display so either selecting either one is ok
                                if (wcscmp(info.szDevice, sourceName.viewGdiDeviceName) == 0) {
                                    // get the refresh rate
                                    UINT numerator = p.targetInfo.refreshRate.Numerator;
                                    UINT denominator = p.targetInfo.refreshRate.Denominator;
                                    double refrate = (double)numerator / (double)denominator;
                                    refreshRate = (float)refrate;
                                    return eComputeStatusOk;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    SL_LOG_ERROR("Failed to retrieve refresh rate from swapchain 0x%llx", chain);
    return eComputeStatusError;
}

ComputeStatus D3D12::getSwapChainBuffer(SwapChain chain, uint32_t index, Resource& buffer)
{
    ID3D12Resource* tmp;
    if (FAILED(((IDXGISwapChain*)chain)->GetBuffer(index, IID_PPV_ARGS(&tmp))))
    {
        SL_LOG_ERROR("Failed to get buffer from swapchain");
        return eComputeStatusError;
    }
    buffer = tmp;
    return eComputeStatusOk;
}

ComputeStatus D3D12::bindSharedState(CommandList InCmdList, UINT node)
{
    auto& ctx = m_dispatchContext.getContext();
    ctx.node = node;
    ctx.cmdList = (ID3D12GraphicsCommandList*)InCmdList;

    ID3D12DescriptorHeap *Heaps[] = { m_heap->descriptorHeap[ctx.node] };
    ctx.cmdList->SetDescriptorHeaps(1, Heaps);

    return eComputeStatusOk;
}

ComputeStatus D3D12::bindKernel(const Kernel kernelToBind)
{
    auto& ctx = m_dispatchContext.getContext();
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_kernels.find(kernelToBind);
        if (it == m_kernels.end())
        {
            SL_LOG_ERROR("Trying to bind kernel which has not been created");
            return eComputeStatusInvalidCall;
        }
        ctx.kernel = (KernelDataBase*)(*it).second;
    }

    if (!ctx.kddMap)
    {
        ctx.kddMap = new KernelDispatchDataMap();
    }
    auto it = ctx.kddMap->find(ctx.kernel->hash);
    if (it == ctx.kddMap->end())
    {
        (*ctx.kddMap)[ctx.kernel->hash] = {};
    }
    else
    {
        (*it).second.numSamplers = 0;
        (*it).second.slot = 0;
    }
    //SL_LOG_INFO("Binding kernel %s:%s", ctx.kernel->name.c_str(), ctx.kernel->entryPoint.c_str());
    
    return eComputeStatusOk;
}

ComputeStatus D3D12::bindSampler(uint32_t pos, uint32_t base, Sampler sampler)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!ctx.kernel || base >= 8) return eComputeStatusInvalidArgument;

    auto &kdd = (*ctx.kddMap)[ctx.kernel->hash];
    if (sampler == Sampler::eSamplerPointClamp)
    {
        kdd.samplers[base] = CD3DX12_STATIC_SAMPLER_DESC(base, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    }
    else if (sampler == Sampler::eSamplerPointMirror)
    {
        kdd.samplers[base] = CD3DX12_STATIC_SAMPLER_DESC(base, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
    }
    else if (sampler == Sampler::eSamplerLinearClamp)
    {
        kdd.samplers[base] = CD3DX12_STATIC_SAMPLER_DESC(base, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    }
    else if (sampler == Sampler::eSamplerLinearMirror)
    {
        kdd.samplers[base] = CD3DX12_STATIC_SAMPLER_DESC(base, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR, D3D12_TEXTURE_ADDRESS_MODE_MIRROR);
    }
    else
    {
        kdd.samplers[base] = CD3DX12_STATIC_SAMPLER_DESC(base, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
    }
    kdd.numSamplers = std::max(base + 1, kdd.numSamplers);

    return eComputeStatusOk;
}

ComputeStatus D3D12::bindConsts(uint32_t pos, uint32_t base, void *data, size_t dataSize, uint32_t instances)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!ctx.kernel) return eComputeStatusInvalidArgument;

    auto &kdd = (*ctx.kddMap)[ctx.kernel->hash];
    if (kdd.addSlot(kdd.slot))
    {
        kdd.rootRanges[kdd.slot].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, base);
        kdd.rootParameters[kdd.slot].InitAsConstantBufferView(base);
        kdd.cb[kdd.slot] = new ConstantBuffer();
        kdd.cb[kdd.slot]->create(m_device, (uint32_t)dataSize, instances);
    }

    if (data)
    {
        auto idx = kdd.cb[kdd.slot]->getIndex();
        kdd.cb[kdd.slot]->copyStagingToGpu(data, idx);
        kdd.handles[kdd.slot] = kdd.cb[kdd.slot]->getGpuVirtualAddress(idx);
        kdd.cb[kdd.slot]->advanceIndex();
    }

#ifndef SL_PRODUCTION
    kdd.validate(kdd.slot, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, base);
    //SL_LOG_INFO("binding CBV 0x%llx", kdd.handles[slot]);
#endif
    kdd.slot++;
    return eComputeStatusOk;
}

ComputeStatus D3D12::bindTexture(uint32_t pos, uint32_t base, Resource resource, uint32_t mipOffset, uint32_t mipLevels)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!ctx.kernel) return eComputeStatusInvalidArgument;

    auto &kdd = (*ctx.kddMap)[ctx.kernel->hash];
    if (kdd.addSlot(kdd.slot))
    {
        kdd.rootRanges[kdd.slot].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, base);
        kdd.rootParameters[kdd.slot].InitAsDescriptorTable(1, &kdd.rootRanges[kdd.slot]);
    }

    // Resource can be null if shader is not using this slot
    if (resource)
    {
        ResourceDriverData data = {};
        CHI_CHECK(getTextureDriverData(resource, data, mipOffset, mipLevels));
        auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->descriptorHeap[ctx.node]->GetGPUDescriptorHandleForHeapStart(), data.descIndex, m_descriptorSize);
        kdd.handles[kdd.slot] = handle.ptr;

#ifndef SL_PRODUCTION
        kdd.validate(kdd.slot, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, base);
        //std::wstring name = getDebugName(resource);
        //chi::ResourceDescription desc;
        //getResourceDescription(resource, desc);
        //SL_LOG_INFO("binding SRV 0x%llx(%S) mip:%u mips:%u (%u,%u:%u)", kdd.handles[slot], name.c_str(), mipOffset, mipLevels, desc.width, desc.height, desc.mips);
#endif
    }
    else
    {
        kdd.handles[kdd.slot] = 0;
    }
    kdd.slot++;
    return eComputeStatusOk;
}

ComputeStatus D3D12::bindRWTexture(uint32_t pos, uint32_t base, Resource resource, uint32_t mipOffset)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!ctx.kernel) return eComputeStatusInvalidArgument;

    auto &kdd = (*ctx.kddMap)[ctx.kernel->hash];
    if (kdd.addSlot(kdd.slot))
    {
        kdd.rootRanges[kdd.slot].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, base);
        kdd.rootParameters[kdd.slot].InitAsDescriptorTable(1, &kdd.rootRanges[kdd.slot]);
    }

    // Resource can be null if shader is not using this slot
    if (resource)
    {
        ResourceDriverData data = {};
        CHI_CHECK(getSurfaceDriverData((ID3D12Resource*)resource, data, mipOffset));
        auto handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->descriptorHeap[ctx.node]->GetGPUDescriptorHandleForHeapStart(), data.descIndex, m_descriptorSize);
        kdd.handles[kdd.slot] = handle.ptr;
#ifndef SL_PRODUCTION
        kdd.validate(kdd.slot, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, base);
        //std::wstring name = getDebugName(resource);
        //chi::ResourceDescription desc;
        //getResourceDescription(resource, desc);
        //SL_LOG_INFO("binding UAV 0x%llx(%S) mip:%u (%u,%u:%u)", kdd.handles[slot], name.c_str(), mipOffset, desc.width, desc.height, desc.mips);
#endif
    }
    else
    {
        kdd.handles[kdd.slot] = 0;
    }
    kdd.slot++;
    return eComputeStatusOk;
}

ComputeStatus D3D12::bindRawBuffer(uint32_t pos, uint32_t base, Resource resource)
{
    // This is still just a UAV for D3D12 so reuse the other method
    // Note that UAV creation checks for buffers and modifies view accordingly (D3D12_BUFFER_UAV_FLAG_RAW etc.)
    return bindRWTexture(pos, base, resource);
}

ComputeStatus D3D12::dispatch(unsigned int blocksX, unsigned int blocksY, unsigned int blocksZ)
{
    auto& ctx = m_dispatchContext.getContext();
    if (!ctx.kernel) return eComputeStatusInvalidArgument;

    auto &kdd = (*ctx.kddMap)[ctx.kernel->hash];
    ComputeStatus Res = eComputeStatusOk;
    
    {
        if (!kdd.rootSignature)
        {
            CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
            rootSignatureDesc.Init((UINT)kdd.rootParameters.size(), kdd.rootParameters.data(), kdd.numSamplers, kdd.samplers, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
            auto hash = hashRootSignature(rootSignatureDesc);
            auto node = ctx.node << 1;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_rootSignatureMap.find(hash);
                if (it == m_rootSignatureMap.end())
                {
                    ID3DBlob *signature;
                    ID3DBlob *error;
                    D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
                    if (error)
                    {
                        SL_LOG_ERROR("D3D12SerializeRootSignature failed %s", (const char*)error->GetBufferPointer());
                        error->Release();
                        return eComputeStatusError;
                    }
                    if (FAILED(m_device->CreateRootSignature(node, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&kdd.rootSignature))))
                    {
                        SL_LOG_ERROR("Failed to create root signature");
                        return eComputeStatusError;
                    }
                    SL_LOG_VERBOSE("Created root signature 0x%llx with hash %llu", kdd.rootSignature, hash);
                    m_rootSignatureMap[hash] = kdd.rootSignature;
                }
                else
                {
                    kdd.rootSignature = (*it).second;
                }
            }

            {
                hash_combine(hash, ctx.kernel->hash);
                std::lock_guard<std::mutex> lock(m_mutex);
                auto it = m_psoMap.find(hash);
                if (it == m_psoMap.end())
                {
                    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
                    psoDesc.pRootSignature = kdd.rootSignature;
                    psoDesc.CS = { ctx.kernel->kernelBlob.data(), ctx.kernel->kernelBlob.size() };
                    psoDesc.NodeMask = node;
                    if (FAILED(m_device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&kdd.pso))))
                    {
                        SL_LOG_ERROR("Failed to create CS pipeline state");
                        return eComputeStatusError;
                    }
                    SL_LOG_VERBOSE("Created pipeline state 0x%llx with hash %llu", kdd.pso, hash);
                    m_psoMap[hash] = kdd.pso;
                }
                else
                {
                    kdd.pso = (*it).second;
                }
            }
        }

        if (!kdd.rootSignature || !kdd.pso)
        {
            SL_LOG_ERROR("Failed to create root signature or pso for kernel %s:%s", ctx.kernel->name.c_str(), ctx.kernel->entryPoint.c_str());
            return eComputeStatusError;
        }

        ctx.cmdList->SetComputeRootSignature(kdd.rootSignature);
        ctx.cmdList->SetPipelineState(kdd.pso);
        auto numSlots = kdd.rootParameters.size();
        for (auto i = 0; i < numSlots; i++)
        {
            if (kdd.rootParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
            {
                ctx.cmdList->SetComputeRootConstantBufferView(i, { kdd.handles[i] });
            }
            else if (kdd.rootParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            {
                // To avoid triggering debug layer error, nullptr is not allowed
                if (kdd.handles[i])
                {
                    ctx.cmdList->SetComputeRootDescriptorTable(i, { kdd.handles[i] });
                }
            }
        }
        ctx.cmdList->Dispatch(blocksX, blocksY, blocksZ);
    }

    return Res;
}

size_t D3D12::hashRootSignature(const CD3DX12_ROOT_SIGNATURE_DESC& desc)
{
    size_t h = 0;
    hash_combine(h, desc.Flags);
    hash_combine(h, desc.NumParameters);
    hash_combine(h, desc.NumStaticSamplers);
    for (uint32_t i = 0; i < desc.NumStaticSamplers; i++)
    {
        hash_combine(h, desc.pStaticSamplers[i].Filter);
        hash_combine(h, desc.pStaticSamplers[i].ShaderRegister);
        hash_combine(h, desc.pStaticSamplers[i].AddressU);
        hash_combine(h, desc.pStaticSamplers[i].AddressV);
        hash_combine(h, desc.pStaticSamplers[i].AddressW);
        hash_combine(h, desc.pStaticSamplers[i].MipLODBias);
        hash_combine(h, desc.pStaticSamplers[i].ShaderVisibility);
    }
    for (uint32_t i = 0; i < desc.NumParameters; i++)
    {
        hash_combine(h, desc.pParameters[i].ParameterType);
        hash_combine(h, desc.pParameters[i].ShaderVisibility);
        if (desc.pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            hash_combine(h, desc.pParameters[i].DescriptorTable.NumDescriptorRanges);
            for (uint32_t j = 0; j < desc.pParameters[i].DescriptorTable.NumDescriptorRanges; j++)
            {
                hash_combine(h, desc.pParameters[i].DescriptorTable.pDescriptorRanges[j].RangeType);
            }
        }
        else if (desc.pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
        {
            hash_combine(h, desc.pParameters[i].Descriptor.RegisterSpace);
        }
        else
        {
            SL_LOG_ERROR("Unsupported parameter type in root signature");
        }
    }
    return h;
}

UINT D3D12::getNewAndIncreaseDescIndex()
{
    // This method is thread safe since it is just a helper for cache texture or surface

    auto node = 0; // FIX THIS
    if ((m_heap->descIndex[node] + 1) >= SL_MAX_D3D12_DESCRIPTORS)
    {
        // We've looped around our Descriptor heap
        // There's no way we can keep the old cached Descriptors as valid..
        // Invalidate all caches and force the SetInput() calls to set up new ones
        // It will be a slow burn for a while, but hopefully that isn't too frequent ??
        SL_LOG_WARN("D3D12 Descriptor heap wrap around. Clearing all cache and reallocating from scratch again. This is impacting performance - please do NOT change the tagged resources every frame");
        memset(m_heap->descIndex, 0, MAX_NUM_NODES * sizeof(UINT));
        assert(m_heap->descIndex[node] == 0);
        m_heap->wrapAroundCount = (m_heap->wrapAroundCount + 1) % SL_DESCRIPTOR_WRAPAROUND_CAPACITY;
        m_resourceData.clear();
    }
    UINT ReturnedIndex = m_heap->descIndex[node] + SL_MAX_D3D12_DESCRIPTORS * m_heap->wrapAroundCount;
    m_heap->descIndex[node] = (m_heap->descIndex[node] + 1) % SL_MAX_D3D12_DESCRIPTORS;

    return ReturnedIndex;
}

ComputeStatus D3D12::getTextureDriverData(Resource res, ResourceDriverData &data, uint32_t mipOffset, uint32_t mipLevels, Sampler sampler)
{
    ID3D12Resource *resource = (ID3D12Resource*)res;
    if (!resource) return eComputeStatusInvalidArgument;

    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t hash = (mipOffset << 16) | mipLevels;

    auto it = m_resourceData.find(resource);
    if (it == m_resourceData.end() || (*it).second.find(hash) == (*it).second.end())
    {
        auto node = 0; // FIX THIS
        data.descIndex = getNewAndIncreaseDescIndex();
        auto currentCPUHandle =  CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->descriptorHeap[node]->GetCPUDescriptorHandleForHeapStart(), data.descIndex, m_descriptorSize);

        D3D12_RESOURCE_DESC desc = resource->GetDesc();

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Format = getCorrectFormat(desc.Format);
        SRVDesc.Texture2D.MipLevels = mipLevels ? mipLevels : desc.MipLevels;
        SRVDesc.Texture2D.MostDetailedMip = mipOffset;

        auto name = getDebugName(resource);
        SL_LOG_VERBOSE("Caching texture 0x%llx(%S) node %u fmt %s size (%u,%u) mip %u mips %u sampler[%d]", resource, name.c_str(), node, getDXGIFormatStr(desc.Format), (UINT)desc.Width, (UINT)desc.Height, SRVDesc.Texture2D.MostDetailedMip, SRVDesc.Texture2D.MipLevels, sampler);

        m_device->CreateShaderResourceView(resource, &SRVDesc, currentCPUHandle);

        data.heap = m_heap;

        m_resourceData[resource][hash] = data;
    }
    else
    {
        data = (*it).second[hash];
    }
    assert(data.heap == m_heap);
    return eComputeStatusOk;
}

ComputeStatus D3D12::getSurfaceDriverData(Resource res, ResourceDriverData &data, uint32_t mipOffset)
{
    ID3D12Resource *resource = (ID3D12Resource*)res;
    if (!resource) return eComputeStatusInvalidArgument;

    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t hash = mipOffset << 16;

    auto it = m_resourceData.find(resource);
    if (it == m_resourceData.end() || (*it).second.find(hash) == (*it).second.end())
    {
        auto node = 0; // FIX THIS
        data.descIndex = getNewAndIncreaseDescIndex();
        

        D3D12_RESOURCE_DESC desc = resource->GetDesc();

        auto name = getDebugName(resource);
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            UAVDesc.Buffer.CounterOffsetInBytes = 0;
            UAVDesc.Buffer.FirstElement = 0;
            UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            UAVDesc.Buffer.NumElements = (UINT)desc.Width / 4;
            UAVDesc.Buffer.StructureByteStride = 0;

            SL_LOG_VERBOSE("Caching raw buffer 0x%llx(%S) node %u fmt %s size (%u,%u)", resource, name.c_str(), node, getDXGIFormatStr(desc.Format), (UINT)desc.Width, (UINT)desc.Height);
        }
        else
        {
            UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            UAVDesc.Format = getCorrectFormat(desc.Format);
            UAVDesc.Texture2D.MipSlice = mipOffset;

            if (!isSupportedFormat(UAVDesc.Format, 0, D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
            {
                SL_LOG_ERROR("Format %s cannot be used as UAV", getDXGIFormatStr(UAVDesc.Format));
                return eComputeStatusError;
            }

            SL_LOG_VERBOSE("Caching rwtexture 0x%llx(%S) node %u fmt %s size (%u,%u) mip %u", resource, name.c_str(), node, getDXGIFormatStr(desc.Format), (UINT)desc.Width, (UINT)desc.Height, UAVDesc.Texture2D.MipSlice);
        }
        
        {
            auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->descriptorHeap[node]->GetCPUDescriptorHandleForHeapStart(), data.descIndex, m_descriptorSize);
            m_device->CreateUnorderedAccessView(resource, nullptr, &UAVDesc, cpuHandle);
        }

        data.heap = m_heap;

        m_resourceData[resource][hash] = data;
    }
    else
    {
        data = (*it).second[hash];
    }
    assert(data.heap == m_heap);

    return eComputeStatusOk;
}

bool D3D12::isSupportedFormat(DXGI_FORMAT format, int flag1, int flag2)
{
    // Make sure all typeless formats are converted before the check is done
    D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = { getCorrectFormat(format), D3D12_FORMAT_SUPPORT1_NONE, D3D12_FORMAT_SUPPORT2_NONE };
    HRESULT hr = m_device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));
    if (SUCCEEDED(hr))
    {
        return (FormatSupport.Support1 & flag1) != 0 || (FormatSupport.Support2 & flag2) != 0;
    }
    SL_LOG_ERROR("Format %s is unsupported - hres %lu flags %d %d", getDXGIFormatStr(format), hr, flag1, flag2);
    return false;
}

ComputeStatus D3D12::createTexture2DResourceSharedImpl(ResourceDescription &InOutResourceDesc, Resource &OutResource, bool UseNativeFormat, ResourceState InitialState)
{
    ID3D12Resource *Res = nullptr;
    D3D12_HEAP_TYPE NativeHeapType;
    D3D12_RESOURCE_STATES NativeInitialState = toD3D12States(InitialState);
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Alignment = 65536;
    texDesc.MipLevels = (UINT16)InOutResourceDesc.mips;
    if (UseNativeFormat)
    {
        assert(InOutResourceDesc.nativeFormat != NativeFormatUnknown &&
                 InOutResourceDesc.format == eFormatINVALID);
        texDesc.Format = (DXGI_FORMAT)InOutResourceDesc.nativeFormat;
    }
    else
    {
        assert(InOutResourceDesc.nativeFormat == NativeFormatUnknown && InOutResourceDesc.format != eFormatINVALID);
        NativeFormat native;
        getNativeFormat(InOutResourceDesc.format, native);
        texDesc.Format = (DXGI_FORMAT)native;
    }
    texDesc.Width = InOutResourceDesc.width;
    texDesc.Height = InOutResourceDesc.height;
    switch(InOutResourceDesc.heapType)
    {
    case eHeapTypeReadback:
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        NativeHeapType = D3D12_HEAP_TYPE_READBACK;
        break;
    case eHeapTypeUpload:
        texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        NativeHeapType = D3D12_HEAP_TYPE_UPLOAD;
        break;
    default:
        assert(0);// fall through
    case eHeapTypeDefault:
        texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        NativeHeapType = D3D12_HEAP_TYPE_DEFAULT;
        break;
    }
    texDesc.DepthOrArraySize = 1;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    CD3DX12_HEAP_PROPERTIES heapProp(NativeHeapType, InOutResourceDesc.creationMask, InOutResourceDesc.visibilityMask ? InOutResourceDesc.visibilityMask : m_visibleNodeMask);

    if (m_allocateCallback)
    {
        ResourceDesc desc = { ResourceType::eResourceTypeTex2d, &texDesc, (uint32_t)NativeInitialState, &heapProp, nullptr };
        auto result = m_allocateCallback(&desc);
        Res = (ID3D12Resource*)result.native;
    }
    else
    {
        m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &texDesc, NativeInitialState, nullptr, IID_PPV_ARGS(&Res));
    }

    OutResource = Res;
    if (!OutResource)
    {
        SL_LOG_ERROR(" CreateCommittedResource failed");
        return eComputeStatusError;
    }
    return eComputeStatusOk;
}

ComputeStatus D3D12::createBufferResourceImpl(ResourceDescription &InOutResourceDesc, Resource &OutResource, ResourceState InitialState)
{
    ID3D12Resource *Res = nullptr;
    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.Width = InOutResourceDesc.width;
    assert(InOutResourceDesc.height == 1);
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.SampleDesc.Quality = 0;
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    switch (InOutResourceDesc.heapType)
    {
    default: assert(0); // Fall through
    case eHeapTypeDefault:
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        break;
    case eHeapTypeUpload:
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        break;
    case eHeapTypeReadback:
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        break;
    }

    D3D12_HEAP_TYPE NativeHeapType = (D3D12_HEAP_TYPE) InOutResourceDesc.heapType; // TODO : proper conversion !

    D3D12_RESOURCE_STATES NativeInitialState = toD3D12States(InitialState);

    CD3DX12_HEAP_PROPERTIES heapProp(NativeHeapType, InOutResourceDesc.creationMask, InOutResourceDesc.visibilityMask ? InOutResourceDesc.visibilityMask : m_visibleNodeMask);

    if (m_allocateCallback)
    {
        ResourceDesc desc = { ResourceType::eResourceTypeBuffer, &bufferDesc, (uint32_t)NativeInitialState, &heapProp };
        auto result = m_allocateCallback(&desc);
        Res = (ID3D12Resource*)result.native;
    }
    else
    {
        m_device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &bufferDesc, NativeInitialState, nullptr, IID_PPV_ARGS(&Res));
    }
    OutResource = Res;
    if (!OutResource)
    {
        SL_LOG_ERROR(" CreateCommittedResource failed");
        return eComputeStatusError;
    }
    return eComputeStatusOk;
}

ComputeStatus D3D12::setDebugName(Resource res, const char name[])
{
    ID3D12Pageable *resource = (ID3D12Pageable*)res;
    resource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(name), name);
    return eComputeStatusOk;
}

ComputeStatus D3D12::copyHostToDeviceBufferImpl(CommandList InCmdList, uint64_t InSize, const void *InData, Resource InUploadResource, Resource InTargetResource, unsigned long long InUploadOffset, unsigned long long InDstOffset)
{
    UINT8 *StagingPtr = nullptr;

    ID3D12Resource *Resource = (ID3D12Resource*)InTargetResource;
    ID3D12Resource *Scratch = (ID3D12Resource*)InUploadResource;

    //SL_LOG_INFO("Scratch size %llu", Scratch->GetDesc().Width * Scratch->GetDesc().Height);

    HRESULT hr = Scratch->Map(0, NULL, reinterpret_cast<void**>(&StagingPtr));
    if (hr != S_OK)
    {
        SL_LOG_ERROR(" failed to map buffer - error 0x%lx", hr);
        return eComputeStatusError;
    }

    memcpy(StagingPtr + InUploadOffset, InData, InSize);

    Scratch->Unmap(0, nullptr);

    ((ID3D12GraphicsCommandList*)InCmdList)->CopyBufferRegion(Resource, InDstOffset, Scratch, InUploadOffset, InSize);

    return eComputeStatusOk;
}

ComputeStatus D3D12::writeTextureImpl(CommandList InCmdList, uint64_t InSize, uint64_t RowPitch, const void* InData, Resource InTargetResource, Resource& InUploadResource)
{
    uint64_t depthPitch = 1;
    uint64_t mipLevel = 0;
    uint64_t arraySlice = 0;
    ID3D12Resource* dest = (ID3D12Resource*)InTargetResource;
    D3D12_RESOURCE_DESC resourceDesc = dest->GetDesc();

    uint32_t subresource = calcSubresource((uint32_t)mipLevel, (uint32_t)arraySlice, 0, (uint32_t)resourceDesc.MipLevels, resourceDesc.DepthOrArraySize);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    uint32_t numRows;
    uint64_t rowSizeInBytes;
    uint64_t totalBytes;

    m_device->GetCopyableFootprints(&resourceDesc, subresource, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);
    assert(numRows <= footprint.Footprint.Height);

    ResourceDescription bufferDesc((uint32_t)totalBytes, 1, chi::eFormatINVALID, HeapType::eHeapTypeUpload, ResourceState::eUnknown);
    CHI_CHECK(createBuffer(bufferDesc, InUploadResource, "writeUploadBuffer"));
    if (!InUploadResource) {
        return eComputeStatusError;
    }

    void* cpuVA = nullptr;
    ID3D12Resource* uploadBuffer = (ID3D12Resource*)InUploadResource;
    uploadBuffer->Map(0, nullptr, &cpuVA);    
    for (uint32_t depthSlice = 0; depthSlice < footprint.Footprint.Depth; depthSlice++)
    {
        for (uint32_t row = 0; row < numRows; row++)
        {
            void* destAddress = (char*)cpuVA + footprint.Footprint.RowPitch * (row + depthSlice * numRows);
            void* srcAddress = (char*)InData + RowPitch * row + depthPitch * depthSlice;
            memcpy(destAddress, srcAddress, std::min(RowPitch, rowSizeInBytes));
        }
    }
    uploadBuffer->Unmap(0, nullptr);

    D3D12_TEXTURE_COPY_LOCATION destCopyLocation = {};
    destCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destCopyLocation.SubresourceIndex = subresource;
    destCopyLocation.pResource = dest;

    D3D12_TEXTURE_COPY_LOCATION srcCopyLocation = {};
    srcCopyLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcCopyLocation.PlacedFootprint = footprint;
    srcCopyLocation.pResource = uploadBuffer;

    ((ID3D12GraphicsCommandList*)InCmdList)->CopyTextureRegion(&destCopyLocation, 0, 0, 0, &srcCopyLocation, nullptr);
    return eComputeStatusOk;
}

ComputeStatus D3D12::clearView(CommandList InCmdList, Resource InResource, const float4 Color, const RECT * pRects, unsigned int NumRects, CLEAR_TYPE &outType)
{
    outType = CLEAR_UNDEFINED;
    
    ID3D12Resource *Resource = (ID3D12Resource*) InResource;
    ResourceDriverData Data = {};
    if (getSurfaceDriverData(Resource, Data) == eComputeStatusOk)
    {
        auto node = 0; // FIX THIS
        CD3DX12_CPU_DESCRIPTOR_HANDLE CPUVisibleCPUHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_heap->descriptorHeapCPU[node]->GetCPUDescriptorHandleForHeapStart(), Data.descIndex, m_descriptorSize);
        CD3DX12_GPU_DESCRIPTOR_HANDLE GPUHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_heap->descriptorHeap[node]->GetGPUDescriptorHandleForHeapStart(), Data.descIndex, m_descriptorSize);

        ((ID3D12GraphicsCommandList*)InCmdList)->ClearUnorderedAccessViewFloat(GPUHandle, CPUVisibleCPUHandle, (ID3D12Resource*)InResource, (const FLOAT*)&Color, NumRects, pRects);

        outType = Data.bZBCSupported ? CLEAR_ZBC_WITH_PADDING : CLEAR_NON_ZBC;
        return eComputeStatusOk;
    }
    return eComputeStatusError;
}

ComputeStatus D3D12::insertGPUBarrierList(CommandList InCmdList, const Resource* InResources, unsigned int InResourceCount, BarrierType InBarrierType)
{
    if (InBarrierType == BarrierType::eBarrierTypeUAV)
    {
        std::vector< D3D12_RESOURCE_BARRIER> Barriers;
        for (unsigned int i = 0; i < InResourceCount; i++)
        {
            const Resource& Res = InResources[i];
            Barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV((ID3D12Resource*)Res));
        }
        ((ID3D12GraphicsCommandList*)InCmdList)->ResourceBarrier((UINT)Barriers.size(), Barriers.data());
    }
    else
    {
        assert(false);
        return eComputeStatusNotSupported;
    }
    return eComputeStatusOk;
}

ComputeStatus D3D12::insertGPUBarrier(CommandList InCmdList, Resource InResource, BarrierType InBarrierType)
{
    if (InBarrierType == BarrierType::eBarrierTypeUAV)
    {
        D3D12_RESOURCE_BARRIER UAV = CD3DX12_RESOURCE_BARRIER::UAV((ID3D12Resource*)InResource);
        ((ID3D12GraphicsCommandList*)InCmdList)->ResourceBarrier(1, &UAV);
    }
    else
    {
        assert(false);
        return eComputeStatusNotSupported;
    }
    return eComputeStatusOk;
}

ComputeStatus D3D12::transitionResourceImpl(CommandList cmdList, const ResourceTransition *transitions, uint32_t count)
{
    if (!cmdList || !transitions)
    {
        return eComputeStatusInvalidArgument;
    }
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    while (count--)
    {
        if (transitions[count].from != transitions[count].to)
        {
            auto from = toD3D12States(transitions[count].from);
            auto to = toD3D12States(transitions[count].to);
            //SL_LOG_HINT("%S %s->%s  %s->%s", getDebugName(transitions[count].resource).c_str(),d3d12state2str(from).c_str(), d3d12state2str(to).c_str(), state2str(transitions[count].from).c_str(), state2str(transitions[count].to).c_str());
            barriers.push_back({ CD3DX12_RESOURCE_BARRIER::Transition((ID3D12Resource*)transitions[count].resource, from, to, transitions[count].subresource) });
        }
    }
    ((ID3D12GraphicsCommandList*)cmdList)->ResourceBarrier((uint32_t)barriers.size(), barriers.data());
    return eComputeStatusOk;
}

ComputeStatus D3D12::copyResource(CommandList InCmdList, Resource InDstResource, Resource InSrcResource)
{
    if (!InCmdList || !InDstResource || !InSrcResource) return eComputeStatusInvalidArgument;
    ((ID3D12GraphicsCommandList*)InCmdList)->CopyResource((ID3D12Resource*)InDstResource, (ID3D12Resource*)InSrcResource);
    return eComputeStatusOk;
}

ComputeStatus D3D12::cloneResource(Resource resource, Resource &clone, const char friendlyName[], ResourceState initialState, unsigned int creationMask, unsigned int visibilityMask)
{
    if (!resource) return eComputeStatusInvalidArgument;

    D3D12_RESOURCE_DESC desc1 = ((ID3D12Resource*)resource)->GetDesc();
    ID3D12Resource *res = nullptr;
        
    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT, creationMask, visibilityMask ? visibilityMask : m_visibleNodeMask);

    if (isSupportedFormat(desc1.Format, 0, D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD | D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))    
    {
        desc1.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }
    else
    {
        initialState &= ~ResourceState::eStorageRW;
    }
    if (isSupportedFormat(desc1.Format, D3D12_FORMAT_SUPPORT1_RENDER_TARGET, 0))
    {
        desc1.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;        
    }
    else
    {
        initialState &= ~(ResourceState::eColorAttachmentRead | ResourceState::eColorAttachmentWrite);
    }
    if (isSupportedFormat(desc1.Format, D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL, 0))
    {
        desc1.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    else
    {
        initialState &= ~(ResourceState::eDepthStencilAttachmentRead | ResourceState::eDepthStencilAttachmentWrite);
    }
    if (m_allocateCallback)
    {
        ResourceDesc desc = { desc1.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ? ResourceType::eResourceTypeBuffer : ResourceType::eResourceTypeTex2d, &desc1, (uint32_t)initialState, &heapProp, nullptr };
        auto result = m_allocateCallback(&desc);
        res = (ID3D12Resource*)result.native;
        // This will trigger onHostResourceCreated which will set state correctly
    }
    else
    {        
        HRESULT hr = m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, creationMask, visibilityMask),
            D3D12_HEAP_FLAG_NONE,
            &desc1,
            toD3D12States(initialState),
            nullptr,
            IID_PPV_ARGS(&res));

        if (hr != S_OK)
        {
            SL_LOG_ERROR(" unable to clone resource");
            return eComputeStatusError;
        }
        // Internal resource, set state
        setResourceState(res, initialState);
        setDebugName(res, friendlyName);
        
        ++m_allocCount;
        auto currentSize = getResourceSize(res);
        m_totalAllocatedSize += currentSize;
        SL_LOG_VERBOSE("Cloning 0x%llx (%s:%u:%u:%s), m_allocCount=%d, currentSize %.1lf MB, totalSize %.1lf MB", res, friendlyName, desc1.Width, desc1.Height, getDXGIFormatStr(desc1.Format), m_allocCount.load(), (double)currentSize / (1024 * 1024), (double)m_totalAllocatedSize.load() / (1024 * 1024));
    }

    clone = res;
    return eComputeStatusOk;
}

ComputeStatus D3D12::copyBufferToReadbackBuffer(CommandList InCmdList, Resource InResource, Resource OutResource, unsigned int InBytesToCopy) 
{
    ID3D12Resource *InD3dResource = (ID3D12Resource*)InResource;
    ID3D12Resource *OutD3dResource = (ID3D12Resource*)OutResource;
    ID3D12GraphicsCommandList* CmdList = (ID3D12GraphicsCommandList*)InCmdList;

#if 0
    ResourceDescription ResourceDesc;
    GetResourceDescription(OutResource, ResourceDesc);
    const size_t readbackBufferSize = ResourceDesc.Width;

    D3D12_RANGE readbackBufferRange{ 0, readbackBufferSize };
    FLOAT * pReadbackBufferData{};
    OutD3dResource->Map(0, &readbackBufferRange, reinterpret_cast<void**>(&pReadbackBufferData));

    memset(pReadbackBufferData, 0xAA, readbackBufferSize);

    D3D12_RANGE emptyRange{ 0, 0 };
    OutD3dResource->Unmap(0, &emptyRange);
#endif

    CmdList->CopyBufferRegion(OutD3dResource, 0, InD3dResource, 0, InBytesToCopy);

    return eComputeStatusOk;
}

ComputeStatus D3D12::getLUIDFromDevice(NVSDK_NGX_LUID *OutId)
{
    memcpy(OutId, &(m_device->GetAdapterLuid()), sizeof(LUID));
    return eComputeStatusOk;
}

ComputeStatus D3D12::beginPerfSection(CommandList cmdList, const char *key, unsigned int node, bool reset)
{
#ifndef SL_PRODUCTION
    PerfData* data = {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto section = m_sectionPerfMap[node].find(key);
        if (section == m_sectionPerfMap[node].end())
        {
            m_sectionPerfMap[node][key] = {};
            section = m_sectionPerfMap[node].find(key);
        }
        data = &(*section).second;
    }
    
    if (reset)
    {
        for (int i = 0; i < SL_READBACK_QUEUE_SIZE; i++)
        {
            data->reset[i] = true;
        }
        data->values.clear();
    }
    
    if (!data->queryHeap[data->queryIdx])
    {
        D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
        queryHeapDesc.Count = 2;
        queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryHeapDesc.NodeMask = (1 << node);
        m_device->CreateQueryHeap(&queryHeapDesc, __uuidof(ID3D12QueryHeap), (void **)&data->queryHeap[data->queryIdx]);

        D3D12_RESOURCE_DESC bufferDesc = {};
        bufferDesc.MipLevels = 1;
        bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
        bufferDesc.Width = 2 * sizeof(uint64_t);
        bufferDesc.Height = 1;
        bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
        bufferDesc.DepthOrArraySize = 1;
        bufferDesc.SampleDesc.Count = 1;
        bufferDesc.SampleDesc.Quality = 0;
        bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COPY_DEST;
        m_device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK, queryHeapDesc.NodeMask, queryHeapDesc.NodeMask), D3D12_HEAP_FLAG_NONE, &bufferDesc, initialState, nullptr, IID_PPV_ARGS(&data->queryBufferReadback[data->queryIdx]));
    }
    else
    {
        ((ID3D12GraphicsCommandList*)cmdList)->ResolveQueryData(data->queryHeap[data->queryIdx], D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, data->queryBufferReadback[data->queryIdx], 0);
        UINT8* pStagingPtr = nullptr;
        uint64_t dxNanoSecondTS[2];
        D3D12_RANGE mapRange = { 0, sizeof(uint64_t) * 2 };
        data->queryBufferReadback[data->queryIdx]->Map(0, &mapRange, reinterpret_cast<void**>(&pStagingPtr));
        if (pStagingPtr)
        {
            dxNanoSecondTS[0] = ((uint64_t*)pStagingPtr)[0];
            dxNanoSecondTS[1] = ((uint64_t*)pStagingPtr)[1];
            data->queryBufferReadback[data->queryIdx]->Unmap(0, NULL);
            float delta = (dxNanoSecondTS[1] - dxNanoSecondTS[0]) / 1e06f;
            if (!data->reset[data->queryIdx])
            {
                if (delta > 0)
                {
                    // Average over last N executions
                    if (data->values.size() == 100)
                    {
                        data->accumulatedTimeMS -= data->values.front();
                        data->values.erase(data->values.begin());
                    }
                    data->accumulatedTimeMS += delta;
                    data->values.push_back(delta);
                }
            }
            else
            {
                data->reset[data->queryIdx] = false;
                data->accumulatedTimeMS = 0;
                data->values.clear();
            }
        }
        else
        {
            data->reset[data->queryIdx] = false;
            data->accumulatedTimeMS = 0;
            data->values.clear();
        }
    }

    ((ID3D12GraphicsCommandList*)cmdList)->EndQuery(data->queryHeap[data->queryIdx], D3D12_QUERY_TYPE_TIMESTAMP, 0);
#endif
    return eComputeStatusOk;
}

ComputeStatus D3D12::endPerfSection(CommandList cmdList, const char* key, float &avgTimeMS, unsigned int node)
{
#ifndef SL_PRODUCTION
    PerfData* data = {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto section = m_sectionPerfMap[node].find(key);
        if (section == m_sectionPerfMap[node].end())
        {
            return eComputeStatusError;
        }
        data = &(*section).second;
    }
    ((ID3D12GraphicsCommandList*)cmdList)->EndQuery(data->queryHeap[data->queryIdx], D3D12_QUERY_TYPE_TIMESTAMP, 1);
    data->queryIdx = (data->queryIdx + 1) % SL_READBACK_QUEUE_SIZE;

    avgTimeMS = data->values.size() ? data->accumulatedTimeMS / (float)data->values.size() : 0;
#else
    avgTimeMS = 0;
#endif
    return eComputeStatusOk;
}

#if SL_ENABLE_PERF_TIMING
ComputeStatus D3D12::beginProfiling(CommandList cmdList, unsigned int Metadata, const void *pData, unsigned int Size)
{
    std::string test(static_cast<const char*>(pData), Size);
    //std::string test = "test";
    ((ID3D12GraphicsCommandList*)cmdList)->BeginEvent(1, test.c_str(), (UINT)test.size());
    return eComputeStatusOk;
}

ComputeStatus D3D12::endProfiling(CommandList cmdList)
{
    ((ID3D12GraphicsCommandList*)cmdList)->EndEvent();
    return eComputeStatusOk;
}

#endif

ComputeStatus D3D12::dumpResource(CommandList cmdList, Resource src, const char *path)
{
    

    ResourceDescription srcDesc;
    CHI_CHECK(getResourceDescription(src, srcDesc));

    ResourceReadbackQueue* rrq = {};
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_readbackMap.find(src);
        if (it == m_readbackMap.end())
        {
            m_readbackMap[src] = {};
            rrq = &m_readbackMap[src];
        }
        else
        {
            rrq = &(*it).second;
        }
    }

    uint32_t bytes = 3 * sizeof(float) * srcDesc.width * srcDesc.height;

    if (rrq->readback[rrq->index])
    {
        for (auto i = m_readbackThreads.begin(); i != m_readbackThreads.end(); i++)
        {
            if ((*i).wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                i = m_readbackThreads.erase(i);
                if (i == m_readbackThreads.end()) break;
            }
        }

        ID3D12Resource *res = (ID3D12Resource*)rrq->readback[rrq->index];
        void *data;
        D3D12_RANGE range{ 0, bytes };
        res->Map(0, &range, &data);
        if (!data)
        {
            SL_LOG_ERROR("Failed to map readback resource");
        }
        else
        {
            std::vector<char> pixels(bytes);
            memcpy(pixels.data(), data, bytes);
            res->Unmap(0, nullptr);
            std::string fpath(path);
            m_readbackThreads.push_back(std::async(std::launch::async, [this, srcDesc, pixels, fpath]()->bool
            {
                return savePFM(fpath, pixels.data(), srcDesc.width, srcDesc.height);
            }));
        }
    }
    else
    {   
        if(!rrq->target)
        {
            ResourceDescription desc(bytes, 1, chi::eFormatINVALID,HeapType::eHeapTypeDefault,ResourceState::eStorageRW,ResourceFlags::eRawOrStructuredBuffer);
            CHI_CHECK(createBuffer(desc, rrq->target, (std::string("chi.target.") + std::to_string((size_t)src)).c_str()));
        }
        ResourceDescription desc(bytes, 1, chi::eFormatINVALID, chi::eHeapTypeReadback, chi::ResourceState::eCopyDestination);
        CHI_CHECK(createBuffer(desc, rrq->readback[rrq->index], (std::string("chi.readback.") + std::to_string((size_t)src) + "." + std::to_string(rrq->index)).c_str()));
    }
    
    {
        extra::ScopedTasks revTransitions;
        chi::ResourceTransition transitions[] =
        {
            {src, chi::ResourceState::eTextureRead, srcDesc.state},
        };
        CHI_CHECK(transitionResources(cmdList, transitions, (uint32_t)countof(transitions), &revTransitions));

        struct alignas(16) CopyDataCB
        {
            sl::float4 sizeAndInvSize;
        };
        CopyDataCB cb;
        cb.sizeAndInvSize = { (float)srcDesc.width, (float)srcDesc.height, 1.0f / (float)srcDesc.width, 1.0f / (float)srcDesc.height };
        CHI_CHECK(bindKernel(m_copyKernel));
        CHI_CHECK(bindConsts(0, 0, &cb, sizeof(CopyDataCB), 3));
        CHI_CHECK(bindTexture(1, 0, src));
        CHI_CHECK(bindRawBuffer(2, 0, rrq->target));
        uint32_t grid[] = { (srcDesc.width + 16 - 1) / 16, (srcDesc.height + 16 - 1) / 16, 1 };
        CHI_CHECK(dispatch(grid[0], grid[1], grid[2]));
    }

    {
        extra::ScopedTasks revTransitions;
        chi::ResourceTransition transitions[] =
        {
            {rrq->target, chi::ResourceState::eCopySource, chi::ResourceState::eStorageRW},
            {rrq->readback[rrq->index], chi::ResourceState::eCopyDestination, chi::ResourceState::eStorageRW},
        };
        CHI_CHECK(transitionResources(cmdList, transitions, (uint32_t)countof(transitions), &revTransitions));
        CHI_CHECK(copyResource(cmdList, rrq->readback[rrq->index], rrq->target));
    }

    rrq->index = (rrq->index + 1) % SL_READBACK_QUEUE_SIZE;
        
    return eComputeStatusOk;
}

void D3D12::destroyResourceDeferredImpl(const Resource resource)
{   
    auto unknown = (IUnknown*)resource;
    ID3D12Pageable* pageable;
    unknown->QueryInterface(&pageable);
    uint64_t currentSize = 0;
    if (pageable)
    {
        pageable->Release();
        if (m_allocCount && m_totalAllocatedSize)
        {
            m_allocCount--;
            currentSize = getResourceSize(resource);
            if (m_totalAllocatedSize >= currentSize)
            {
                m_totalAllocatedSize -= currentSize;
            }
        }
    }
    auto name = getDebugName((ID3D12Resource*)resource);
    auto ref = ((IUnknown*)resource)->Release();
    SL_LOG_VERBOSE("Releasing resource 0x%llx (%S) ref count %u - currentSize %.2f - totalSize %.2f", resource, name.c_str(), ref, currentSize / (1024.0f * 1024.0f), m_totalAllocatedSize / (1024.0f * 1024.0f));
}

DXGI_FORMAT D3D12::getCorrectFormat(DXGI_FORMAT Format)
{
    switch (Format)
    {
    case DXGI_FORMAT_D16_UNORM: // casting from non typeless is supported from RS2+
        assert(m_dbgSupportRs2RelaxedConversionRules);
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_D32_FLOAT: // casting from non typeless is supported from RS2+
        assert(m_dbgSupportRs2RelaxedConversionRules); // fallthrough
    case DXGI_FORMAT_R32_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_R32G32_TYPELESS:
        return DXGI_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R16G16_TYPELESS:
        return DXGI_FORMAT_R16G16_FLOAT;
    case DXGI_FORMAT_R16_TYPELESS:
        return DXGI_FORMAT_R16_FLOAT;
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        return DXGI_FORMAT_B8G8R8X8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case DXGI_FORMAT_D24_UNORM_S8_UINT: // casting from non typeless is supported from RS2+
        assert(m_dbgSupportRs2RelaxedConversionRules); // fallthrough
    case DXGI_FORMAT_R24G8_TYPELESS:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: // casting from non typeless is supported from RS2+
        assert(m_dbgSupportRs2RelaxedConversionRules); // fallthrough
    case DXGI_FORMAT_R32G8X24_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:
        return Format;
    };
}

// {694B3E1C-0E33-416F-BA83-FE248DA1E85D}
static const GUID sResourceStateGUID = { 0x694b3e1c, 0xe33, 0x416f, { 0xba, 0x83, 0xfe, 0x24, 0x8d, 0xa1, 0xe8, 0x5d } };

ComputeStatus D3D12::onHostResourceCreated(Resource resource, const ResourceInfo& info)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        // If we have this resource in cache then it is stale since MS just created a new one with recycled pointer
        auto it = m_resourceData.find(resource);
        if (it != m_resourceData.end())
        {
            auto name = getDebugName((ID3D12Resource*)resource);
            SL_LOG_VERBOSE("Detected stale resource 0x%llx(%S) - removing from cache", resource, name.c_str());
            m_resourceData.erase(it);
        }
    }

    setResourceState(resource, info.desc.state);
    
    // Not calling generic method since we already locked on mutex
    //m_resourceStateMap[resource] = info;

    return eComputeStatusOk;
}

ComputeStatus D3D12::setResourceState(Resource resource, ResourceState state, uint32_t subresource)
{
    if (!resource)
    {
        return eComputeStatusOk;
    }

    ID3D12Pageable* pageable = {};
    ((IUnknown*)resource)->QueryInterface(&pageable);
    if (pageable)
    {
        if (FAILED(pageable->SetPrivateData(sResourceStateGUID, sizeof(state), &state)))
        {
            SL_LOG_ERROR("Failed to set state for resource 0x%llx", resource);
        }
        pageable->Release();
    }
    return eComputeStatusOk;
}

ComputeStatus D3D12::getResourceState(Resource resource, ResourceState& state)
{
    if (!resource)
    {
        state = ResourceState::eUnknown;
        return eComputeStatusOk;
    }

    state = ResourceState::eGeneral;

    ID3D12Pageable* pageable = {};
    ((IUnknown*)resource)->QueryInterface(&pageable);
    if (pageable)
    {
        uint32_t size = sizeof(state);
        if (FAILED(pageable->GetPrivateData(sResourceStateGUID, &size, &state)))
        {
            SL_LOG_ERROR("resource 0x%llx does not have a state", resource);
            return eComputeStatusInvalidArgument;
        }
        pageable->Release();
    }

    return eComputeStatusOk;
}

ComputeStatus D3D12::getResourceDescription(Resource resource, ResourceDescription& outDesc)
{
    if (!resource) return eComputeStatusInvalidArgument;

    // First make sure this is not an DXGI or some other resource
    auto unknown = (IUnknown*)resource;
    ID3D12Resource* pageable;
    unknown->QueryInterface(&pageable);
    if (!pageable)
    {
        return eComputeStatusError;
    }

    D3D12_RESOURCE_DESC desc = ((ID3D12Resource*)resource)->GetDesc();
    outDesc = {};
    outDesc.format = eFormatINVALID;
    outDesc.width = (UINT)desc.Width;
    outDesc.height = desc.Height;
    outDesc.nativeFormat = desc.Format;
    outDesc.mips = desc.MipLevels;
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        outDesc.gpuVirtualAddress = pageable->GetGPUVirtualAddress();
        outDesc.flags |= ResourceFlags::eRawOrStructuredBuffer | ResourceFlags::eConstantBuffer;
    }
    else
    {
        outDesc.flags |= ResourceFlags::eShaderResource;
    }
    if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    {
        outDesc.flags |= ResourceFlags::eShaderResourceStorage;
    }

    uint32_t size = sizeof(outDesc.state);
    if (FAILED(pageable->GetPrivateData(sResourceStateGUID, &size, &outDesc.state)))
    {
        std::wstring name = getDebugName(resource);
        SL_LOG_ERROR("resource 0x%llx(%S) does not have a state", resource, name.c_str());
        return eComputeStatusInvalidArgument;
    }
    pageable->Release();

    return eComputeStatusOk;
}

}
}
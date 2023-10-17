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

#include <wrl/client.h>

#include "source/core/sl.interposer/d3d12/d3d12Device.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandList.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandQueue.h"

#include "source/core/sl.api/internal.h"
#include "source/core/sl.interposer/hook.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin-manager/pluginManager.h"
#include "include/sl_hooks.h"
#include "source/platforms/sl.chi/compute.h"

namespace sl
{
namespace interposer
{

static_assert(offsetof(D3D12Device, m_base) == 16, "This location must be maintained to keep compatibility with Nsight tools");

#ifndef SL_PRODUCTION
const char* getDXGIFormatStr(uint32_t format)
{
    static constexpr const char* GDXGI_FORMAT_STR[] = {
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

struct TrackingInfo
{
    std::mutex mtx;
    std::vector<ID3D12Resource*> resources;
    chi::ICompute* compute = {};
    uint64_t totalBytes = 0;
    bool enabled = false;
};

TrackingInfo s_track{};

extern "C"
{
    void updateTrackedResources()
    {
        if (!s_track.enabled) return;

        std::scoped_lock lock(s_track.mtx);

        if (!s_track.compute)
        {
            param::getPointerParam(param::getInterface(), sl::param::common::kComputeAPI, &s_track.compute);
        }

        auto it = s_track.resources.begin();
        while (it != s_track.resources.end())
        {
            auto res = (*it);
            res->AddRef();
            if (res->Release() == 1)
            {
                chi::ResourceFootprint footprint;
                sl::Resource r{};
                r.native = res;
                s_track.compute->getResourceFootprint(&r, footprint);
                s_track.totalBytes -= footprint.totalBytes;

                D3D12_RESOURCE_DESC desc = res->GetDesc();
                SL_LOG_HINT("Engine released 0x%llx (%u,%u):%s, total: %lluMB", res, (uint32_t)desc.Width, desc.Height, getDXGIFormatStr(desc.Format), s_track.totalBytes / (1024 * 1024));

                res->Release();
                it = s_track.resources.erase(it);
                continue;
            }
            it++;
        }
    }
}

void trackResource(ID3D12Resource* resource)
{
    if (!s_track.enabled) return;

    std::scoped_lock lock(s_track.mtx);

    if (!s_track.compute)
    {
        param::getPointerParam(param::getInterface(), sl::param::common::kComputeAPI, &s_track.compute);
    }

    uint64_t bytes = 0;
    if (s_track.compute)
    {
        chi::ResourceFootprint footprint;
        sl::Resource r{};
        r.native = resource;
        s_track.compute->getResourceFootprint(&r, footprint);
        bytes = footprint.totalBytes;
        s_track.totalBytes += bytes;

        bool found = false;
        auto it = s_track.resources.begin();
        while (it != s_track.resources.end())
        {
            auto res = (*it);
            res->AddRef();
            if (res->Release() == 1)
            {
                chi::ResourceFootprint footprint;
                sl::Resource r{};
                r.native = res;
                s_track.compute->getResourceFootprint(&r, footprint);
                s_track.totalBytes -= footprint.totalBytes;

                D3D12_RESOURCE_DESC desc = res->GetDesc();
                SL_LOG_HINT("Engine released 0x%llx (%u,%u):%s, total: %lluMB", res, (uint32_t)desc.Width, desc.Height, getDXGIFormatStr(desc.Format), s_track.totalBytes / (1024 * 1024));
                
                res->Release();
                it = s_track.resources.erase(it);
                continue;
            }
            if (res == resource)
            {
                // recycled pointer
                found = true;
            }
            it++;
        }
        if (!found)
        {
            resource->AddRef();
            s_track.resources.push_back(resource);
        }
    }
    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    SL_LOG_HINT("Engine created 0x%llx (%u,%u):%s, size: %lluMB total: %lluMB", resource, (uint32_t)desc.Width, desc.Height, getDXGIFormatStr(desc.Format), bytes / (1024 * 1024), s_track.totalBytes / (1024 * 1024));
}

#define SL_TRACK_RESOURCE if (s_track.enabled && SUCCEEDED(hr)) trackResource((ID3D12Resource*)(*ppvResource));
#else
// No tracking in production regardless of any flags
#define SL_TRACK_RESOURCE
#endif

D3D12Device::D3D12Device(ID3D12Device* original) :
    m_base(original),
    m_interfaceVersion(0)
{
    assert(m_base != nullptr);
#ifndef SL_PRODUCTION
    s_track.enabled = sl::interposer::getInterface()->getConfig().trackEngineAllocations;
    if (s_track.enabled)
    {
        SL_LOG_INFO("Engine resource allocation tracking enabled");
    }
#endif
    m_base->AddRef();
}

bool D3D12Device::checkAndUpgradeInterface(REFIID riid)
{
    if (riid == __uuidof(this) || riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object))
    {
        return true;
    }

    std::vector<IID> iidLookup = 
    {
        __uuidof(ID3D12Device),
        __uuidof(ID3D12Device1),
        __uuidof(ID3D12Device2),
        __uuidof(ID3D12Device3),
        __uuidof(ID3D12Device4),
        __uuidof(ID3D12Device5),
        __uuidof(ID3D12Device6),
        __uuidof(ID3D12Device7),
        __uuidof(ID3D12Device8),
        __uuidof(ID3D12Device9),
        __uuidof(ID3D12Device10),
    };

    for (uint32_t version = 0; version < (uint32_t)iidLookup.size(); ++version)
    {
        if (riid != iidLookup[version])
        {
            continue;
        }

        if (version > m_interfaceVersion)
        {
            IUnknown* new_interface = nullptr;
            if (FAILED(m_base->QueryInterface(riid, reinterpret_cast<void**>(&new_interface))))
            {
                return false;
            }
            SL_LOG_VERBOSE("Upgraded ID3D12Device v%u to v%u", m_interfaceVersion, version);
            m_base->Release();
            m_base = static_cast<ID3D12Device*>(new_interface);
            m_interfaceVersion = version;
        }

        return true;
    }

    return false;
}

HRESULT STDMETHODCALLTYPE D3D12Device::QueryInterface(REFIID riid, void** ppvObj)
{
    if (!ppvObj)
    {
        return E_POINTER;
    }

    // SL Special case, we are requesting base interface
    if (riid == __uuidof(StreamlineRetrieveBaseInterface))
    {
        *ppvObj = m_base;
        m_base->AddRef();
        return S_OK;
    }

    if (checkAndUpgradeInterface(riid))
    {
        AddRef();
        *ppvObj = this;
        return S_OK;
    }

    return m_base->QueryInterface(riid, ppvObj);
}
ULONG   STDMETHODCALLTYPE D3D12Device::AddRef()
{
    m_base->AddRef();
    return ++m_refCount;
}
ULONG   STDMETHODCALLTYPE D3D12Device::Release()
{
    auto refOrig = m_base->Release();
    auto ref = --m_refCount;
    if (ref > 0)
    {
        return ref;
    }
    // Base and our interface don't start with identical reference counts so no point in comparing them
    if(m_base == sl::plugin_manager::getInterface()->getD3D12Device())
    {
        sl::plugin_manager::getInterface()->setD3D12Device(nullptr);
    }
    
    SL_LOG_INFO("Destroyed D3D12Device proxy 0x%llx - native device 0x%llx ref count %ld", this, m_base, refOrig);

    delete this;
    return 0;
}

HRESULT STDMETHODCALLTYPE D3D12Device::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
    return m_base->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
    return m_base->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
    return m_base->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetName(LPCWSTR Name)
{
    return m_base->SetName(Name);
}

UINT STDMETHODCALLTYPE D3D12Device::GetNodeCount()
{
    return m_base->GetNodeCount();
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID riid, void** ppCommandQueue)
{
    if (!pDesc || !ppCommandQueue)
    {
        return E_INVALIDARG;
    }

    const HRESULT hr = m_base->CreateCommandQueue(pDesc, riid, ppCommandQueue);
    if (FAILED(hr))
    {
        SL_LOG_WARN("ID3D12Device::CreateCommandQueue failed with error code %s", std::system_category().message(hr).c_str());
        return hr;
    }

    if(ppCommandQueue && *ppCommandQueue)
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eID3D12Device_CreateCommandQueue);
        for (auto [hook, feature] : hooks) ((PFunCreateCommandQueueAfter*)hook)(pDesc, riid, ppCommandQueue);
    }

    auto cmdQueueProxy = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue*>(*ppCommandQueue));

    if (cmdQueueProxy->checkAndUpgradeInterface(riid))
    {
        *ppCommandQueue = cmdQueueProxy;
    }
    else
    {
        delete cmdQueueProxy;
    }

    return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid, void** ppCommandAllocator)
{
    return m_base->CreateCommandAllocator(type, riid, ppCommandAllocator);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC* pDesc, REFIID riid, void** ppPipelineState)
{
    return m_base->CreateGraphicsPipelineState(pDesc, riid, ppPipelineState);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC* pDesc, REFIID riid, void** ppPipelineState)
{
    return m_base->CreateComputePipelineState(pDesc, riid, ppPipelineState);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* pCommandAllocator, ID3D12PipelineState* pInitialState, REFIID riid, void** ppCommandList)
{
    const HRESULT hr = m_base->CreateCommandList(nodeMask, type, pCommandAllocator, pInitialState, riid, ppCommandList);
    if (FAILED(hr))
    {
        SL_LOG_WARN("ID3D12Device::CreateCommandList failed with error code %s", std::system_category().message(hr).c_str());
        return hr;
    }

    if ((sl::plugin_manager::getInterface()->getPreferences().flags & PreferenceFlags::eDisableCLStateTracking) && !sl::plugin_manager::getInterface()->isProxyNeeded("ID3D12GraphicsCommandList"))
    {
        SL_LOG_VERBOSE_ONCE("Command list state tracking disabled, not using D3D12GraphicsCommandList proxy for optimal performance");
    }
    else
    {
        auto cmdListProxy = new D3D12GraphicsCommandList(this, static_cast<ID3D12GraphicsCommandList*>(*ppCommandList));

        if (cmdListProxy->checkAndUpgradeInterface(riid))
        {
            *ppCommandList = cmdListProxy;
        }
        else
        {
            delete cmdListProxy;
        }
    }

    return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CheckFeatureSupport(D3D12_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize)
{
    return m_base->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC* pDescriptorHeapDesc, REFIID riid, void** ppvHeap)
{
    return m_base->CreateDescriptorHeap(pDescriptorHeapDesc, riid, ppvHeap);
}
UINT    STDMETHODCALLTYPE D3D12Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
    return m_base->GetDescriptorHandleIncrementSize(DescriptorHeapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateRootSignature(UINT nodeMask, const void* pBlobWithRootSignature, SIZE_T blobLengthInBytes, REFIID riid, void** ppvRootSignature)
{
    return m_base->CreateRootSignature(nodeMask, pBlobWithRootSignature, blobLengthInBytes, riid, ppvRootSignature);
}
void    STDMETHODCALLTYPE D3D12Device::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
    m_base->CreateConstantBufferView(pDesc, DestDescriptor);
}

void    STDMETHODCALLTYPE D3D12Device::CreateShaderResourceView(ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
    m_base->CreateShaderResourceView(pResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateUnorderedAccessView(ID3D12Resource* pResource, ID3D12Resource* pCounterResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
    m_base->CreateUnorderedAccessView(pResource, pCounterResource, pDesc, DestDescriptor);

}
void    STDMETHODCALLTYPE D3D12Device::CreateRenderTargetView(ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
    m_base->CreateRenderTargetView(pResource, pDesc, DestDescriptor);

}
void    STDMETHODCALLTYPE D3D12Device::CreateDepthStencilView(ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
    m_base->CreateDepthStencilView(pResource, pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CreateSampler(const D3D12_SAMPLER_DESC* pDesc, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
    m_base->CreateSampler(pDesc, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::CopyDescriptors(UINT NumDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* pDestDescriptorRangeStarts, const UINT* pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorRangeStarts, const UINT* pSrcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
    m_base->CopyDescriptors(NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes, NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes, DescriptorHeapsType);
}
void    STDMETHODCALLTYPE D3D12Device::CopyDescriptorsSimple(UINT NumDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart, D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
    m_base->CopyDescriptorsSimple(NumDescriptors, DestDescriptorRangeStart, SrcDescriptorRangeStart, DescriptorHeapsType);
}
D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC* pResourceDescs)
{
    return m_base->GetResourceAllocationInfo(visibleMask, numResourceDescs, pResourceDescs);
}
D3D12_HEAP_PROPERTIES STDMETHODCALLTYPE D3D12Device::GetCustomHeapProperties(UINT nodeMask, D3D12_HEAP_TYPE heapType)
{
    return m_base->GetCustomHeapProperties(nodeMask, heapType);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pResourceDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource, void** ppvResource)
{
    auto hr = m_base->CreateCommittedResource(pHeapProperties, HeapFlags, pResourceDesc, InitialResourceState, pOptimizedClearValue, riidResource, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap(const D3D12_HEAP_DESC* pDesc, REFIID riid, void** ppvHeap)
{
    return m_base->CreateHeap(pDesc, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource)
{
    auto hr = m_base->CreatePlacedResource(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource)
{
    auto hr = m_base->CreateReservedResource(pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateSharedHandle(ID3D12DeviceChild* pObject, const SECURITY_ATTRIBUTES* pAttributes, DWORD Access, LPCWSTR Name, HANDLE* pHandle)
{
    return m_base->CreateSharedHandle(pObject, pAttributes, Access, Name, pHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void** ppvObj)
{
    return m_base->OpenSharedHandle(NTHandle, riid, ppvObj);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE* pNTHandle)
{
    return m_base->OpenSharedHandleByName(Name, Access, pNTHandle);
}
HRESULT STDMETHODCALLTYPE D3D12Device::MakeResident(UINT NumObjects, ID3D12Pageable* const* ppObjects)
{
    return m_base->MakeResident(NumObjects, ppObjects);
}
HRESULT STDMETHODCALLTYPE D3D12Device::Evict(UINT NumObjects, ID3D12Pageable* const* ppObjects)
{
    return m_base->Evict(NumObjects, ppObjects);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid, void** ppFence)
{
    return m_base->CreateFence(InitialValue, Flags, riid, ppFence);
}
HRESULT STDMETHODCALLTYPE D3D12Device::GetDeviceRemovedReason()
{
    return m_base->GetDeviceRemovedReason();
}
void    STDMETHODCALLTYPE D3D12Device::GetCopyableFootprints(const D3D12_RESOURCE_DESC* pResourceDesc, UINT FirstSubresource, UINT NumSubresources, UINT64 BaseOffset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts, UINT* pNumRows, UINT64* pRowSizeInBytes, UINT64* pTotalBytes)
{
    m_base->GetCopyableFootprints(pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts, pNumRows, pRowSizeInBytes, pTotalBytes);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateQueryHeap(const D3D12_QUERY_HEAP_DESC* pDesc, REFIID riid, void** ppvHeap)
{
    return m_base->CreateQueryHeap(pDesc, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetStablePowerState(BOOL Enable)
{
    return m_base->SetStablePowerState(Enable);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC* pDesc, ID3D12RootSignature* pRootSignature, REFIID riid, void** ppvCommandSignature)
{
    return m_base->CreateCommandSignature(pDesc, pRootSignature, riid, ppvCommandSignature);
}
void    STDMETHODCALLTYPE D3D12Device::GetResourceTiling(ID3D12Resource* pTiledResource, UINT* pNumTilesForEntireResource, D3D12_PACKED_MIP_INFO* pPackedMipDesc, D3D12_TILE_SHAPE* pStandardTileShapeForNonPackedMips, UINT* pNumSubresourceTilings, UINT FirstSubresourceTilingToGet, D3D12_SUBRESOURCE_TILING* pSubresourceTilingsForNonPackedMips)
{
    m_base->GetResourceTiling(pTiledResource, pNumTilesForEntireResource, pPackedMipDesc, pStandardTileShapeForNonPackedMips, pNumSubresourceTilings, FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}
LUID    STDMETHODCALLTYPE D3D12Device::GetAdapterLuid()
{
    return m_base->GetAdapterLuid();
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreatePipelineLibrary(const void* pLibraryBlob, SIZE_T BlobLength, REFIID riid, void** ppPipelineLibrary)
{
    return static_cast<ID3D12Device1*>(m_base)->CreatePipelineLibrary(pLibraryBlob, BlobLength, riid, ppPipelineLibrary);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetEventOnMultipleFenceCompletion(ID3D12Fence* const* ppFences, const UINT64* pFenceValues, UINT NumFences, D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags, HANDLE hEvent)
{
    return static_cast<ID3D12Device1*>(m_base)->SetEventOnMultipleFenceCompletion(ppFences, pFenceValues, NumFences, Flags, hEvent);
}
HRESULT STDMETHODCALLTYPE D3D12Device::SetResidencyPriority(UINT NumObjects, ID3D12Pageable* const* ppObjects, const D3D12_RESIDENCY_PRIORITY* pPriorities)
{
    return static_cast<ID3D12Device1*>(m_base)->SetResidencyPriority(NumObjects, ppObjects, pPriorities);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreatePipelineState(const D3D12_PIPELINE_STATE_STREAM_DESC* pDesc, REFIID riid, void** ppPipelineState)
{
    return static_cast<ID3D12Device2*>(m_base)->CreatePipelineState(pDesc, riid, ppPipelineState);
}

HRESULT STDMETHODCALLTYPE D3D12Device::OpenExistingHeapFromAddress(const void* pAddress, REFIID riid, void** ppvHeap)
{
    return static_cast<ID3D12Device3*>(m_base)->OpenExistingHeapFromAddress(pAddress, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::OpenExistingHeapFromFileMapping(HANDLE hFileMapping, REFIID riid, void** ppvHeap)
{
    return static_cast<ID3D12Device3*>(m_base)->OpenExistingHeapFromFileMapping(hFileMapping, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::EnqueueMakeResident(D3D12_RESIDENCY_FLAGS Flags, UINT NumObjects, ID3D12Pageable* const* ppObjects, ID3D12Fence* pFenceToSignal, UINT64 FenceValueToSignal)
{
    return static_cast<ID3D12Device3*>(m_base)->EnqueueMakeResident(Flags, NumObjects, ppObjects, pFenceToSignal, FenceValueToSignal);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandList1(UINT NodeMask, D3D12_COMMAND_LIST_TYPE Type, D3D12_COMMAND_LIST_FLAGS Flags, REFIID riid, void** ppCommandList)
{
    const HRESULT hr = static_cast<ID3D12Device4*>(m_base)->CreateCommandList1(NodeMask, Type, Flags, riid, ppCommandList);
    if (FAILED(hr))
    {
        SL_LOG_WARN("ID3D12Device4::CreateCommandList1 failed with error code %s", std::system_category().message(hr).c_str());
        return hr;
    }

    if ((sl::plugin_manager::getInterface()->getPreferences().flags & PreferenceFlags::eDisableCLStateTracking) && !sl::plugin_manager::getInterface()->isProxyNeeded("ID3D12GraphicsCommandList"))
    {
        SL_LOG_VERBOSE_ONCE("Command list state tracking disabled, not using D3D12GraphicsCommandList proxy for optimal performance");
    }
    else
    {
        auto cmdListProxy = new D3D12GraphicsCommandList(this, static_cast<ID3D12GraphicsCommandList*>(*ppCommandList));

        if (cmdListProxy->checkAndUpgradeInterface(riid))
        {
            *ppCommandList = cmdListProxy;
        }
        else
        {
            delete cmdListProxy;
        }
    }

    return hr;

}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateProtectedResourceSession(const D3D12_PROTECTED_RESOURCE_SESSION_DESC* pDesc, REFIID riid, void** ppSession)
{
    return static_cast<ID3D12Device4*>(m_base)->CreateProtectedResourceSession(pDesc, riid, ppSession);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource1(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riidResource, void** ppvResource)
{
    auto hr = static_cast<ID3D12Device4*>(m_base)->CreateCommittedResource1(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riidResource, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateHeap1(const D3D12_HEAP_DESC* pDesc, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riid, void** ppvHeap)
{
    return static_cast<ID3D12Device4*>(m_base)->CreateHeap1(pDesc, pProtectedSession, riid, ppvHeap);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource1(const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riid, void** ppvResource)
{
    auto hr = static_cast<ID3D12Device4*>(m_base)->CreateReservedResource1(pDesc, InitialState, pOptimizedClearValue, pProtectedSession, riid, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}
D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo1(UINT VisibleMask, UINT NumResourceDescs, const D3D12_RESOURCE_DESC* pResourceDescs, D3D12_RESOURCE_ALLOCATION_INFO1* pResourceAllocationInfo1)
{
    return static_cast<ID3D12Device4*>(m_base)->GetResourceAllocationInfo1(VisibleMask, NumResourceDescs, pResourceDescs, pResourceAllocationInfo1);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateLifetimeTracker(ID3D12LifetimeOwner* pOwner, REFIID riid, void** ppvTracker)
{
    return static_cast<ID3D12Device5*>(m_base)->CreateLifetimeTracker(pOwner, riid, ppvTracker);
}
void    STDMETHODCALLTYPE D3D12Device::RemoveDevice()
{
    static_cast<ID3D12Device5*>(m_base)->RemoveDevice();
}
HRESULT STDMETHODCALLTYPE D3D12Device::EnumerateMetaCommands(UINT* pNumMetaCommands, D3D12_META_COMMAND_DESC* pDescs)
{
    return static_cast<ID3D12Device5*>(m_base)->EnumerateMetaCommands(pNumMetaCommands, pDescs);
}
HRESULT STDMETHODCALLTYPE D3D12Device::EnumerateMetaCommandParameters(REFGUID CommandId, D3D12_META_COMMAND_PARAMETER_STAGE Stage, UINT* pTotalStructureSizeInBytes, UINT* pParameterCount, D3D12_META_COMMAND_PARAMETER_DESC* pParameterDescs)
{
    return static_cast<ID3D12Device5*>(m_base)->EnumerateMetaCommandParameters(CommandId, Stage, pTotalStructureSizeInBytes, pParameterCount, pParameterDescs);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateMetaCommand(REFGUID CommandId, UINT NodeMask, const void* pCreationParametersData, SIZE_T CreationParametersDataSizeInBytes, REFIID riid, void** ppMetaCommand)
{
    return static_cast<ID3D12Device5*>(m_base)->CreateMetaCommand(CommandId, NodeMask, pCreationParametersData, CreationParametersDataSizeInBytes, riid, ppMetaCommand);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateStateObject(const D3D12_STATE_OBJECT_DESC* pDesc, REFIID riid, void** ppStateObject)
{
    return static_cast<ID3D12Device5*>(m_base)->CreateStateObject(pDesc, riid, ppStateObject);
}
void    STDMETHODCALLTYPE D3D12Device::GetRaytracingAccelerationStructurePrebuildInfo(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* pInfo)
{
    static_cast<ID3D12Device5*>(m_base)->GetRaytracingAccelerationStructurePrebuildInfo(pDesc, pInfo);
}
D3D12_DRIVER_MATCHING_IDENTIFIER_STATUS STDMETHODCALLTYPE D3D12Device::CheckDriverMatchingIdentifier(D3D12_SERIALIZED_DATA_TYPE SerializedDataType, const D3D12_SERIALIZED_DATA_DRIVER_MATCHING_IDENTIFIER* pIdentifierToCheck)
{
    return static_cast<ID3D12Device5*>(m_base)->CheckDriverMatchingIdentifier(SerializedDataType, pIdentifierToCheck);
}

HRESULT STDMETHODCALLTYPE D3D12Device::SetBackgroundProcessingMode(D3D12_BACKGROUND_PROCESSING_MODE Mode, D3D12_MEASUREMENTS_ACTION MeasurementsAction, HANDLE hEventToSignalUponCompletion, BOOL* pbFurtherMeasurementsDesired)
{
    return static_cast<ID3D12Device6*>(m_base)->SetBackgroundProcessingMode(Mode, MeasurementsAction, hEventToSignalUponCompletion, pbFurtherMeasurementsDesired);
}

HRESULT STDMETHODCALLTYPE D3D12Device::AddToStateObject(const D3D12_STATE_OBJECT_DESC* pAddition, ID3D12StateObject* pStateObjectToGrowFrom, REFIID riid, void** ppNewStateObject)
{
    return static_cast<ID3D12Device7*>(m_base)->AddToStateObject(pAddition, pStateObjectToGrowFrom, riid, ppNewStateObject);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateProtectedResourceSession1(const D3D12_PROTECTED_RESOURCE_SESSION_DESC1* pDesc, REFIID riid, void** ppSession)
{
    return static_cast<ID3D12Device7*>(m_base)->CreateProtectedResourceSession1(pDesc, riid, ppSession);
}

D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE D3D12Device::GetResourceAllocationInfo2(UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC1* pResourceDescs, D3D12_RESOURCE_ALLOCATION_INFO1* pResourceAllocationInfo1)
{
    return static_cast<ID3D12Device8*>(m_base)->GetResourceAllocationInfo2(visibleMask, numResourceDescs, pResourceDescs, pResourceAllocationInfo1);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource2(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1* pDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, REFIID riidResource, void** ppvResource)
{
    auto hr = static_cast<ID3D12Device8*>(m_base)->CreateCommittedResource2(pHeapProperties, HeapFlags, pDesc, InitialResourceState, pOptimizedClearValue, pProtectedSession, riidResource, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource1(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource)
{
    auto hr = static_cast<ID3D12Device8*>(m_base)->CreatePlacedResource1(pHeap, HeapOffset, pDesc, InitialState, pOptimizedClearValue, riid, ppvResource);
    return hr;
}
void    STDMETHODCALLTYPE D3D12Device::CreateSamplerFeedbackUnorderedAccessView(ID3D12Resource* pTargetedResource, ID3D12Resource* pFeedbackResource, D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
    static_cast<ID3D12Device8*>(m_base)->CreateSamplerFeedbackUnorderedAccessView(pTargetedResource, pFeedbackResource, DestDescriptor);
}
void    STDMETHODCALLTYPE D3D12Device::GetCopyableFootprints1(const D3D12_RESOURCE_DESC1* pResourceDesc, UINT FirstSubresource, UINT NumSubresources, UINT64 BaseOffset, D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts, UINT* pNumRows, UINT64* pRowSizeInBytes, UINT64* pTotalBytes)
{
    static_cast<ID3D12Device8*>(m_base)->GetCopyableFootprints1(pResourceDesc, FirstSubresource, NumSubresources, BaseOffset, pLayouts, pNumRows, pRowSizeInBytes, pTotalBytes);
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateShaderCacheSession(const D3D12_SHADER_CACHE_SESSION_DESC* pDesc, REFIID riid, void** ppvSession)
{
    return static_cast<ID3D12Device9*>(m_base)->CreateShaderCacheSession(pDesc, riid, ppvSession);
}
HRESULT STDMETHODCALLTYPE D3D12Device::ShaderCacheControl(D3D12_SHADER_CACHE_KIND_FLAGS Kinds, D3D12_SHADER_CACHE_CONTROL_FLAGS Control)
{
    return static_cast<ID3D12Device9*>(m_base)->ShaderCacheControl(Kinds, Control);
}
HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommandQueue1(const D3D12_COMMAND_QUEUE_DESC* pDesc, REFIID CreatorID, REFIID riid, void** ppCommandQueue)
{
    const HRESULT hr = static_cast<ID3D12Device9*>(m_base)->CreateCommandQueue1(pDesc, CreatorID, riid, ppCommandQueue);
    if (SUCCEEDED(hr))
    {
        const auto cmdQueueProxy = new D3D12CommandQueue(this, static_cast<ID3D12CommandQueue*>(*ppCommandQueue));

        // Upgrade to the actual interface version requested here
        if (cmdQueueProxy->checkAndUpgradeInterface(riid))
        {
            * ppCommandQueue = cmdQueueProxy;
        }
        else
        {
            delete cmdQueueProxy;
        }
    }
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateCommittedResource3(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC1* pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, UINT32 NumCastableFormats, DXGI_FORMAT* pCastableFormats, REFIID riidResource, void** ppvResource)
{
    auto hr = static_cast<ID3D12Device10*>(m_base)->CreateCommittedResource3(pHeapProperties, HeapFlags, pDesc, InitialLayout, pOptimizedClearValue, pProtectedSession, NumCastableFormats, pCastableFormats, riidResource, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreatePlacedResource2(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC1* pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE* pOptimizedClearValue, UINT32 NumCastableFormats, DXGI_FORMAT* pCastableFormats, REFIID riid, void** ppvResource)
{
    auto hr = static_cast<ID3D12Device10*>(m_base)->CreatePlacedResource2(pHeap, HeapOffset, pDesc, InitialLayout, pOptimizedClearValue, NumCastableFormats, pCastableFormats, riid, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}

HRESULT STDMETHODCALLTYPE D3D12Device::CreateReservedResource2(const D3D12_RESOURCE_DESC* pDesc, D3D12_BARRIER_LAYOUT InitialLayout, const D3D12_CLEAR_VALUE* pOptimizedClearValue, ID3D12ProtectedResourceSession* pProtectedSession, UINT32 NumCastableFormats, DXGI_FORMAT* pCastableFormats, REFIID riid, void** ppvResource)
{
    auto hr = static_cast<ID3D12Device10*>(m_base)->CreateReservedResource2(pDesc, InitialLayout, pOptimizedClearValue, pProtectedSession, NumCastableFormats, pCastableFormats, riid, ppvResource);
    SL_TRACK_RESOURCE;
    return hr;
}

}
}

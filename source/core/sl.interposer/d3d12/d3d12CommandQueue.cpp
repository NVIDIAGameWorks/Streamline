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
#include "source/core/sl.log/log.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.plugin-manager/pluginManager.h"

namespace sl
{
namespace interposer
{

static_assert(offsetof(D3D12CommandQueue, m_base) == 16, "This location must be maintained to keep compatibility with Nsight tools");

D3D12CommandQueue::D3D12CommandQueue(D3D12Device* device, ID3D12CommandQueue* original) :
    m_base(original),
    m_interfaceVersion(0),
    m_device(device)
{
    // Same ref count as base interface to start with
    m_base->AddRef();
    m_refCount.store(m_base->Release());
}

bool D3D12CommandQueue::checkAndUpgradeInterface(REFIID riid)
{
    if (riid == __uuidof(this) || riid == __uuidof(IUnknown) ||
        riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) || riid == __uuidof(ID3D12Pageable))
    {
        return true;
    }

    std::vector<IID> iidLookup = {
      __uuidof(ID3D12CommandQueue),
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
                return false;
            m_base->Release();
            m_base = static_cast<ID3D12CommandQueue*>(new_interface);
            m_interfaceVersion = version;
        }

        return true;
    }

    return false;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::QueryInterface(REFIID riid, void** ppvObj)
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

ULONG   STDMETHODCALLTYPE D3D12CommandQueue::AddRef()
{
    m_base->AddRef();
    return ++m_refCount;
}

ULONG   STDMETHODCALLTYPE D3D12CommandQueue::Release()
{
    auto refOrig = m_base->Release();
    auto ref = --m_refCount;
    if (ref > 0) return ref;
    // Base and our interface don't start with identical reference counts so no point in comparing them
    delete this;
    return 0;
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
    return m_base->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
    return m_base->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
    return m_base->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::SetName(LPCWSTR Name)
{
    return m_base->SetName(Name);
}

HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetDevice(REFIID riid, void** ppvDevice)
{
    return m_device->QueryInterface(riid, ppvDevice);
}

void    STDMETHODCALLTYPE D3D12CommandQueue::UpdateTileMappings(ID3D12Resource* pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE* pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE* pResourceRegionSizes, ID3D12Heap* pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS* pRangeFlags, const UINT* pHeapRangeStartOffsets, const UINT* pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags)
{
    m_base->UpdateTileMappings(pResource, NumResourceRegions, pResourceRegionStartCoordinates, pResourceRegionSizes, pHeap, NumRanges, pRangeFlags, pHeapRangeStartOffsets, pRangeTileCounts, Flags);
}
void    STDMETHODCALLTYPE D3D12CommandQueue::CopyTileMappings(ID3D12Resource* pDstResource, const D3D12_TILED_RESOURCE_COORDINATE* pDstRegionStartCoordinate, ID3D12Resource* pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE* pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE* pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags)
{
    m_base->CopyTileMappings(pDstResource, pDstRegionStartCoordinate, pSrcResource, pSrcRegionStartCoordinate, pRegionSize, Flags);
}

#ifndef SL_PRODUCTION
extern "C" void updateTrackedResources();
#endif

void    STDMETHODCALLTYPE D3D12CommandQueue::ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists)
{
#ifndef SL_PRODUCTION
    updateTrackedResources();
#endif
    std::vector<ID3D12CommandList*> cmdLists(NumCommandLists);
    for (UINT i = 0; i < NumCommandLists; i++)
    {
        assert(ppCommandLists[i] != nullptr);

        Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> cmdListProxy;
        if (SUCCEEDED(ppCommandLists[i]->QueryInterface(__uuidof(D3D12GraphicsCommandList), &cmdListProxy)))
        {
            // Get the original interface from our proxy
            cmdLists[i] = cmdListProxy->m_base;
        }
        else
        {
            // No proxy, use the original pointer
            cmdLists[i] = ppCommandLists[i];
        }
    }

    m_base->ExecuteCommandLists(NumCommandLists, cmdLists.data());
}
void    STDMETHODCALLTYPE D3D12CommandQueue::SetMarker(UINT Metadata, const void* pData, UINT Size)
{
    m_base->SetMarker(Metadata, pData, Size);
}
void    STDMETHODCALLTYPE D3D12CommandQueue::BeginEvent(UINT Metadata, const void* pData, UINT Size)
{
    m_base->BeginEvent(Metadata, pData, Size);
}
void    STDMETHODCALLTYPE D3D12CommandQueue::EndEvent()
{
    m_base->EndEvent();
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::Signal(ID3D12Fence* pFence, UINT64 Value)
{
    return m_base->Signal(pFence, Value);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::Wait(ID3D12Fence* pFence, UINT64 Value)
{
    return m_base->Wait(pFence, Value);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetTimestampFrequency(UINT64* pFrequency)
{
    return m_base->GetTimestampFrequency(pFrequency);
}
HRESULT STDMETHODCALLTYPE D3D12CommandQueue::GetClockCalibration(UINT64* pGpuTimestamp, UINT64* pCpuTimestamp)
{
    return m_base->GetClockCalibration(pGpuTimestamp, pCpuTimestamp);
}
D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE D3D12CommandQueue::GetDesc()
{
    return m_base->GetDesc();
}

}
}

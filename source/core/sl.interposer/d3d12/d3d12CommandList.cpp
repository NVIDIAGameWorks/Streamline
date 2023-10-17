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

#include "include/sl.h"
#include "source/core/sl.interposer/d3d12/d3d12Device.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandList.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.plugin-manager/pluginManager.h"
#include "include/sl_hooks.h"

namespace sl
{
namespace interposer
{

static_assert(offsetof(D3D12GraphicsCommandList, m_base) == 16, "This location must be maintained to keep compatibility with Nsight tools");

D3D12GraphicsCommandList::D3D12GraphicsCommandList(D3D12Device* device, ID3D12GraphicsCommandList* original) :
    m_base(original),
    m_interfaceVersion(0),
    m_device(device)
{
    assert(m_base != nullptr && m_device != nullptr);
    m_trackState = (sl::plugin_manager::getInterface()->getPreferences().flags & PreferenceFlags::eDisableCLStateTracking) == 0;
    if (!m_trackState)
    {
        SL_LOG_WARN_ONCE("State tracking for command list 0x%llx has been DISABLED, please ensure to restore CL state correctly on the host side.", this);
    }
    // Same ref count as base interface to start with
    m_base->AddRef();
    m_refCount.store(m_base->Release());
}

bool D3D12GraphicsCommandList::checkAndUpgradeInterface(REFIID riid)
{
    if (riid == __uuidof(this) || riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) ||
        riid == __uuidof(ID3D12DeviceChild) || riid == __uuidof(ID3D12CommandList))
    {
        return true;
    }

    std::vector<IID> iidLookup = {
      __uuidof(ID3D12GraphicsCommandList),
      __uuidof(ID3D12GraphicsCommandList1),
      __uuidof(ID3D12GraphicsCommandList2),
      __uuidof(ID3D12GraphicsCommandList3),
      __uuidof(ID3D12GraphicsCommandList4),
      __uuidof(ID3D12GraphicsCommandList5),
      __uuidof(ID3D12GraphicsCommandList6),
      __uuidof(ID3D12GraphicsCommandList7),
      __uuidof(ID3D12GraphicsCommandList8),
    };

    for (uint32_t version = 0; version < uint32_t(iidLookup.size()); ++version)
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
            m_base = static_cast<ID3D12GraphicsCommandList*>(new_interface);
            m_interfaceVersion = version;
        }

        return true;
    }

    return false;
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::QueryInterface(REFIID riid, void** ppvObj)
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

ULONG   STDMETHODCALLTYPE D3D12GraphicsCommandList::AddRef()
{
    m_base->AddRef();
    return ++m_refCount;
}

ULONG   STDMETHODCALLTYPE D3D12GraphicsCommandList::Release()
{
    auto refOrig = m_base->Release();
    auto ref = --m_refCount;
    if (ref > 0) return ref;
    // Base and our interface don't start with identical reference counts so no point in comparing them
    delete this;
    return 0;
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
    return m_base->GetPrivateData(guid, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
    return m_base->SetPrivateData(guid, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
    return m_base->SetPrivateDataInterface(guid, pData);
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::SetName(LPCWSTR Name)
{
    return m_base->SetName(Name);
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::GetDevice(REFIID riid, void** ppvDevice)
{
    return m_device->QueryInterface(riid, ppvDevice);
}

D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE D3D12GraphicsCommandList::GetType()
{
    return m_base->GetType();
}

HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::Close()
{
    return m_base->Close();
}
HRESULT STDMETHODCALLTYPE D3D12GraphicsCommandList::Reset(ID3D12CommandAllocator* pAllocator, ID3D12PipelineState* pInitialState)
{
    if (m_trackState)
    {
        m_rootSignature = {};
        m_pso = pInitialState;
        m_so = {};
        m_numHeaps = {};
        m_mapHandles = {};
        m_mapCBV.clear();
        m_mapSRV.clear();
        m_mapUAV.clear();
        m_mapConstants.clear();
    }
    return m_base->Reset(pAllocator, pInitialState);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearState(ID3D12PipelineState* pPipelineState)
{
    if (m_trackState)
    {
        m_rootSignature = {};
        m_pso = pPipelineState;
        m_so = {};
        m_numHeaps = {};
        m_mapHandles = {};
        m_mapCBV.clear();
        m_mapSRV.clear();
        m_mapUAV.clear();
        m_mapConstants.clear();
    }
    m_base->ClearState(pPipelineState);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
    m_base->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
    m_base->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
    m_base->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyBufferRegion(ID3D12Resource* pDstBuffer, UINT64 DstOffset, ID3D12Resource* pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes)
{
    m_base->CopyBufferRegion(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox)
{
    m_base->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyResource(ID3D12Resource* pDstResource, ID3D12Resource* pSrcResource)
{
    m_base->CopyResource(pDstResource, pSrcResource);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyTiles(ID3D12Resource* pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE* pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE* pTileRegionSize, ID3D12Resource* pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags)
{
    m_base->CopyTiles(pTiledResource, pTileRegionStartCoordinate, pTileRegionSize, pBuffer, BufferStartOffsetInBytes, Flags);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResolveSubresource(ID3D12Resource* pDstResource, UINT DstSubresource, ID3D12Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format)
{
    m_base->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
    m_base->IASetPrimitiveTopology(PrimitiveTopology);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports)
{
    m_base->RSSetViewports(NumViewports, pViewports);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT* pRects)
{
    m_base->RSSetScissorRects(NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetBlendFactor(const FLOAT BlendFactor[4])
{
    m_base->OMSetBlendFactor(BlendFactor);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetStencilRef(UINT StencilRef)
{
    m_base->OMSetStencilRef(StencilRef);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPipelineState(ID3D12PipelineState* pPipelineState)
{
    m_base->SetPipelineState(pPipelineState);
    
    if (m_trackState)
    {
        // PSO and RT PSO are mutually exclusive so setting RT PSO to null (see SetPipelineState1)
        m_so = {};        
        m_pso = pPipelineState;
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers)
{
    m_base->ResourceBarrier(NumBarriers, pBarriers);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ExecuteBundle(ID3D12GraphicsCommandList* pCommandList)
{
    assert(pCommandList != nullptr);

    // Get original command list pointer from proxy object
    const auto command_list_proxy = static_cast<D3D12GraphicsCommandList*>(pCommandList);

    // Merge bundle command list trackers into the current one  
    m_base->ExecuteBundle(command_list_proxy->m_base);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetDescriptorHeaps(UINT NumDescriptorHeaps, ID3D12DescriptorHeap* const* ppDescriptorHeaps)
{
    m_base->SetDescriptorHeaps(NumDescriptorHeaps, ppDescriptorHeaps);

    if (NumDescriptorHeaps > kMaxHeapCount)
    {
        SL_LOG_WARN("Too many descriptor heaps %u", NumDescriptorHeaps);
    }
    else if(m_trackState)
    {
        m_numHeaps = (char)NumDescriptorHeaps;
        memcpy(m_heaps, ppDescriptorHeaps, sizeof(ID3D12DescriptorHeap*) * m_numHeaps);
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootSignature(ID3D12RootSignature* pRootSignature)
{
    m_base->SetComputeRootSignature(pRootSignature);

    // App can set the same root signature multiple times so check
    if (m_trackState && pRootSignature != m_rootSignature)
    {
        m_rootSignature = pRootSignature;
        // Clear root signature cached items
        m_mapCBV.clear();
        m_mapSRV.clear();
        m_mapUAV.clear();
        m_mapConstants.clear();
        m_mapHandles.clear();
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootSignature(ID3D12RootSignature* pRootSignature)
{
    m_base->SetGraphicsRootSignature(pRootSignature);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
    m_base->SetComputeRootDescriptorTable(RootParameterIndex, BaseDescriptor);

    if (m_trackState)
    {
        m_mapHandles[RootParameterIndex] = BaseDescriptor;
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
    m_base->SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
    m_base->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);

    if (m_trackState)
    {
        auto& entry = m_mapConstants[RootParameterIndex];
        entry.Num32BitValuesToSet = 1;
        entry.SrcData[0] = SrcData;
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
    m_base->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
    m_base->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);

    assert(Num32BitValuesToSet <= kMaxComputeRoot32BitConstCount);

    if (Num32BitValuesToSet > kMaxComputeRoot32BitConstCount)
    {
        SL_LOG_WARN("Too many 32bit root constants %u", Num32BitValuesToSet);
    }
    else if(m_trackState)
    {
        auto& entry = m_mapConstants[RootParameterIndex];
        entry.Num32BitValuesToSet = Num32BitValuesToSet;
        memcpy(entry.SrcData, pSrcData, sizeof(uint32_t) * Num32BitValuesToSet);
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
    m_base->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
    m_base->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);

    if (m_trackState)
    {
        m_mapCBV[RootParameterIndex] = BufferLocation;
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
    m_base->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
    m_base->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);

    if (m_trackState)
    {
        m_mapSRV[RootParameterIndex] = BufferLocation;
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
    m_base->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
    m_base->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);

    if (m_trackState)
    {
        m_mapUAV[RootParameterIndex] = BufferLocation;
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
    m_base->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView)
{
    m_base->IASetIndexBuffer(pView);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews)
{
    m_base->IASetVertexBuffers(StartSlot, NumViews, pViews);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SOSetTargets(UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW* pViews)
{
    m_base->SOSetTargets(StartSlot, NumViews, pViews);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, D3D12_CPU_DESCRIPTOR_HANDLE const* pDepthStencilDescriptor)
{
    m_base->OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects)
{
    m_base->ClearDepthStencilView(DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT* pRects)
{
    m_base->ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource* pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT* pRects)
{
    m_base->ClearUnorderedAccessViewUint(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource* pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT* pRects)
{
    m_base->ClearUnorderedAccessViewFloat(ViewGPUHandleInCurrentHeap, ViewCPUHandle, pResource, Values, NumRects, pRects);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DiscardResource(ID3D12Resource* pResource, const D3D12_DISCARD_REGION* pRegion)
{
    m_base->DiscardResource(pResource, pRegion);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::BeginQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index)
{
    m_base->BeginQuery(pQueryHeap, Type, Index);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EndQuery(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index)
{
    m_base->EndQuery(pQueryHeap, Type, Index);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResolveQueryData(ID3D12QueryHeap* pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource* pDestinationBuffer, UINT64 AlignedDestinationBufferOffset)
{
    m_base->ResolveQueryData(pQueryHeap, Type, StartIndex, NumQueries, pDestinationBuffer, AlignedDestinationBufferOffset);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPredication(ID3D12Resource* pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation)
{
    m_base->SetPredication(pBuffer, AlignedBufferOffset, Operation);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetMarker(UINT Metadata, const void* pData, UINT Size)
{
    m_base->SetMarker(Metadata, pData, Size);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::BeginEvent(UINT Metadata, const void* pData, UINT Size)
{
    m_base->BeginEvent(Metadata, pData, Size);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EndEvent()
{
    m_base->EndEvent();
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ExecuteIndirect(ID3D12CommandSignature* pCommandSignature, UINT MaxCommandCount, ID3D12Resource* pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource* pCountBuffer, UINT64 CountBufferOffset)
{
    m_base->ExecuteIndirect(pCommandSignature, MaxCommandCount, pArgumentBuffer, ArgumentBufferOffset, pCountBuffer, CountBufferOffset);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::AtomicCopyBufferUINT(ID3D12Resource* pDstBuffer, UINT64 DstOffset, ID3D12Resource* pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource* const* ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64* pDependentSubresourceRanges)
{
    static_cast<ID3D12GraphicsCommandList1*>(m_base)->AtomicCopyBufferUINT(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies, ppDependentResources, pDependentSubresourceRanges);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::AtomicCopyBufferUINT64(ID3D12Resource* pDstBuffer, UINT64 DstOffset, ID3D12Resource* pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource* const* ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64* pDependentSubresourceRanges)
{
    static_cast<ID3D12GraphicsCommandList1*>(m_base)->AtomicCopyBufferUINT64(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, Dependencies, ppDependentResources, pDependentSubresourceRanges);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetDepthBounds(FLOAT Min, FLOAT Max)
{
    static_cast<ID3D12GraphicsCommandList1*>(m_base)->OMSetDepthBounds(Min, Max);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetSamplePositions(UINT NumSamplesPerPixel, UINT NumPixels, D3D12_SAMPLE_POSITION* pSamplePositions)
{
    static_cast<ID3D12GraphicsCommandList1*>(m_base)->SetSamplePositions(NumSamplesPerPixel, NumPixels, pSamplePositions);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ResolveSubresourceRegion(ID3D12Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, ID3D12Resource* pSrcResource, UINT SrcSubresource, D3D12_RECT* pSrcRect, DXGI_FORMAT Format, D3D12_RESOLVE_MODE ResolveMode)
{
    static_cast<ID3D12GraphicsCommandList1*>(m_base)->ResolveSubresourceRegion(pDstResource, DstSubresource, DstX, DstY, pSrcResource, SrcSubresource, pSrcRect, Format, ResolveMode);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetViewInstanceMask(UINT Mask)
{
    static_cast<ID3D12GraphicsCommandList1*>(m_base)->SetViewInstanceMask(Mask);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::WriteBufferImmediate(UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER* pParams, const D3D12_WRITEBUFFERIMMEDIATE_MODE* pModes)
{
    static_cast<ID3D12GraphicsCommandList2*>(m_base)->WriteBufferImmediate(Count, pParams, pModes);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetProtectedResourceSession(ID3D12ProtectedResourceSession* pProtectedResourceSession)
{
    static_cast<ID3D12GraphicsCommandList3*>(m_base)->SetProtectedResourceSession(pProtectedResourceSession);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::BeginRenderPass(UINT NumRenderTargets, const D3D12_RENDER_PASS_RENDER_TARGET_DESC* pRenderTargets, const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->BeginRenderPass(NumRenderTargets, pRenderTargets, pDepthStencil, Flags);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EndRenderPass(void)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->EndRenderPass();
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::InitializeMetaCommand(ID3D12MetaCommand* pMetaCommand, const void* pInitializationParametersData, SIZE_T InitializationParametersDataSizeInBytes)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->InitializeMetaCommand(pMetaCommand, pInitializationParametersData, InitializationParametersDataSizeInBytes);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::ExecuteMetaCommand(ID3D12MetaCommand* pMetaCommand, const void* pExecutionParametersData, SIZE_T ExecutionParametersDataSizeInBytes)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->ExecuteMetaCommand(pMetaCommand, pExecutionParametersData, ExecutionParametersDataSizeInBytes);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::BuildRaytracingAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc, UINT NumPostbuildInfoDescs, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pPostbuildInfoDescs)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->BuildRaytracingAccelerationStructure(pDesc, NumPostbuildInfoDescs, pPostbuildInfoDescs);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::EmitRaytracingAccelerationStructurePostbuildInfo(const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pDesc, UINT NumSourceAccelerationStructures, const D3D12_GPU_VIRTUAL_ADDRESS* pSourceAccelerationStructureData)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->EmitRaytracingAccelerationStructurePostbuildInfo(pDesc, NumSourceAccelerationStructures, pSourceAccelerationStructureData);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::CopyRaytracingAccelerationStructure(D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData, D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->CopyRaytracingAccelerationStructure(DestAccelerationStructureData, SourceAccelerationStructureData, Mode);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::SetPipelineState1(ID3D12StateObject* pStateObject)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->SetPipelineState1(pStateObject);

    if (m_trackState && m_so != pStateObject)
    {
        // PSO and RT PSO are mutually exclusive so setting PSO to null (see SetPipelineState)
        m_pso = {};        
        m_so = pStateObject;
    }
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::DispatchRays(const D3D12_DISPATCH_RAYS_DESC* pDesc)
{
    static_cast<ID3D12GraphicsCommandList4*>(m_base)->DispatchRays(pDesc);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetShadingRate(D3D12_SHADING_RATE baseShadingRate, const D3D12_SHADING_RATE_COMBINER* combiners)
{
    static_cast<ID3D12GraphicsCommandList5*>(m_base)->RSSetShadingRate(baseShadingRate, combiners);
}
void STDMETHODCALLTYPE D3D12GraphicsCommandList::RSSetShadingRateImage(ID3D12Resource* shadingRateImage)
{
    static_cast<ID3D12GraphicsCommandList5*>(m_base)->RSSetShadingRateImage(shadingRateImage);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::DispatchMesh(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
    static_cast<ID3D12GraphicsCommandList6*>(m_base)->DispatchMesh(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::Barrier(UINT32 NumBarrierGroups, const D3D12_BARRIER_GROUP* pBarrierGroups)
{
    static_cast<ID3D12GraphicsCommandList7*>(m_base)->Barrier(NumBarrierGroups, pBarrierGroups);
}

void STDMETHODCALLTYPE D3D12GraphicsCommandList::OMSetFrontAndBackStencilRef(UINT FrontStencilRef, UINT BackStencilRef)
{
    static_cast<ID3D12GraphicsCommandList8*>(m_base)->OMSetFrontAndBackStencilRef(FrontStencilRef, BackStencilRef);
}

}
}

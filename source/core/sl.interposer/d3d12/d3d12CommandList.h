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

#include <map>
#include "source/core/sl.interposer/d3d12/d3d12.h"

struct D3D12Device;

namespace sl
{
namespace interposer
{

constexpr int kMaxHeapCount = 4;
constexpr int kMaxComputeRoot32BitConstCount = 64;

struct DECLSPEC_UUID("5B2662FB-EB28-4AEC-819E-1C1B4DE060F6") D3D12GraphicsCommandList : ID3D12GraphicsCommandList8
{
    D3D12GraphicsCommandList(D3D12Device * device, ID3D12GraphicsCommandList * original);

    D3D12GraphicsCommandList(const D3D12GraphicsCommandList&) = delete;
    D3D12GraphicsCommandList& operator=(const D3D12GraphicsCommandList&) = delete;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override final;
    ULONG   STDMETHODCALLTYPE AddRef() override final;
    ULONG   STDMETHODCALLTYPE Release() override final;

#pragma region ID3D12Object
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT * pDataSize, void* pData) override final;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) override final;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown * pData) override final;
    HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name) override final;
#pragma endregion
#pragma region ID3D12DeviceChild
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppvDevice) override final;
#pragma endregion
#pragma region ID3D12CommandList
    D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType() override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList
    HRESULT STDMETHODCALLTYPE Close() override final;
    HRESULT STDMETHODCALLTYPE Reset(ID3D12CommandAllocator * pAllocator, ID3D12PipelineState * pInitialState) override final;
    void    STDMETHODCALLTYPE ClearState(ID3D12PipelineState * pPipelineState) override final;
    void    STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) override final;
    void    STDMETHODCALLTYPE DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) override final;
    void    STDMETHODCALLTYPE Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) override final;
    void    STDMETHODCALLTYPE CopyBufferRegion(ID3D12Resource * pDstBuffer, UINT64 DstOffset, ID3D12Resource * pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes) override final;
    void    STDMETHODCALLTYPE CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION * pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION * pSrc, const D3D12_BOX * pSrcBox) override final;
    void    STDMETHODCALLTYPE CopyResource(ID3D12Resource * pDstResource, ID3D12Resource * pSrcResource) override final;
    void    STDMETHODCALLTYPE CopyTiles(ID3D12Resource * pTiledResource, const D3D12_TILED_RESOURCE_COORDINATE * pTileRegionStartCoordinate, const D3D12_TILE_REGION_SIZE * pTileRegionSize, ID3D12Resource * pBuffer, UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags) override final;
    void    STDMETHODCALLTYPE ResolveSubresource(ID3D12Resource * pDstResource, UINT DstSubresource, ID3D12Resource * pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) override final;
    void    STDMETHODCALLTYPE IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology) override final;
    void    STDMETHODCALLTYPE RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT * pViewports) override final;
    void    STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects, const D3D12_RECT * pRects) override final;
    void    STDMETHODCALLTYPE OMSetBlendFactor(const FLOAT BlendFactor[4]) override final;
    void    STDMETHODCALLTYPE OMSetStencilRef(UINT StencilRef) override final;
    void    STDMETHODCALLTYPE SetPipelineState(ID3D12PipelineState * pPipelineState) override final;
    void    STDMETHODCALLTYPE ResourceBarrier(UINT NumBarriers, const D3D12_RESOURCE_BARRIER * pBarriers) override final;
    void    STDMETHODCALLTYPE ExecuteBundle(ID3D12GraphicsCommandList * pCommandList) override final;
    void    STDMETHODCALLTYPE SetDescriptorHeaps(UINT NumDescriptorHeaps, ID3D12DescriptorHeap* const* ppDescriptorHeaps) override final;
    void    STDMETHODCALLTYPE SetComputeRootSignature(ID3D12RootSignature * pRootSignature) override final;
    void    STDMETHODCALLTYPE SetGraphicsRootSignature(ID3D12RootSignature * pRootSignature) override final;
    void    STDMETHODCALLTYPE SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) override final;
    void    STDMETHODCALLTYPE SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor) override final;
    void    STDMETHODCALLTYPE SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) override final;
    void    STDMETHODCALLTYPE SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues) override final;
    void    STDMETHODCALLTYPE SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues) override final;
    void    STDMETHODCALLTYPE SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues) override final;
    void    STDMETHODCALLTYPE SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override final;
    void    STDMETHODCALLTYPE SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override final;
    void    STDMETHODCALLTYPE SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override final;
    void    STDMETHODCALLTYPE SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override final;
    void    STDMETHODCALLTYPE SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override final;
    void    STDMETHODCALLTYPE SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation) override final;
    void    STDMETHODCALLTYPE IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW * pView) override final;
    void    STDMETHODCALLTYPE IASetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW * pViews) override final;
    void    STDMETHODCALLTYPE SOSetTargets(UINT StartSlot, UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW * pViews) override final;
    void    STDMETHODCALLTYPE OMSetRenderTargets(UINT NumRenderTargetDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE * pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, const D3D12_CPU_DESCRIPTOR_HANDLE * pDepthStencilDescriptor) override final;
    void    STDMETHODCALLTYPE ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT * pRects) override final;
    void    STDMETHODCALLTYPE ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT * pRects) override final;
    void    STDMETHODCALLTYPE ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource * pResource, const UINT Values[4], UINT NumRects, const D3D12_RECT * pRects) override final;
    void    STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap, D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource * pResource, const FLOAT Values[4], UINT NumRects, const D3D12_RECT * pRects) override final;
    void    STDMETHODCALLTYPE DiscardResource(ID3D12Resource * pResource, const D3D12_DISCARD_REGION * pRegion) override final;
    void    STDMETHODCALLTYPE BeginQuery(ID3D12QueryHeap * pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) override final;
    void    STDMETHODCALLTYPE EndQuery(ID3D12QueryHeap * pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index) override final;
    void    STDMETHODCALLTYPE ResolveQueryData(ID3D12QueryHeap * pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource * pDestinationBuffer, UINT64 AlignedDestinationBufferOffset) override final;
    void    STDMETHODCALLTYPE SetPredication(ID3D12Resource * pBuffer, UINT64 AlignedBufferOffset, D3D12_PREDICATION_OP Operation) override final;
    void    STDMETHODCALLTYPE SetMarker(UINT Metadata, const void* pData, UINT Size) override final;
    void    STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void* pData, UINT Size) override final;
    void    STDMETHODCALLTYPE EndEvent() override final;
    void    STDMETHODCALLTYPE ExecuteIndirect(ID3D12CommandSignature * pCommandSignature, UINT MaxCommandCount, ID3D12Resource * pArgumentBuffer, UINT64 ArgumentBufferOffset, ID3D12Resource * pCountBuffer, UINT64 CountBufferOffset) override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList1
    void    STDMETHODCALLTYPE AtomicCopyBufferUINT(ID3D12Resource * pDstBuffer, UINT64 DstOffset, ID3D12Resource * pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource* const* ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 * pDependentSubresourceRanges) override final;
    void    STDMETHODCALLTYPE AtomicCopyBufferUINT64(ID3D12Resource * pDstBuffer, UINT64 DstOffset, ID3D12Resource * pSrcBuffer, UINT64 SrcOffset, UINT Dependencies, ID3D12Resource* const* ppDependentResources, const D3D12_SUBRESOURCE_RANGE_UINT64 * pDependentSubresourceRanges) override final;
    void    STDMETHODCALLTYPE OMSetDepthBounds(FLOAT Min, FLOAT Max) override final;
    void    STDMETHODCALLTYPE SetSamplePositions(UINT NumSamplesPerPixel, UINT NumPixels, D3D12_SAMPLE_POSITION * pSamplePositions) override final;
    void    STDMETHODCALLTYPE ResolveSubresourceRegion(ID3D12Resource * pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, ID3D12Resource * pSrcResource, UINT SrcSubresource, D3D12_RECT * pSrcRect, DXGI_FORMAT Format, D3D12_RESOLVE_MODE ResolveMode) override final;
    void    STDMETHODCALLTYPE SetViewInstanceMask(UINT Mask) override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList2
    void    STDMETHODCALLTYPE WriteBufferImmediate(UINT Count, const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER * pParams, const D3D12_WRITEBUFFERIMMEDIATE_MODE * pModes) override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList3
    void    STDMETHODCALLTYPE SetProtectedResourceSession(ID3D12ProtectedResourceSession * pProtectedResourceSession) override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList4
    void    STDMETHODCALLTYPE BeginRenderPass(UINT NumRenderTargets, const D3D12_RENDER_PASS_RENDER_TARGET_DESC * pRenderTargets, const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC * pDepthStencil, D3D12_RENDER_PASS_FLAGS Flags) override final;
    void    STDMETHODCALLTYPE EndRenderPass(void) override final;
    void    STDMETHODCALLTYPE InitializeMetaCommand(ID3D12MetaCommand * pMetaCommand, const void* pInitializationParametersData, SIZE_T InitializationParametersDataSizeInBytes) override final;
    void    STDMETHODCALLTYPE ExecuteMetaCommand(ID3D12MetaCommand * pMetaCommand, const void* pExecutionParametersData, SIZE_T ExecutionParametersDataSizeInBytes) override final;
    void    STDMETHODCALLTYPE BuildRaytracingAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC * pDesc, UINT NumPostbuildInfoDescs, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC * pPostbuildInfoDescs) override final;
    void    STDMETHODCALLTYPE EmitRaytracingAccelerationStructurePostbuildInfo(const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC * pDesc, UINT NumSourceAccelerationStructures, const D3D12_GPU_VIRTUAL_ADDRESS * pSourceAccelerationStructureData) override final;
    void    STDMETHODCALLTYPE CopyRaytracingAccelerationStructure(D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData, D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode) override final;
    void    STDMETHODCALLTYPE SetPipelineState1(ID3D12StateObject * pStateObject) override final;
    void    STDMETHODCALLTYPE DispatchRays(const D3D12_DISPATCH_RAYS_DESC * pDesc) override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList5
    void   STDMETHODCALLTYPE RSSetShadingRate(D3D12_SHADING_RATE baseShadingRate, const D3D12_SHADING_RATE_COMBINER * combiners) override final;
    void   STDMETHODCALLTYPE RSSetShadingRateImage(ID3D12Resource * shadingRateImage) override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList6
    void   STDMETHODCALLTYPE DispatchMesh(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList7
    void   STDMETHODCALLTYPE Barrier(UINT32 NumBarrierGroups, const D3D12_BARRIER_GROUP * pBarrierGroups) override final;
#pragma endregion
#pragma region ID3D12GraphicsCommandList8
    void   STDMETHODCALLTYPE OMSetFrontAndBackStencilRef(UINT FrontStencilRef, UINT BackStencilRef) override final;
#pragma endregion

    uint8_t padding[8];
    ID3D12GraphicsCommandList* m_base{}; // IMPORTANT: Must be at a fixed offset to support tools, do not move!

    bool checkAndUpgradeInterface(REFIID riid);

    bool m_trackState = true;
    std::atomic<LONG> m_refCount = 1;
    unsigned int m_interfaceVersion{};
    D3D12Device* const m_device{};

    // Used to restore states
    struct Constants
    {
        uint32_t DestOffsetIn32BitValues = 0;
        uint32_t Num32BitValuesToSet = 0;
        uint32_t SrcData[kMaxComputeRoot32BitConstCount] = {};
    };
    uint8_t m_numHeaps{};
    ID3D12RootSignature* m_rootSignature{};
    ID3D12PipelineState* m_pso{};
    ID3D12StateObject* m_so{};
    ID3D12DescriptorHeap* m_heaps[kMaxHeapCount]{};
    std::map<uint32_t, D3D12_GPU_DESCRIPTOR_HANDLE> m_mapHandles{};
    std::map<uint32_t, D3D12_GPU_VIRTUAL_ADDRESS> m_mapCBV{};
    std::map<uint32_t, D3D12_GPU_VIRTUAL_ADDRESS> m_mapSRV{};
    std::map<uint32_t, D3D12_GPU_VIRTUAL_ADDRESS> m_mapUAV{};
    std::map<uint32_t, Constants> m_mapConstants{};
};

}
}

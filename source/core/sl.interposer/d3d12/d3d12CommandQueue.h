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
#include <atomic>

struct D3D12Device;
struct D3D12CommandQueueDownlevel;

namespace sl
{
namespace interposer
{

struct DECLSPEC_UUID("22C3768E-AB10-4870-B03B-2B52E21B1063") D3D12CommandQueue : ID3D12CommandQueue
{
    D3D12CommandQueue(D3D12Device * device, ID3D12CommandQueue * original);

    D3D12CommandQueue(const D3D12CommandQueue&) = delete;
    D3D12CommandQueue& operator=(const D3D12CommandQueue&) = delete;

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
#pragma region ID3D12CommandQueue
    void    STDMETHODCALLTYPE UpdateTileMappings(ID3D12Resource * pResource, UINT NumResourceRegions, const D3D12_TILED_RESOURCE_COORDINATE * pResourceRegionStartCoordinates, const D3D12_TILE_REGION_SIZE * pResourceRegionSizes, ID3D12Heap * pHeap, UINT NumRanges, const D3D12_TILE_RANGE_FLAGS * pRangeFlags, const UINT * pHeapRangeStartOffsets, const UINT * pRangeTileCounts, D3D12_TILE_MAPPING_FLAGS Flags) override final;
    void    STDMETHODCALLTYPE CopyTileMappings(ID3D12Resource * pDstResource, const D3D12_TILED_RESOURCE_COORDINATE * pDstRegionStartCoordinate, ID3D12Resource * pSrcResource, const D3D12_TILED_RESOURCE_COORDINATE * pSrcRegionStartCoordinate, const D3D12_TILE_REGION_SIZE * pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags) override final;
    void    STDMETHODCALLTYPE ExecuteCommandLists(UINT NumCommandLists, ID3D12CommandList* const* ppCommandLists) override final;
    void    STDMETHODCALLTYPE SetMarker(UINT Metadata, const void* pData, UINT Size) override final;
    void    STDMETHODCALLTYPE BeginEvent(UINT Metadata, const void* pData, UINT Size) override final;
    void    STDMETHODCALLTYPE EndEvent() override final;
    HRESULT STDMETHODCALLTYPE Signal(ID3D12Fence * pFence, UINT64 Value) override final;
    HRESULT STDMETHODCALLTYPE Wait(ID3D12Fence * pFence, UINT64 Value) override final;
    HRESULT STDMETHODCALLTYPE GetTimestampFrequency(UINT64 * pFrequency) override final;
    HRESULT STDMETHODCALLTYPE GetClockCalibration(UINT64 * pGpuTimestamp, UINT64 * pCpuTimestamp) override final;
    D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc() override final;
#pragma endregion

    uint8_t padding[8];
    ID3D12CommandQueue* m_base{}; // IMPORTANT: Must be at a fixed offset to support tools, do not move!

    bool checkAndUpgradeInterface(REFIID riid);

    std::atomic<LONG> m_refCount = 1;
    unsigned int m_interfaceVersion{};
    D3D12Device* const m_device{};
};

}
}

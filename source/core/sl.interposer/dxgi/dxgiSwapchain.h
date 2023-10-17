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

#include <dxgi1_5.h>
#include <cinttypes>
#include <atomic>
#include <include/sl_version.h>

struct ID3D11Device;
struct ID3D12Device;

namespace sl
{
namespace interposer
{

struct DECLSPEC_UUID("D3F0BBFF-3091-4074-9D9E-B99CE2E5CF9A") DXGISwapChain : IDXGISwapChain4
{
    DXGISwapChain(ID3D12Device* device, IDXGISwapChain* original);
    DXGISwapChain(ID3D11Device* device, IDXGISwapChain* original);

    DXGISwapChain(const DXGISwapChain&) = delete;
    DXGISwapChain& operator=(const DXGISwapChain&) = delete;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override final;
    ULONG   STDMETHODCALLTYPE AddRef() override final;
    ULONG   STDMETHODCALLTYPE Release() override final;

#pragma region IDXGIObject
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void* pData) override final;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown * pUnknown) override final;
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT * pDataSize, void* pData) override final;
    HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent) override final;
#pragma endregion
#pragma region IDXGIDeviceSubObject
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void** ppDevice) override final;
#pragma endregion
#pragma region IDXGISwapChain
    HRESULT STDMETHODCALLTYPE Present(UINT SyncInterval, UINT Flags) override final;
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT Buffer, REFIID riid, void** ppSurface) override final;
    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL Fullscreen, IDXGIOutput * pTarget) override final;
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL * pFullscreen, IDXGIOutput * *ppTarget) override final;
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC * pDesc) override final;
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) override final;
    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC * pNewTargetParameters) override final;
    HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput * *ppOutput) override final;
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS * pStats) override final;
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT * pLastPresentCount) override final;
#pragma endregion
#pragma region IDXGISwapChain1
    HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_SWAP_CHAIN_DESC1 * pDesc) override final;
    HRESULT STDMETHODCALLTYPE GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC * pDesc) override final;
    HRESULT STDMETHODCALLTYPE GetHwnd(HWND * pHwnd) override final;
    HRESULT STDMETHODCALLTYPE GetCoreWindow(REFIID refiid, void** ppUnk) override final;
    HRESULT STDMETHODCALLTYPE Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS * pPresentParameters) override final;
    BOOL    STDMETHODCALLTYPE IsTemporaryMonoSupported() override final;
    HRESULT STDMETHODCALLTYPE GetRestrictToOutput(IDXGIOutput * *ppRestrictToOutput) override final;
    HRESULT STDMETHODCALLTYPE SetBackgroundColor(const DXGI_RGBA * pColor) override final;
    HRESULT STDMETHODCALLTYPE GetBackgroundColor(DXGI_RGBA * pColor) override final;
    HRESULT STDMETHODCALLTYPE SetRotation(DXGI_MODE_ROTATION Rotation) override final;
    HRESULT STDMETHODCALLTYPE GetRotation(DXGI_MODE_ROTATION * pRotation) override final;
#pragma endregion
#pragma region IDXGISwapChain2
    HRESULT STDMETHODCALLTYPE SetSourceSize(UINT Width, UINT Height) override final;
    HRESULT STDMETHODCALLTYPE GetSourceSize(UINT * pWidth, UINT * pHeight) override final;
    HRESULT STDMETHODCALLTYPE SetMaximumFrameLatency(UINT MaxLatency) override final;
    HRESULT STDMETHODCALLTYPE GetMaximumFrameLatency(UINT * pMaxLatency) override final;
    HANDLE  STDMETHODCALLTYPE GetFrameLatencyWaitableObject() override final;
    HRESULT STDMETHODCALLTYPE SetMatrixTransform(const DXGI_MATRIX_3X2_F * pMatrix) override final;
    HRESULT STDMETHODCALLTYPE GetMatrixTransform(DXGI_MATRIX_3X2_F * pMatrix) override final;
#pragma endregion
#pragma region IDXGISwapChain3
    UINT    STDMETHODCALLTYPE GetCurrentBackBufferIndex() override final;
    HRESULT STDMETHODCALLTYPE CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT * pColorSpaceSupport) override final;
    HRESULT STDMETHODCALLTYPE SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace) override final;
    HRESULT STDMETHODCALLTYPE ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT * pCreationNodeMask, IUnknown* const* ppPresentQueue) override final;
#pragma endregion
#pragma region IDXGISwapChain4
    HRESULT STDMETHODCALLTYPE SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void* pMetaData) override final;
#pragma endregion

    uint8_t padding[8];
    IDXGISwapChain* m_base; // IMPORTANT: Must be at a fixed offset to support tools, do not move!

    bool checkAndUpgradeInterface(REFIID riid);

    IUnknown* const m_d3dDevice;
    std::atomic<LONG> m_refCount = 1;
    uint32_t m_d3dVersion;
    uint32_t m_interfaceVersion;
    sl::Version m_cachedHostSDKVersion;
};
}
}

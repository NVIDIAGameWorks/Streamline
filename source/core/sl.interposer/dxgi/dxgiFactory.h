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

#include <dxgi1_6.h>
#include <cinttypes>
#include <atomic>

namespace sl
{
namespace interposer
{

struct DECLSPEC_UUID("AABDF0C6-6A76-4F65-987D-F2CC4C27ED0E") DXGIFactory : IDXGIFactory7
{
    DXGIFactory(IDXGIFactory* original);
    
    DXGIFactory(const DXGIFactory&) = delete;
    DXGIFactory& operator=(const DXGIFactory&) = delete;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObj) override final;
    ULONG   STDMETHODCALLTYPE AddRef() override final;
    ULONG   STDMETHODCALLTYPE Release() override final;

#pragma region IDXGIObject
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize, const void* pData) override final;
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID Name, const IUnknown * pUnknown) override final;
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT * pDataSize, void* pData) override final;
    HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void** ppParent) override final;
#pragma endregion
#pragma region IDXGIFactory
    HRESULT STDMETHODCALLTYPE EnumAdapters(UINT Adapter, IDXGIAdapter * *ppAdapter) override final;
    HRESULT STDMETHODCALLTYPE MakeWindowAssociation(HWND WindowHandle, UINT Flags) override final;
    HRESULT STDMETHODCALLTYPE GetWindowAssociation(HWND * pWindowHandle) override final;
    HRESULT STDMETHODCALLTYPE CreateSwapChain(IUnknown * pDevice, DXGI_SWAP_CHAIN_DESC * pDesc,IDXGISwapChain * *ppSwapChain) override final;
    HRESULT STDMETHODCALLTYPE CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter * *ppAdapter) override final;
#pragma endregion
#pragma region IDXGIFactory1
    HRESULT STDMETHODCALLTYPE EnumAdapters1(UINT Adapter,IDXGIAdapter1 * *ppAdapter) override final;
    BOOL STDMETHODCALLTYPE IsCurrent(void) override final;
#pragma endregion
#pragma region IDXGIFactory2
    BOOL STDMETHODCALLTYPE IsWindowedStereoEnabled(void) override final;

    HRESULT STDMETHODCALLTYPE CreateSwapChainForHwnd(IUnknown * pDevice,
        HWND hWnd,
        const DXGI_SWAP_CHAIN_DESC1 * pDesc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC * pFullscreenDesc,
        IDXGIOutput * pRestrictToOutput,
        IDXGISwapChain1 * *ppSwapChain) override final;

    HRESULT STDMETHODCALLTYPE CreateSwapChainForCoreWindow(
        IUnknown * pDevice,
        IUnknown * pWindow,
        const DXGI_SWAP_CHAIN_DESC1 * pDesc,
        IDXGIOutput * pRestrictToOutput,
        IDXGISwapChain1 * *ppSwapChain) override final;

    HRESULT STDMETHODCALLTYPE GetSharedResourceAdapterLuid(HANDLE hResource, LUID * pLuid) override final;

    HRESULT STDMETHODCALLTYPE RegisterStereoStatusWindow(HWND WindowHandle,UINT wMsg,DWORD * pdwCookie) override final;
    HRESULT STDMETHODCALLTYPE RegisterStereoStatusEvent(HANDLE hEvent, DWORD * pdwCookie) override final;
    void STDMETHODCALLTYPE UnregisterStereoStatus(DWORD dwCookie) override final;
    HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusWindow(HWND WindowHandle, UINT wMsg, DWORD * pdwCookie) override final;
    HRESULT STDMETHODCALLTYPE RegisterOcclusionStatusEvent(HANDLE hEvent,DWORD * pdwCookie) override final;
    void STDMETHODCALLTYPE UnregisterOcclusionStatus(DWORD dwCookie) override final;
    HRESULT STDMETHODCALLTYPE CreateSwapChainForComposition(
        _In_  IUnknown * pDevice,
        _In_  const DXGI_SWAP_CHAIN_DESC1 * pDesc,
        _In_opt_  IDXGIOutput * pRestrictToOutput,
        _COM_Outptr_  IDXGISwapChain1 * *ppSwapChain) override final;
#pragma endregion
#pragma region IDXGIFactory3
    UINT STDMETHODCALLTYPE GetCreationFlags(void) override final;
#pragma endregion
#pragma region IDXGIFactory4
    HRESULT STDMETHODCALLTYPE EnumAdapterByLuid(LUID AdapterLuid,REFIID riid,void** ppvAdapter) override final;
    HRESULT STDMETHODCALLTYPE EnumWarpAdapter(REFIID riid, void** ppvAdapter) override final;
#pragma endregion
#pragma region IDXGIFactory5
    HRESULT STDMETHODCALLTYPE CheckFeatureSupport(DXGI_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize) override final;
#pragma endregion
#pragma region IDXGIFactory6
    HRESULT STDMETHODCALLTYPE EnumAdapterByGpuPreference(UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void** ppvAdapter) override final;
#pragma endregion
#pragma region IDXGIFactory7
    HRESULT STDMETHODCALLTYPE RegisterAdaptersChangedEvent(HANDLE hEvent,DWORD * pdwCookie) override final;
    HRESULT STDMETHODCALLTYPE UnregisterAdaptersChangedEvent(DWORD dwCookie) override final;
#pragma endregion

    uint8_t padding[8];
    IDXGIFactory* m_base; // IMPORTANT: Must be at a fixed offset to support tools, do not move!

    bool checkAndUpgradeInterface(REFIID riid);

    std::atomic<LONG> m_refCount = 1;
    uint32_t m_interfaceVersion;
};
}
}

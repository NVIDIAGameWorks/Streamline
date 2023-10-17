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
#include <d3d11.h>

#include "source/core/sl.interposer/dxgi/DXGIFactory.h"
#include "source/core/sl.interposer/dxgi/DXGISwapchain.h"
#include "source/core/sl.interposer/d3d12/d3d12Device.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandQueue.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin-manager/pluginManager.h"
#include "include/sl_hooks.h"

namespace sl
{
namespace interposer
{

static_assert(offsetof(DXGIFactory, m_base) == 16, "This location must be maintained to keep compatibility with Nsight tools");

UINT queryDevice(IUnknown*& device, Microsoft::WRL::ComPtr<IUnknown>& deviceProxy)
{
    if (Microsoft::WRL::ComPtr<ID3D11Device> d3d11Device; SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Device), &d3d11Device)))
    {
        device = d3d11Device.Get();
        deviceProxy = device;
        return 11;
    }
    if (Microsoft::WRL::ComPtr<D3D12CommandQueue> cmdQueueD3D12ProxySL; SUCCEEDED(device->QueryInterface(__uuidof(D3D12CommandQueue), &cmdQueueD3D12ProxySL)))
    {
        device = cmdQueueD3D12ProxySL->m_base;
        deviceProxy = std::move(reinterpret_cast<Microsoft::WRL::ComPtr<IUnknown> &>(cmdQueueD3D12ProxySL));
        return 12;
    }
    if (Microsoft::WRL::ComPtr<ID3D12CommandQueue> cmdQueueD3D12Proxy; SUCCEEDED(device->QueryInterface(__uuidof(ID3D12CommandQueue), &cmdQueueD3D12Proxy)))
    {
        // Base interface, bypassed SL
        SL_LOG_WARN("Detected base interface 'ID3D12CommandQueue' while expecting SL proxy - please use slUpgradeDevice to obtain SL proxies for DXGI/D3D interfaces");
        device = cmdQueueD3D12Proxy.Get();
        deviceProxy = device;
        return 12;
    }


    // This can happen when we get called with standard interfaces like ID3D12CommandQueue - most likely another interposer is present
    return 0;
}

template <typename T>
void setupSwapchainProxy(T*& swapchain, UINT d3dVersion, const Microsoft::WRL::ComPtr<IUnknown>& deviceProxy, DXGI_USAGE usage)
{
    DXGISwapChain* swapchainProxy = nullptr;

    //! IMPORTANT: Check if any plugin required hook into the swap chain
    if (!sl::plugin_manager::getInterface()->isProxyNeeded("IDXGISwapChain"))
    {
        SL_LOG_INFO("IDXGISwapChain proxy not required, skipping");
        return;
    }

    if (d3dVersion == 11)
    {
        const Microsoft::WRL::ComPtr<ID3D11Device>& device = reinterpret_cast<const Microsoft::WRL::ComPtr<ID3D11Device> &>(deviceProxy);
        swapchainProxy = new DXGISwapChain((ID3D11Device*)device.Get(), swapchain);
    }
    else
    {
        // d3d12
        if (Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain3; SUCCEEDED(swapchain->QueryInterface(__uuidof(IDXGISwapChain3), &swapchain3)))
        {
            // Incoming "proxy" could really be our proxy or base interface so check
            D3D12CommandQueue* cmdQueueSL{};
            if (SUCCEEDED(deviceProxy->QueryInterface(&cmdQueueSL)))
            {
                // SL proxy!
                cmdQueueSL->Release();
                swapchainProxy = new DXGISwapChain(cmdQueueSL->m_device->m_base, swapchain3.Get());
            }
            else
            { 
                ID3D12CommandQueue* cmdQueue{};
                if (SUCCEEDED(deviceProxy->QueryInterface(&cmdQueue)))
                {
                    // Host could be using AMD AGS or some other SDK which would bypass SL and provide base interfaces
                    cmdQueue->Release();
                    ID3D12Device* device;
                    if(SUCCEEDED(cmdQueue->GetDevice(__uuidof(*device), reinterpret_cast<void**>(&device))))
                    {
                        swapchainProxy = new DXGISwapChain(device, swapchain3.Get());
                        device->Release();
                    }
                }
            }
        }
        else
        {
            SL_LOG_WARN("Skipping swap chain because it is missing support for the IDXGISwapChain3 interface.");
        }
    }

    if (swapchainProxy != nullptr)
    {
        // we're losing the ref to the swapchain here - so we're decrementing the ref count
        if (swapchain && swapchain != swapchainProxy)
        {
            swapchain->Release();
        }
        swapchain = swapchainProxy;
    }
}

DXGIFactory::DXGIFactory(IDXGIFactory* original) :
    m_base(original),
    m_interfaceVersion(0)
{
    assert(m_base != nullptr);
    m_base->AddRef();
}

bool DXGIFactory::checkAndUpgradeInterface(REFIID riid)
{
    if (riid == __uuidof(this) || riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject))
    {
        return true;
    }

    static const IID iidLookup[] = {
        __uuidof(IDXGIFactory),
        __uuidof(IDXGIFactory1),
        __uuidof(IDXGIFactory2),
        __uuidof(IDXGIFactory3),
        __uuidof(IDXGIFactory4),
        __uuidof(IDXGIFactory5),
        __uuidof(IDXGIFactory6),
        __uuidof(IDXGIFactory7),
    };

    for (unsigned int version = 0; version < ARRAYSIZE(iidLookup); ++version)
    {
        if (riid != iidLookup[version])
            continue;

        if (version > m_interfaceVersion)
        {
            IUnknown* newInterface = nullptr;
            if (FAILED(m_base->QueryInterface(riid, reinterpret_cast<void**>(&newInterface))))
            {
                return false;
            }
            SL_LOG_VERBOSE("Upgraded IDXGIFactory v%u to v%u", m_interfaceVersion, version);
            m_base->Release();
            m_base = static_cast<IDXGIFactory*>(newInterface);
            m_interfaceVersion = version;
        }
        return true;
    }

    return false;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::QueryInterface(REFIID riid, void** ppvObj)
{
    if (ppvObj == nullptr)
        return E_POINTER;

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

ULONG STDMETHODCALLTYPE DXGIFactory::AddRef()
{
    m_base->AddRef();
    return ++m_refCount;
}

ULONG STDMETHODCALLTYPE DXGIFactory::Release()
{
    auto refOrig = m_base->Release();
    const ULONG ref = --m_refCount;
    if (ref > 0)
    {
        return ref;
    }
    // Base and our interface don't start with identical reference counts so no point in comparing them

    SL_LOG_INFO("Destroyed DXGIFactory proxy 0x%llx - native factory 0x%llx ref count %ld", this, m_base, refOrig);

    delete this;
    return 0;
}

HRESULT STDMETHODCALLTYPE DXGIFactory::SetPrivateData(REFGUID Name, UINT DataSize, const void* pData)
{
    return m_base->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown)
{
    return m_base->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData)
{
    return m_base->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetParent(REFIID riid, void** ppParent)
{
    return m_base->GetParent(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapters(UINT Adapter, IDXGIAdapter** ppAdapter) 
{ 
    return m_base->EnumAdapters(Adapter, ppAdapter); 
}
HRESULT STDMETHODCALLTYPE DXGIFactory::MakeWindowAssociation(HWND WindowHandle, UINT Flags) 
{ 
    return m_base->MakeWindowAssociation(WindowHandle, Flags); 
}
HRESULT STDMETHODCALLTYPE DXGIFactory::GetWindowAssociation(HWND* pWindowHandle) 
{ 
    return m_base->GetWindowAssociation(pWindowHandle);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChain(IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
{
    if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
    {
        return DXGI_ERROR_INVALID_CALL;
    }

    *ppSwapChain = nullptr;

    DXGI_SWAP_CHAIN_DESC desc = *pDesc;

    Microsoft::WRL::ComPtr<IUnknown> deviceProxy;
    const UINT d3dVersion = queryDevice(pDevice, deviceProxy);
    if (!d3dVersion)
    {
        SL_LOG_ERROR( "Unable to find device proxy - please use slUpgradeDevice to obtain SL proxies for DXGI/D3D interfaces");
        return DXGI_ERROR_INVALID_CALL;
    }

    HRESULT hr = S_OK;
    bool skip = false;
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGIFactory_CreateSwapChain);
        for (auto [hook, feature] : hooks)
        {
            hr = ((PFunCreateSwapChainBefore*)hook)(m_base, pDevice, &desc, ppSwapChain, skip);
            if (FAILED(hr))
            {
                SL_LOG_WARN("PFunCreateSwapChainBefore failed %s", std::system_category().message(hr).c_str());
                return hr;
            }
        }
    }

    if (!skip)
    {
        hr = m_base->CreateSwapChain(pDevice, &desc, ppSwapChain);
        if (FAILED(hr))
        {
            SL_LOG_WARN("CreateSwapChain failed %s", std::system_category().message(hr).c_str());
            return hr;
        }
    }

    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eIDXGIFactory_CreateSwapChain);
        for (auto [hook, feature] : hooks) ((PFunCreateSwapChainAfter*)hook)(m_base, pDevice, &desc, ppSwapChain);
    }

    setupSwapchainProxy(*ppSwapChain, d3dVersion, deviceProxy, desc.BufferUsage);

    return hr;
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSoftwareAdapter(HMODULE Module, IDXGIAdapter** ppAdapter)
{
    return m_base->CreateSoftwareAdapter(Module, ppAdapter);
}

#pragma region IDXGIFactory1
HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapters1(UINT Adapter, IDXGIAdapter1** ppAdapter)
{
    return ((IDXGIFactory1*)m_base)->EnumAdapters1(Adapter, ppAdapter);
}
BOOL STDMETHODCALLTYPE DXGIFactory::IsCurrent(void)
{
    return ((IDXGIFactory2*)m_base)->IsCurrent();
}

#pragma endregion
#pragma region IDXGIFactory2
BOOL STDMETHODCALLTYPE DXGIFactory::IsWindowedStereoEnabled(void)
{
    return ((IDXGIFactory2*)m_base)->IsWindowedStereoEnabled();
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForHwnd(IUnknown* pDevice,
    HWND hWnd,
    const DXGI_SWAP_CHAIN_DESC1* pDesc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc,
    IDXGIOutput* pRestrictToOutput,
    IDXGISwapChain1** ppSwapChain)
{

    if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
    {
        return DXGI_ERROR_INVALID_CALL;
    }

    *ppSwapChain = nullptr;

    DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc = {};
    if (pFullscreenDesc != nullptr)
    {
        fullscreenDesc = *pFullscreenDesc;
    }
    else
    {
        fullscreenDesc.Windowed = TRUE;
    }

    Microsoft::WRL::ComPtr<IUnknown> deviceProxy;
    const UINT d3dVersion = queryDevice(pDevice, deviceProxy);
    if (!d3dVersion)
    {
        SL_LOG_ERROR( "Unable to find device proxy - please use slUpgradeDevice to obtain SL proxies for DXGI/D3D interfaces");
        return DXGI_ERROR_INVALID_CALL;
    }

    assert(m_interfaceVersion >= 2);
    
    HRESULT hr = S_OK;
    bool skip = false;
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGIFactory_CreateSwapChainForHwnd);
        for (auto [hook, feature] : hooks)
        {
            hr = ((PFunCreateSwapChainForHwndBefore*)hook)((IDXGIFactory2*)m_base, pDevice, hWnd, &desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain, skip);
            if (FAILED(hr))
            {
                SL_LOG_WARN("PFunCreateSwapChainForHwndBefore failed %s", std::system_category().message(hr).c_str());
                return hr;
            }
        }
    }

    if (!skip)
    {
        hr = ((IDXGIFactory2*)m_base)->CreateSwapChainForHwnd(pDevice, hWnd, pDesc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
        if (FAILED(hr))
        {
            SL_LOG_WARN("CreateSwapChainForHwnd failed %s", std::system_category().message(hr).c_str());
            return hr;
        }
    }

    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eIDXGIFactory_CreateSwapChainForHwnd);
        for (auto [hook, feature] : hooks) ((PFunCreateSwapChainForHwndAfter*)hook)((IDXGIFactory2*)m_base, pDevice, hWnd, &desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
    }
    setupSwapchainProxy(*ppSwapChain, d3dVersion, deviceProxy, desc.BufferUsage);

    return hr;
    
}

HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForCoreWindow(
    IUnknown* pDevice,
    IUnknown* pWindow,
    const DXGI_SWAP_CHAIN_DESC1* pDesc,
    IDXGIOutput* pRestrictToOutput,
    IDXGISwapChain1** ppSwapChain)
{
    if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
    {
        return DXGI_ERROR_INVALID_CALL;
    }

    *ppSwapChain = nullptr;

    DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

    Microsoft::WRL::ComPtr<IUnknown> deviceProxy;
    const UINT d3dVersion = queryDevice(pDevice, deviceProxy);
    if (!d3dVersion)
    {
        SL_LOG_ERROR( "Unable to find device proxy - please use slUpgradeDevice to obtain SL proxies for DXGI/D3D interfaces");
        return DXGI_ERROR_INVALID_CALL;
    }
    
    assert(m_interfaceVersion >= 2);

    HRESULT hr = S_OK;
    bool skip = false;
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGIFactory_CreateSwapChainForCoreWindow);
        for (auto [hook, feature] : hooks)
        {
            hr = ((PFunCreateSwapChainForCoreWindowBefore*)hook)((IDXGIFactory2*)m_base, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain, skip);
            if (FAILED(hr))
            {
                SL_LOG_WARN("PFunCreateSwapChainForCoreWindowBefore failed %s", std::system_category().message(hr).c_str());
                return hr;
            }
        }
    }

    if (!skip)
    {
        hr = ((IDXGIFactory2*)m_base)->CreateSwapChainForCoreWindow(pDevice, pWindow, pDesc, pRestrictToOutput, ppSwapChain);
        if (FAILED(hr))
        {
            SL_LOG_WARN("CreateSwapChainForCoreWindow failed %s", std::system_category().message(hr).c_str());
            return hr;
        }
    }

    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eIDXGIFactory_CreateSwapChainForCoreWindow);
        for (auto [hook, feature] : hooks) ((PFunCreateSwapChainForCoreWindowAfter*)hook)((IDXGIFactory2*)m_base, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);
    }

    setupSwapchainProxy(*ppSwapChain, d3dVersion, deviceProxy, desc.BufferUsage);

    return hr;
    
}

HRESULT STDMETHODCALLTYPE DXGIFactory::GetSharedResourceAdapterLuid(HANDLE hResource, LUID* pLuid)
{
    return ((IDXGIFactory2*)m_base)->GetSharedResourceAdapterLuid(hResource, pLuid);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterStereoStatusWindow(HWND WindowHandle, UINT wMsg, DWORD* pdwCookie)
{
    return ((IDXGIFactory2*)m_base)->RegisterStereoStatusWindow(WindowHandle, wMsg, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterStereoStatusEvent(HANDLE hEvent, DWORD* pdwCookie)
{
    return ((IDXGIFactory2*)m_base)->RegisterStereoStatusEvent(hEvent, pdwCookie);
}
void STDMETHODCALLTYPE DXGIFactory::UnregisterStereoStatus(DWORD dwCookie)
{
    return ((IDXGIFactory2*)m_base)->UnregisterStereoStatus(dwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterOcclusionStatusWindow(HWND WindowHandle, UINT wMsg, DWORD* pdwCookie)
{
    return ((IDXGIFactory2*)m_base)->RegisterOcclusionStatusWindow(WindowHandle, wMsg, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterOcclusionStatusEvent(HANDLE hEvent, DWORD* pdwCookie)
{
    return ((IDXGIFactory2*)m_base)->RegisterOcclusionStatusEvent(hEvent, pdwCookie);
}
void STDMETHODCALLTYPE DXGIFactory::UnregisterOcclusionStatus(DWORD dwCookie)
{
    return ((IDXGIFactory2*)m_base)->UnregisterOcclusionStatus(dwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::CreateSwapChainForComposition(
    _In_  IUnknown* pDevice,
    _In_  const DXGI_SWAP_CHAIN_DESC1* pDesc,
    _In_opt_  IDXGIOutput* pRestrictToOutput,
    _COM_Outptr_  IDXGISwapChain1** ppSwapChain)
{
    return ((IDXGIFactory2*)m_base)->CreateSwapChainForComposition(
        pDevice,
        pDesc,
        pRestrictToOutput,
        ppSwapChain);
}
#pragma endregion
#pragma region IDXGIFactory3
UINT STDMETHODCALLTYPE DXGIFactory::GetCreationFlags(void)
{
    return ((IDXGIFactory3*)m_base)->GetCreationFlags();
}
#pragma endregion
#pragma region IDXGIFactory4
HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapterByLuid(LUID AdapterLuid, REFIID riid, void** ppvAdapter)
{
    return ((IDXGIFactory4*)m_base)->EnumAdapterByLuid(AdapterLuid, riid, ppvAdapter);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::EnumWarpAdapter(REFIID riid, void** ppvAdapter)
{
    return ((IDXGIFactory4*)m_base)->EnumWarpAdapter(riid, ppvAdapter);
}
#pragma endregion
#pragma region IDXGIFactory5
HRESULT STDMETHODCALLTYPE DXGIFactory::CheckFeatureSupport(DXGI_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize)
{
    return ((IDXGIFactory5*)m_base)->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}
#pragma endregion
#pragma region IDXGIFactory6
HRESULT STDMETHODCALLTYPE DXGIFactory::EnumAdapterByGpuPreference(UINT Adapter, DXGI_GPU_PREFERENCE GpuPreference, REFIID riid, void** ppvAdapter)
{
    return ((IDXGIFactory6*)m_base)->EnumAdapterByGpuPreference(Adapter, GpuPreference, riid, ppvAdapter);
}
#pragma endregion
#pragma region IDXGIFactory7
HRESULT STDMETHODCALLTYPE DXGIFactory::RegisterAdaptersChangedEvent(HANDLE hEvent, DWORD* pdwCookie)
{
    return ((IDXGIFactory7*)m_base)->RegisterAdaptersChangedEvent(hEvent, pdwCookie);
}
HRESULT STDMETHODCALLTYPE DXGIFactory::UnregisterAdaptersChangedEvent(DWORD dwCookie)
{
    return ((IDXGIFactory7*)m_base)->UnregisterAdaptersChangedEvent(dwCookie);
}

}
}

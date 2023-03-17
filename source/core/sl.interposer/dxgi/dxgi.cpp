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
#include <assert.h>

#include "source/core/sl.interposer/hook.h"
#include "source/core/sl.interposer/dxgi/dxgiSwapchain.h"
#include "source/core/sl.interposer/dxgi/dxgiFactory.h"
#include "source/core/sl.interposer/d3d12/d3d12.h"
#include "source/core/sl.interposer/d3d12/d3d12Device.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandQueue.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.plugin-manager/pluginManager.h"
#include "include/sl_hooks.h"

namespace sl
{
namespace interposer
{

static sl::interposer::ExportedFunction hookCreateDXGIFactory("CreateDXGIFactory");
static sl::interposer::ExportedFunction hookCreateDXGIFactory1("CreateDXGIFactory1");
static sl::interposer::ExportedFunction hookCreateDXGIFactory2("CreateDXGIFactory2");
static sl::interposer::ExportedFunction hookGetDebugInterface1("DXGIGetDebugInterface1");
static sl::interposer::ExportedFunction hookDeclareAdapterRemovalSupport("DXGIDeclareAdapterRemovalSupport");

static sl::interposer::ExportedFunction hookCreateSwapChain("IDXGIFactory::CreateSwapChain");
static sl::interposer::ExportedFunction hookCreateSwapChainForHwnd("IDXGIFactory2::CreateSwapChainForHwnd");
static sl::interposer::ExportedFunction hookCreateSwapChainForCoreWindow("IDXGIFactory2::CreateSwapChainForCoreWindow");
static sl::interposer::ExportedFunction hookCreateSwapChainForComposition("IDXGIFactory2::CreateSwapChainForComposition");

void loadDXGIModule();


extern UINT queryDevice(IUnknown*& device, Microsoft::WRL::ComPtr<IUnknown>& deviceProxy);

template <typename T>
extern void setupSwapchainProxy(T*& swapchain, UINT d3dVersion, const Microsoft::WRL::ComPtr<IUnknown>& deviceProxy, DXGI_USAGE usage);

HRESULT STDMETHODCALLTYPE IDXGIFactory_CreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
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
        // We cannot resolve the provided device, this happens when another interposer is present
        //
        // Restore the original code at the target function so we can call the original method
        sl::interposer::getInterface()->restoreOriginalCode(hookCreateSwapChain);
        auto hr = sl::interposer::call(IDXGIFactory_CreateSwapChain, hookCreateSwapChain)(pFactory, pDevice, &desc, ppSwapChain);
        // Now restore current code (jmp interposer.dll::hook_for_this_function)
        sl::interposer::getInterface()->restoreCurrentCode(hookCreateSwapChain);
        return hr;
    }

    HRESULT hr = S_OK;
    bool skip = false;
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGIFactory_CreateSwapChain);
        for (auto [hook, feature] : hooks)
        {
            hr = ((PFunCreateSwapChainBefore*)hook)(pFactory, pDevice, &desc, ppSwapChain, skip);
            if (FAILED(hr))
            {
                SL_LOG_WARN("PFunCreateSwapChainBefore failed %s", std::system_category().message(hr).c_str());
                return hr;
            }
        }
    }

    if (!skip)
    {
        hr = sl::interposer::call(IDXGIFactory_CreateSwapChain, hookCreateSwapChain)(pFactory, pDevice, &desc, ppSwapChain);
        if (FAILED(hr))
        {
            SL_LOG_WARN("CreateSwapChain failed %s", std::system_category().message(hr).c_str());
            return hr;
        }
    }

    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eIDXGIFactory_CreateSwapChain);
        for (auto [hook, feature] : hooks) ((PFunCreateSwapChainAfter*)hook)(pFactory, pDevice, &desc, ppSwapChain);
    }

    setupSwapchainProxy(*ppSwapChain, d3dVersion, deviceProxy, desc.BufferUsage);

    return hr;
}

HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForHwnd(IDXGIFactory2* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
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
        // We cannot resolve the provided device, this happens when another interposer is present
        //
        // Restore the original code at the target function so we can call the original method
        sl::interposer::getInterface()->restoreOriginalCode(hookCreateSwapChainForHwnd);
        auto hr = sl::interposer::call(IDXGIFactory2_CreateSwapChainForHwnd, hookCreateSwapChainForHwnd)(pFactory, pDevice, hWnd, &desc, fullscreenDesc.Windowed ? nullptr : &fullscreenDesc, pRestrictToOutput, ppSwapChain);
        // Now restore current code (jmp interposer.dll::hook_for_this_function)
        sl::interposer::getInterface()->restoreCurrentCode(hookCreateSwapChainForHwnd);
        return hr;
    }

    HRESULT hr = S_OK;
    bool skip = false;
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGIFactory_CreateSwapChainForHwnd);
        for (auto [hook, feature] : hooks)
        {
            hr = ((PFunCreateSwapChainForHwndBefore*)hook)(pFactory, pDevice, hWnd, &desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain, skip);
            if (FAILED(hr))
            {
                SL_LOG_WARN("PFunCreateSwapChainForHwndBefore failed %s", std::system_category().message(hr).c_str());
                return hr;
            }
        }
    }

    if (!skip)
    {
        hr = sl::interposer::call(IDXGIFactory2_CreateSwapChainForHwnd, hookCreateSwapChainForHwnd)(pFactory, pDevice, hWnd, &desc, fullscreenDesc.Windowed ? nullptr : &fullscreenDesc, pRestrictToOutput, ppSwapChain);
        if (FAILED(hr))
        {
            SL_LOG_WARN("CreateSwapChainForHwnd failed %s", std::system_category().message(hr).c_str());
            return hr;
        }
    }

    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eIDXGIFactory_CreateSwapChainForHwnd);
        for (auto [hook, feature] : hooks) ((PFunCreateSwapChainForHwndAfter*)hook)(pFactory, pDevice, hWnd, &desc, pFullscreenDesc, pRestrictToOutput, ppSwapChain);
    }
    setupSwapchainProxy(*ppSwapChain, d3dVersion, deviceProxy, desc.BufferUsage);

    return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForCoreWindow(IDXGIFactory2* pFactory, IUnknown* pDevice, IUnknown* pWindow, const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
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
        // We cannot resolve the provided device, this happens when another interposer is present
        //
        // Restore the original code at the target function so we can call the original method
        sl::interposer::getInterface()->restoreOriginalCode(hookCreateSwapChainForCoreWindow);
        auto hr = sl::interposer::call(IDXGIFactory2_CreateSwapChainForCoreWindow, hookCreateSwapChainForCoreWindow)(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);
        // Now restore current code (jmp interposer.dll::hook_for_this_function)
        sl::interposer::getInterface()->restoreCurrentCode(hookCreateSwapChainForCoreWindow);
        return hr;
    }

    HRESULT hr = S_OK;
    bool skip = false;
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGIFactory_CreateSwapChainForCoreWindow);
        for (auto [hook, feature] : hooks)
        {
            hr = ((PFunCreateSwapChainForCoreWindowBefore*)hook)(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain, skip);
            if (FAILED(hr))
            {
                SL_LOG_WARN("PFunCreateSwapChainForCoreWindowBefore failed %s", std::system_category().message(hr).c_str());
                return hr;
            }
        }
    }

    
    if (!skip)
    {
        hr = sl::interposer::call(IDXGIFactory2_CreateSwapChainForCoreWindow, hookCreateSwapChainForCoreWindow)(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);
        if (FAILED(hr))
        {
            SL_LOG_WARN("CreateSwapChainForCoreWindow failed %s", std::system_category().message(hr).c_str());
            return hr;
        }
    }
    
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eIDXGIFactory_CreateSwapChainForCoreWindow);
        for (auto [hook, feature] : hooks) ((PFunCreateSwapChainForCoreWindowAfter*)hook)(pFactory, pDevice, pWindow, &desc, pRestrictToOutput, ppSwapChain);
    }

    setupSwapchainProxy(*ppSwapChain, d3dVersion, deviceProxy, desc.BufferUsage);

    return hr;
}
HRESULT STDMETHODCALLTYPE IDXGIFactory2_CreateSwapChainForComposition(IDXGIFactory2* pFactory, IUnknown* pDevice, const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
{
    if (pDevice == nullptr || pDesc == nullptr || ppSwapChain == nullptr)
        return DXGI_ERROR_INVALID_CALL;

    *ppSwapChain = nullptr;

    DXGI_SWAP_CHAIN_DESC1 desc = *pDesc;

    Microsoft::WRL::ComPtr<IUnknown> deviceProxy;
    const UINT d3dVersion = queryDevice(pDevice, deviceProxy);
    if (!d3dVersion)
    {
        // We cannot resolve the provided device, this happens when another interposer is present
        //
        // Restore the original code at the target function so we can call the original method
        sl::interposer::getInterface()->restoreOriginalCode(hookCreateSwapChainForComposition);
        auto hr = sl::interposer::call(IDXGIFactory2_CreateSwapChainForComposition, hookCreateSwapChainForComposition)(pFactory, pDevice, &desc, pRestrictToOutput, ppSwapChain);
        // Now restore current code (jmp interposer.dll::hook_for_this_function)
        sl::interposer::getInterface()->restoreCurrentCode(hookCreateSwapChainForComposition);
        return hr;
    }

    auto hr = sl::interposer::call(IDXGIFactory2_CreateSwapChainForComposition, hookCreateSwapChainForComposition)(pFactory, pDevice, &desc, pRestrictToOutput, ppSwapChain);
    if (FAILED(hr))
    {
        return hr;
    }

    setupSwapchainProxy(*ppSwapChain, d3dVersion, deviceProxy, desc.BufferUsage);

    return hr;
}

}
}

using namespace sl::interposer;

void CreateDXGIFactoryInternal(REFIID riid, void**& ppFactory)
{
    if (sl::interposer::getInterface()->isEnabled())
    {
        //! IMPORTANT: Check if any plugin required hook into the factory or swap-chain
        //! 
        //! Note that swap-chain proxy cannot be created without a proxy factory hence the dependency.
        if (!sl::plugin_manager::getInterface()->isProxyNeeded("IDXGIFactory") &&
            !sl::plugin_manager::getInterface()->isProxyNeeded("IDXGISwapChain"))
        {
            SL_LOG_INFO("IDXGIFactory proxy not required, skipping");
            return;
        }

        auto config = sl::interposer::getInterface()->getConfig();
        if (config.useDXGIProxy)
        {
            SL_LOG_VERBOSE("Using DXGI proxy interface");
            IDXGIFactory* const factory = static_cast<IDXGIFactory*>(*ppFactory);
            auto proxy = new DXGIFactory(factory);
            if (proxy->checkAndUpgradeInterface(riid))
            {
                *ppFactory = proxy;
            }
            else
            {
                delete proxy;
                proxy = {};
            }
        }
        else
        {
            // No proxy, inject hooks directly to DXGI factory virtual table

            SL_LOG_VERBOSE("Injecting hooks into DXGI proxy virtual table");

            IDXGIFactory* const factory = static_cast<IDXGIFactory*>(*ppFactory);

            hookCreateSwapChain.replacement = IDXGIFactory_CreateSwapChain;
            sl::interposer::getInterface()->registerHookForClassInstance(factory, 10, hookCreateSwapChain);

            // Check for DXGI 1.2 support and install IDXGIFactory2 hooks if it exists
            if (Microsoft::WRL::ComPtr<IDXGIFactory2> factory2; SUCCEEDED(factory->QueryInterface(__uuidof(IDXGIFactory2), &factory2)))
            {
                hookCreateSwapChainForHwnd.replacement = IDXGIFactory2_CreateSwapChainForHwnd;
                sl::interposer::getInterface()->registerHookForClassInstance(factory2.Get(), 15, hookCreateSwapChainForHwnd);
                hookCreateSwapChainForCoreWindow.replacement = IDXGIFactory2_CreateSwapChainForCoreWindow;
                sl::interposer::getInterface()->registerHookForClassInstance(factory2.Get(), 16, hookCreateSwapChainForCoreWindow);
                hookCreateSwapChainForComposition.replacement = IDXGIFactory2_CreateSwapChainForComposition;
                sl::interposer::getInterface()->registerHookForClassInstance(factory2.Get(), 24, hookCreateSwapChainForComposition);
            }
        }
    }
    else
    {
        SL_LOG_WARN_ONCE("Streamline interposer has been disabled");
    }
}

extern "C" HRESULT WINAPI CreateDXGIFactory(REFIID riid, void** ppFactory)
{
    // Factory1 always exists
    return CreateDXGIFactory1(riid, ppFactory);
}

extern "C" HRESULT WINAPI CreateDXGIFactory1(REFIID riid, void** ppFactory)
{
    loadDXGIModule();

    const HRESULT hr = sl::interposer::call(CreateDXGIFactory1, hookCreateDXGIFactory1)(riid, ppFactory);
    if (FAILED(hr))
    {
        return hr;
    }

    CreateDXGIFactoryInternal(riid, ppFactory);

    return hr;
}
extern "C" HRESULT WINAPI CreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory)
{
    loadDXGIModule();

    const HRESULT hr = sl::interposer::call(CreateDXGIFactory2, hookCreateDXGIFactory2)(Flags, riid, ppFactory);
    if (FAILED(hr))
    {
        return hr;
    }

    CreateDXGIFactoryInternal(riid, ppFactory);

    return hr;
}

extern "C" HRESULT WINAPI DXGIGetDebugInterface1(UINT Flags, REFIID riid, void** pDebug)
{
    loadDXGIModule();

    static const auto trampoline = sl::interposer::call(DXGIGetDebugInterface1, hookGetDebugInterface1);

    if (!trampoline)
    {
        // Not supported on this OS so ignore
        return E_NOINTERFACE;
    }

    return trampoline(Flags, riid, pDebug);
}

extern "C" HRESULT WINAPI DXGIDeclareAdapterRemovalSupport()
{
    loadDXGIModule();

    static const auto trampoline = sl::interposer::call(DXGIDeclareAdapterRemovalSupport, hookDeclareAdapterRemovalSupport);

    if (!trampoline)
    {
        // Not supported on this OS so ignore
        return S_OK;
    }
    return trampoline();
}

namespace sl
{
namespace interposer
{

static void loadDXGIModule()
{
    if (hookCreateDXGIFactory.target)
    {
        // Already done, nothing to do here
        return;
    }

    ExportedFunctionList dxgiFunctions;
    getInterface()->enumerateModuleExports(L"dxgi.dll", dxgiFunctions);
    for (auto& f : dxgiFunctions)
    {
        if (f == hookCreateDXGIFactory)
        {
            hookCreateDXGIFactory.target = f.target;
            hookCreateDXGIFactory.replacement = CreateDXGIFactory;
        }
        else if (f == hookCreateDXGIFactory1)
        {
            hookCreateDXGIFactory1.target = f.target;
            hookCreateDXGIFactory1.replacement = CreateDXGIFactory1;
        }
        else if (f == hookCreateDXGIFactory2)
        {
            hookCreateDXGIFactory2.target = f.target;
            hookCreateDXGIFactory2.replacement = CreateDXGIFactory2;
        }
        else if (f == hookGetDebugInterface1)
        {
            hookGetDebugInterface1.target = f.target;
            hookGetDebugInterface1.replacement = DXGIGetDebugInterface1;
        }
        else if (f == hookDeclareAdapterRemovalSupport)
        {
            hookDeclareAdapterRemovalSupport.target = f.target;
            hookDeclareAdapterRemovalSupport.replacement = DXGIDeclareAdapterRemovalSupport;
        }
    }
}

}
}


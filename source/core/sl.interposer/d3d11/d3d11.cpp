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

#include <Windows.h>
#include <wrl/client.h>
#include <d3d11.h>

#include "source/core/sl.interposer/hook.h"
#include "source/core/sl.interposer/dxgi/dxgiFactory.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.plugin-manager/pluginManager.h"

sl::interposer::ExportedFunction d3d11CreateDeviceAndSwapChain("D3D11CreateDeviceAndSwapChain");

//! ----------------------------------------------
//! IMPORTANT: STREAMLINE DOES NOT INTERPOSE D3D11

extern "C" HRESULT WINAPI D3D11CreateDevice(IDXGIAdapter * pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL * pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device * *ppDevice, D3D_FEATURE_LEVEL * pFeatureLevel, ID3D11DeviceContext * *ppImmediateContext)
{
    return D3D11CreateDeviceAndSwapChain(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, pFeatureLevel, ppImmediateContext);
}

extern "C" HRESULT WINAPI D3D11CreateDeviceAndSwapChain(IDXGIAdapter * pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags, const D3D_FEATURE_LEVEL * pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, const DXGI_SWAP_CHAIN_DESC * pSwapChainDesc, IDXGISwapChain * *ppSwapChain, ID3D11Device * *ppDevice, D3D_FEATURE_LEVEL * pFeatureLevel, ID3D11DeviceContext * *ppImmediateContext)
{
    // Load system dll and hook the API we need
    if (!d3d11CreateDeviceAndSwapChain.target)
    {
        sl::interposer::ExportedFunctionList dxgiFunctions;
        if (!sl::interposer::getInterface()->enumerateModuleExports(L"d3d11.dll", dxgiFunctions))
        {
            SL_LOG_ERROR( "Failed to import d3d11.dll");
            return S_FALSE;
        }
        for (auto& f : dxgiFunctions)
        {
            if (f == d3d11CreateDeviceAndSwapChain)
            {
                d3d11CreateDeviceAndSwapChain.target = f.target;
                d3d11CreateDeviceAndSwapChain.replacement = D3D11CreateDeviceAndSwapChain;
            }
        }
        if (!d3d11CreateDeviceAndSwapChain.target)
        {
            SL_LOG_ERROR( "Failed to find d3d11.dll::D3D11CreateDeviceAndSwapChain");
            return S_FALSE;
        }
    }

    // We avoid creating swapchain here because we need a device before any of the SL plugins can be initialized
    HRESULT hr = sl::interposer::call(D3D11CreateDeviceAndSwapChain, d3d11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, nullptr, nullptr, ppDevice, pFeatureLevel, nullptr);
    if (FAILED(hr))
    {
        SL_LOG_WARN("D3D11CreateDeviceAndSwapChain failed with error code %lx", hr);
        return hr;
    }

    // It is OK for device to be nullptr as long as the above call did not fail
    if (!ppDevice || !(*ppDevice))
    {
        return hr;
    }

    auto device = *ppDevice;

    SL_LOG_WARN("Automatically assigning d3d11 device, if this is not desired please use `D3D11CreateDevice` followed by `slSetD3DDevice`");

    //! IMPORTANT: Set device as soon as it is available since code below can trigger swap-chain related hooks
    //! which then indirectly want to initialize plugins and we need device for that. This is required in order
    //! to support plugins that hook the swap-chain.
    sl::plugin_manager::getInterface()->setD3D11Device(device);

    // Now it is safe to create a swap chain
    if (pSwapChainDesc != nullptr)
    {
        assert(ppSwapChain != nullptr);

        Microsoft::WRL::ComPtr<IDXGIDevice1> dxgiDevice = {};
        hr = device->QueryInterface(__uuidof(IDXGIDevice1), &dxgiDevice);
        assert(SUCCEEDED(hr));

        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter(pAdapter);
        if (!adapter)
        {
            hr = dxgiDevice->GetAdapter(&adapter);
            assert(SUCCEEDED(hr));
        }

        // This will always return native interface and not our proxy
        Microsoft::WRL::ComPtr<IDXGIFactory> factory;
        hr = adapter->GetParent(IID_PPV_ARGS(&factory));
        assert(SUCCEEDED(hr));

        if (sl::interposer::getInterface()->getConfig().useDXGIProxy && sl::interposer::getInterface()->getConfig().enableInterposer)
        {
            // Create temporary proxy so we can create correct swap-chain
            auto proxyFactory = new sl::interposer::DXGIFactory(factory.Get());
            hr = proxyFactory->CreateSwapChain(device, const_cast<DXGI_SWAP_CHAIN_DESC*>(pSwapChainDesc), ppSwapChain);
            proxyFactory->Release();
        }
        else
        {
            hr = factory->CreateSwapChain(device, const_cast<DXGI_SWAP_CHAIN_DESC*>(pSwapChainDesc), ppSwapChain);
        }
        assert(SUCCEEDED(hr));
    }

    if (SUCCEEDED(hr))
    {
        if (ppImmediateContext)
        {
            device->GetImmediateContext(ppImmediateContext);
        }
    }
    else
    {
        sl::plugin_manager::getInterface()->setD3D11Device(nullptr);
        *ppDevice = nullptr;
        device->Release();
    }

    return hr;
}

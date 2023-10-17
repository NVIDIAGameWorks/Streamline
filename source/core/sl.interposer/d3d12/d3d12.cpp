
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
#include <dxgi1_6.h>

#include "source/core/sl.interposer/hook.h"
#include "source/core/sl.interposer/d3d12/d3d12Device.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.plugin-manager/pluginManager.h"

void loadD3D12Module();

static sl::interposer::ExportedFunction hookD3D12GetDebugInterface("D3D12GetDebugInterface");
static sl::interposer::ExportedFunction hookD3D12CreateDevice("D3D12CreateDevice");
static sl::interposer::ExportedFunction hookD3D12CreateRootSignatureDeserializer("D3D12CreateRootSignatureDeserializer");
static sl::interposer::ExportedFunction hookD3D12CreateVersionedRootSignatureDeserializer("D3D12CreateVersionedRootSignatureDeserializer");
static sl::interposer::ExportedFunction hookD3D12EnableExperimentalFeatures("D3D12EnableExperimentalFeatures");
static sl::interposer::ExportedFunction hookD3D12SerializeRootSignature("D3D12SerializeRootSignature");
static sl::interposer::ExportedFunction hookD3D12SerializeVersionedRootSignature("D3D12SerializeVersionedRootSignature");
static sl::interposer::ExportedFunction hookD3D12GetInterface("D3D12GetInterface");

extern "C" HRESULT WINAPI D3D12CreateDevice(
    IUnknown * pAdapter,
    D3D_FEATURE_LEVEL MinimumFeatureLevel,
    REFIID riid,
    void** ppDevice)
{
    loadD3D12Module();

#ifndef SL_PRODUCTION
    bool bEnableDebugLayer = sl::interposer::getInterface()->getConfig().enableD3D12DebugLayer;
    if (bEnableDebugLayer)
    {
        // enable debug layer
        ID3D12Debug* debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            SL_LOG_INFO("Enabling D3D12 debug layer...");
            debugController->EnableDebugLayer();
        }
        else
        {
            SL_LOG_WARN("Tried to enable D3D12 debug layer, but failed to get debug interface");
        }
    }
#endif

    const HRESULT hr = sl::interposer::call(D3D12CreateDevice, hookD3D12CreateDevice)(pAdapter, MinimumFeatureLevel, riid, ppDevice);
    if (FAILED(hr))
    {
        if (ppDevice)
        {
            // User actually requested a device so report an error
            SL_LOG_WARN("D3D12CreateDevice failed with error code %lx", hr);
        }
        return hr;
    }

    if (ppDevice && *ppDevice)
    {
        //! Note that proxies for command list or command queue cannot be created without a proxy device
        bool proxyRequested = sl::plugin_manager::getInterface()->isProxyNeeded("ID3D12Device") ||
                              sl::plugin_manager::getInterface()->isProxyNeeded("ID3D12CommandQueue") ||
                              sl::plugin_manager::getInterface()->isProxyNeeded("ID3D12GraphicsCommandList");
        if (sl::interposer::getInterface()->isEnabled() && proxyRequested)
        {
            auto deviceProxy = new sl::interposer::D3D12Device(static_cast<ID3D12Device*>(*ppDevice));

            // Upgrade to the actual interface version requested here
            if (deviceProxy->checkAndUpgradeInterface(riid))
            {
                *ppDevice = deviceProxy;
            }
            else // Do not hook object if we do not support the requested interface
            {
                delete deviceProxy; // Delete instead of release to keep reference count untouched
                deviceProxy = {};
            }
            // Legacy way of automatic device selection, in SL 2.0+ host must do it explicitly
            if (deviceProxy && sl::plugin_manager::getInterface()->getHostSDKVersion() < sl::Version(2, 0, 0))
            {
                sl::plugin_manager::getInterface()->setD3D12Device(deviceProxy->m_base);
            }
        }
        else if (sl::plugin_manager::getInterface()->getHostSDKVersion() < sl::Version(2, 0, 0))
        {
            SL_LOG_INFO("ID3D12Device proxy not required, skipping");
            // Legacy way of automatic device selection, in SL 2.0+ host must do it explicitly
            sl::plugin_manager::getInterface()->setD3D12Device((ID3D12Device*)*ppDevice);
        }
    }
    
    return hr;
}

extern "C" HRESULT WINAPI D3D12GetDebugInterface(
    REFIID riid,
    void** ppvDebug)
{
    loadD3D12Module();
    return sl::interposer::call(D3D12GetDebugInterface, hookD3D12GetDebugInterface)(riid, ppvDebug);
}

extern "C" HRESULT WINAPI D3D12CreateRootSignatureDeserializer(
    LPCVOID pSrcData,
    SIZE_T SrcDataSizeInBytes,
    REFIID pRootSignatureDeserializerInterface,
    void** ppRootSignatureDeserializer)
{
    loadD3D12Module();
    return sl::interposer::call(D3D12CreateRootSignatureDeserializer, hookD3D12CreateRootSignatureDeserializer)(pSrcData, SrcDataSizeInBytes, pRootSignatureDeserializerInterface, ppRootSignatureDeserializer);
}

extern "C" HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(
    LPCVOID pSrcData,
    SIZE_T SrcDataSizeInBytes,
    REFIID pRootSignatureDeserializerInterface,
    void** ppRootSignatureDeserializer)
{
    loadD3D12Module();
    return sl::interposer::call(D3D12CreateVersionedRootSignatureDeserializer, hookD3D12CreateVersionedRootSignatureDeserializer)(pSrcData, SrcDataSizeInBytes, pRootSignatureDeserializerInterface, ppRootSignatureDeserializer);
}

extern "C" HRESULT WINAPI D3D12EnableExperimentalFeatures(
    UINT NumFeatures,
    const IID * pIIDs,
    void* pConfigurationStructs,
    UINT * pConfigurationStructSizes)
{
    loadD3D12Module();
    return sl::interposer::call(D3D12EnableExperimentalFeatures, hookD3D12EnableExperimentalFeatures)(NumFeatures, pIIDs, pConfigurationStructs, pConfigurationStructSizes);
}

extern "C" HRESULT WINAPI D3D12SerializeRootSignature(
    const D3D12_ROOT_SIGNATURE_DESC * pRootSignature,
    D3D_ROOT_SIGNATURE_VERSION Version,
    ID3DBlob * *ppBlob,
    ID3DBlob * *ppErrorBlob)
{
    loadD3D12Module();
    return sl::interposer::call(D3D12SerializeRootSignature, hookD3D12SerializeRootSignature)(pRootSignature, Version, ppBlob, ppErrorBlob);
}

extern "C" HRESULT WINAPI D3D12SerializeVersionedRootSignature(
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC * pRootSignature,
    ID3DBlob * *ppBlob,
    ID3DBlob * *ppErrorBlob)
{
    loadD3D12Module();
    return sl::interposer::call(D3D12SerializeVersionedRootSignature, hookD3D12SerializeVersionedRootSignature)(pRootSignature, ppBlob, ppErrorBlob);
}

extern "C" HRESULT D3D12GetInterface(REFCLSID rclsid, REFIID   riid, void** ppvDebug)
{
    loadD3D12Module();
    return sl::interposer::call(D3D12GetInterface, hookD3D12GetInterface)(rclsid, riid, ppvDebug);
}

static void loadD3D12Module()
{
    if (hookD3D12CreateDevice.target)
    {
        // Already done, nothing to do here
        return;
    }

    sl::interposer::ExportedFunctionList dxgiFunctions;
    sl::interposer::getInterface()->enumerateModuleExports(L"d3d12.dll", dxgiFunctions);
    for (auto& f : dxgiFunctions)
    {
        if (f == hookD3D12CreateDevice)
        {
            hookD3D12CreateDevice.target = f.target;
            hookD3D12CreateDevice.replacement = D3D12CreateDevice;
        }
        else if (f == hookD3D12GetDebugInterface)
        {
            hookD3D12GetDebugInterface.target = f.target;
            hookD3D12GetDebugInterface.replacement = D3D12GetDebugInterface;
        }
        else if (f == hookD3D12CreateRootSignatureDeserializer)
        {
            hookD3D12CreateRootSignatureDeserializer.target = f.target;
            hookD3D12CreateRootSignatureDeserializer.replacement = D3D12CreateRootSignatureDeserializer;
        }
        else if (f == hookD3D12CreateVersionedRootSignatureDeserializer)
        {
            hookD3D12CreateVersionedRootSignatureDeserializer.target = f.target;
            hookD3D12CreateVersionedRootSignatureDeserializer.replacement = D3D12CreateVersionedRootSignatureDeserializer;
        }
        else if (f == hookD3D12EnableExperimentalFeatures)
        {
            hookD3D12EnableExperimentalFeatures.target = f.target;
            hookD3D12EnableExperimentalFeatures.replacement = D3D12EnableExperimentalFeatures;
        }
        else if (f == hookD3D12SerializeRootSignature)
        {
            hookD3D12SerializeRootSignature.target = f.target;
            hookD3D12SerializeRootSignature.replacement = D3D12SerializeRootSignature;
        }
        else if (f == hookD3D12SerializeVersionedRootSignature)
        {
            hookD3D12SerializeVersionedRootSignature.target = f.target;
            hookD3D12SerializeVersionedRootSignature.replacement = D3D12SerializeVersionedRootSignature;
        }
        else if (f == hookD3D12GetInterface)
        {
            hookD3D12GetInterface.target = f.target;
            hookD3D12GetInterface.replacement = D3D12GetInterface;
        }
    }
}

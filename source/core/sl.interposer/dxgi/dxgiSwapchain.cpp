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

#include "source/core/sl.interposer/dxgi/dxgiSwapchain.h"
#include "source/core/sl.interposer/d3d12/d3d12Device.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandQueue.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin-manager/pluginManager.h"

namespace sl
{
namespace interposer
{
extern UINT queryDevice(IUnknown*& device, Microsoft::WRL::ComPtr<IUnknown>& device_proxy);

DXGISwapChain::DXGISwapChain(ID3D11Device* device, IDXGISwapChain* original) :
    m_base(original),
    m_interfaceVersion(0),
    m_d3dDevice(device),
    m_d3dVersion(11)
{
    assert(m_base != nullptr && m_d3dDevice != nullptr);
    m_d3dDevice->AddRef();
}

DXGISwapChain::DXGISwapChain(ID3D11Device* device, IDXGISwapChain1* original) :
    m_base(original),
    m_interfaceVersion(1),
    m_d3dDevice(device),
    m_d3dVersion(11)
{
    assert(m_base != nullptr && m_d3dDevice != nullptr);
    m_d3dDevice->AddRef();
}

DXGISwapChain::DXGISwapChain(D3D12Device* device, IDXGISwapChain3* original) :
    m_base(original),
    m_interfaceVersion(3),
    m_d3dDevice(device),
    m_d3dVersion(12)
{
    assert(m_base != nullptr && m_d3dDevice != nullptr);
    m_d3dDevice->AddRef();
}

bool DXGISwapChain::checkAndUpgradeInterface(REFIID riid)
{
    if (riid == __uuidof(this) || riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) || riid == __uuidof(IDXGIDeviceSubObject))
    {
        return true;
    }

    static const IID iidLookup[] = {
        __uuidof(IDXGISwapChain),
        __uuidof(IDXGISwapChain1),
        __uuidof(IDXGISwapChain2),
        __uuidof(IDXGISwapChain3),
        __uuidof(IDXGISwapChain4),
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
            SL_LOG_VERBOSE("Upgraded IDXGISwapChain v%u to v%u", m_interfaceVersion, version);
            m_base->Release();
            m_base = static_cast<IDXGISwapChain*>(newInterface);
            m_interfaceVersion = version;
        }
        return true;
    }

    return false;
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::QueryInterface(REFIID riid, void** ppvObj)
{
    if (ppvObj == nullptr)
        return E_POINTER;

    // SL Special case, we are requesting base interface
    if (riid == __uuidof(StreamlineRetreiveBaseInterface))
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

ULONG   STDMETHODCALLTYPE DXGISwapChain::AddRef()
{
    m_base->AddRef();
    return InterlockedIncrement(&m_refCount);
}

ULONG   STDMETHODCALLTYPE DXGISwapChain::Release()
{
    const ULONG ref = InterlockedDecrement(&m_refCount);
    if (ref != 0)
    {
        return m_base->Release(), ref;
    }

    // Release the explicit reference to device that was added in the DXGISwapChain constructor above
    m_d3dDevice->Release();

    // Inform our plugins that swap-chain has been destroyed
    using Destroyed_t = void(IDXGISwapChain*);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGISwapChain_Destroyed);
    for (auto hook : hooks) ((Destroyed_t*)hook)(m_base);

    // Only release internal reference after the runtime has been destroyed, so any references it held are cleaned up at this point
    const ULONG ref_orig = m_base->Release();
    if (ref_orig != 0)
    {
        SL_LOG_WARN("Reference count for IDXGISwapChain v%u %llx is inconsistent.", m_interfaceVersion, this);
    }

    delete this;

    return 0;
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::SetPrivateData(REFGUID Name, UINT DataSize, const void* pData)
{
    return m_base->SetPrivateData(Name, DataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetPrivateDataInterface(REFGUID Name, const IUnknown* pUnknown)
{
    return m_base->SetPrivateDataInterface(Name, pUnknown);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetPrivateData(REFGUID Name, UINT* pDataSize, void* pData)
{
    return m_base->GetPrivateData(Name, pDataSize, pData);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetParent(REFIID riid, void** ppParent)
{
    return m_base->GetParent(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDevice(REFIID riid, void** ppDevice)
{
    return m_d3dDevice->QueryInterface(riid, ppDevice);
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::Present(UINT SyncInterval, UINT Flags)
{
    using Present_t = HRESULT(IDXGISwapChain*, UINT SyncInterval, UINT Flags, bool& skip);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGISwapChain_Present);
    bool skip = false;
    for (auto hook : hooks) ((Present_t*)hook)(m_base, SyncInterval, Flags, skip);

    HRESULT hr = S_OK;
    if (!skip) hr = m_base->Present(SyncInterval, Flags);
    return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBuffer(UINT Buffer, REFIID riid, void** ppSurface)
{
    using GetBuffer_t = HRESULT(IDXGISwapChain*, UINT Buffer, REFIID riid, void** ppSurface, bool& skip);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGISwapChain_GetBuffer);
    bool skip = false;
    for (auto hook : hooks) ((GetBuffer_t*)hook)(m_base, Buffer, riid, ppSurface, skip);

    HRESULT hr = S_OK;
    if (!skip) hr = m_base->GetBuffer(Buffer, riid, ppSurface);
    return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetFullscreenState(BOOL Fullscreen, IDXGIOutput* pTarget)
{
    SL_LOG_VERBOSE("Redirecting IDXGISwapChain::SetFullscreenState Fullscreen = %u", Fullscreen);

    using GetFullscreenState_t = HRESULT(IDXGISwapChain*, BOOL pFullscreen, IDXGIOutput* ppTarget);
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGISwapChain_SetFullscreenState);
        for (auto hook : hooks)
        {
            ((GetFullscreenState_t*)hook)(m_base, Fullscreen, pTarget);
        }
    }

    auto hr = m_base->SetFullscreenState(Fullscreen, pTarget);
    if (FAILED(hr))
    {
        return hr;
    }

    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eIDXGISwapChain_SetFullscreenState);
        for (auto hook : hooks)
        {
            ((GetFullscreenState_t*)hook)(m_base, Fullscreen, pTarget);
        }
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenState(BOOL* pFullscreen, IDXGIOutput** ppTarget)
{
    return m_base->GetFullscreenState(pFullscreen, ppTarget);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc(DXGI_SWAP_CHAIN_DESC* pDesc)
{
    return m_base->GetDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    using ResizeBuffers_t = HRESULT(IDXGISwapChain*, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);
    {
        const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGISwapChain_ResizeBuffers);
        for (auto hook : hooks)
        {
            ((ResizeBuffers_t*)hook)(m_base, BufferCount, Width, Height, NewFormat, SwapChainFlags);
        }
    }

    HRESULT hr = m_base->ResizeBuffers(BufferCount, Width, Height, NewFormat, SwapChainFlags);

    if (FAILED(hr))
    {
        return hr;
    }

    {
        const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooks(FunctionHookID::eIDXGISwapChain_ResizeBuffers);
        for (auto hook : hooks)
        {
            ((ResizeBuffers_t*)hook)(m_base, BufferCount, Width, Height, NewFormat, SwapChainFlags);
        }
    }

    return hr;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeTarget(const DXGI_MODE_DESC* pNewTargetParameters)
{
    return m_base->ResizeTarget(pNewTargetParameters);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetContainingOutput(IDXGIOutput** ppOutput)
{
    return m_base->GetContainingOutput(ppOutput);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFrameStatistics(DXGI_FRAME_STATISTICS* pStats)
{
    return m_base->GetFrameStatistics(pStats);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetLastPresentCount(UINT* pLastPresentCount)
{
    return m_base->GetLastPresentCount(pLastPresentCount);
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::GetDesc1(DXGI_SWAP_CHAIN_DESC1* pDesc)
{
    return static_cast<IDXGISwapChain1*>(m_base)->GetDesc1(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetFullscreenDesc(DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pDesc)
{
    return static_cast<IDXGISwapChain1*>(m_base)->GetFullscreenDesc(pDesc);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetHwnd(HWND* pHwnd)
{
    return static_cast<IDXGISwapChain1*>(m_base)->GetHwnd(pHwnd);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetCoreWindow(REFIID refiid, void** ppUnk)
{
    return static_cast<IDXGISwapChain1*>(m_base)->GetCoreWindow(refiid, ppUnk);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::Present1(UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters)
{
    using Present1_t = HRESULT(IDXGISwapChain*, UINT SyncInterval, UINT PresentFlags, const DXGI_PRESENT_PARAMETERS* pPresentParameters, bool& skip);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGISwapChain_Present1);
    bool skip = false;
    for (auto hook : hooks) ((Present1_t*)hook)(m_base, SyncInterval, PresentFlags, pPresentParameters, skip);

    HRESULT hr = S_OK;
    if (!skip) hr = static_cast<IDXGISwapChain1*>(m_base)->Present1(SyncInterval, PresentFlags, pPresentParameters);
    return hr;
}
BOOL    STDMETHODCALLTYPE DXGISwapChain::IsTemporaryMonoSupported()
{
    return static_cast<IDXGISwapChain1*>(m_base)->IsTemporaryMonoSupported();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetRestrictToOutput(IDXGIOutput** ppRestrictToOutput)
{
    return static_cast<IDXGISwapChain1*>(m_base)->GetRestrictToOutput(ppRestrictToOutput);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetBackgroundColor(const DXGI_RGBA* pColor)
{
    return static_cast<IDXGISwapChain1*>(m_base)->SetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetBackgroundColor(DXGI_RGBA* pColor)
{
    return static_cast<IDXGISwapChain1*>(m_base)->GetBackgroundColor(pColor);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetRotation(DXGI_MODE_ROTATION Rotation)
{
    return static_cast<IDXGISwapChain1*>(m_base)->SetRotation(Rotation);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetRotation(DXGI_MODE_ROTATION* pRotation)
{
    return static_cast<IDXGISwapChain1*>(m_base)->GetRotation(pRotation);
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::SetSourceSize(UINT Width, UINT Height)
{
    return static_cast<IDXGISwapChain2*>(m_base)->SetSourceSize(Width, Height);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetSourceSize(UINT* pWidth, UINT* pHeight)
{
    return static_cast<IDXGISwapChain2*>(m_base)->GetSourceSize(pWidth, pHeight);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetMaximumFrameLatency(UINT MaxLatency)
{
    return static_cast<IDXGISwapChain2*>(m_base)->SetMaximumFrameLatency(MaxLatency);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetMaximumFrameLatency(UINT* pMaxLatency)
{
    return static_cast<IDXGISwapChain2*>(m_base)->GetMaximumFrameLatency(pMaxLatency);
}
HANDLE  STDMETHODCALLTYPE DXGISwapChain::GetFrameLatencyWaitableObject()
{
    return static_cast<IDXGISwapChain2*>(m_base)->GetFrameLatencyWaitableObject();
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetMatrixTransform(const DXGI_MATRIX_3X2_F* pMatrix)
{
    return static_cast<IDXGISwapChain2*>(m_base)->SetMatrixTransform(pMatrix);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::GetMatrixTransform(DXGI_MATRIX_3X2_F* pMatrix)
{
    return static_cast<IDXGISwapChain2*>(m_base)->GetMatrixTransform(pMatrix);
}

UINT STDMETHODCALLTYPE DXGISwapChain::GetCurrentBackBufferIndex()
{

    using GetCurrentBackBufferIndex_t = UINT(IDXGISwapChain*, bool& skip);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eIDXGISwapChain_GetCurrentBackBufferIndex);
    bool skip = false;
    UINT res = 0;
    for (auto hook : hooks) res = ((GetCurrentBackBufferIndex_t*)hook)(m_base, skip);
    if (!skip) res = static_cast<IDXGISwapChain3*>(m_base)->GetCurrentBackBufferIndex();
    return res;
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::CheckColorSpaceSupport(DXGI_COLOR_SPACE_TYPE ColorSpace, UINT* pColorSpaceSupport)
{
    return static_cast<IDXGISwapChain3*>(m_base)->CheckColorSpaceSupport(ColorSpace, pColorSpaceSupport);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::SetColorSpace1(DXGI_COLOR_SPACE_TYPE ColorSpace)
{
    return static_cast<IDXGISwapChain3*>(m_base)->SetColorSpace1(ColorSpace);
}
HRESULT STDMETHODCALLTYPE DXGISwapChain::ResizeBuffers1(UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT Format, UINT SwapChainFlags, const UINT* pCreationNodeMask, IUnknown* const* ppPresentQueue)
{
    // Need to extract the original command queue object from the proxies passed in
    assert(ppPresentQueue != nullptr);
    std::vector<IUnknown*> present_queues(BufferCount);
    for (UINT i = 0; i < BufferCount; ++i)
    {
        present_queues[i] = ppPresentQueue[i];
        Microsoft::WRL::ComPtr<IUnknown> command_queue_proxy;
        queryDevice(present_queues[i], command_queue_proxy);
    }

    const HRESULT hr = static_cast<IDXGISwapChain3*>(m_base)->ResizeBuffers1(BufferCount, Width, Height, Format, SwapChainFlags, pCreationNodeMask, present_queues.data());
    if (hr == DXGI_ERROR_INVALID_CALL)
    {
        SL_LOG_WARN("IDXGISwapChain3::ResizeBuffers1 failed with error code %s", std::system_category().message(hr).c_str());
    }
    else if (FAILED(hr))
    {
        SL_LOG_ERROR("IDXGISwapChain3::ResizeBuffers1 failed with error code %s", std::system_category().message(hr).c_str());
        return hr;
    }

    return hr;
}

HRESULT STDMETHODCALLTYPE DXGISwapChain::SetHDRMetaData(DXGI_HDR_METADATA_TYPE Type, UINT Size, void* pMetaData)
{
    return static_cast<IDXGISwapChain4*>(m_base)->SetHDRMetaData(Type, Size, pMetaData);
}
}
}
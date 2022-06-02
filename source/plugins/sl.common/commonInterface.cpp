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

#include <map>

#include "include/sl.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.thread/thread.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "source/platforms/sl.chi/d3d11.h"
#include "source/platforms/sl.chi/d3d12.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "_artifacts/gitVersion.h"
#include "external/nvapi/nvapi.h"

namespace sl
{

using namespace common;

struct EvaluateCallbacks
{
    PFunBeginEvent* beginEvaluate; 
    chi::PFunCommonCmdList* endEvaluate;
};

struct CommonInterfaceContext
{
    chi::PlatformType platform;
    sl::chi::ICompute* compute;
    uint32_t frameIndex = 0;

    thread::ThreadContext<chi::D3D11ThreadContext>* threadsD3D11 = {};
    thread::ThreadContext<chi::D3D12ThreadContext>* threadsD3D12 = {};

    std::map<Feature, EvaluateCallbacks> evalCallbacks;

    NvPhysicalGpuHandle nvGPUHandle[NVAPI_MAX_PHYSICAL_GPUS] = { };
    NvU32 nvGPUCount = 0;

    common::GPUArch gpuInfo{};

    chi::CommonThreadContext& getThreadContext()
    {
        if (platform == chi::ePlatformTypeD3D11)
        {
            if (!threadsD3D11)
            {
                threadsD3D11 = new thread::ThreadContext<chi::D3D11ThreadContext>();
            }
            return threadsD3D11->getContext();
        }
        else
        {
            if (!threadsD3D12)
            {
                threadsD3D12 = new thread::ThreadContext<chi::D3D12ThreadContext>();
            }
            return threadsD3D12->getContext();
        }
    }
};

static CommonInterfaceContext ctx = {};

//! Get GPU information and share with other plugins
//! 
//! We need to add support for non-NVIDIA GPUs
bool getGPUInfo(common::GPUArch*& info, LUID* id)
{
#if defined(SL_WINDOWS)    
    NVAPI_VALIDATE_RF(NvAPI_EnumPhysicalGPUs(ctx.nvGPUHandle, &ctx.nvGPUCount));
    // Limiting to two GPUs (iGPU + dGPU) or 2x dGPU
    ctx.nvGPUCount = std::min(2UL, ctx.nvGPUCount);
    ctx.gpuInfo.gpuCount = (uint32_t)ctx.nvGPUCount;
    NvU32 driverVersion;
    NvAPI_ShortString driverName;
    NVAPI_VALIDATE_RF(NvAPI_SYS_GetDriverAndBranchVersion(&driverVersion, driverName));
    SL_LOG_INFO("-----------------------------------------");
    ctx.gpuInfo.driverVersionMajor = driverVersion / 100;
    ctx.gpuInfo.driverVersionMinor = driverVersion % 100;
    SL_LOG_INFO("NVIDIA driver %u.%u", ctx.gpuInfo.driverVersionMajor, ctx.gpuInfo.driverVersionMinor);
    for (NvU32 gpu = 0; gpu < ctx.nvGPUCount; ++gpu)
    {
        if (id)
        {
            LUID luidTmp;
            NVAPI_VALIDATE_RF(NvAPI_GPU_GetAdapterIdFromPhysicalGpu(ctx.nvGPUHandle[gpu], &luidTmp));
            if (memcmp(&luidTmp, id, sizeof(LUID)) != 0)
            {
                continue;
            }
        }
        NV_GPU_ARCH_INFO archInfo;
        archInfo.version = NV_GPU_ARCH_INFO_VER;
        NVAPI_VALIDATE_RF(NvAPI_GPU_GetArchInfo(ctx.nvGPUHandle[gpu], &archInfo));
        ctx.gpuInfo.architecture[gpu] = archInfo.architecture;
        ctx.gpuInfo.implementation[gpu] = archInfo.implementation;
        ctx.gpuInfo.revision[gpu] = archInfo.revision;
        SL_LOG_INFO("GPU %u architecture 0x%x adapter mask 0x%0x", gpu, ctx.gpuInfo.architecture[gpu], 1 << gpu);
    };
    SL_LOG_INFO("-----------------------------------------");
    info = &ctx.gpuInfo;
#endif
    return true;
}

//! Current thread context
chi::CommonThreadContext* getThreadContext()
{
    return &ctx.getThreadContext();
}

namespace common
{

//! Create compute API which is then shared with all active plugins
bool createCompute(void* device, uint32_t deviceType)
{
    pfunResourceAllocateCallback* allocate = {};
    pfunResourceReleaseCallback* release = {};
    param::getPointerParam(api::getContext()->parameters, param::global::kPFunAllocateResource, &allocate);
    param::getPointerParam(api::getContext()->parameters, param::global::kPFunReleaseResource, &release);

    ctx.platform = (chi::PlatformType)deviceType;

    if (deviceType == chi::ePlatformTypeD3D11)
    {
        ctx.compute = sl::chi::getD3D11();
    }
    else if (deviceType == chi::ePlatformTypeD3D12)
    {
        ctx.compute = sl::chi::getD3D12();
    }

    CHI_CHECK_RF(ctx.compute->init(device, api::getContext()->parameters));
    CHI_CHECK_RF(ctx.compute->setCallbacks(allocate, release, getThreadContext));

    api::getContext()->parameters->set(sl::param::common::kComputeAPI, ctx.compute);

    return true;
}

//! Destroy compute API when sl.common is released
bool destroyCompute()
{
    CHI_CHECK_RF(ctx.compute->shutdown());
    if (ctx.threadsD3D11)
    {
        delete ctx.threadsD3D11;
    }
    if (ctx.threadsD3D12)
    {
        delete ctx.threadsD3D12;
    }
    ctx.threadsD3D11 = {};
    ctx.threadsD3D12 = {};
    return true;
}

//! Common register callbacks from other plugins
//! 
//! Used to dispatch evaluate calls to the correct plugin.
//! 
void registerEvaluateCallbacks(Feature feature, PFunBeginEvent* beginEvaluate, chi::PFunCommonCmdList* endEvaluate)
{
    ctx.evalCallbacks[feature] = { beginEvaluate, endEvaluate };
}

//! Common evaluate feature
//! 
//! Here we intercept evaluate calls from host and figure out the 
//! callbacks for the requested feature (sl plugin)
bool evaluateFeature(CommandBuffer* cmdBuffer, Feature feature, uint32_t frameIndex, uint32_t id)
{
    auto evalCallbacks = ctx.evalCallbacks[feature];
    if (!evalCallbacks.beginEvaluate || !evalCallbacks.endEvaluate)
    {
        SL_LOG_ERROR_ONCE("Could not find 'evaluateFeature' callbacks for feature %u", feature);
        return false;
    }

    // First we need to get to the correct base interface we need to use
    CommandBuffer* cmdList = nullptr;
    if (cmdBuffer)
    {
        if (ctx.platform == chi::ePlatformTypeD3D11)
        {
            // No interposing for d3d11 
            cmdList = cmdBuffer;
        }
        else if (ctx.platform == chi::ePlatformTypeD3D12)
        {
            // Streamline D3D12GraphicsCommandList -> ID3D12GraphicsCommandList
            auto& thread = (chi::D3D12ThreadContext&)ctx.getThreadContext();
            thread.cmdList = (interposer::D3D12GraphicsCommandList*)cmdBuffer;
            cmdList = thread.cmdList->m_base;
        }

    }

    // This allows us to map correct constants and tags to this evaluate call
    common::EventData event = { id, frameIndex };

    // Push the state (d3d11 only, nop otherwise)
    CHI_CHECK_RF(ctx.compute->pushState(cmdList));

    evalCallbacks.beginEvaluate(cmdList, event);
    evalCallbacks.endEvaluate(cmdList);
    
    // Pop the state (d3d11 only, nop otherwise)
    CHI_CHECK_RF(ctx.compute->popState(cmdList));
    // Restore the pipeline so host can continue running like we never existed
    CHI_CHECK_RF(ctx.compute->restorePipeline(cmdList));

    return true;
}
}

//! Hooks

//! D3D12

HRESULT slHookCreateCommittedResource(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource, void** ppvResource)
{
    if (!ppvResource || *ppvResource == nullptr) return S_OK;

    chi::ResourceInfo info = {};
    info.desc.width = (uint32_t)pDesc->Width;
    info.desc.height = (uint32_t)pDesc->Height;
    info.desc.nativeFormat = (uint32_t)pDesc->Format;
    info.desc.mips = (uint32_t)pDesc->MipLevels;
    info.desc.flags = pDesc->Dimension == D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER ? chi::ResourceFlags::eRawOrStructuredBuffer : chi::ResourceFlags::eShaderResource;
    CHI_VALIDATE(ctx.compute->getResourceState(InitialState, info.desc.state));
    CHI_VALIDATE(ctx.compute->getFormat(info.desc.nativeFormat, info.desc.format));
    CHI_VALIDATE(ctx.compute->onHostResourceCreated(*ppvResource, info));
    return S_OK;
}

HRESULT slHookCreateReservedResource(const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource)
{
    if (!ppvResource || *ppvResource == nullptr) return S_OK;

    chi::ResourceInfo info = {};
    info.desc.width = (uint32_t)pDesc->Width;
    info.desc.height = (uint32_t)pDesc->Height;
    info.desc.nativeFormat = (uint32_t)pDesc->Format;
    info.desc.mips = (uint32_t)pDesc->MipLevels;
    info.desc.flags = pDesc->Dimension == D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER ? chi::ResourceFlags::eRawOrStructuredBuffer : chi::ResourceFlags::eShaderResource;
    CHI_VALIDATE(ctx.compute->getResourceState(InitialState, info.desc.state));
    CHI_VALIDATE(ctx.compute->getFormat(info.desc.nativeFormat, info.desc.format));
    CHI_VALIDATE(ctx.compute->onHostResourceCreated(*ppvResource, info));
    return S_OK;
}

HRESULT slHookCreatePlacedResource(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource)
{
    if (!ppvResource || *ppvResource == nullptr) return S_OK;

    chi::ResourceInfo info = {};
    info.desc.width = (uint32_t)pDesc->Width;
    info.desc.height = (uint32_t)pDesc->Height;
    info.desc.nativeFormat = (uint32_t)pDesc->Format;
    info.desc.mips = (uint32_t)pDesc->MipLevels;
    info.desc.flags = pDesc->Dimension == D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER ? chi::ResourceFlags::eRawOrStructuredBuffer : chi::ResourceFlags::eShaderResource;
    CHI_VALIDATE(ctx.compute->getResourceState(InitialState, info.desc.state));
    CHI_VALIDATE(ctx.compute->getFormat(info.desc.nativeFormat, info.desc.format));
    CHI_VALIDATE(ctx.compute->onHostResourceCreated(*ppvResource, info));
    return S_OK;
}

// Command list

void slHookResourceBarrier(ID3D12GraphicsCommandList* pCmdList, UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers)
{
    for (UINT i = 0; i < NumBarriers; i++)
    {
        if (pBarriers[i].Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
        {
            chi::ResourceState state;
            CHI_VALIDATE(ctx.compute->getResourceState(pBarriers[i].Transition.StateAfter, state));
            CHI_VALIDATE(ctx.compute->setResourceState(pBarriers[i].Transition.pResource, state));// , pBarriers[i].Transition.Subresource);
        }
    }
}

void presentCommon(UINT Flags)
{
    if ((Flags & DXGI_PRESENT_TEST))
    {
        return;
    }
    if (ctx.compute)
    {
        ctx.frameIndex++;
        // This will release any resources scheduled to be destroyed few frames behind
        CHI_VALIDATE(ctx.compute->collectGarbage(ctx.frameIndex));
    }

    // Our stats including GPU load info
    static std::string s_stats;
    auto v = api::getContext()->pluginVersion;
    
    for (uint32_t i = 0; i < ctx.nvGPUCount; i++)
    {
        NV_GPU_DYNAMIC_PSTATES_INFO_EX gpuLoads{};
        gpuLoads.version = NV_GPU_DYNAMIC_PSTATES_INFO_EX_VER;
        NvAPI_GPU_GetDynamicPstatesInfoEx(ctx.nvGPUHandle[i], &gpuLoads);
        ctx.gpuInfo.gpuLoad[i] = gpuLoads.utilization[i].percentage;
    }

    s_stats = extra::format("sl.common {} - {}", v.toStr(), GIT_LAST_COMMIT);
    api::getContext()->parameters->set(sl::param::common::kStats, (void*)s_stats.c_str());
}

HRESULT slHookPresent1(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, DXGI_PRESENT_PARAMETERS* params, bool& Skip)
{
    presentCommon(Flags);
    return S_OK;
}
HRESULT slHookPresent(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, bool& Skip)
{
    presentCommon(Flags);
    return S_OK;
}

HRESULT slHookResizeSwapChainPre(IDXGISwapChain* swapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags)
{
    CHI_VALIDATE(ctx.compute->clearCache());
    return S_OK;
}

}

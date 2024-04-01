/*
* Copyright (c) 2022-2023 NVIDIA CORPORATION. All rights reserved
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

#include <sstream>
#include <atomic>
#include <future>
#include <unordered_map>
#include <assert.h>

#include "include/sl.h"
#include "include/sl_deepdvc.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.param/parameters.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/platforms/sl.chi/vulkan.h"
#include "source/plugins/sl.deepdvc/versions.h"
#include "source/plugins/sl.imgui/imgui.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "external/json/include/nlohmann/json.hpp"
#include "external/nvapi/nvapi.h"

#include "_artifacts/gitVersion.h"
#include "_artifacts/json/deepdvc_json.h"

#include "external/ngx-sdk/include/nvsdk_ngx.h"
#include "external/ngx-sdk/include/nvsdk_ngx_helpers.h"
#include "external/ngx-sdk/include/nvsdk_ngx_helpers_vk.h"
#include "external/ngx-sdk/include/nvsdk_ngx_defs.h"
#include "external/ngx-sdk/include/nvsdk_ngx_defs_deepdvc.h"
#include "external/ngx-sdk/include/nvsdk_ngx_helpers_deepdvc.h"

#ifdef DEEPDVC_PRESENT_HOOK
 // Enable DEEPDVC_PRESENT_HOOK for testing on games without DeepDVC support
#include <d3d12.h>
#include <dxgi1_6.h>
#include "_artifacts/json/deepdvc_hooks_json.h"
#endif


using json = nlohmann::json;
namespace sl
{

namespace deepDVC
{

struct DeepDVCViewport
{
    uint32_t id = {};
    DeepDVCOptions consts = {};
    NVSDK_NGX_Handle* handle = {};
};

struct UIStats
{
    std::mutex mtx;
    std::string mode{};
    std::string viewport{};
    std::string runtime{};
};

struct DeepDVCContext
{
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(DeepDVCContext);
    void onCreateContext() {};
    void onDestroyContext() {};

    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};

    common::ViewportIdFrameData<4, false> constsPerViewport = { "deepDVC" };
    std::map<uint32_t, DeepDVCViewport> viewports = {};
    DeepDVCViewport* currentViewport = {};

    UIStats uiStats{};

    // width and height for VRAM calculation
    uint32_t inputWidth{};
    uint32_t inputHeight{};

    chi::ICompute* compute = {};
#ifdef DEEPDVC_PRESENT_HOOK
    chi::ICommandListContext* cmdList{};
    chi::CommandQueue cmdQueue{};
    sl::chi::Resource temp{};
#endif

    common::NGXContext* ngxContext = {};

#ifndef SL_PRODUCTION
    std::string ngxVersion{};
#endif

    std::map<void*, chi::ResourceState> cachedStates = {};
    std::map<void*, NVSDK_NGX_Resource_VK> cachedVkResources = {};

    RenderAPI platform;

    NVSDK_NGX_Resource_VK* cachedVkResource(sl::Resource* res)
    {
        auto it = cachedVkResources.find(res->native);
        return it == cachedVkResources.end() ? nullptr : &(*it).second;
    }

    void cacheState(chi::Resource res, uint32_t nativeState = 0)
    {
        //if (cachedStates.find(res) == cachedStates.end())
        {
            // Providing state is now mandatory, defaults to "common" which is 0
            chi::ResourceState state;
            compute->getResourceState(nativeState, state);
            cachedStates[res->native] = state;

            if (res && platform == RenderAPI::eVulkan)
            {
                NVSDK_NGX_Resource_VK ngx = {};
                if (res->native)
                {
                    chi::ResourceDescription desc = {};
                    desc.state = state;
                    CHI_CHECK_RV(compute->getResourceDescription(res, desc));
                    ngx.Resource.ImageViewInfo.ImageView = (VkImageView)res->view;
                    ngx.Resource.ImageViewInfo.Image = (VkImage)res->native;
                    ngx.Resource.ImageViewInfo.SubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };
                    ngx.Resource.ImageViewInfo.Format = (VkFormat)desc.nativeFormat;
                    ngx.Resource.ImageViewInfo.Width = desc.width;
                    ngx.Resource.ImageViewInfo.Height = desc.height;
                    ngx.Type = NVSDK_NGX_RESOURCE_VK_TYPE_VK_IMAGEVIEW;
                    ngx.ReadWrite = (desc.flags & chi::ResourceFlags::eShaderResourceStorage) != 0;
                    cachedVkResources[res->native] = ngx;
                }
            }
        }
    }
};
}

#ifdef DEEPDVC_PRESENT_HOOK
static std::string JSON = std::string(deepdvc_hooks_json, &deepdvc_hooks_json[deepdvc_hooks_json_len]);
#else
static std::string JSON = std::string(deepdvc_json, &deepdvc_json[deepdvc_json_len]);
#endif

void updateEmbeddedJSON(json& config)
{
    common::SystemCaps* caps = {};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kSystemCaps, &caps);
    common::PFunUpdateCommonEmbeddedJSONConfig* updateCommonEmbeddedJSONConfig{};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kPFunUpdateCommonEmbeddedJSONConfig, &updateCommonEmbeddedJSONConfig);
    if (caps && updateCommonEmbeddedJSONConfig)
    {
        // update JSON config plug-in requirements
        common::PluginInfo info{};
        info.SHA = GIT_LAST_COMMIT_SHORT;
        info.minGPUArchitecture = NV_GPU_ARCHITECTURE_TU100;
        info.minOS = Version(10, 0, 0);
        info.needsNGX = true;
        info.requiredTags = { {kBufferTypeScalingOutputColor, ResourceLifecycle::eValidUntilEvaluate} };
        updateCommonEmbeddedJSONConfig(&config, info);
    }
    // TODO: Check DeepDVC min driver version
}

SL_PLUGIN_DEFINE("sl.deepdvc", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON.c_str(), updateEmbeddedJSON, deepDVC, DeepDVCContext)

Result slSetData(const BaseStructure* inputs, CommandBuffer* cmdBuffer)
{
    auto& ctx = (*deepDVC::getContext());
    auto options = findStruct<DeepDVCOptions>(inputs);
    auto viewport = findStruct<ViewportHandle>(inputs);
    if (!options || !viewport)
    {
        SL_LOG_ERROR("Invalid input data");
        return Result::eErrorMissingInputParameter;
    }
    ctx.constsPerViewport.set(0, *viewport, static_cast<DeepDVCOptions*>(options));
    ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DeepDVC_Strength, (uint32_t)options->intensity);
    ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DeepDVC_SaturationBoost, (uint32_t)options->saturationBoost);
    return Result::eOk;
}

Result deepDVCBeginEvaluation(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*deepDVC::getContext());
    auto& viewport = ctx.viewports[data.id];
    viewport.id = data.id;

    // Options are set per viewport, frame index is always 0
    DeepDVCOptions* consts{};
    if (!ctx.constsPerViewport.get({ data.id, 0 }, &consts))
    {
        return Result::eErrorMissingConstants;
    }

    viewport.consts = *consts;
    ctx.currentViewport = &viewport;
    if (!viewport.handle)
    {
        ctx.cachedStates.clear();
        if (ctx.ngxContext)
        {
            if (ctx.ngxContext->createFeature(cmdList, NVSDK_NGX_Feature_DeepDVC, &viewport.handle, "sl.deepdvc"))
            {
                SL_LOG_INFO("Created deepdvc feature. Intensity = (%f), Saturation Boost = (%f). Viewport = (%u)", viewport.consts.intensity, viewport.consts.saturationBoost, data.id);
            }
        }
    }
    return Result::eOk;
}

Result deepDVCEndEvaluation(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*deepDVC::getContext());
    if (!ctx.currentViewport)
    {
        return Result::eErrorInvalidParameter;
    }

    const uint32_t id = ctx.currentViewport->id;
    const DeepDVCOptions& options = ctx.currentViewport->consts;

    CommonResource outColor{};
    
    SL_CHECK(getTaggedResource(kBufferTypeScalingOutputColor, outColor, id));
    
    auto outExtent = outColor.getExtent();
    ctx.cacheState(outColor, outColor.getState());

    chi::ResourceDescription outDesc{};
    CHI_VALIDATE(ctx.compute->getResourceState(outColor.getState(), outDesc.state));
    CHI_VALIDATE(ctx.compute->getResourceDescription(outColor, outDesc));
    if (!outExtent)
    {
        outExtent = { 0, 0, outDesc.width, outDesc.height };
    }
    if (outExtent.left + outExtent.width > outDesc.width || outExtent.top + outExtent.height > outDesc.height)
    {
        SL_LOG_ERROR("DeepDVC invalid scaling output color extent. Check extent dimensions.");
    }

    // store input texture size for VRAM calculation
    ctx.inputWidth = outExtent.width;
    ctx.inputHeight = outExtent.height;

    CHI_VALIDATE(ctx.compute->beginPerfSection(cmdList, "sl.deepdvc"));

    extra::ScopedTasks revTransitions;
    chi::ResourceTransition transitions[] =
    {
        {outColor, chi::ResourceState::eStorageRW, ctx.cachedStates[outColor]}
    };
    ctx.compute->transitionResources(cmdList, transitions, (uint32_t)countof(transitions), &revTransitions);
    if (ctx.ngxContext)
    {
        if (ctx.platform == RenderAPI::eVulkan)
        {
            ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_Color, ctx.cachedVkResource(outColor));
        }
        else
        {
            ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_Color, (void*)outColor);
        }
        ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X, outExtent.left);
        ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y, outExtent.top);
        ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, outExtent.width);
        ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, outExtent.height);
        ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DeepDVC_Strength, options.intensity);
        ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DeepDVC_SaturationBoost, options.saturationBoost);
        ctx.ngxContext->evaluateFeature(cmdList, ctx.currentViewport->handle, "sl.deepdvc");
    }

    float ms = 0;
    CHI_VALIDATE(ctx.compute->endPerfSection(cmdList, "sl.deepdvc", ms));

    auto parameters = api::getContext()->parameters;

#ifndef SL_PRODUCTION
    {
        std::scoped_lock lock(ctx.uiStats.mtx);
        ctx.uiStats.mode = getDeepDVCModeAsStr(options.mode);
        ctx.uiStats.viewport = extra::format("Viewport {}x{}", outExtent.width, outExtent.height);
        ctx.uiStats.runtime = extra::format("Execution time {}ms", ms);
    }
#endif

    // Tell others that we are actually active this frame
    uint32_t frame = 0;
    CHI_VALIDATE(ctx.compute->getFinishedFrameIndex(frame));
    parameters->set(sl::param::deepDVC::kCurrentFrame, frame + 1);

    ctx.currentViewport = {};
    return Result::eOk;
}

//! -------------------------------------------------------------------------------------------------
//! Required interface


//! Plugin startup
//!
//! Called only if plugin reports `supported : true` in the JSON config.
//! Note that supported flag can flip back to false if this method fails.
//!
//! @param device Either ID3D12Device or struct VkDevices (see internal.h)
bool slOnPluginStartup(const char* jsonConfig, void* device)
{
    SL_PLUGIN_COMMON_STARTUP();


    auto& ctx = (*deepDVC::getContext());

    auto parameters = api::getContext()->parameters;

    getPointerParam(parameters, param::global::kNGXContext, &ctx.ngxContext);

    if (!ctx.ngxContext)
    {
        SL_LOG_ERROR("Missing NGX context - DeepDVCContext cannot run");
        return false;
    }

    if (!ctx.ngxContext->params)
    {
        SL_LOG_ERROR("Missing NGX default parameters - DeepDVCContext cannot run");
        return false;
    }

    int supported = 0;
    NVSDK_NGX_Result ngxResult = ctx.ngxContext->params->Get(NVSDK_NGX_Parameter_DeepDVC_Available, &supported);
    if (NVSDK_NGX_FAILED(ngxResult))
    {
        SL_LOG_ERROR("NGX parameter indicating DeepDVCContext support cannot be found (0x%x) - DeepDVCContext cannot run", ngxResult);
        return false;
    }

    if (!supported)
    {
        SL_LOG_ERROR("NGX indicates DeepDVCContext is not available - DeepDVCContext cannot run");
        return false;
    }

    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &ctx.registerEvaluateCallbacks))
    {
        SL_LOG_ERROR( "Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        return false;
    }
    ctx.registerEvaluateCallbacks(kFeatureDeepDVC, deepDVCBeginEvaluation, deepDVCEndEvaluation);

    param::getPointerParam(parameters, sl::param::common::kComputeAPI, &ctx.compute);
    ctx.compute->getRenderAPI(ctx.platform);

#ifndef SL_PRODUCTION
    // Check for UI and register our callback
    imgui::ImGUI* ui{};
    param::getPointerParam(parameters, param::imgui::kInterface, &ui);
    if (ui)
    {
        // Runs async from the present thread where UI is rendered just before frame is presented
        auto renderUI = [&ctx](imgui::ImGUI* ui, bool finalFrame)->void
        {
            auto v = api::getContext()->pluginVersion;
            std::scoped_lock lock(ctx.uiStats.mtx);
            uint32_t lastFrame, frame;
            if (api::getContext()->parameters->get(sl::param::deepDVC::kCurrentFrame, &lastFrame))
            {
                ctx.compute->getFinishedFrameIndex(frame);
                if (lastFrame < frame)
                {
                    ctx.uiStats.mode = "Mode: Off";
                    ctx.uiStats.viewport = ctx.uiStats.runtime = {};
                }
                if (ui->collapsingHeader(extra::format("sl.deepdvc v{}", (v.toStr() + "." + GIT_LAST_COMMIT_SHORT)).c_str(), imgui::kTreeNodeFlagDefaultOpen))
                {
                    ui->text(ctx.uiStats.mode.c_str());
                    ui->text(ctx.uiStats.viewport.c_str());
                    ui->text(ctx.uiStats.runtime.c_str());
                }
            }
        };
        ui->registerRenderCallbacks(renderUI, nullptr);
    }
#endif
    return true;
}

//! Plugin shutdown
//!
//! Called by loader when unloading the plugin
void slOnPluginShutdown()
{
    auto& ctx = (*deepDVC::getContext());
    ctx.registerEvaluateCallbacks(kFeatureDeepDVC, nullptr, nullptr);

    // it will shutdown it down automatically
    plugin::onShutdown(api::getContext());

    ctx.compute = {};
}

sl::Result slDeepDVCSetOptions(const sl::ViewportHandle& viewport, const sl::DeepDVCOptions& options)
{
    auto v = viewport;
    v.next = (sl::BaseStructure*)&options;
    return slSetData(&v, nullptr);
}

sl::Result slDeepDVCGetState(const sl::ViewportHandle& viewport, sl::DeepDVCState& state)
{
    auto& ctx = (*deepDVC::getContext());
    if (ctx.ngxContext)
    {
        void* callback = NULL;
        ctx.ngxContext->params->Get(NVSDK_NGX_Parameter_DeepDVC_GetStatsCallback, &callback);
        if (!callback)
        {
            SL_LOG_ERROR("DLSSContext 'getStats' callback is missing, please make sure DLSSContext feature is up to date");
            return Result::eErrorNGXFailed;
        }
        auto getStats = (PFN_NVSDK_NGX_DeepDVC_GetStatsCallback)callback;
        auto res = getStats(ctx.ngxContext->params);
        if (NVSDK_NGX_FAILED(res))
        {
            SL_LOG_ERROR("DLSSContext 'getStats' callback failed - error %u", res);
            return Result::eErrorNGXFailed;
        }
        ctx.ngxContext->params->Get(NVSDK_NGX_Parameter_SizeInBytes, &state.estimatedVRAMUsageInBytes);
    }
    return Result::eOk;
}

#ifdef DEEPDVC_PRESENT_HOOK
//! ------------------------- DXGI AND D3D HOOKS -------------------------------
//!
//!
HRESULT slHookCreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain)
{
    auto& ctx = (*deepDVC::getContext());
    ctx.cmdQueue = pDevice;
    ctx.compute->createCommandListContext(ctx.cmdQueue, 1, ctx.cmdList, "game command list");
    common::EventData data;
    sl::ViewportHandle viewport = { 0 };
    sl::DeepDVCOptions options;
    options.mode = DeepDVCMode::eOn;
    options.intensity = 0.50f;
    options.saturationBoost = 0.25f;
    slDeepDVCSetOptions(viewport, options);
    deepDVCBeginEvaluation(ctx.cmdList->getCmdList(), data, nullptr, 0);
    SL_LOG_WARN("slHookCreateSwapChainFor---------- Buffer Count = %d | Format = %d | Buffer Usage = %d | Swap effect = %d | flags = %d", pDesc->BufferCount, pDesc->BufferDesc.Format, pDesc->BufferUsage, pDesc->SwapEffect, pDesc->Flags);
    return S_OK;
}

HRESULT slHookCreateSwapChainForHwnd(IDXGIFactory2* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain)
{
    auto& ctx = (*deepDVC::getContext());
    ctx.cmdQueue = pDevice;
    ctx.compute->createCommandListContext(ctx.cmdQueue, 1, ctx.cmdList, "game command list");
    common::EventData data;
    sl::ViewportHandle viewport = { 0 };
    sl::DeepDVCOptions options;
    options.mode = DeepDVCMode::eOn;
    options.intensity = 0.50f;
    options.saturationBoost = 0.25f;
    slDeepDVCSetOptions(viewport, options);
    deepDVCBeginEvaluation(ctx.cmdList->getCmdList(), data, nullptr, 0);
    SL_LOG_WARN("slHookCreateSwapChainForHwnd ---------- Buffer Count = %d | Format = %d | Buffer Usage = %d | Swap effect = %d | flags = %d", pDesc->BufferCount, pDesc->Format, pDesc->BufferUsage, pDesc->SwapEffect, pDesc->Flags);
    return S_OK;
}

HRESULT slHookPresent1(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, const DXGI_PRESENT_PARAMETERS* pPresentParameters, bool& Skip)
{
    auto& ctx = (*deepDVC::getContext());

    //if (ctx->swapChain == swapChain)
    {
        int currentIdx = ((IDXGISwapChain3*)swapChain)->GetCurrentBackBufferIndex();
        const DeepDVCOptions& options = ctx.currentViewport->consts;

        chi::Resource backBuffer;
        ctx.compute->getSwapChainBuffer(swapChain, currentIdx, backBuffer);

        chi::ResourceDescription outDesc{};
        CHI_VALIDATE(ctx.compute->getResourceDescription(backBuffer, outDesc));
        CHI_VALIDATE(ctx.compute->getResourceState(backBuffer->state, outDesc.state));

        if (!ctx.temp)
        {
            chi::ResourceDescription desc(outDesc.width, outDesc.height, outDesc.format, chi::HeapType::eHeapTypeDefault, chi::ResourceState::eStorageRW, chi::ResourceFlags::eShaderResourceStorage | chi::ResourceFlags::eColorAttachment);
            CHI_VALIDATE(ctx.compute->createTexture2D(desc, ctx.temp, "sl.deepdvc.temp"));
        }
        extra::ScopedTasks revTransitions;
        chi::ResourceTransition transitions[] =
        {
            {backBuffer, chi::ResourceState::eStorageRW, outDesc.state}
        };
        ctx.cmdList->beginCommandList();
        ctx.compute->transitionResources(ctx.cmdList->getCmdList(), transitions, (uint32_t)countof(transitions), &revTransitions);
        ctx.compute->copyResource(ctx.cmdList->getCmdList(), ctx.temp, backBuffer);
        if (ctx.ngxContext)
        {
            ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_Color, (void*)ctx.temp->native);
            ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DeepDVC_Strength, options.intensity);
            ctx.ngxContext->params->Set(NVSDK_NGX_Parameter_DeepDVC_SaturationBoost, options.saturationBoost);
            ctx.ngxContext->evaluateFeature(ctx.cmdList->getCmdList(), ctx.currentViewport->handle, "sl.deepdvc");
        }
        ctx.compute->copyResource(ctx.cmdList->getCmdList(), backBuffer, ctx.temp);
        ctx.cmdList->executeCommandList();
        ctx.cmdList->waitForCommandList(sl::chi::FlushType::eCurrent);
        ctx.compute->destroyResource(backBuffer);
    }
    return S_OK;
}

HRESULT slHookPresent(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, bool& Skip)
{
    return slHookPresent1(swapChain, SyncInterval, Flags, nullptr, Skip);
}
#endif



SL_EXPORT void *slGetPluginFunction(const char *functionName)
{
    // Forward declarations
    bool slOnPluginLoad(sl::param::IParameters * params, const char* loaderJSON, const char** pluginJSON);

    //! Redirect to OTA if any
    SL_EXPORT_OTA;

    // Core API
    SL_EXPORT_FUNCTION(slOnPluginLoad);
    SL_EXPORT_FUNCTION(slOnPluginShutdown);
    SL_EXPORT_FUNCTION(slOnPluginStartup);
    SL_EXPORT_FUNCTION(slSetData);
    SL_EXPORT_FUNCTION(slDeepDVCSetOptions);
    SL_EXPORT_FUNCTION(slDeepDVCGetState);

#ifdef DEEPDVC_PRESENT_HOOK
    SL_EXPORT_FUNCTION(slHookCreateSwapChain);
    SL_EXPORT_FUNCTION(slHookCreateSwapChainForHwnd);
    SL_EXPORT_FUNCTION(slHookPresent);
    SL_EXPORT_FUNCTION(slHookPresent1);
#endif
    return nullptr;
}
}
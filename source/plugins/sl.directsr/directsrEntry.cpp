/*
* Copyright (c) 2024 NVIDIA CORPORATION. All rights reserved
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

#include "external/dx-agility-sdk-headers-1.714.0-preview/include/d3d12.h"
#include "external/dx-agility-sdk-headers-1.714.0-preview/include/directsr.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <future>
#include <sstream>
#include <unordered_set>

#include <wrl/client.h>

#include <dxgi1_6.h>

#include "include/sl.h"
#include "include/sl_struct.h"
#include "include/sl_directsr.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "external/json/include/nlohmann/json.hpp"

#include "source/plugins/sl.common/commonInterface.h"

#include "_artifacts/json/directsr_json.h"
#include "source/plugins/sl.directsr/versions.h"
#include "_artifacts/gitVersion.h"

#include <libloaderapi.h>

using json = nlohmann::json;


namespace sl
{

sl::Result slDirectSRGetOptimalSettings(const sl::DirectSROptions& options, sl::DirectSROptimalSettings& settings);

namespace directsr
{

class DirectSRInstance
{
    private:
    Microsoft::WRL::ComPtr<IDSRDevice> m_pDsrDevice;

    Microsoft::WRL::ComPtr<IDSRSuperResEngine> m_pDsrEngine;
    Microsoft::WRL::ComPtr<IDSRSuperResUpscaler> m_pDsrUpscaler;
    bool m_needsRecreate;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastExecuteTime;

    public:
    uint32_t id;
    DirectSROptions m_options;

    DirectSRInstance(uint32_t id) : id(id) {}

    sl::Result setOptions(Microsoft::WRL::ComPtr<IDSRDevice> pDsrDevice,
                          const DirectSROptions *options)
    {
        // If options that require a re-creation are changed
        if (options->pCommandQueue != m_options.pCommandQueue ||
            options->outputWidth != m_options.outputWidth ||
            options->outputHeight != m_options.outputHeight ||
            options->colorBuffersHDR != m_options.colorBuffersHDR ||
            options->variantIndex != m_options.variantIndex ||
            options->optType != m_options.optType)
        {
            m_needsRecreate = true;
        }
        m_pDsrDevice = pDsrDevice;
        m_options = *options;
        return sl::Result::eOk;
    }

    sl::Result prepareUpscalerEngine(const bool mvecJittered,
                                     const DXGI_FORMAT targetFormat,
                                     const DXGI_FORMAT sourceColorFormat,
                                     const DXGI_FORMAT sourceDepthFormat,
                                     const DXGI_FORMAT exposureScaleFormat)
    {
        HRESULT res = S_OK;

        if (!m_needsRecreate)
        {
            return sl::Result::eOk;
        }
        m_needsRecreate = false;

        DSR_SUPERRES_CREATE_ENGINE_PARAMETERS createParams;

        // Find the variant to use
        {
            DSR_SUPERRES_VARIANT_DESC desc;
            res = m_pDsrDevice->GetSuperResVariantDesc(m_options.variantIndex, &desc);
            if (res != S_OK)
            {
                SL_LOG_ERROR("Failed to get variant desc: %x", res);
                return sl::Result::eErrorD3DAPI;
            }
            createParams.VariantId = desc.VariantId;
        }

        createParams.Flags = DSR_SUPERRES_CREATE_ENGINE_FLAG_NONE;

        // XXX[ljm] force auto exposure for now
        if (true)
        {
            createParams.Flags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_AUTO_EXPOSURE;
        }

        createParams.Flags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_ALLOW_DRS;

        if (mvecJittered)
        {
            createParams.Flags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_MOTION_VECTORS_USE_JITTER_OFFSETS;
        }

        createParams.Flags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_ALLOW_SUBRECT_OUTPUT;

        if (m_options.colorBuffersHDR == Boolean::eFalse)
        {
            createParams.Flags |= DSR_SUPERRES_CREATE_ENGINE_FLAG_FORCE_LDR_COLORS;
        }

        createParams.TargetFormat = targetFormat;
        createParams.SourceColorFormat = sourceColorFormat;
        createParams.SourceDepthFormat = sourceDepthFormat;
        createParams.ExposureScaleFormat = exposureScaleFormat;

        // XXX[ljm] do more depth-type coaslescing here
        if (createParams.SourceDepthFormat == DXGI_FORMAT_R24G8_TYPELESS)
        {
            createParams.SourceDepthFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        }

        createParams.TargetSize.Width = m_options.outputWidth;
        createParams.TargetSize.Height = m_options.outputHeight;

        sl::DirectSROptimalSettings settings;
        sl::slDirectSRGetOptimalSettings(m_options, settings);
        createParams.MaxSourceSize.Width = settings.renderWidthMax;
        createParams.MaxSourceSize.Height = settings.renderHeightMax;
        res = m_pDsrDevice->CreateSuperResEngine(&createParams,
                                               __uuidof(IDSRSuperResEngine),
                                               &m_pDsrEngine);
        if (res != S_OK)
        {
            SL_LOG_ERROR("CreateSuperResEngine failed %x", res);
            return sl::Result::eErrorD3DAPI;
        }

        res = m_pDsrEngine->CreateUpscaler(m_options.pCommandQueue,
                                           __uuidof(IDSRSuperResUpscaler),
                                           &m_pDsrUpscaler);
        if (res != S_OK)
        {
            SL_LOG_ERROR("CreateUpscaler failed %x", res);
            return sl::Result::eErrorD3DAPI;
        }

        SL_LOG_INFO("Upscaler engine prepared: %p", m_pDsrUpscaler);
        return sl::Result::eOk;
    }

    sl::Result evaluate(bool resetHistory,
                        DSR_FLOAT2 mvecScale,
                        DSR_FLOAT2 jitterOffset,
                        float cameraNear,
                        float cameraFar,
                        float cameraFOV,
                        ID3D12Resource *pTargetTexture,
                        D3D12_RECT      targetRegion,
                        ID3D12Resource *pSourceColorTexture,
                        D3D12_RECT      sourceColorRegion,
                        ID3D12Resource *pSourceDepthTexture,
                        D3D12_RECT      sourceDepthRegion,
                        ID3D12Resource *pMotionVectorsTexture,
                        D3D12_RECT      motionVectorsRegion)
    {
        DSR_SUPERRES_UPSCALER_EXECUTE_FLAGS flags = DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_NONE;
        if (resetHistory)
        {
            flags |= DSR_SUPERRES_UPSCALER_EXECUTE_FLAG_RESET_HISTORY;
        }

        DSR_SUPERRES_UPSCALER_EXECUTE_PARAMETERS dsrExec{};

        dsrExec.pTargetTexture = pTargetTexture;
        dsrExec.pSourceColorTexture = pSourceColorTexture;
        dsrExec.pSourceDepthTexture = pSourceDepthTexture;
        dsrExec.pMotionVectorsTexture = pMotionVectorsTexture;

        dsrExec.TargetRegion = targetRegion;
        dsrExec.SourceColorRegion = sourceColorRegion;
        dsrExec.SourceDepthRegion = sourceDepthRegion;
        dsrExec.MotionVectorsRegion = motionVectorsRegion;

        dsrExec.MotionVectorScale = mvecScale;
        dsrExec.CameraJitter = jitterOffset;
        dsrExec.ExposureScale = m_options.exposureScale;
        dsrExec.PreExposure = m_options.preExposure;
        dsrExec.Sharpness = m_options.sharpness;
        dsrExec.CameraNear = cameraNear;
        dsrExec.CameraFar = cameraFar;
        dsrExec.CameraFovAngleVert = cameraFOV;

        HRESULT res = S_OK;
        std::chrono::time_point<std::chrono::high_resolution_clock> executeTime =
            std::chrono::high_resolution_clock::now();
        float timeDelta = std::chrono::duration<float>(executeTime - m_lastExecuteTime).count();
        res = m_pDsrUpscaler->Execute(&dsrExec, timeDelta, flags);
        m_lastExecuteTime = executeTime;
        if (res != S_OK)
        {
            SL_LOG_ERROR("upscaler->Execute failed %x", res);
        }
        return sl::Result::eOk;
    }
};

}
}

namespace sl
{

namespace directsr
{
struct DirectSRContext
{
    // SL generic global state
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(DirectSRContext);
    void onCreateContext() {};
    void onDestroyContext() {};
    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};

    // Plugin specific global state
    std::map<uint32_t, DirectSRInstance*> viewports = {};
    Microsoft::WRL::ComPtr<ID3D12DSRDeviceFactory> dsrFactory;
    Microsoft::WRL::ComPtr<IDSRDevice> dsrDevice;
    HMODULE hD3D12{};
};
}

static std::string JSON = std::string(directsr_json, &directsr_json[directsr_json_len]);

void updateEmbeddedJSON(json& config);

SL_PLUGIN_DEFINE("sl.directsr", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON.c_str(), updateEmbeddedJSON, directsr, DirectSRContext)

void updateEmbeddedJSON(json& config)
{
    // Check if plugin is supported or not on this platform and set the flag accordingly
    common::PFunUpdateCommonEmbeddedJSONConfig* updateCommonEmbeddedJSONConfig{};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kPFunUpdateCommonEmbeddedJSONConfig, &updateCommonEmbeddedJSONConfig);

    // DLSS min driver
    common::PluginInfo info{};
    info.SHA = GIT_LAST_COMMIT_SHORT;
    info.minOS = Version(10, 0, 0);
    info.requiredTags = { { kBufferTypeDepth, ResourceLifecycle::eValidUntilEvaluate}, {kBufferTypeMotionVectors, ResourceLifecycle::eValidUntilEvaluate},
                          { kBufferTypeScalingInputColor, ResourceLifecycle::eValidUntilEvaluate}, { kBufferTypeScalingOutputColor, ResourceLifecycle::eValidUntilEvaluate} };

    if (updateCommonEmbeddedJSONConfig)
    {
        updateCommonEmbeddedJSONConfig(&config, info);
    }
}

//! -------------------------------------------------------------------------------------------------
//! Required interface

sl::Result directsrBegin(chi::CommandList pCmdList, const common::EventData &data, const sl::BaseStructure **inputs, uint32_t numInputs)
{
    auto& ctx = (*directsr::getContext());
    sl::Constants *commonConsts;

    if (!common::getConsts(data, &commonConsts))
    {
        // Can't find common constants, warn/error logged by the above function
        return Result::eErrorMissingConstants;
    }

    // Find the appropriate instance in viewports, if not exist create it
    auto it = ctx.viewports.find(data.id);
    if (it == ctx.viewports.end())
    {
        return sl::Result::eErrorInvalidParameter;
    }

    sl::directsr::DirectSRInstance *viewport = ctx.viewports[data.id];

    CommonResource colorOut{};
    CommonResource colorIn{};
    CommonResource depth{};
    SL_CHECK(getTaggedResource(kBufferTypeScalingOutputColor, colorOut, data.id, false, inputs, numInputs));
    SL_CHECK(getTaggedResource(kBufferTypeScalingInputColor, colorIn, data.id, false, inputs, numInputs));
    SL_CHECK(getTaggedResource(kBufferTypeDepth, depth, data.id, false, inputs, numInputs));

    return viewport->prepareUpscalerEngine(commonConsts->motionVectorsJittered == Boolean::eTrue,
                                           ((ID3D12Resource*)(void*)colorOut)->GetDesc().Format,
                                           ((ID3D12Resource*)(void*)colorIn)->GetDesc().Format,
                                           ((ID3D12Resource*)(void*)depth)->GetDesc().Format,
                                           DXGI_FORMAT_UNKNOWN
                                           );
}

sl::Result directsrEnd(chi::CommandList pCmdList, const common::EventData &data, const sl::BaseStructure **inputs, uint32_t numInputs)
{
    auto& ctx = (*directsr::getContext());
    sl::Constants *commonConsts;

    if (!common::getConsts(data, &commonConsts))
    {
        // Can't find common constants, warn/error logged by the above function
        return Result::eErrorMissingConstants;
    }

    // Find the appropriate instance in viewports, if not exist create it
    auto it = ctx.viewports.find(data.id);
    if (it == ctx.viewports.end())
    {
        return sl::Result::eErrorInvalidParameter;
    }

    sl::directsr::DirectSRInstance *viewport = ctx.viewports[data.id];

    CommonResource colorOut{};
    CommonResource colorIn{};
    CommonResource depth{};
    CommonResource mvec{};
    SL_CHECK(getTaggedResource(kBufferTypeScalingOutputColor, colorOut, data.id, false, inputs, numInputs));
    SL_CHECK(getTaggedResource(kBufferTypeScalingInputColor, colorIn, data.id, false, inputs, numInputs));
    SL_CHECK(getTaggedResource(kBufferTypeDepth, depth, data.id, false, inputs, numInputs));
    SL_CHECK(getTaggedResource(kBufferTypeMotionVectors, mvec, data.id, false, inputs, numInputs));

    uint32_t renderWidth = ((sl::Extent)colorIn).width;
    uint32_t renderHeight = ((sl::Extent)colorIn).height;

    DSR_FLOAT2 mvecScale;
    mvecScale.X = commonConsts->mvecScale.x * renderWidth;
    mvecScale.Y = commonConsts->mvecScale.y * renderHeight;

    DSR_FLOAT2 jitterOffset;
    jitterOffset.X = commonConsts->jitterOffset.x;
    jitterOffset.Y = commonConsts->jitterOffset.y;

    return viewport->evaluate(commonConsts->reset == sl::Boolean::eTrue,
                              mvecScale,
                              jitterOffset,
                              commonConsts->cameraNear,
                              commonConsts->cameraFar,
                              commonConsts->cameraFOV,
                              (ID3D12Resource*)(void*)colorOut,
                              (D3D12_RECT)(sl::Extent)colorOut,
                              (ID3D12Resource*)(void*)colorIn,
                              (D3D12_RECT)(sl::Extent)colorIn,
                              (ID3D12Resource*)(void*)depth,
                              (D3D12_RECT)(sl::Extent)depth,
                              (ID3D12Resource*)(void*)mvec,
                              (D3D12_RECT)(sl::Extent)mvec);
}

sl::Result slDirectSRGetOptimalSettings(const sl::DirectSROptions& options, sl::DirectSROptimalSettings& settings)
{
    auto& ctx = (*directsr::getContext());

    DSR_SUPERRES_SOURCE_SETTINGS dsrSettings;

    DSR_SIZE outputSizeDsr;
    outputSizeDsr.Width = options.outputWidth;
    outputSizeDsr.Height = options.outputHeight;

    HRESULT res = S_OK;
    res = ctx.dsrDevice->QuerySuperResSourceSettings(options.variantIndex,
                                                     outputSizeDsr,
                                                     DXGI_FORMAT_R16G16B16A16_FLOAT,
                                                     (DSR_OPTIMIZATION_TYPE)options.optType,
                                                     DSR_SUPERRES_CREATE_ENGINE_FLAG_ALLOW_DRS,
                                                     &dsrSettings);
    if (res != S_OK)
    {
        SL_LOG_ERROR("Failed to QuerySuperResSourceSettings: %x", res);
        return sl::Result::eErrorD3DAPI;
    }

    settings.optimalRenderWidth = dsrSettings.OptimalSize.Width;
    settings.optimalRenderHeight = dsrSettings.OptimalSize.Height;

    settings.renderWidthMin = dsrSettings.MinDynamicSize.Width;
    settings.renderHeightMin = dsrSettings.MinDynamicSize.Height;
    settings.renderWidthMax = dsrSettings.MaxDynamicSize.Width;
    settings.renderHeightMax = dsrSettings.MaxDynamicSize.Height;

    settings.optimalColorFormat = dsrSettings.OptimalColorFormat;
    settings.optimalDepthFormat = dsrSettings.OptimalDepthFormat;
    return sl::Result::eOk;
}

sl::Result slDirectSRGetVariantInfo(uint32_t *numVariants, sl::DirectSRVariantInfo *variantInfo)
{
    if (numVariants == nullptr && variantInfo == nullptr) return sl::Result::eErrorInvalidParameter;

    auto& ctx = (*directsr::getContext());

    if (variantInfo == nullptr) {
        *numVariants = ctx.dsrDevice->GetNumSuperResVariants();
        return sl::Result::eOk;
    } else {
        for (uint32_t i = 0; i < *numVariants; i++) {
            DSR_SUPERRES_VARIANT_DESC desc;
            if (ctx.dsrDevice->GetSuperResVariantDesc(i, &desc) != S_OK) {
                return sl::Result::eErrorInvalidParameter;
            }

            // Copy all the fields over
            memcpy(variantInfo[i].name, desc.VariantName, sizeof(variantInfo[i].name));
            variantInfo[i].flags = (sl::DirectSRVariantFlags)desc.Flags;
            variantInfo[i].optimalTargetFormat = desc.OptimalTargetFormat;
            for (uint32_t j = 0; j < 7; j++) {
                variantInfo[i].optimizationRankings[j] = (sl::DirectSROptimizationType)desc.OptimizationRankings[j];
            }
        }
    }

    return sl::Result::eOk;
}

sl::Result slDirectSRSetOptions(const sl::ViewportHandle& viewport, const sl::DirectSROptions& options)
{
    auto& ctx = (*directsr::getContext());

    // Create an instance if one doesn't exist for the given viewport
    auto it = ctx.viewports.find(viewport);
    if (it == ctx.viewports.end())
    {
        ctx.viewports[viewport] = new sl::directsr::DirectSRInstance(viewport);
    }

    sl::directsr::DirectSRInstance *instance = ctx.viewports[viewport];
    instance->setOptions(ctx.dsrDevice, &options);
    return Result::eOk;
}

static void freePluginGlobalState()
{
    auto& ctx = (*directsr::getContext());

    if (ctx.dsrDevice != nullptr)
    {
        ctx.dsrDevice.Reset();
    }
    if (ctx.dsrFactory != nullptr)
    {
        ctx.dsrFactory.Reset();
    }
    if (ctx.hD3D12 != nullptr)
    {
        FreeLibrary(ctx.hD3D12);
    }
}

//! Plugin startup
//!
//! Called only if plugin reports `supported : true` in the JSON config.
//! Note that supported flag can flip back to false if this method fails.
//!
//! @param device Either ID3D12Device or struct VkDevices (see internal.h)
bool slOnPluginStartup(const char* jsonConfig, void* device)
{
    SL_PLUGIN_COMMON_STARTUP();

    auto& ctx = (*directsr::getContext());
    // Get DirectSR handle and create a device
    PFN_D3D12_GET_INTERFACE fpD3D12GetInterface = nullptr;
    sl::param::IParameters *parameters = nullptr;
    HRESULT dsrRes = S_OK;
    uint32_t count;

    // Load the JSON config and check the graphics API in use before proceeding.
    {
        json& config = *(json*)api::getContext()->loaderConfig;
        sl::RenderAPI renderApi;
        config.at("deviceType").get_to(renderApi);

        if (renderApi != sl::RenderAPI::eD3D12)
        {
            SL_LOG_WARN("sl.directsr is only compatible with D3D12!");
            goto fail;
        }
    }

    ctx.hD3D12 = LoadLibraryA("d3d12.dll");
    if (ctx.hD3D12 == nullptr)
    {
        SL_LOG_WARN("Failed to load d3d12.dll");
        goto fail;
    }

    fpD3D12GetInterface = (PFN_D3D12_GET_INTERFACE)GetProcAddress(ctx.hD3D12, "D3D12GetInterface");
    if (fpD3D12GetInterface == nullptr)
    {
        SL_LOG_WARN("GetProcAddress for D3D12GetInterface failed");
        goto fail;
    }

    dsrRes = fpD3D12GetInterface(CLSID_D3D12DSRDeviceFactory, __uuidof(ID3D12DSRDeviceFactory), &ctx.dsrFactory);
    if (dsrRes != S_OK)
    {
        SL_LOG_WARN("GetInterface for D3D12DSRDeviceFactory failed %x", dsrRes);
        goto fail;
    }

    // Create a DirectSR Device for use by specific instances
    dsrRes = ctx.dsrFactory->CreateDSRDevice((ID3D12Device*)device, 0 /* node */, __uuidof(IDSRDevice), &ctx.dsrDevice);
    if (dsrRes != S_OK)
    {
        SL_LOG_WARN("CreateDSRDevice failed %x", dsrRes);
        goto fail;
    }

    // List out the available variants in the log
    count = ctx.dsrDevice->GetNumSuperResVariants();
    SL_LOG_INFO("DirectSR on plugin startup, variant count: %d", count);
    for (uint32_t i = 0; i < count; i++)
    {
        DSR_SUPERRES_VARIANT_DESC desc;
        dsrRes = ctx.dsrDevice->GetSuperResVariantDesc(i, &desc);
        if (dsrRes == S_OK)
        {
            SL_LOG_INFO("Variant name is %s, flags (0x%x)", desc.VariantName, (uint32_t)desc.Flags);
        }
        else
        {
            SL_LOG_WARN("Failed to get variant (%d) desc: %x", i, dsrRes);
        }
    }

    // Register our evaluate callbacks
    parameters = api::getContext()->parameters;
    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &ctx.registerEvaluateCallbacks))
    {
        SL_LOG_WARN( "Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        goto fail;
    }

    ctx.registerEvaluateCallbacks(kFeatureDirectSR, directsrBegin, directsrEnd);

    return true;

fail:
    freePluginGlobalState();
    SL_LOG_WARN("sl.directsr failed");
    return false;
}

//! Plugin shutdown
//!
//! Called by loader when unloading the plugin
void slOnPluginShutdown()
{
    auto& ctx = (*directsr::getContext());
    ctx.registerEvaluateCallbacks(kFeatureDirectSR, nullptr, nullptr);

    // cleanup viewports
    for (const auto& [id, instance] : ctx.viewports)
    {
        delete instance;
    }
    ctx.viewports.clear();

    freePluginGlobalState();

    // Common shutdown
    plugin::onShutdown(api::getContext());
}

SL_EXPORT void *slGetPluginFunction(const char *functionName)
{
    SL_EXPORT_FUNCTION(slOnPluginLoad);
    SL_EXPORT_FUNCTION(slOnPluginShutdown);
    SL_EXPORT_FUNCTION(slOnPluginStartup);

    // App-facing Entrypoints
    SL_EXPORT_FUNCTION(slDirectSRGetOptimalSettings);
    SL_EXPORT_FUNCTION(slDirectSRGetVariantInfo);
    SL_EXPORT_FUNCTION(slDirectSRSetOptions);

    return nullptr;
}

}

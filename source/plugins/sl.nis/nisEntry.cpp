/*
* Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
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
#include "include/sl_nis.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.param/parameters.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.nis/versions.h"
#include "source/plugins/sl.imgui/imgui.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "external/json/include/nlohmann/json.hpp"
#include "_artifacts/gitVersion.h"
#include "_artifacts/json/nis_json.h"

#include "./NIS/NIS_Config.h"
// Compute shader permutations
#include "./NIS_shaders.h"

using json = nlohmann::json;
namespace sl
{

namespace nis
{
struct NISViewport
{
    uint32_t id = {};
    NISOptions consts = {};
};

struct UIStats
{
    std::mutex mtx;
    std::string mode{};
    std::string viewport{};
    std::string runtime{};
};

struct NISContext
{
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(NISContext);
    void onCreateContext() {};
    void onDestroyContext() {};

    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};

    common::ViewportIdFrameData<4, false> constsPerViewport = { "nis" };
    std::map<uint32_t, NISViewport> viewports = {};
    NISViewport* currentViewport = {};

    chi::Resource scalerCoef = {};
    chi::Resource usmCoef = {};
    chi::Resource uploadScalerCoef = {};
    chi::Resource uploadUsmCoef = {};

    UIStats uiStats{};

    NISConfig config;

    chi::ICompute* compute = {};

    std::unordered_map<uint32_t, chi::Kernel> shaders;

    // Specifies compute shader block width
    constexpr static uint32_t blockWidth = 32;
    // Specifies compute shader block height
    constexpr static uint32_t blockHeight = 24;
    // Specifies compute shader thread group size
    constexpr static uint32_t threadGroupSize = 128;

    uint32_t hash_combine(uint32_t a, uint32_t b, uint32_t c)
    {
        return  a + b * 10 + c * 100;
    }

    bool addShaderPermutation(NISMode scalerMode, uint32_t viewPorts, NISHDR HDRMode,
        uint8_t* byteCode, uint32_t len, const char* filename, const char* entryPoint = "main") {
        chi::Kernel kernel = {};
        CHI_CHECK_RF(compute->createKernel((void*)byteCode, len, filename, entryPoint, kernel));
        if (kernel)
            shaders[hash_combine((uint32_t)scalerMode, viewPorts, (uint32_t)HDRMode)] = kernel;
        return true;
    }

    chi::Kernel getKernel(NISMode scalerMode, uint32_t viewPorts, NISHDR HDRMode) {
        uint32_t key = hash_combine((uint32_t)scalerMode, viewPorts, (uint32_t)HDRMode);
        if (shaders.find(key) == shaders.end())
            return {};
        return shaders[key];
    }
};
}

constexpr uint32_t kMaxNumViewports = 4;

static std::string JSON = std::string(nis_json, &nis_json[nis_json_len]);

void updateEmbeddedJSON(json& config)
{
    // Check if plugin is supported or not on this platform and set the flag accordingly
    common::SystemCaps* caps = {};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kSystemCaps, &caps);
    common::PFunUpdateCommonEmbeddedJSONConfig* updateCommonEmbeddedJSONConfig{};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kPFunUpdateCommonEmbeddedJSONConfig, &updateCommonEmbeddedJSONConfig);
    if (caps && updateCommonEmbeddedJSONConfig)
    {
        // Our plugin runs on any system so use all defaults
        common::PluginInfo info{};
        info.SHA = GIT_LAST_COMMIT_SHORT;
        info.requiredTags = { { kBufferTypeScalingInputColor, ResourceLifecycle::eValidUntilEvaluate}, {kBufferTypeScalingOutputColor, ResourceLifecycle::eValidUntilEvaluate} };
        updateCommonEmbeddedJSONConfig(&config, info);
    }
}

SL_PLUGIN_DEFINE("sl.nis", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON.c_str(), updateEmbeddedJSON, nis, NISContext)

Result slSetData(const BaseStructure* inputs, CommandBuffer* cmdBuffer)
{
    auto& ctx = (*nis::getContext());
    auto options = findStruct<NISOptions>(inputs);
    auto viewport = findStruct<ViewportHandle>(inputs);
    if (!options)
    {
        return Result::eErrorMissingInputParameter;
    }
    ctx.constsPerViewport.set(0, *viewport, static_cast<NISOptions*>(options));
    
    return Result::eOk;
}

bool initializeNIS(chi::CommandList cmdList, const common::EventData& data)
{
    auto& ctx = (*nis::getContext());
    if (!ctx.scalerCoef && !ctx.usmCoef)
    {
        auto texDesc = sl::chi::ResourceDescription(kFilterSize / 4, kPhaseCount, sl::chi::eFormatRGBA32F);
        CHI_CHECK_RF(ctx.compute->createTexture2D(texDesc, ctx.scalerCoef, "nisScalerCoef"));
        CHI_CHECK_RF(ctx.compute->createTexture2D(texDesc, ctx.usmCoef, "nisUSMCoef"));

        int rowPitchAlignment = 1; // D3D11
        RenderAPI platform;
        ctx.compute->getRenderAPI(platform);
        if (platform == RenderAPI::eD3D12)
        {
            rowPitchAlignment = 256; // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
        }

        const int rowPitch = kFilterSize * sizeof(float);
        const int deviceRowPitch = extra::align(kFilterSize * sizeof(float), rowPitchAlignment);
        const int totalBytes = deviceRowPitch * kPhaseCount;

        if (!ctx.uploadScalerCoef)
        {
            chi::ResourceDescription bufferDesc(deviceRowPitch * kPhaseCount, 1, chi::eFormatINVALID, chi::HeapType::eHeapTypeUpload, chi::ResourceState::eUnknown);
            CHI_CHECK_RF(ctx.compute->createBuffer(bufferDesc, ctx.uploadScalerCoef, "sl.ctx.uploadScalerCoef"));
            CHI_CHECK_RF(ctx.compute->createBuffer(bufferDesc, ctx.uploadUsmCoef, "sl.ctx.uploadUsmCoef"));
        }

        std::vector<float> blobScale(totalBytes / sizeof(float));
        std::vector<float> blobUsm(totalBytes / sizeof(float));

        auto fillBlob = [rowPitch, deviceRowPitch](std::vector<float>& blob, const void* data)->void
        {
            void* cpuVA = blob.data();
            for (uint32_t row = 0; row < kPhaseCount; row++)
            {
                void* destAddress = (char*)cpuVA + deviceRowPitch * row;
                void* srcAddress = (char*)data + rowPitch * row;
                memcpy(destAddress, srcAddress, rowPitch);
            }
        };

        fillBlob(blobScale, coef_scale);
        fillBlob(blobUsm, coef_usm);

        CHI_CHECK_RF(ctx.compute->copyHostToDeviceTexture(cmdList, totalBytes, rowPitch, blobScale.data(), ctx.scalerCoef, ctx.uploadScalerCoef));
        CHI_CHECK_RF(ctx.compute->copyHostToDeviceTexture(cmdList, totalBytes, rowPitch, blobUsm.data(), ctx.usmCoef, ctx.uploadUsmCoef));
    }
    return true;
}

Result nisBeginEvaluation(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*nis::getContext());
    if (ctx.viewports.size() > (size_t)kMaxNumViewports)
    {
        SL_LOG_WARN("Exceeded max number (%u) of allowed viewports for NIS", kMaxNumViewports);
    }
    auto& viewport = ctx.viewports[data.id];
    viewport.id = data.id;

    // Options are set per viewport, frame index is always 0
    NISOptions* consts{};
    if (!ctx.constsPerViewport.get({ data.id, 0 }, &consts))
    {
        return Result::eErrorMissingConstants;
    }

    viewport.consts = *consts;
    ctx.currentViewport = &viewport;

    initializeNIS(cmdList, data);
    return Result::eOk;
}

Result nisEndEvaluation(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*nis::getContext());
    if (!ctx.currentViewport)
    {
        return Result::eErrorInvalidParameter;
    }

    const uint32_t id = ctx.currentViewport->id;
    const NISOptions& consts = ctx.currentViewport->consts;

    if (consts.mode != NISMode::eScaler && consts.mode != NISMode::eSharpen) {
        SL_LOG_ERROR( "Invalid NISContext mode %d", consts.mode);
        return Result::eErrorInvalidParameter;
    }
    if (consts.hdrMode != NISHDR::eNone && consts.hdrMode != NISHDR::eLinear && consts.hdrMode != NISHDR::ePQ) {
        SL_LOG_ERROR( "Invalid NISContext HDR mode %d", consts.hdrMode);
        return Result::eErrorInvalidParameter;
    }

    CommonResource colorIn{};
    CommonResource colorOut{};
    
    SL_CHECK(getTaggedResource(kBufferTypeScalingInputColor, colorIn, id, false, inputs, numInputs));
    SL_CHECK(getTaggedResource(kBufferTypeScalingOutputColor, colorOut, id, false, inputs, numInputs));
    
    auto inExtent = colorIn.getExtent();
    auto outExtent = colorOut.getExtent();

    // get resource states and descriptors
    chi::ResourceDescription inDesc{};
    CHI_VALIDATE(ctx.compute->getResourceState(colorIn.getState(), inDesc.state));
    CHI_VALIDATE(ctx.compute->getResourceDescription(colorIn, inDesc));
    chi::ResourceDescription outDesc{};
    CHI_VALIDATE(ctx.compute->getResourceState(colorOut.getState(), outDesc.state));
    CHI_VALIDATE(ctx.compute->getResourceDescription(colorOut, outDesc));
    if (!inExtent)
    {
        inExtent.width = inDesc.width;
        inExtent.height = inDesc.height;
    }
    if (!outExtent)
    {
        outExtent.width = outDesc.width;
        outExtent.height = outDesc.height;
    }


    uint32_t viewPortsSupport = inExtent.width != inDesc.width || inExtent.height != inDesc.height ||
        outExtent.width != outDesc.width || outExtent.height != outDesc.height;

    float sharpness = consts.sharpness;
    NISHDR hdrMode = consts.hdrMode;

    // if hdr is enabled then enable viewports
    if (hdrMode == NISHDR::eLinear || hdrMode == NISHDR::ePQ)
    {
        viewPortsSupport = true;
    }

    NISHDRMode nisHdrMode = hdrMode == NISHDR::eLinear ? NISHDRMode::Linear :
        hdrMode == NISHDR::ePQ ? NISHDRMode::PQ : NISHDRMode::None;

    chi::Kernel kernel = ctx.getKernel(consts.mode, viewPortsSupport, hdrMode);
    if (!kernel)
    {
        SL_LOG_ERROR( "Failed to find NISContext shader permutation mode: %d viewportSupport: %d, hdrMode: %d", consts.mode, viewPortsSupport, consts.hdrMode);
        return Result::eErrorInvalidParameter;
    }

    // Resource state already obtained and stored in resource description
    
    if (consts.mode == NISMode::eScaler)
    {
        if (!NVScalerUpdateConfig(ctx.config, sharpness,
            inExtent.left, inExtent.top, inExtent.width, inExtent.height, inDesc.width, inDesc.height,
            outExtent.left, outExtent.top, outExtent.width, outExtent.height, outDesc.width, outDesc.height,
            nisHdrMode))
        {
            SL_LOG_ERROR( "NVScaler configuration error, scale out of bounds or textures width/height with zero value");
            return Result::eErrorInvalidParameter;
        }
    }
    else
    {
        // Sharpening only (no upscaling)
        if (!NVSharpenUpdateConfig(ctx.config, sharpness,
            inExtent.left, inExtent.top, inExtent.width, inExtent.height, inDesc.width, inDesc.height,
            outExtent.left, outExtent.top, nisHdrMode))
        {
            SL_LOG_ERROR( "NVSharpen configuration error, textures width/height width zero value");
            return Result::eErrorInvalidParameter;
        }
    }

#if SL_ENABLE_TIMING
    CHI_VALIDATE(ctx.compute->beginPerfSection(cmdList, "sl.nis"));
#endif

    // Resource state already obtained and stored in resource description
    extra::ScopedTasks revTransitions;
    std::vector<chi::ResourceTransition> transitions {
        {colorIn, chi::ResourceState::eTextureRead, inDesc.state},
        {colorOut, chi::ResourceState::eStorageRW, outDesc.state}
    };
    ctx.compute->transitionResources(cmdList, transitions.data(), (uint32_t)(transitions.size()), &revTransitions);

    CHI_VALIDATE(ctx.compute->bindSharedState(cmdList));
    CHI_VALIDATE(ctx.compute->bindKernel(kernel));
    CHI_VALIDATE(ctx.compute->bindConsts(0, 0, &ctx.config, sizeof(ctx.config), kMaxNumViewports * 3));
    CHI_VALIDATE(ctx.compute->bindSampler(1, 0, chi::eSamplerLinearClamp));
    CHI_VALIDATE(ctx.compute->bindTexture(2, 0, colorIn));
    CHI_VALIDATE(ctx.compute->bindRWTexture(3, 0, colorOut));
    if (consts.mode == NISMode::eScaler)
    {
        CHI_VALIDATE(ctx.compute->bindTexture(4, 1, ctx.scalerCoef));
        CHI_VALIDATE(ctx.compute->bindTexture(5, 2, ctx.usmCoef));
    }
    CHI_VALIDATE(ctx.compute->dispatch(UINT(std::ceil(outDesc.width / float(ctx.blockWidth))), UINT(std::ceil(outDesc.height / float(ctx.blockHeight))), 1));

    float ms = 0;
#if SL_ENABLE_TIMING
    CHI_VALIDATE(ctx.compute->endPerfSection(cmdList, "sl.nis", ms));
#endif

    auto parameters = api::getContext()->parameters;

#ifndef SL_PRODUCTION
    // Report our stats, shown by sl.nis plugin
    /*static std::string s_stats;
    auto v = api::getContext()->pluginVersion;
    s_stats = extra::format("sl.nis {} - ({}x{})->({}x{}) - {}ms", v.toStr() + "." + GIT_LAST_COMMIT_SHORT, inExtent.width, inExtent.height,outDesc.width, outDesc.height, ms);
    parameters->set(sl::param::nis::kStats, (void*)s_stats.c_str());*/

    std::scoped_lock lock(ctx.uiStats.mtx);
    ctx.uiStats.mode = getNISModeAsStr(consts.mode);
    ctx.uiStats.viewport = extra::format("Viewport {}x{} -> {}x{}", inExtent.width, inExtent.height, outExtent.width, outExtent.height);
    ctx.uiStats.runtime = extra::format("Execution time {}ms", ms);
#endif

    // Tell others that we are actually active this frame
    uint32_t frame = 0;
    CHI_VALIDATE(ctx.compute->getFinishedFrameIndex(frame));
    parameters->set(sl::param::nis::kCurrentFrame, frame + 1);

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

    auto& ctx = (*nis::getContext());

    auto parameters = api::getContext()->parameters;

    if (!getPointerParam(parameters, param::common::kComputeAPI, &ctx.compute))
    {
        SL_LOG_ERROR( "Can't find %s", param::common::kComputeAPI);
        return false;
    }

    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &ctx.registerEvaluateCallbacks))
    {
        SL_LOG_ERROR( "Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        return false;
    }
    ctx.registerEvaluateCallbacks(kFeatureNIS, nisBeginEvaluation, nisEndEvaluation);

    RenderAPI platform;
    ctx.compute->getRenderAPI(platform);
    switch (platform)
    {
    case RenderAPI::eVulkan:
        {
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::eNone, NIS_Sharpen_V0_H0_spv, NIS_Sharpen_V0_H0_spv_len, "NIS_Sharpen_V0_H0.spv");
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::eLinear, NIS_Sharpen_V0_H1_spv, NIS_Sharpen_V0_H1_spv_len, "NIS_Sharpen_V0_H1.spv");
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::ePQ, NIS_Sharpen_V0_H2_spv, NIS_Sharpen_V0_H2_spv_len, "NIS_Sharpen_V0_H2.spv");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::eNone, NIS_Sharpen_V1_H0_spv, NIS_Sharpen_V1_H0_spv_len, "NIS_Sharpen_V1_H0.spv");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::eLinear, NIS_Sharpen_V1_H1_spv, NIS_Sharpen_V1_H1_spv_len, "NIS_Sharpen_V1_H1.spv");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::ePQ, NIS_Sharpen_V1_H2_spv, NIS_Sharpen_V1_H2_spv_len, "NIS_Sharpen_V1_H2.spv");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::eNone, NIS_Scaler_V0_H0_spv, NIS_Scaler_V0_H0_spv_len, "NIS_Scaler_V0_H0.spv");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::eLinear, NIS_Scaler_V0_H1_spv, NIS_Scaler_V0_H1_spv_len, "NIS_Scaler_V0_H1.spv");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::ePQ, NIS_Scaler_V0_H2_spv, NIS_Scaler_V0_H2_spv_len, "NIS_Scaler_V0_H2.spv");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::eNone, NIS_Scaler_V1_H0_spv, NIS_Scaler_V1_H0_spv_len, "NIS_Scaler_V1_H0.spv");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::eLinear, NIS_Scaler_V1_H1_spv, NIS_Scaler_V1_H1_spv_len, "NIS_Scaler_V1_H1.spv");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::ePQ, NIS_Scaler_V1_H2_spv, NIS_Scaler_V1_H2_spv_len, "NIS_Scaler_V1_H2.spv");
            break;
        }
    case RenderAPI::eD3D12:
        {
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::eNone, NIS_Sharpen_V0_H0_cs6, NIS_Sharpen_V0_H0_cs6_len, "NIS_Sharpen_V0_H0.cs6");
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::eLinear, NIS_Sharpen_V0_H1_cs6, NIS_Sharpen_V0_H1_cs6_len, "NIS_Sharpen_V0_H1.cs6");
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::ePQ, NIS_Sharpen_V0_H2_cs6, NIS_Sharpen_V0_H2_cs6_len, "NIS_Sharpen_V0_H2.cs6");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::eNone, NIS_Sharpen_V1_H0_cs6, NIS_Sharpen_V1_H0_cs6_len, "NIS_Sharpen_V1_H0.cs6");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::eLinear, NIS_Sharpen_V1_H1_cs6, NIS_Sharpen_V1_H1_cs6_len, "NIS_Sharpen_V1_H1.cs6");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::ePQ, NIS_Sharpen_V1_H2_cs6, NIS_Sharpen_V1_H2_cs6_len, "NIS_Sharpen_V1_H2.cs6");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::eNone, NIS_Scaler_V0_H0_cs6, NIS_Scaler_V0_H0_cs6_len, "NIS_Scaler_V0_H0.cs6");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::eLinear, NIS_Scaler_V0_H1_cs6, NIS_Scaler_V0_H1_cs6_len, "NIS_Scaler_V0_H1.cs6");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::ePQ, NIS_Scaler_V0_H2_cs6, NIS_Scaler_V0_H2_cs6_len, "NIS_Scaler_V0_H2.cs6");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::eNone, NIS_Scaler_V1_H0_cs6, NIS_Scaler_V1_H0_cs6_len, "NIS_Scaler_V1_H0.cs6");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::eLinear, NIS_Scaler_V1_H1_cs6, NIS_Scaler_V1_H1_cs6_len, "NIS_Scaler_V1_H1.cs6");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::ePQ, NIS_Scaler_V1_H2_cs6, NIS_Scaler_V1_H2_cs6_len, "NIS_Scaler_V1_H2.cs6");
            break;
        }
    default:
        {
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::eNone, NIS_Sharpen_V0_H0_cs, NIS_Sharpen_V0_H0_cs_len, "NIS_Sharpen_V0_H0.cs");
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::eLinear, NIS_Sharpen_V0_H1_cs, NIS_Sharpen_V0_H1_cs_len, "NIS_Sharpen_V0_H1.cs");
            ctx.addShaderPermutation(NISMode::eSharpen, 0, NISHDR::ePQ, NIS_Sharpen_V0_H2_cs, NIS_Sharpen_V0_H2_cs_len, "NIS_Sharpen_V0_H2.cs");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::eNone, NIS_Sharpen_V1_H0_cs, NIS_Sharpen_V1_H0_cs_len, "NIS_Sharpen_V1_H0.cs");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::eLinear, NIS_Sharpen_V1_H1_cs, NIS_Sharpen_V1_H1_cs_len, "NIS_Sharpen_V1_H1.cs");
            ctx.addShaderPermutation(NISMode::eSharpen, 1, NISHDR::ePQ, NIS_Sharpen_V1_H2_cs, NIS_Sharpen_V1_H2_cs_len, "NIS_Sharpen_V1_H2.cs");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::eNone, NIS_Scaler_V0_H0_cs, NIS_Scaler_V0_H0_cs_len, "NIS_Scaler_V0_H0.cs");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::eLinear, NIS_Scaler_V0_H1_cs, NIS_Scaler_V0_H1_cs_len, "NIS_Scaler_V0_H1.cs");
            ctx.addShaderPermutation(NISMode::eScaler, 0, NISHDR::ePQ, NIS_Scaler_V0_H2_cs, NIS_Scaler_V0_H2_cs_len, "NIS_Scaler_V0_H2.cs");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::eNone, NIS_Scaler_V1_H0_cs, NIS_Scaler_V1_H0_cs_len, "NIS_Scaler_V1_H0.cs");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::eLinear, NIS_Scaler_V1_H1_cs, NIS_Scaler_V1_H1_cs_len, "NIS_Scaler_V1_H1.cs");
            ctx.addShaderPermutation(NISMode::eScaler, 1, NISHDR::ePQ, NIS_Scaler_V1_H2_cs, NIS_Scaler_V1_H2_cs_len, "NIS_Scaler_V1_H2.cs");
            break;
        }
    }
  
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
            if (api::getContext()->parameters->get(sl::param::nis::kCurrentFrame, &lastFrame))
            {
                ctx.compute->getFinishedFrameIndex(frame);
                if (lastFrame < frame)
                {
                    ctx.uiStats.mode = "Mode: Off";
                    ctx.uiStats.viewport = ctx.uiStats.runtime = {};
                }
                if (ui->collapsingHeader(extra::format("sl.nis v{}", (v.toStr() + "." + GIT_LAST_COMMIT_SHORT)).c_str(), imgui::kTreeNodeFlagDefaultOpen))
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
    auto& ctx = (*nis::getContext());
    ctx.registerEvaluateCallbacks(kFeatureNIS, nullptr, nullptr);

    // it will shutdown it down automatically
    plugin::onShutdown(api::getContext());

    if (ctx.uploadScalerCoef)
    {
        ctx.compute->destroyResource(ctx.uploadScalerCoef);
    }
    if (ctx.uploadUsmCoef)
    {
        ctx.compute->destroyResource(ctx.uploadUsmCoef);
    }
    ctx.compute->destroyResource(ctx.scalerCoef);
    ctx.compute->destroyResource(ctx.usmCoef);

    for (auto& e : ctx.shaders)
    {
        CHI_VALIDATE(ctx.compute->destroyKernel(e.second));
    }
    ctx.compute = {};
}

sl::Result slNISSetOptions(const sl::ViewportHandle& viewport, const sl::NISOptions& options)
{
    auto v = viewport;
    v.next = (sl::BaseStructure*)&options;
    return slSetData(&v, nullptr);
}

sl::Result slNISGetState(const sl::ViewportHandle& viewport, sl::NISState& state)
{
    auto& ctx = (*nis::getContext());
    if (!ctx.compute) return Result::eErrorInvalidState;

    state.estimatedVRAMUsageInBytes = 0;

    chi::ResourceFootprint footprint{};
    ctx.compute->getResourceFootprint(ctx.uploadScalerCoef, footprint);
    state.estimatedVRAMUsageInBytes += footprint.totalBytes;
    ctx.compute->getResourceFootprint(ctx.uploadUsmCoef, footprint);
    state.estimatedVRAMUsageInBytes += footprint.totalBytes;
    ctx.compute->getResourceFootprint(ctx.scalerCoef, footprint);
    state.estimatedVRAMUsageInBytes += footprint.totalBytes;
    ctx.compute->getResourceFootprint(ctx.usmCoef, footprint);
    state.estimatedVRAMUsageInBytes += footprint.totalBytes;

    return Result::eOk;
}

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
    SL_EXPORT_FUNCTION(slNISSetOptions);
    SL_EXPORT_FUNCTION(slNISGetState);

    return nullptr;
}
}

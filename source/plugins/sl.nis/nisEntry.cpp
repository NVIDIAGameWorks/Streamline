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

#include <sstream>
#include <atomic>
#include <future>
#include <unordered_map>

#include "include/sl.h"
#include "include/sl_nis.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.param/parameters.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.nis/versions.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "external/json/include/nlohmann/json.hpp"
#include "_artifacts/gitVersion.h"

#include "./NIS/NIS_Config.h"
// Compute shader permutations
#include "./NIS_shaders.h"

using json = nlohmann::json;
namespace sl
{

struct NIS
{
    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};

    chi::Resource scalerCoef = {};
    chi::Resource usmCoef = {};
    chi::Resource uploadScalerCoef = {};
    chi::Resource uploadUsmCoef = {};

    NISConstants consts;
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

    bool addShaderPermutation(uint32_t scalerMode, uint32_t viewPorts, uint32_t HDRMode,
        uint8_t* byteCode, uint32_t len, const char* filename, const char* entryPoint = "main") {
        chi::Kernel kernel = {};
        CHI_CHECK_RF(compute->createKernel((void*) byteCode, len, filename, entryPoint, kernel));
        if (kernel)
            shaders[hash_combine(scalerMode, viewPorts, HDRMode)] = kernel;
        return true;
    }

    chi::Kernel getKernel(uint32_t scalerMode, uint32_t viewPorts, uint32_t HDRMode) {
        uint32_t key = hash_combine(scalerMode, viewPorts, HDRMode);
        if (shaders.find(key) == shaders.end())
            return {};
        return shaders[key];
    }
};

static NIS nis = {};

void slSetConstants(void* data, uint32_t frameIndex, uint32_t id)
{
    memcpy((void*)&nis.consts, data, sizeof(nis.consts));
}

void getNISConstants(const common::EventData& ev, NISConstants& consts)
{
    memcpy(&consts, (void*)&nis.consts, sizeof(consts));
}

bool initializeNIS(chi::CommandList cmdList, const common::EventData& data)
{
    if (!nis.scalerCoef && !nis.usmCoef)
    {
        auto texDesc = sl::chi::ResourceDescription(kFilterSize / 4, kPhaseCount, sl::chi::eFormatRGBA32F);
        CHI_CHECK_RF(nis.compute->createTexture2D(texDesc, nis.scalerCoef, "nisScalerCoef"));
        CHI_CHECK_RF(nis.compute->createTexture2D(texDesc, nis.usmCoef, "nisUSMCoef"));

        int rowPitchAlignment = 1; // D3D11
        chi::PlatformType platform;
        nis.compute->getPlatformType(platform);
        if (platform == chi::ePlatformTypeD3D12)
        {
            rowPitchAlignment = 256; // D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
        }

        const int rowPitch = kFilterSize * sizeof(float);
        const int deviceRowPitch = extra::align(kFilterSize * sizeof(float), rowPitchAlignment);
        const int totalBytes = deviceRowPitch * kPhaseCount;

        if (!nis.uploadScalerCoef)
        {
            chi::ResourceDescription bufferDesc(deviceRowPitch * kPhaseCount, 1, chi::eFormatINVALID, chi::HeapType::eHeapTypeUpload, chi::ResourceState::eUnknown);
            CHI_CHECK_RF(nis.compute->createBuffer(bufferDesc, nis.uploadScalerCoef, "sl.nis.uploadScalerCoef"));
            CHI_CHECK_RF(nis.compute->createBuffer(bufferDesc, nis.uploadUsmCoef, "sl.nis.uploadUsmCoef"));
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

        CHI_CHECK_RF(nis.compute->copyHostToDeviceTexture(cmdList, totalBytes, rowPitch, blobScale.data(), nis.scalerCoef, nis.uploadScalerCoef));
        CHI_CHECK_RF(nis.compute->copyHostToDeviceTexture(cmdList, totalBytes, rowPitch, blobUsm.data(), nis.usmCoef, nis.uploadUsmCoef));
    }
    return true;
}

void nisBeginEvaluation(chi::CommandList cmdList, const common::EventData& data)
{
    getNISConstants(data, nis.consts);
    initializeNIS(cmdList, data);
}

void nisEndEvaluation(chi::CommandList cmdList)
{
    if (nis.consts.mode != NISMode::eNISModeScaler && nis.consts.mode != NISMode::eNISModeSharpen) {
        SL_LOG_ERROR("Invalid NIS mode %d", nis.consts.mode);
        return;
    }
    if (nis.consts.hdrMode != NISHDR::eNISHDRNone && nis.consts.hdrMode != NISHDR::eNISHDRLinear && nis.consts.hdrMode != NISHDR::eNISHDRPQ) {
        SL_LOG_ERROR("Invalid NIS HDR mode %d", nis.consts.hdrMode);
        return;
    }

    chi::Resource colorIn{};
    chi::Resource colorOut{};
    sl::Extent inExtent{};
    sl::Extent outExtent{};
    uint32_t colorInNativeState{}, colorOutNativeState{};

    if (!getTaggedResource(eBufferTypeScalingInputColor, colorIn, 0, &inExtent, &colorInNativeState))
    {
        SL_LOG_ERROR("Failed to find resource '%s', please make sure to tag all NIS resources", colorIn);
        return;
    }

    if (!getTaggedResource(eBufferTypeScalingOutputColor, colorOut, 0, &outExtent, &colorOutNativeState))
    {
        SL_LOG_ERROR("Failed to find resource '%s', please make sure to tag all NIS resources", colorOut);
        return;
    }

    chi::ResourceDescription inDesc = {};
    CHI_VALIDATE(nis.compute->getResourceDescription(colorIn, inDesc));
    chi::ResourceDescription outDesc = {};
    CHI_VALIDATE(nis.compute->getResourceDescription(colorOut, outDesc));
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

    float sharpness = nis.consts.sharpness;
    NISHDRMode hdrMode = nis.consts.hdrMode == NISHDR::eNISHDRLinear ? NISHDRMode::Linear :
        nis.consts.hdrMode == NISHDR::eNISHDRPQ ? NISHDRMode::PQ : NISHDRMode::None;

    uint32_t viewPortsSupport = inExtent.width != inDesc.width || inExtent.height != inDesc.height ||
        outExtent.width != outDesc.width || outExtent.height != outDesc.height;

    chi::Kernel kernel = nis.getKernel(nis.consts.mode, viewPortsSupport, nis.consts.hdrMode);
    if (!kernel)
    {
        SL_LOG_ERROR("Failed to find NIS shader permutation mode: %d viewportSupport: %d, hdrMode: %d", nis.consts.mode, viewPortsSupport, nis.consts.hdrMode);
        return;
    }

    chi::ResourceState colorInState, colorOutState;
    if (colorInNativeState && colorOutNativeState)
    {
        CHI_VALIDATE(nis.compute->getResourceState(colorInNativeState, colorInState));
        CHI_VALIDATE(nis.compute->getResourceState(colorOutNativeState, colorOutState));
    }
    else
    {
        CHI_VALIDATE(nis.compute->getResourceState(colorIn, colorInState));
        CHI_VALIDATE(nis.compute->getResourceState(colorOut, colorOutState));
    }

    if (nis.consts.mode == NISMode::eNISModeScaler)
    {
        if (!NVScalerUpdateConfig(nis.config, sharpness,
            inExtent.left, inExtent.top, inExtent.width, inExtent.height, inDesc.width, inDesc.height,
            outExtent.left, outExtent.top, outExtent.width, outExtent.height, outDesc.width, outDesc.height,
            hdrMode))
        {
            SL_LOG_ERROR("NVScaler configuration error, scale out of bounds or textures width/height with zero value");
        }
    }
    else
    {
        // Sharpening only (no upscaling)
        if (!NVSharpenUpdateConfig(nis.config, sharpness,
            inExtent.left, inExtent.top, inExtent.width, inExtent.height, inDesc.width, inDesc.height,
            outExtent.left, outExtent.top, hdrMode))
        {
            SL_LOG_ERROR("NVSharpen configuration error, textures width/height width zero value");
        }
    }

    CHI_VALIDATE(nis.compute->beginPerfSection(cmdList, "sl.nis"));

    extra::ScopedTasks revTransitions;
    chi::ResourceTransition transitions[] =
    {
        {colorIn, chi::ResourceState::eTextureRead, colorInState},
        {colorOut, chi::ResourceState::eStorageRW, colorOutState}
    };
    nis.compute->transitionResources(cmdList, transitions, (uint32_t)countof(transitions), &revTransitions);

    CHI_VALIDATE(nis.compute->bindSharedState(cmdList));
    CHI_VALIDATE(nis.compute->bindKernel(kernel));
    CHI_VALIDATE(nis.compute->bindConsts(0, 0, &nis.config, sizeof(nis.config), 1));
    CHI_VALIDATE(nis.compute->bindSampler(1, 0, chi::eSamplerLinearClamp));
    CHI_VALIDATE(nis.compute->bindTexture(2, 0, colorIn));
    CHI_VALIDATE(nis.compute->bindRWTexture(3, 0, colorOut));
    if (nis.consts.mode == NISMode::eNISModeScaler)
    {
        CHI_VALIDATE(nis.compute->bindTexture(4, 1, nis.scalerCoef));
        CHI_VALIDATE(nis.compute->bindTexture(5, 2, nis.usmCoef));
    }
    CHI_VALIDATE(nis.compute->dispatch(UINT(std::ceil(outDesc.width / float(nis.blockWidth))), UINT(std::ceil(outDesc.height / float(nis.blockHeight))), 1));

    float ms = 0;
    CHI_VALIDATE(nis.compute->endPerfSection(cmdList, "sl.nis", ms));

    auto parameters = api::getContext()->parameters;

#ifndef SL_PRODUCTION
    // Report our stats, shown by sl.nis plugin
    static std::string s_stats;
    auto v = api::getContext()->pluginVersion;
    s_stats = extra::format("sl.nis {} - ({}x{})->({}x{}) - {}ms - {}", v.toStr(), inExtent.width, inExtent.height,outDesc.width, outDesc.height, ms, GIT_LAST_COMMIT);
    parameters->set(sl::param::nis::kStats, (void*)s_stats.c_str());
#endif

    // Tell others that we are actually active this frame
    uint32_t frame = 0;
    CHI_VALIDATE(nis.compute->getFinishedFrameIndex(frame));
    parameters->set(sl::param::nis::kCurrentFrame, frame + 1);
}

//! -------------------------------------------------------------------------------------------------
//! Required interface


//! Plugin startup
//!
//! Called only if plugin reports `supported : true` in the JSON config.
//! Note that supported flag can flip back to false if this method fails.
//!
//! @param jsonConfig Configuration provided by the loader (plugin manager or another plugin)
//! @param device Either ID3D12Device or struct VkDevices (see internal.h)
//! @param parameters Shared parameters from host and other plugins
bool slOnPluginStartup(const char* jsonConfig, void* device, sl::param::IParameters* parameters)
{
    SL_PLUGIN_COMMON_STARTUP();

   
    if (!getPointerParam(parameters, param::common::kComputeAPI, &nis.compute))
    {
        SL_LOG_ERROR("Can't find %s", param::common::kComputeAPI);
        return false;
    }

    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &nis.registerEvaluateCallbacks))
    {
        SL_LOG_ERROR("Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        return false;
    }
    nis.registerEvaluateCallbacks(eFeatureNIS, nisBeginEvaluation, nisEndEvaluation);

    chi::PlatformType platform;
    nis.compute->getPlatformType(platform);
    switch (platform)
    {
    case chi::ePlatformTypeVK:
        {
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRNone, NIS_Sharpen_V0_H0_spv, NIS_Sharpen_V0_H0_spv_len, "NIS_Sharpen_V0_H0.spv");
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRLinear, NIS_Sharpen_V0_H1_spv, NIS_Sharpen_V0_H1_spv_len, "NIS_Sharpen_V0_H1.spv");
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRPQ, NIS_Sharpen_V0_H2_spv, NIS_Sharpen_V0_H2_spv_len, "NIS_Sharpen_V0_H2.spv");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRNone, NIS_Sharpen_V1_H0_spv, NIS_Sharpen_V1_H0_spv_len, "NIS_Sharpen_V1_H0.spv");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRLinear, NIS_Sharpen_V1_H1_spv, NIS_Sharpen_V1_H1_spv_len, "NIS_Sharpen_V1_H1.spv");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRPQ, NIS_Sharpen_V1_H2_spv, NIS_Sharpen_V1_H2_spv_len, "NIS_Sharpen_V1_H2.spv");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRNone, NIS_Scaler_V0_H0_spv, NIS_Scaler_V0_H0_spv_len, "NIS_Scaler_V0_H0.spv");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRLinear, NIS_Scaler_V0_H1_spv, NIS_Scaler_V0_H1_spv_len, "NIS_Scaler_V0_H1.spv");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRPQ, NIS_Scaler_V0_H2_spv, NIS_Scaler_V0_H2_spv_len, "NIS_Scaler_V0_H2.spv");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRNone, NIS_Scaler_V1_H0_spv, NIS_Scaler_V1_H0_spv_len, "NIS_Scaler_V1_H0.spv");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRLinear, NIS_Scaler_V1_H1_spv, NIS_Scaler_V1_H1_spv_len, "NIS_Scaler_V1_H1.spv");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRPQ, NIS_Scaler_V1_H2_spv, NIS_Scaler_V1_H2_spv_len, "NIS_Scaler_V1_H2.spv");
            break;
        }
    case chi::ePlatformTypeD3D12:
        {
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRNone, NIS_Sharpen_V0_H0_cs6, NIS_Sharpen_V0_H0_cs6_len, "NIS_Sharpen_V0_H0.cs6");
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRLinear, NIS_Sharpen_V0_H1_cs6, NIS_Sharpen_V0_H1_cs6_len, "NIS_Sharpen_V0_H1.cs6");
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRPQ, NIS_Sharpen_V0_H2_cs6, NIS_Sharpen_V0_H2_cs6_len, "NIS_Sharpen_V0_H2.cs6");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRNone, NIS_Sharpen_V1_H0_cs6, NIS_Sharpen_V1_H0_cs6_len, "NIS_Sharpen_V1_H0.cs6");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRLinear, NIS_Sharpen_V1_H1_cs6, NIS_Sharpen_V1_H1_cs6_len, "NIS_Sharpen_V1_H1.cs6");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRPQ, NIS_Sharpen_V1_H2_cs6, NIS_Sharpen_V1_H2_cs6_len, "NIS_Sharpen_V1_H2.cs6");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRNone, NIS_Scaler_V0_H0_cs6, NIS_Scaler_V0_H0_cs6_len, "NIS_Scaler_V0_H0.cs6");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRLinear, NIS_Scaler_V0_H1_cs6, NIS_Scaler_V0_H1_cs6_len, "NIS_Scaler_V0_H1.cs6");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRPQ, NIS_Scaler_V0_H2_cs6, NIS_Scaler_V0_H2_cs6_len, "NIS_Scaler_V0_H2.cs6");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRNone, NIS_Scaler_V1_H0_cs6, NIS_Scaler_V1_H0_cs6_len, "NIS_Scaler_V1_H0.cs6");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRLinear, NIS_Scaler_V1_H1_cs6, NIS_Scaler_V1_H1_cs6_len, "NIS_Scaler_V1_H1.cs6");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRPQ, NIS_Scaler_V1_H2_cs6, NIS_Scaler_V1_H2_cs6_len, "NIS_Scaler_V1_H2.cs6");
            break;
        }
    default:
        {
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRNone, NIS_Sharpen_V0_H0_cs, NIS_Sharpen_V0_H0_cs_len, "NIS_Sharpen_V0_H0.cs");
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRLinear, NIS_Sharpen_V0_H1_cs, NIS_Sharpen_V0_H1_cs_len, "NIS_Sharpen_V0_H1.cs");
            nis.addShaderPermutation(eNISModeSharpen, 0, eNISHDRPQ, NIS_Sharpen_V0_H2_cs, NIS_Sharpen_V0_H2_cs_len, "NIS_Sharpen_V0_H2.cs");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRNone, NIS_Sharpen_V1_H0_cs, NIS_Sharpen_V1_H0_cs_len, "NIS_Sharpen_V1_H0.cs");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRLinear, NIS_Sharpen_V1_H1_cs, NIS_Sharpen_V1_H1_cs_len, "NIS_Sharpen_V1_H1.cs");
            nis.addShaderPermutation(eNISModeSharpen, 1, eNISHDRPQ, NIS_Sharpen_V1_H2_cs, NIS_Sharpen_V1_H2_cs_len, "NIS_Sharpen_V1_H2.cs");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRNone, NIS_Scaler_V0_H0_cs, NIS_Scaler_V0_H0_cs_len, "NIS_Scaler_V0_H0.cs");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRLinear, NIS_Scaler_V0_H1_cs, NIS_Scaler_V0_H1_cs_len, "NIS_Scaler_V0_H1.cs");
            nis.addShaderPermutation(eNISModeScaler, 0, eNISHDRPQ, NIS_Scaler_V0_H2_cs, NIS_Scaler_V0_H2_cs_len, "NIS_Scaler_V0_H2.cs");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRNone, NIS_Scaler_V1_H0_cs, NIS_Scaler_V1_H0_cs_len, "NIS_Scaler_V1_H0.cs");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRLinear, NIS_Scaler_V1_H1_cs, NIS_Scaler_V1_H1_cs_len, "NIS_Scaler_V1_H1.cs");
            nis.addShaderPermutation(eNISModeScaler, 1, eNISHDRPQ, NIS_Scaler_V1_H2_cs, NIS_Scaler_V1_H2_cs_len, "NIS_Scaler_V1_H2.cs");
            break;
        }
    }
  
    return true;
}

//! Plugin shutdown
//!
//! Called by loader when unloading the plugin
void slOnPluginShutdown()
{
    nis.registerEvaluateCallbacks(eFeatureNIS, nullptr, nullptr);

    // it will shutdown it down automatically
    plugin::onShutdown(api::getContext());

    if (nis.uploadScalerCoef)
    {
        nis.compute->destroyResource(nis.uploadScalerCoef);
    }
    if (nis.uploadUsmCoef)
    {
        nis.compute->destroyResource(nis.uploadUsmCoef);
    }
    nis.compute->destroyResource(nis.scalerCoef);
    nis.compute->destroyResource(nis.usmCoef);

    for (auto& e : nis.shaders)
    {
        CHI_VALIDATE(nis.compute->destroyKernel(e.second));
    }
}

const char *JSON = R"json(
{
    "id" : 2,
    "priority" : 1,
    "namespace" : "nis",
    "hooks" :
    [
    ]
}
)json";

uint32_t getSupportedAdapterMask()
{
    return ~0;
}

SL_PLUGIN_DEFINE("sl.nis", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON, getSupportedAdapterMask())

SL_EXPORT void *slGetPluginFunction(const char *functionName)
{
    // Forward declarations
    const char *slGetPluginJSONConfig();
    void slSetParameters(sl::param::IParameters *p);

    // Redirect to OTA if any
    SL_EXPORT_OTA;

    // Core API
    SL_EXPORT_FUNCTION(slSetParameters);
    SL_EXPORT_FUNCTION(slOnPluginShutdown);
    SL_EXPORT_FUNCTION(slOnPluginStartup);
    SL_EXPORT_FUNCTION(slGetPluginJSONConfig);
    SL_EXPORT_FUNCTION(slSetConstants);

    return nullptr;
}
}
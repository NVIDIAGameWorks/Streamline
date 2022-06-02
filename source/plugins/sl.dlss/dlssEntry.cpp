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

#include <dxgi1_6.h>
#include <sstream>
#include <atomic>
#include <future>

#include "include/sl.h"
#include "include/sl_dlss.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "external/json/include/nlohmann/json.hpp"
#include "external/nvapi/nvapi.h"

#include "source/platforms/sl.chi/d3d12.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "source/plugins/sl.dlss/versions.h"

#include "shaders/mvec_cs.h"
#include "shaders/mvec_spv.h"
#include "_artifacts/gitVersion.h"

#include "external/ngx/Include/nvsdk_ngx.h"
#include "external/ngx/Include/nvsdk_ngx_helpers.h"
#include "external/ngx/Include/nvsdk_ngx_defs.h"

using json = nlohmann::json;

namespace sl
{

using funNGXInit = NVSDK_NGX_Result(*)(unsigned long long InApplicationId, const wchar_t *InApplicationDataPath, ID3D12Device *InDevice, NVSDK_NGX_Version InSDKVersion);
using funNGXShutdown = NVSDK_NGX_Result(*)(void);
using funNGXCreate = NVSDK_NGX_Result(*)(ID3D12GraphicsCommandList *InCmdList, NVSDK_NGX_Feature InFeatureId, const NVSDK_NGX_Parameter *InParameters, NVSDK_NGX_Handle **OutHandle);
using funNGXRelease = NVSDK_NGX_Result(*)(NVSDK_NGX_Handle *InHandle);
using funNGXEval = NVSDK_NGX_Result(*)(ID3D12GraphicsCommandList *InCmdList, const NVSDK_NGX_Handle *InHandle, const NVSDK_NGX_Parameter *InParameters, PFN_NVSDK_NGX_ProgressCallback InCallback);

struct DLSSViewport
{
    uint32_t id = {};
    DLSSConstants consts{};
    DLSSSettings settings;
    NVSDK_NGX_Handle* handle = {};
    sl::chi::Resource mvec;
    float2 inputTexelSize;
#ifndef SL_PRODUCTION
    chi::Resource colorHistory[2] = {};
    chi::Resource interim = {};
    chi::Resource mask = {};
    chi::Resource dilatedMVec = {};
#endif
};

struct DLSS
{
    std::future<bool> initLambda;

    Constants* commonConsts{};

    common::NGXContext *ngxContext = {};
    sl::chi::ICompute *compute;
    sl::chi::Kernel mvecKernel;

    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};
    common::ViewportIdFrameData<4, false> constsPerViewport = { "dlss" };
    std::map<chi::Resource, chi::ResourceState> cachedStates = {};
    std::map<uint32_t, DLSSViewport> viewports = {};
    DLSSViewport* viewport = {};

    chi::PlatformType platform;

    void cacheState(chi::Resource res, uint32_t nativeState = 0)
    {
        //if (cachedStates.find(res) == cachedStates.end())
        {
            chi::ResourceState state;
            if(nativeState)
            {
                compute->getResourceState(nativeState, state);
            }
            else
            {
                compute->getResourceState(res, state);
            }
            cachedStates[res] = state;
        }
    }
};

DLSS s_dlss = {};

bool slGetSettings(const void* c, void* s);

bool slSetConstants(const void* data, uint32_t frameIndex, uint32_t id)
{
    auto consts = ( DLSSConstants*)data;

    s_dlss.constsPerViewport.set(frameIndex, id, consts);

    return true;
}

void dlssBeginEvent(chi::CommandList pCmdList, const common::EventData& data)
{
    auto parameters = api::getContext()->parameters;

    if (!common::getConsts(data, &s_dlss.commonConsts))
    {
        return;
    }

    auto it = s_dlss.viewports.find(data.id);
    if (it == s_dlss.viewports.end())
    {
        s_dlss.viewports[data.id] = {};
    }
    auto& viewport = s_dlss.viewports[data.id];
    viewport.id = data.id;

    DLSSConstants* consts{};
    if (!s_dlss.constsPerViewport.get(data, &consts))
    {
        return;
    }

    s_dlss.viewport = &viewport;
    bool modeOrSizeChanged = consts->mode != viewport.consts.mode || consts->outputWidth != viewport.consts.outputWidth || consts->outputHeight != viewport.consts.outputHeight;
    if(!viewport.handle || modeOrSizeChanged)
    {
        s_dlss.commonConsts->reset = Boolean::eTrue;
        s_dlss.cachedStates.clear();
        viewport.consts = *consts;  // mandatory
        slGetSettings(consts, &viewport.settings);

        if(s_dlss.ngxContext)
        {
            if (viewport.handle)
            {
                SL_LOG_INFO("Detected resize, recreating DLSS feature");
                if (!s_dlss.ngxContext->releaseFeature(viewport.handle))
                {
                    SL_LOG_ERROR("Failed to destroy DLSS feature");
                }
                viewport.handle = {};
                s_dlss.compute->destroyResource(viewport.mvec);
            }

            {
                int dlssCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
                if (consts->colorBuffersHDR == Boolean::eTrue)
                {
                    dlssCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
                }
                if (consts->sharpness > 0.0f)
                {
                    dlssCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_DoSharpening;
                }
                if (s_dlss.commonConsts->depthInverted == Boolean::eTrue)
                {
                    dlssCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
                }
                if (s_dlss.commonConsts->motionVectorsJittered == Boolean::eTrue)
                {
                    dlssCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_MVJittered;
                }

                // Optional
                chi::Resource exposure = {};
                getTaggedResource(eBufferTypeExposure, exposure, s_dlss.viewport->id);

                if (!exposure)
                {
                    dlssCreateFlags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
                }

                // Mandatory
                chi::Resource colorIn{};
                chi::Resource colorOut{};
                chi::Resource depth{};
                chi::Resource mvec{};

                sl::Extent colorInExt{};
                sl::Extent colorOutExt{};
                sl::Extent mvecExt{};
                sl::Extent depthExt{};

                getTaggedResource(eBufferTypeScalingInputColor, colorIn, s_dlss.viewport->id, &colorInExt);
                getTaggedResource(eBufferTypeScalingOutputColor, colorOut, s_dlss.viewport->id, &colorOutExt);
                getTaggedResource(eBufferTypeDepth, depth, s_dlss.viewport->id, &depthExt);
                getTaggedResource(eBufferTypeMVec, mvec, s_dlss.viewport->id, &mvecExt);

                // We will log the extent information for easier debugging, if not specified assuming the full buffer size
                chi::ResourceDescription desc;
                if (!colorInExt)
                {
                    s_dlss.compute->getResourceDescription(colorIn, desc);
                    colorInExt = { 0,0,desc.width,desc.height };
                }
                if (!colorOutExt)
                {
                    s_dlss.compute->getResourceDescription(colorOut, desc);
                    colorOutExt = { 0,0,desc.width,desc.height };
                }
                if (!mvecExt)
                {
                    s_dlss.compute->getResourceDescription(mvec, desc);
                    mvecExt = { 0,0,desc.width,desc.height };
                }
                if (!depthExt)
                {
                    s_dlss.compute->getResourceDescription(depth, desc);
                    depthExt = { 0,0,desc.width,desc.height };
                }

                if (mvecExt.width > colorInExt.width || mvecExt.height > colorInExt.height)
                {
                    SL_LOG_INFO("Detected high resolution mvec for DLSS");
                    dlssCreateFlags &= ~NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
                }

                NVSDK_NGX_PerfQuality_Value perfQualityValue = (NVSDK_NGX_PerfQuality_Value)(viewport.consts.mode - 1);

                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_CreationNodeMask, 1);
                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_VisibilityNodeMask, 1);
                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Width, viewport.settings.optimalRenderWidth);
                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Height, viewport.settings.optimalRenderHeight);
                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_OutWidth, viewport.consts.outputWidth);
                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_OutHeight, viewport.consts.outputHeight);
                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_PerfQualityValue, perfQualityValue);
                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Feature_Create_Flags, dlssCreateFlags);
                s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_FreeMemOnReleaseFeature, 1);
                if (s_dlss.ngxContext->createFeature(pCmdList, NVSDK_NGX_Feature_SuperSampling, &viewport.handle))
                {
                    SL_LOG_INFO("Created DLSS feature (%u,%u)(optimal) -> (%u,%u) for viewport %u", viewport.settings.optimalRenderWidth, viewport.settings.optimalRenderHeight, viewport.consts.outputWidth, viewport.consts.outputHeight, data.id);
                    // Log the extent information for easier debugging
                    SL_LOG_INFO("DLSS color_in extents (%u,%u,%u,%u)", colorInExt.left, colorInExt.top, colorInExt.width, colorInExt.height);
                    SL_LOG_INFO("DLSS color_out extents (%u,%u,%u,%u)", colorOutExt.left, colorOutExt.top, colorOutExt.width, colorOutExt.height);
                    SL_LOG_INFO("DLSS depth extents (%u,%u,%u,%u)", depthExt.left, depthExt.top, depthExt.width, depthExt.height);
                    SL_LOG_INFO("DLSS mvec extents (%u,%u,%u,%u)", mvecExt.left, mvecExt.top, mvecExt.width, mvecExt.height);
                }
            }
        }
    }
}

void dlssEndEvent(chi::CommandList pCmdList)
{
    if(s_dlss.viewport)
    {
        // Run DLSS, we skipped dispatch for in-engine TAAU
        auto parameters = api::getContext()->parameters;
        Constants* consts = s_dlss.commonConsts;
        
        {
            chi::Resource colorIn{};
            chi::Resource colorOut{};
            chi::Resource depth{};
            chi::Resource mvec{};
            chi::Resource transparency{};
            chi::Resource exposure{};
            chi::Resource animTexture{};
            chi::Resource mvecReflections{};
            chi::Resource rayTraceDist{};
            chi::Resource currentColorBias{};
            chi::Resource particleMask{};

            sl::Extent colorInExt{};
            sl::Extent colorOutExt{};
            sl::Extent mvecExt{};
            sl::Extent depthExt{};
            sl::Extent transparencyExt{};
            sl::Extent currentColorBiasExt{};
            
            uint32_t colorInState{};
            uint32_t colorOutState{};
            uint32_t depthState{};
            uint32_t mvecState{};
            uint32_t transparencyState{};
            uint32_t exposureState{};
            uint32_t currentColorBiasState{};

            // Mandatory
            getTaggedResource(eBufferTypeScalingInputColor, colorIn, s_dlss.viewport->id, &colorInExt, &colorInState);
            getTaggedResource(eBufferTypeScalingOutputColor, colorOut, s_dlss.viewport->id, &colorOutExt, &colorOutState);
            getTaggedResource(eBufferTypeDepth, depth, s_dlss.viewport->id, &depthExt, &depthState);
            getTaggedResource(eBufferTypeMVec, mvec, s_dlss.viewport->id, &mvecExt, &mvecState);

            // Optional
            getTaggedResource(eBufferTypeTransparencyHint, transparency, s_dlss.viewport->id, &transparencyExt);
            getTaggedResource(eBufferTypeExposure, exposure, s_dlss.viewport->id, nullptr, &exposureState);
            getTaggedResource(eBufferTypeAnimatedTextureHint, animTexture, s_dlss.viewport->id);
            getTaggedResource(eBufferTypeReflectionMotionVectors, mvecReflections, s_dlss.viewport->id);
            getTaggedResource(eBufferTypeRaytracingDistance, rayTraceDist, s_dlss.viewport->id);
            getTaggedResource(eBufferTypeBiasCurrentColorHint, currentColorBias, s_dlss.viewport->id, &currentColorBiasExt, &currentColorBiasState);
            getTaggedResource(eBufferTypeParticleHint, particleMask, s_dlss.viewport->id);

            if (!depth || !mvec || !colorIn || !colorOut)
            {
                SL_LOG_ERROR("Missing DLSS inputs");
                return;
            }


            /*chi::ResourceDescription desc[4];
            s_dlss.compute->getResourceDescription(depth, desc[0]);
            s_dlss.compute->getResourceDescription(mvec, desc[1]);
            s_dlss.compute->getResourceDescription(colorIn, desc[2]);
            s_dlss.compute->getResourceDescription(colorOut, desc[3]);*/

            // Depending if camera motion is provided or not we can use input directly or not
            auto mvecIn = mvec;
            
            parameters->set(sl::param::dlss::kMVecBuffer, mvecIn);

            CHI_VALIDATE(s_dlss.compute->beginPerfSection(pCmdList, "sl.dlss"));

            {
                s_dlss.cacheState(colorIn, colorInState);
                s_dlss.cacheState(colorOut, colorOutState);
                s_dlss.cacheState(depth, depthState);
                s_dlss.cacheState(mvecIn, mvecState);
                s_dlss.cacheState(transparency);
                s_dlss.cacheState(exposure, exposureState);
                s_dlss.cacheState(animTexture);
                s_dlss.cacheState(mvecReflections);
                s_dlss.cacheState(rayTraceDist);
                s_dlss.cacheState(currentColorBias, currentColorBiasState);
                s_dlss.cacheState(particleMask);

                unsigned int renderWidth = colorInExt.width;
                unsigned int renderHeight = colorInExt.height;
                if (renderWidth == 0 || renderHeight == 0)
                {
                    chi::ResourceDescription desc;
                    s_dlss.compute->getResourceDescription(colorIn, desc);
                    renderWidth = desc.width;
                    renderHeight = desc.height;
                }

                bool mvecPixelSpace = false;

                if (consts->cameraMotionIncluded == Boolean::eFalse)
                {
                    // Need to compute camera motion ourselves

                    // TODO - this is not optimal in the case of dynamic resizing, but cameraMotionIncluded should be true for most existing DLSS titles.
                    // To optimize this, we would want to realloc only when the size is larger than we've seen before, and use subrects
                    if (s_dlss.viewport->mvec)
                    {
                        chi::ResourceDescription desc;
                        s_dlss.compute->getResourceDescription(s_dlss.viewport->mvec, desc);
                        if (desc.width != renderWidth || desc.height != renderHeight)
                        {
                            s_dlss.compute->destroyResource(s_dlss.viewport->mvec);
                            s_dlss.viewport->mvec = nullptr;
                        }
                    }
                    if (!s_dlss.viewport->mvec)
                    {
                        sl::chi::ResourceDescription desc(renderWidth, renderHeight, sl::chi::eFormatRG16F,sl::chi::HeapType::eHeapTypeDefault, sl::chi::ResourceState::eTextureRead);
                        CHI_VALIDATE(s_dlss.compute->createTexture2D(desc, s_dlss.viewport->mvec, "sl.dlss.mvec"));
                        s_dlss.cacheState(s_dlss.viewport->mvec);
                    }

                    mvecIn = s_dlss.viewport->mvec;

                    // In this case we will always convert to pixel space
                    mvecPixelSpace = true;

                    // No camera motion, need to compute ourselves and store in s_dlss.mvec
                    extra::ScopedTasks revTransitions;
                    chi::ResourceTransition transitions[] =
                    {
                        {mvecIn, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead}
                    };
                    s_dlss.compute->transitionResources(pCmdList, transitions, (uint32_t)countof(transitions), &revTransitions);
                    CHI_VALIDATE(s_dlss.compute->bindSharedState(pCmdList));

                    struct MVecParamStruct
                    {
                        sl::float4x4 clipToPrevClip;
                        sl::float4 texSize;
                        sl::float2 mvecScale;
                        uint32_t debug;
                    };
                    MVecParamStruct cb;
                    cb.texSize.x = (float)renderWidth;
                    cb.texSize.y = (float)renderHeight;
                    cb.texSize.z = 1.0f / renderWidth;
                    cb.texSize.w = 1.0f / renderHeight;
                    cb.mvecScale = { consts->mvecScale.x, consts->mvecScale.y }; // scaling everything to -1,1 range then to -width,width
                    cb.debug = 0;
                    memcpy(&cb.clipToPrevClip, &consts->clipToPrevClip, sizeof(float) * 16);
                    CHI_VALIDATE(s_dlss.compute->bindKernel(s_dlss.mvecKernel));
                    CHI_VALIDATE(s_dlss.compute->bindTexture(0, 0, mvec));
                    CHI_VALIDATE(s_dlss.compute->bindTexture(1, 1, depth));
                    CHI_VALIDATE(s_dlss.compute->bindRWTexture(2, 0, s_dlss.viewport->mvec));
                    CHI_VALIDATE(s_dlss.compute->bindConsts(3, 0, &cb, sizeof(MVecParamStruct), 3));
                    uint32_t grid[] = { (renderWidth + 16 - 1) / 16, (renderHeight + 16 - 1) / 16, 1 };
                    CHI_VALIDATE(s_dlss.compute->dispatch(grid[0], grid[1], grid[2]));
                }

                static int s_mode = 1;
                static float s_clampingFactor = 4.0f;
                if (extra::keyboard::getInterface()->wasKeyPressed("depth_up"))
                {
                    s_mode = s_mode + 1;
                }
                if (extra::keyboard::getInterface()->wasKeyPressed("depth_down"))
                {
                    s_mode = std::max(0, s_mode - 1);
                }
                if (extra::keyboard::getInterface()->wasKeyPressed("clamp_up"))
                {
                    s_clampingFactor = s_clampingFactor + 1.0f;
                }
                if (extra::keyboard::getInterface()->wasKeyPressed("clamp_down"))
                {
                    s_clampingFactor = std::max(0.0f, s_clampingFactor - 1.0f);
                }

                if (s_dlss.ngxContext)
                {
                    // DLSS
                    extra::ScopedTasks revTransitions;
                    chi::ResourceTransition transitions[] =
                    {
                        {mvecIn, chi::ResourceState::eTextureRead, s_dlss.cachedStates[mvecIn]},
                        {depth, chi::ResourceState::eTextureRead, s_dlss.cachedStates[depth]},
                        {colorIn, chi::ResourceState::eTextureRead, s_dlss.cachedStates[colorIn]},
                        {colorOut, chi::ResourceState::eStorageRW, s_dlss.cachedStates[colorOut]},
                        {transparency, chi::ResourceState::eTextureRead, s_dlss.cachedStates[transparency]},
                        {exposure, chi::ResourceState::eTextureRead, s_dlss.cachedStates[exposure]},
                        {rayTraceDist, chi::ResourceState::eTextureRead, s_dlss.cachedStates[rayTraceDist]},
                        {animTexture, chi::ResourceState::eTextureRead, s_dlss.cachedStates[animTexture]},
                        {mvecReflections, chi::ResourceState::eTextureRead, s_dlss.cachedStates[mvecReflections]},
                        {particleMask, chi::ResourceState::eTextureRead, s_dlss.cachedStates[particleMask]},
                        {currentColorBias, chi::ResourceState::eTextureRead, s_dlss.cachedStates[currentColorBias]},
                    };
                    s_dlss.compute->transitionResources(pCmdList, transitions, (uint32_t)countof(transitions), &revTransitions);

                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Reset, consts->reset == Boolean::eTrue);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_MV_Scale_X, (mvecPixelSpace ? 1.0f : (float)(consts->mvecScale.x * renderWidth)));
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_MV_Scale_Y, (mvecPixelSpace ? 1.0f : (float)(consts->mvecScale.y * renderHeight)));
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Jitter_Offset_X, consts->jitterOffset.x);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Jitter_Offset_Y, consts->jitterOffset.y);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Sharpness, s_dlss.viewport->consts.sharpness);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Pre_Exposure, s_dlss.viewport->consts.preExposure);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Exposure_Scale, s_dlss.viewport->consts.exposureScale);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Sharpness, s_dlss.viewport->consts.sharpness);

                    {
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Color, colorIn);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_MotionVectors, mvecIn);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Output, colorOut);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_TransparencyMask, transparency);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_ExposureTexture, exposure);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Depth, depth);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_Mask, currentColorBias);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_AnimatedTextureMask, animTexture);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_RayTracingHitDistance, rayTraceDist);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_MotionVectorsReflection, mvecReflections);
                        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_IsParticleMask, particleMask);
                    }

                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_X, colorInExt.left);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Color_Subrect_Base_Y, colorInExt.top);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_X, depthExt.left);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Depth_Subrect_Base_Y, depthExt.top);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_X, mvecExt.left);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_MV_SubrectBase_Y, mvecExt.top);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_X, transparencyExt.left);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Translucency_SubrectBase_Y, transparencyExt.top);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_X, currentColorBiasExt.left);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Input_Bias_Current_Color_SubrectBase_Y, currentColorBiasExt.top);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_X, colorOutExt.left);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Output_Subrect_Base_Y, colorOutExt.top);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Width, renderWidth);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Render_Subrect_Dimensions_Height, renderHeight);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Indicator_Invert_X_Axis, s_dlss.viewport->consts.indicatorInvertAxisX);
                    s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_DLSS_Indicator_Invert_Y_Axis, s_dlss.viewport->consts.indicatorInvertAxisY);

                    s_dlss.ngxContext->evaluateFeature(pCmdList, s_dlss.viewport->handle);
                }

                float ms = 0;
                CHI_VALIDATE(s_dlss.compute->endPerfSection(pCmdList, "sl.dlss", ms));

#ifndef SL_PRODUCTION
                static std::string s_stats;
                auto v = api::getContext()->pluginVersion;
                
                s_stats = extra::format("sl.dlss {} - DLSS - ({}x{})->({}x{}) - {}ms - {}", v.toStr(),
                    renderWidth, renderHeight, s_dlss.viewport->consts.outputWidth, s_dlss.viewport->consts.outputHeight, ms, GIT_LAST_COMMIT);
                parameters->set(sl::param::dlss::kStats, (void*)s_stats.c_str());

                {
                    sl::chi::ResourceArea area{ colorOut, {0, 0}, {s_dlss.viewport->consts.outputWidth, s_dlss.viewport->consts.outputHeight} };
                    sl::float4 txtClr = { 1,1,1,0 };
                    int offset = area.dimensions.height - 16;
                    s_dlss.compute->renderText(pCmdList, area.dimensions.width / 2 - (uint32_t)s_stats.size() * 4, offset, s_stats.c_str(), area, txtClr);
                }
#endif

                uint32_t frame = 0;
                s_dlss.compute->getFinishedFrameIndex(frame);
                parameters->set(sl::param::dlss::kCurrentFrame, frame + 1);
            }
        }
    }
}

//! -------------------------------------------------------------------------------------------------
//! Required interface

bool slGetSettings(const void *c, void *s)
{
    auto parameters = api::getContext()->parameters;
    getPointerParam(parameters, param::global::kNGXContext, &s_dlss.ngxContext);
    if (!s_dlss.ngxContext)
    {
        SL_LOG_ERROR("NGX context is missing, please make sure DLSS feature is enabled and supported on the platform");
        return false;
    }
    auto settings = (DLSSSettings*)s;
    DLSSSettings1* settings1{};
    auto consts = (const DLSSConstants*)c;

    if (settings->ext)
    {
        settings1 = (DLSSSettings1*)settings->ext;
    }

    // Settings
    {
        void* callback = NULL;
        s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_DLSSOptimalSettingsCallback, &callback);
        if (!callback)
        {
            SL_LOG_ERROR("DLSS 'getOptimalSettings' callback is missing, please make sure DLSS feature is up to date");
            return false;
        }

        // These are selections made by user in UI
        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Width, consts->outputWidth);
        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_Height, consts->outputHeight);
        // SL DLSS modes start with 'off' so subtract one, the rest is mapped 1:1
        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_PerfQualityValue, (NVSDK_NGX_PerfQuality_Value)(consts->mode - 1));
        s_dlss.ngxContext->params->Set(NVSDK_NGX_Parameter_RTXValue, false);

        NVSDK_NGX_Result res = NVSDK_NGX_Result_Success;
        auto getOptimalSettings = (PFN_NVSDK_NGX_DLSS_GetOptimalSettingsCallback)callback;
        res = getOptimalSettings(s_dlss.ngxContext->params);
        if (NVSDK_NGX_FAILED(res))
        {
            SL_LOG_ERROR("DLSS 'getOptimalSettings' callback failed - error %u", res);
            return false;
        }
        s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_OutWidth, &settings->optimalRenderWidth);
        s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_OutHeight, &settings->optimalRenderHeight);
        s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_Sharpness, &settings->optimalSharpness);
        if (settings1)
        {
            s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width, &settings1->renderWidthMax);
            s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height, &settings1->renderHeightMax);
            s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width, &settings1->renderWidthMin);
            s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height, &settings1->renderHeightMin);
        }
    }
    
    // Stats
    if (settings1)
    {
        void* callback = NULL;
        s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_DLSSGetStatsCallback, &callback);
        if (!callback)
        {
            SL_LOG_ERROR("DLSS 'getStats' callback is missing, please make sure DLSS feature is up to date");
            return false;
        }
        auto getStats = (PFN_NVSDK_NGX_DLSS_GetStatsCallback)callback;
        auto res = getStats(s_dlss.ngxContext->params);
        if (NVSDK_NGX_FAILED(res))
        {
            SL_LOG_ERROR("DLSS 'getStats' callback failed - error %u", res);
            return false;
        }
        s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_SizeInBytes, &settings1->allocatedBytes);
    }
    return true;
}

bool slAllocateResources(sl::CommandBuffer* cmdBuffer, Feature feature, uint32_t id)
{
    common::EventData data{ id, 0 };
    dlssBeginEvent(cmdBuffer, data);
    auto it = s_dlss.viewports.find(id);
    return it != s_dlss.viewports.end() && (*it).second.handle != nullptr;
}

bool slFreeResources(Feature feature, uint32_t id)
{
    auto it = s_dlss.viewports.find(id);
    if (it != s_dlss.viewports.end())
    {
        auto& instance = (*it).second;
        s_dlss.ngxContext->releaseFeature(instance.handle);
        CHI_VALIDATE(s_dlss.compute->destroyResource(instance.mvec));
        s_dlss.viewports.erase(it);
        return true;
    }
    return false;
}

//! Plugin startup
//!
//! Called only if plugin reports `supported : true` in the JSON config.
//! Note that supported flag can flip back to false if this method fails.
//!
//! @param jsonConfig Configuration provided by the loader (plugin manager or another plugin)
//! @param device Either ID3D12Device or struct VkDevices (see internal.h)
//! @param parameters Shared parameters from host and other plugins
bool slOnPluginStartup(const char *jsonConfig, void *device, sl::param::IParameters *parameters)
{
    SL_PLUGIN_COMMON_STARTUP();

    getPointerParam(parameters, param::global::kNGXContext, &s_dlss.ngxContext);

    if (s_dlss.ngxContext)
    {
        int needsUpdatedDriver = 0;
        unsigned int minDriverVersionMajor = 0;
        unsigned int minDriverVersionMinor = 0;
        CHECK_NGX_RETURN_ON_ERROR(s_dlss.ngxContext->params->Get(NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver));
        if (needsUpdatedDriver)
        {
            SL_LOG_ERROR("DLSS cannot be loaded due to an outdated driver. Minimum version required : %u.%u", minDriverVersionMajor, minDriverVersionMinor);
            return false;
        }
    }
    else
    {
        SL_LOG_ERROR("Missing NGX context - DLSS cannot run");
        return false;
    }

    // Register our event callbacks
    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &s_dlss.registerEvaluateCallbacks))
    {
        SL_LOG_ERROR("Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        return false;
    }
    s_dlss.registerEvaluateCallbacks(eFeatureDLSS, dlssBeginEvent, dlssEndEvent);

    extra::keyboard::getInterface()->registerKey("depth_up", extra::keyboard::VirtKey(VK_OEM_6, true, true));
    extra::keyboard::getInterface()->registerKey("depth_down", extra::keyboard::VirtKey(VK_OEM_4, true, true));
    extra::keyboard::getInterface()->registerKey("clamp_down", extra::keyboard::VirtKey(VK_OEM_COMMA, true, true));
    extra::keyboard::getInterface()->registerKey("clamp_up", extra::keyboard::VirtKey(VK_OEM_PERIOD, true, true));

    param::getPointerParam(parameters, sl::param::common::kComputeAPI, &s_dlss.compute);

    {
        json& config = *(json*)api::getContext()->loaderConfig;
        int appId = 0;
        config.at("appId").get_to(appId);
    }
    
    s_dlss.compute->getPlatformType(s_dlss.platform);
    if (s_dlss.platform == chi::ePlatformTypeVK)
    {
        CHI_CHECK_RF(s_dlss.compute->createKernel((void*)mvec_spv, mvec_spv_len, "mvec.cs", "main", s_dlss.mvecKernel));
    }
    else
    {
        CHI_CHECK_RF(s_dlss.compute->createKernel((void*)mvec_cs, mvec_cs_len, "mvec.cs", "main", s_dlss.mvecKernel));
    }

    return true;
}

//! Plugin shutdown
//!
//! Called by loader when unloading the plugin
void slOnPluginShutdown()
{
    s_dlss.registerEvaluateCallbacks(eFeatureDLSS, nullptr, nullptr);

    // Common shutdown, if we loaded an OTA
    // it will shutdown it down automatically
    plugin::onShutdown(api::getContext());

    for(auto v : s_dlss.viewports)
    {
        s_dlss.ngxContext->releaseFeature(v.second.handle);
        CHI_VALIDATE(s_dlss.compute->destroyResource(v.second.mvec));
    }
    CHI_VALIDATE(s_dlss.compute->destroyKernel(s_dlss.mvecKernel));
}

const char *JSON = R"json(
{
    "id" : 0,
    "priority" : 1,
    "namespace" : "dlss",
    "hooks" :
    [
    ]
}
)json";

uint32_t getSupportedAdapterMask()
{
    // Check if plugin is supported or not on this platform and set the flag accordingly
    common::GPUArch* info = {};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kGPUInfo, &info);
    uint32_t adapterMask = 0;
    if (info)
    {
        for (uint32_t i = 0; i < info->gpuCount; i++)
        {
            auto turingOrBetter = info->architecture[i] >= NV_GPU_ARCHITECTURE_ID::NV_GPU_ARCHITECTURE_TU100;
            if (turingOrBetter)
            {
                adapterMask |= 1 << i;
            }
            else
            {
                SL_LOG_WARN("DLSS on GPU %u architecture 0x%x with driver %u.%u NOT supported.", i, info->architecture[i], info->driverVersionMajor, info->driverVersionMinor);
            }
        }
    }
    if (adapterMask)
    {
        api::getContext()->parameters->set(param::global::kNeedNGX, true);
    }

    return adapterMask;
}

SL_PLUGIN_DEFINE("sl.dlss", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON, getSupportedAdapterMask())

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
    SL_EXPORT_FUNCTION(slGetSettings);
    SL_EXPORT_FUNCTION(slAllocateResources);
    SL_EXPORT_FUNCTION(slFreeResources);

    return nullptr;
}

}
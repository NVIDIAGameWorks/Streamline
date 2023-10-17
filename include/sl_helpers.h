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

#pragma once

#include <string.h>

#define FEATURE_SPECIFIC_BUFFER_TYPE_ID(feature, number) feature << 16 | number

#include "sl.h"
#include "sl_consts.h"
#include "sl_reflex.h"
#include "sl_dlss.h"
#include "sl_nis.h"
#include "sl_nrd.h"

namespace sl
{

inline float4x4 transpose(const float4x4& m)
{
    float4x4 r;
    r[0] = { m[0].x, m[1].x, m[2].x, m[3].x };
    r[1] = { m[0].y, m[1].y, m[2].y, m[3].y };
    r[2] = { m[0].z, m[1].z, m[2].z, m[3].z };
    r[3] = { m[0].w, m[1].w, m[2].w, m[3].w };
    return r;
};

#define SL_CASE_STR(a) case a : return #a;

inline const char* getResultAsStr(Result v)
{
    switch (v)
    {
        SL_CASE_STR(Result::eOk);
        SL_CASE_STR(Result::eErrorIO);
        SL_CASE_STR(Result::eErrorDriverOutOfDate);
        SL_CASE_STR(Result::eErrorOSOutOfDate);
        SL_CASE_STR(Result::eErrorOSDisabledHWS);
        SL_CASE_STR(Result::eErrorDeviceNotCreated);
        SL_CASE_STR(Result::eErrorNoSupportedAdapterFound);
        SL_CASE_STR(Result::eErrorAdapterNotSupported);
        SL_CASE_STR(Result::eErrorNoPlugins);
        SL_CASE_STR(Result::eErrorVulkanAPI);
        SL_CASE_STR(Result::eErrorDXGIAPI);
        SL_CASE_STR(Result::eErrorD3DAPI);
        SL_CASE_STR(Result::eErrorNRDAPI);
        SL_CASE_STR(Result::eErrorNVAPI);
        SL_CASE_STR(Result::eErrorReflexAPI);
        SL_CASE_STR(Result::eErrorNGXFailed);
        SL_CASE_STR(Result::eErrorJSONParsing);
        SL_CASE_STR(Result::eErrorMissingProxy);
        SL_CASE_STR(Result::eErrorMissingResourceState);
        SL_CASE_STR(Result::eErrorInvalidIntegration);
        SL_CASE_STR(Result::eErrorMissingInputParameter);
        SL_CASE_STR(Result::eErrorNotInitialized);
        SL_CASE_STR(Result::eErrorComputeFailed);
        SL_CASE_STR(Result::eErrorInitNotCalled);
        SL_CASE_STR(Result::eErrorExceptionHandler);
        SL_CASE_STR(Result::eErrorInvalidParameter);
        SL_CASE_STR(Result::eErrorMissingConstants);
        SL_CASE_STR(Result::eErrorDuplicatedConstants);
        SL_CASE_STR(Result::eErrorMissingOrInvalidAPI);
        SL_CASE_STR(Result::eErrorCommonConstantsMissing);
        SL_CASE_STR(Result::eErrorUnsupportedInterface);
        SL_CASE_STR(Result::eErrorFeatureMissing);
        SL_CASE_STR(Result::eErrorFeatureNotSupported);
        SL_CASE_STR(Result::eErrorFeatureMissingHooks);
        SL_CASE_STR(Result::eErrorFeatureFailedToLoad);
        SL_CASE_STR(Result::eErrorFeatureWrongPriority);
        SL_CASE_STR(Result::eErrorFeatureMissingDependency);
        SL_CASE_STR(Result::eErrorFeatureManagerInvalidState);
        SL_CASE_STR(Result::eErrorInvalidState);
    };
    return "Unknown";
}

inline const char* getNRDMethodAsStr(NRDMethods v)
{
    switch (v)
    {
        SL_CASE_STR(NRDMethods::eOff);
        SL_CASE_STR(NRDMethods::eReblurDiffuse);
        SL_CASE_STR(NRDMethods::eReblurDiffuseOcclusion);
        SL_CASE_STR(NRDMethods::eReblurSpecular);
        SL_CASE_STR(NRDMethods::eReblurSpecularOcclusion);
        SL_CASE_STR(NRDMethods::eReblurDiffuseSpecular);
        SL_CASE_STR(NRDMethods::eReblurDiffuseSpecularOcclusion);
        SL_CASE_STR(NRDMethods::eReblurDiffuseDirectionalOcclusion);
        SL_CASE_STR(NRDMethods::eSigmaShadow);
        SL_CASE_STR(NRDMethods::eSigmaShadowTranslucency);
        SL_CASE_STR(NRDMethods::eRelaxDiffuse);
        SL_CASE_STR(NRDMethods::eRelaxSpecular);
        SL_CASE_STR(NRDMethods::eRelaxDiffuseSpecular);
    };
    return "Unknown";
}

inline const char* getNISModeAsStr(NISMode v)
{
    switch (v)
    {
        SL_CASE_STR(NISMode::eOff);
        SL_CASE_STR(NISMode::eScaler);
        SL_CASE_STR(NISMode::eSharpen);
    };
    return "Unknown";
}

inline const char* getNISHDRAsStr(NISHDR v)
{
    switch (v)
    {
        SL_CASE_STR(NISHDR::eNone);
        SL_CASE_STR(NISHDR::eLinear);
        SL_CASE_STR(NISHDR::ePQ);
    };
    return "Unknown";
}

inline const char* getReflexModeAsStr(ReflexMode mode)
{
    switch (mode)
    {
        SL_CASE_STR(ReflexMode::eOff);
        SL_CASE_STR(ReflexMode::eLowLatency);
        SL_CASE_STR(ReflexMode::eLowLatencyWithBoost);
    };
    return "Unknown";
}

inline const char* getReflexMarkerAsStr(ReflexMarker marker)
{
    switch (marker)
    {
        SL_CASE_STR(ReflexMarker::eSimulationStart);
        SL_CASE_STR(ReflexMarker::eSimulationEnd);
        SL_CASE_STR(ReflexMarker::eRenderSubmitStart);
        SL_CASE_STR(ReflexMarker::eRenderSubmitEnd);
        SL_CASE_STR(ReflexMarker::ePresentStart);
        SL_CASE_STR(ReflexMarker::ePresentEnd);
        SL_CASE_STR(ReflexMarker::eInputSample);
        SL_CASE_STR(ReflexMarker::eTriggerFlash);
        SL_CASE_STR(ReflexMarker::ePCLatencyPing);
        SL_CASE_STR(ReflexMarker::eOutOfBandRenderSubmitStart);
        SL_CASE_STR(ReflexMarker::eOutOfBandRenderSubmitEnd);
        SL_CASE_STR(ReflexMarker::eOutOfBandPresentStart);
        SL_CASE_STR(ReflexMarker::eOutOfBandPresentEnd);
    };
    return "Unknown";
}

inline const char* getDLSSModeAsStr(DLSSMode mode)
{
    switch (mode)
    {
        SL_CASE_STR(DLSSMode::eOff);
        SL_CASE_STR(DLSSMode::eDLAA);
        SL_CASE_STR(DLSSMode::eMaxPerformance);
        SL_CASE_STR(DLSSMode::eBalanced);
        SL_CASE_STR(DLSSMode::eMaxQuality);
        SL_CASE_STR(DLSSMode::eUltraPerformance);
        SL_CASE_STR(DLSSMode::eUltraQuality);
    };
    return "Unknown";
}

inline const char* getBufferTypeAsStr(BufferType buf)
{
    switch (buf)
    {
        SL_CASE_STR(kBufferTypeDepth);
        SL_CASE_STR(kBufferTypeMotionVectors);
        SL_CASE_STR(kBufferTypeHUDLessColor);
        SL_CASE_STR(kBufferTypeScalingInputColor);
        SL_CASE_STR(kBufferTypeScalingOutputColor);
        SL_CASE_STR(kBufferTypeNormals);
        SL_CASE_STR(kBufferTypeRoughness);
        SL_CASE_STR(kBufferTypeAlbedo);
        SL_CASE_STR(kBufferTypeSpecularAlbedo);
        SL_CASE_STR(kBufferTypeIndirectAlbedo);
        SL_CASE_STR(kBufferTypeSpecularMotionVectors);
        SL_CASE_STR(kBufferTypeDisocclusionMask);
        SL_CASE_STR(kBufferTypeEmissive);
        SL_CASE_STR(kBufferTypeExposure);
        SL_CASE_STR(kBufferTypeNormalRoughness);
        SL_CASE_STR(kBufferTypeDiffuseHitNoisy);
        SL_CASE_STR(kBufferTypeDiffuseHitDenoised);
        SL_CASE_STR(kBufferTypeSpecularHitNoisy);
        SL_CASE_STR(kBufferTypeSpecularHitDenoised);
        SL_CASE_STR(kBufferTypeShadowNoisy);
        SL_CASE_STR(kBufferTypeShadowDenoised);
        SL_CASE_STR(kBufferTypeAmbientOcclusionNoisy);
        SL_CASE_STR(kBufferTypeAmbientOcclusionDenoised);
        SL_CASE_STR(kBufferTypeUIColorAndAlpha);
        SL_CASE_STR(kBufferTypeShadowHint);
        SL_CASE_STR(kBufferTypeReflectionHint);
        SL_CASE_STR(kBufferTypeParticleHint);
        SL_CASE_STR(kBufferTypeTransparencyHint);
        SL_CASE_STR(kBufferTypeAnimatedTextureHint);
        SL_CASE_STR(kBufferTypeBiasCurrentColorHint);
        SL_CASE_STR(kBufferTypeRaytracingDistance);
        SL_CASE_STR(kBufferTypeReflectionMotionVectors);
        SL_CASE_STR(kBufferTypePosition);
        SL_CASE_STR(kBufferTypeInvalidDepthMotionHint);
        SL_CASE_STR(kBufferTypeAlpha);
        SL_CASE_STR(kBufferTypeOpaqueColor);
        SL_CASE_STR(kBufferTypeReactiveMaskHint);
        SL_CASE_STR(kBufferTypeTransparencyAndCompositionMaskHint);
        SL_CASE_STR(kBufferTypeReflectedAlbedo);
        SL_CASE_STR(kBufferTypeColorBeforeParticles);
        SL_CASE_STR(kBufferTypeColorBeforeTransparency);
        SL_CASE_STR(kBufferTypeColorBeforeFog);
        SL_CASE_STR(kBufferTypeSpecularHitDistance);
        SL_CASE_STR(kBufferTypeSpecularRayDirectionHitDistance);
        SL_CASE_STR(kBufferTypeSpecularRayDirection);
        SL_CASE_STR(kBufferTypeDiffuseHitDistance);
        SL_CASE_STR(kBufferTypeDiffuseRayDirectionHitDistance);
        SL_CASE_STR(kBufferTypeDiffuseRayDirection);
        SL_CASE_STR(kBufferTypeHiResDepth);
        SL_CASE_STR(kBufferTypeLinearDepth);
        SL_CASE_STR(kBufferTypeBidirectionalDistortionField);
        SL_CASE_STR(kBufferTypeTransparencyLayer);
        SL_CASE_STR(kBufferTypeTransparencyLayerOpacity);
    };
    return "Unknown";
}

inline const char* getFeatureAsStr(Feature f)
{
    switch (f)
    {
        SL_CASE_STR(kFeatureDLSS);
        SL_CASE_STR(kFeatureNRD);
        SL_CASE_STR(kFeatureNIS);
        SL_CASE_STR(kFeatureReflex);
        SL_CASE_STR(kFeatureDLSS_G);
        SL_CASE_STR(kFeatureImGUI);
        SL_CASE_STR(kFeatureCommon);
        SL_CASE_STR(kFeatureDLSS_RR);
    }
    return "Unknown";
}

// Get the feature file name as a string. For a given feature kFeatureDLSS with
// a plugin name sl.dlss.dll the value "dlss" will be returned
inline const char* getFeatureFilenameAsStrNoSL(Feature f)
{
    switch (f)
    {
        case kFeatureDLSS: return "dlss";
        case kFeatureNRD:  return "nrd";
        case kFeatureNIS: return "nis";
        case kFeatureReflex: return "reflex";
        case kFeatureDLSS_G: return "dlss_g";
        case kFeatureImGUI: return "imgui";
        case kFeatureCommon: return "common";
        case kFeatureDLSS_RR: return "dlss_d";
        default: return "Unknown";
    }
}

inline const char* getLogLevelAsStr(LogLevel v)
{
    switch (v)
    {
        SL_CASE_STR(LogLevel::eOff);
        SL_CASE_STR(LogLevel::eDefault);
        SL_CASE_STR(LogLevel::eVerbose);
    };
    return "Unknown";
}

inline const char* getResourceTypeAsStr(ResourceType v)
{
    switch (v)
    {
        SL_CASE_STR(ResourceType::eTex2d);
        SL_CASE_STR(ResourceType::eBuffer);
        SL_CASE_STR(ResourceType::eCommandQueue);
        SL_CASE_STR(ResourceType::eCommandBuffer);
        SL_CASE_STR(ResourceType::eCommandPool);
        SL_CASE_STR(ResourceType::eFence);
        SL_CASE_STR(ResourceType::eSwapchain);
    };
    return "Unknown";
}

inline const char* getResourceLifecycleAsStr(ResourceLifecycle v)
{
    switch (v)
    {
        SL_CASE_STR(ResourceLifecycle::eOnlyValidNow);
        SL_CASE_STR(ResourceLifecycle::eValidUntilPresent);
        SL_CASE_STR(ResourceLifecycle::eValidUntilEvaluate);
    };
    return "Unknown";
}

} // namespace sl

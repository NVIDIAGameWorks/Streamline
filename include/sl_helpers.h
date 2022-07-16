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

inline const char* getNRDMethodAsStr(NRDMethods v)
{
    switch (v)
    {
        SL_CASE_STR(eNRDMethodOff);
        SL_CASE_STR(eNRDMethodReblurDiffuse);
        SL_CASE_STR(eNRDMethodReblurDiffuseOcclusion);
        SL_CASE_STR(eNRDMethodReblurSpecular);
        SL_CASE_STR(eNRDMethodReblurSpecularOcclusion);
        SL_CASE_STR(eNRDMethodReblurDiffuseSpecular);
        SL_CASE_STR(eNRDMethodReblurDiffuseSpecularOcclusion);
        SL_CASE_STR(eNRDMethodReblurDiffuseDirectionalOcclusion);
        SL_CASE_STR(eNRDMethodSigmaShadow);
        SL_CASE_STR(eNRDMethodSigmaShadowTranslucency);
        SL_CASE_STR(eNRDMethodRelaxDiffuse);
        SL_CASE_STR(eNRDMethodRelaxSpecular);
        SL_CASE_STR(eNRDMethodRelaxDiffuseSpecular);
    };
    return "Unknown";
}

inline const char* getNISModeAsStr(NISMode v)
{
    switch (v)
    {
        SL_CASE_STR(eNISModeOff);
        SL_CASE_STR(eNISModeScaler);
        SL_CASE_STR(eNISModeSharpen);
    };
    return "Unknown";
}

inline const char* getNISHDRAsStr(NISHDR v)
{
    switch (v)
    {
        SL_CASE_STR(eNISHDRNone);
        SL_CASE_STR(eNISHDRLinear);
        SL_CASE_STR(eNISHDRPQ);
    };
    return "Unknown";
}

inline const char* getReflexModeAsStr(ReflexMode mode)
{
    switch (mode)
    {
        SL_CASE_STR(eReflexModeOff);
        SL_CASE_STR(eReflexModeLowLatency);
        SL_CASE_STR(eReflexModeLowLatencyWithBoost);
    };
    return "Unknown";
}

inline const char* getReflexMarkerAsStr(ReflexMarker marker)
{
    switch (marker)
    {
        SL_CASE_STR(eReflexMarkerSimulationStart);
        SL_CASE_STR(eReflexMarkerSimulationEnd);
        SL_CASE_STR(eReflexMarkerRenderSubmitStart);
        SL_CASE_STR(eReflexMarkerRenderSubmitEnd);
        SL_CASE_STR(eReflexMarkerPresentStart);
        SL_CASE_STR(eReflexMarkerPresentEnd);
        SL_CASE_STR(eReflexMarkerInputSample);
        SL_CASE_STR(eReflexMarkerTriggerFlash);
        SL_CASE_STR(eReflexMarkerPCLatencyPing);
        SL_CASE_STR(eReflexMarkerSleep);
    };
    return "Unknown";
}

inline const char* getDLSSModeAsStr(DLSSMode mode)
{
    switch(mode)
    {
        SL_CASE_STR(eDLSSModeOff);
        SL_CASE_STR(eDLSSModeMaxPerformance);
        SL_CASE_STR(eDLSSModeBalanced);
        SL_CASE_STR(eDLSSModeMaxQuality);
        SL_CASE_STR(eDLSSModeUltraPerformance);
        SL_CASE_STR(eDLSSModeUltraQuality);
    };
    return "Unknown";
}

inline const char* getBufferTypeAsStr(BufferType buf)
{
    switch (buf)
    {
        SL_CASE_STR(eBufferTypeDepth);
        SL_CASE_STR(eBufferTypeMVec);
        SL_CASE_STR(eBufferTypeHUDLessColor);
        SL_CASE_STR(eBufferTypeScalingInputColor);
        SL_CASE_STR(eBufferTypeScalingOutputColor);
        SL_CASE_STR(eBufferTypeNormals);
        SL_CASE_STR(eBufferTypeRoughness);
        SL_CASE_STR(eBufferTypeAlbedo);
        SL_CASE_STR(eBufferTypeSpecularAlbedo);
        SL_CASE_STR(eBufferTypeIndirectAlbedo);
        SL_CASE_STR(eBufferTypeSpecularMVec);
        SL_CASE_STR(eBufferTypeDisocclusionMask);
        SL_CASE_STR(eBufferTypeEmissive);
        SL_CASE_STR(eBufferTypeExposure);
        SL_CASE_STR(eBufferTypeNormalRoughness);
        SL_CASE_STR(eBufferTypeDiffuseHitNoisy);
        SL_CASE_STR(eBufferTypeDiffuseHitDenoised);
        SL_CASE_STR(eBufferTypeSpecularHitNoisy);
        SL_CASE_STR(eBufferTypeSpecularHitDenoised);
        SL_CASE_STR(eBufferTypeShadowNoisy);
        SL_CASE_STR(eBufferTypeShadowDenoised);
        SL_CASE_STR(eBufferTypeAmbientOcclusionNoisy);
        SL_CASE_STR(eBufferTypeAmbientOcclusionDenoised);
        SL_CASE_STR(eBufferTypeUIHint);
        SL_CASE_STR(eBufferTypeShadowHint);
        SL_CASE_STR(eBufferTypeReflectionHint);
        SL_CASE_STR(eBufferTypeParticleHint);
        SL_CASE_STR(eBufferTypeTransparencyHint);
        SL_CASE_STR(eBufferTypeAnimatedTextureHint);
        SL_CASE_STR(eBufferTypeBiasCurrentColorHint);
        SL_CASE_STR(eBufferTypeRaytracingDistance);
        SL_CASE_STR(eBufferTypeReflectionMotionVectors);
        SL_CASE_STR(eBufferTypePosition);
    };
    return "Unknown";
}

inline const char* getFeatureAsStr(Feature f)
{
    switch (f)
    {
        SL_CASE_STR(eFeatureDLSS);
        SL_CASE_STR(eFeatureNRD);
        SL_CASE_STR(eFeatureNIS);
        SL_CASE_STR(eFeatureReflex);
        SL_CASE_STR(eFeatureCommon);
    }
    return "Unknown";
}

inline const char* getLogLevelAsStr(LogLevel v)
{
    switch (v)
    {
        SL_CASE_STR(eLogLevelOff);
        SL_CASE_STR(eLogLevelDefault);
        SL_CASE_STR(eLogLevelVerbose);
    };
    return "Unknown";
}

inline const char* getResourceTypeAsStr(ResourceType v)
{
    switch (v)
    {
        SL_CASE_STR(eResourceTypeTex2d);
        SL_CASE_STR(eResourceTypeBuffer);
    };
    return "Unknown";
}

}

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
#include <vector>

#include "include/sl.h"
#include "include/sl_helpers.h"
#include "include/sl_nrd.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "external/json/include/nlohmann/json.hpp"

#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "source/plugins/sl.nrd/versions.h"

#include "external/nrd/Include/NRD.h"
#include "_artifacts/shaders/nrd_prep_cs.h"
#include "_artifacts/shaders/nrd_pack_cs.h"
#include "_artifacts/shaders/nrd_prep_spv.h"
#include "_artifacts/shaders/nrd_pack_spv.h"
#include "_artifacts/json/nrd_json.h"
#include "_artifacts/gitVersion.h"

#define NRD_CHECK(f) {nrd::Result r = f;if(r != nrd::Result::SUCCESS) { SL_LOG_ERROR( "%s failed error %u",#f,r); return false;}};
#define NRD_CHECK1(f) {nrd::Result r = f;if(r != nrd::Result::SUCCESS) { SL_LOG_ERROR( "%s failed error %u",#f,r); return Result::eErrorNRDAPI;}};

using json = nlohmann::json;
namespace sl
{
    constexpr uint32_t kNrdInputBufferTagCount = static_cast<uint32_t>(nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST);
    constexpr uint32_t kNrdOutputBufferTagCount = static_cast<uint32_t>(nrd::ResourceType::TRANSIENT_POOL) - kNrdInputBufferTagCount;

    enum class DenoiserClass
    {
        eReblur,
        eSigma,
        eRelax,
        eReference,
        eMv,
        eCount
    };

    enum class ResourceTypeRole
    {
        eInput,
        eOutput,
        eRw,
    };

    struct ResourceTypeDesc
    {
        nrd::ResourceType resourceType;
        ResourceTypeRole typeRole;
        bool isOptional;
    };

    std::vector<ResourceTypeDesc> const kReblurDiffuseBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST,      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST,     ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurDiffuseOcclusionBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_HITDIST,               ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_DIFF_HITDIST,              ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurDiffuseShBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_SH0,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_SH1,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::OUT_DIFF_SH0,                  ResourceTypeRole::eOutput,      false },
        { nrd::ResourceType::OUT_DIFF_SH1,                  ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurSpecularBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST,      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST,     ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurSpecularOcclusionBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_HITDIST,               ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_SPEC_HITDIST,              ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurSpecularShBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_SH0,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_SH1,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::OUT_SPEC_SH0,                  ResourceTypeRole::eOutput,      false },
        { nrd::ResourceType::OUT_SPEC_SH1,                  ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurDiffuseSpecularBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST,      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST,      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::IN_SPEC_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST,     ResourceTypeRole::eOutput,      false },
        { nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST,     ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurDiffuseSpecularOcclusionBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_HITDIST,               ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_HITDIST,               ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_DIFF_HITDIST,              ResourceTypeRole::eOutput,      false },
        { nrd::ResourceType::OUT_SPEC_HITDIST,              ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurDiffuseSpecularShBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_SH0,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_SH1,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_SH0,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_SH1,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::IN_SPEC_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::OUT_DIFF_SH0,                  ResourceTypeRole::eOutput,      false },
        { nrd::ResourceType::OUT_DIFF_SH1,                  ResourceTypeRole::eOutput,      false },
        { nrd::ResourceType::OUT_SPEC_SH0,                  ResourceTypeRole::eOutput,      false },
        { nrd::ResourceType::OUT_SPEC_SH1,                  ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReblurDiffuseDirectionalOcclusionBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_DIRECTION_HITDIST,     ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_CONFIDENCE,            ResourceTypeRole::eInput,       true },
        { nrd::ResourceType::OUT_DIFF_DIRECTION_HITDIST,    ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kSigmaShadowBuffers{
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SHADOWDATA,                 ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY,       ResourceTypeRole::eRw,          false },
    };

    std::vector<ResourceTypeDesc> const kSigmaShadowTransluscencyBuffers{
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SHADOWDATA,                 ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SHADOW_TRANSLUCENCY,        ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY,       ResourceTypeRole::eRw,          false },
    };

    std::vector<ResourceTypeDesc> const kRelaxDiffuseBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST,      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST,     ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kRelaxSpecularBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST,      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST,     ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kRelaxDiffuseSpecularBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST,      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST,      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST,     ResourceTypeRole::eOutput,      false },
        { nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST,     ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kReferenceBuffers{
        { nrd::ResourceType::IN_RADIANCE,                   ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_RADIANCE,                  ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kSpecularReflectionMvBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_NORMAL_ROUGHNESS,           ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_VIEWZ,                      ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_SPEC_HITDIST,               ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_REFLECTION_MV,             ResourceTypeRole::eOutput,      false },
    };

    std::vector<ResourceTypeDesc> const kSpecularDeltaMvBuffers{
        { nrd::ResourceType::IN_MV,                         ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DELTA_PRIMARY_POS,          ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::IN_DELTA_SECONDARY_POS,        ResourceTypeRole::eInput,       false },
        { nrd::ResourceType::OUT_DELTA_MV,                  ResourceTypeRole::eOutput,      false },
    };

    BufferType convert2BufferType(nrd::ResourceType resourceType)
    {
        switch (resourceType)
        {
        case nrd::ResourceType::IN_MV:                      return kBufferTypeMotionVectors;
        case nrd::ResourceType::IN_NORMAL_ROUGHNESS:        return kBufferTypeNormalRoughness;
        case nrd::ResourceType::IN_VIEWZ:                   return kBufferTypeDepth;
        case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:   return kBufferTypeInDiffuseRadianceHitDist;
        case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:                   return kBufferTypeInSpecularRadianceHitDist;
        case nrd::ResourceType::IN_DIFF_HITDIST:                   return kBufferTypeInDiffuseHitDist;
        case nrd::ResourceType::IN_SPEC_HITDIST:                   return kBufferTypeInSpecularHitDist;
        case nrd::ResourceType::IN_DIFF_DIRECTION_HITDIST:                   return kBufferTypeInDiffuseDirectionHitDist;
        case nrd::ResourceType::IN_DIFF_SH0:                   return kBufferTypeInDiffuseSH0;
        case nrd::ResourceType::IN_DIFF_SH1:                   return kBufferTypeInDiffuseSH1;
        case nrd::ResourceType::IN_SPEC_SH0:                   return kBufferTypeInSpecularSH0;
        case nrd::ResourceType::IN_SPEC_SH1:                   return kBufferTypeInSpecularSH1;
        case nrd::ResourceType::IN_DIFF_CONFIDENCE:                   return kBufferTypeInDiffuseConfidence;
        case nrd::ResourceType::IN_SPEC_CONFIDENCE:                   return kBufferTypeInSpecularConfidence;
        case nrd::ResourceType::IN_DISOCCLUSION_THRESHOLD_MIX:                   return kBufferTypeInDisocclusionThresholdMix;
        case nrd::ResourceType::IN_BASECOLOR_METALNESS:                   return kBufferTypeInBasecolorMetalness;
        case nrd::ResourceType::IN_SHADOWDATA:                   return kBufferTypeInShadowData;
        case nrd::ResourceType::IN_SHADOW_TRANSLUCENCY:                   return kBufferTypeInShadowTransluscency;
        case nrd::ResourceType::IN_RADIANCE:                   return kBufferTypeInRadiance;
        case nrd::ResourceType::IN_DELTA_PRIMARY_POS:                   return kBufferTypeInDeltaPrimaryPos;
        case nrd::ResourceType::IN_DELTA_SECONDARY_POS:                   return kBufferTypeInDeltaSecondaryPos;

        case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:                   return kBufferTypeOutDiffuseRadianceHitDist;
        case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:                   return kBufferTypeOutSpecularRadianceHitDist;
        case nrd::ResourceType::OUT_DIFF_SH0:                   return kBufferTypeOutDiffuseSH0;
        case nrd::ResourceType::OUT_DIFF_SH1:                   return kBufferTypeOutDiffuseSH1;
        case nrd::ResourceType::OUT_SPEC_SH0:                   return kBufferTypeOutSpecularSH0;
        case nrd::ResourceType::OUT_SPEC_SH1:                   return kBufferTypeOutSpecularSH1;
        case nrd::ResourceType::OUT_DIFF_HITDIST:                   return kBufferTypeOutDiffuseHitDist;
        case nrd::ResourceType::OUT_SPEC_HITDIST:                   return kBufferTypeOutSpecularHitDist;
        case nrd::ResourceType::OUT_DIFF_DIRECTION_HITDIST:                   return kBufferTypeOutDiffuseDirectionHitDist;
        case nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY:                   return kBufferTypeOutShadowTransluscency;
        case nrd::ResourceType::OUT_RADIANCE:                   return kBufferTypeOutRadiance;
        case nrd::ResourceType::OUT_REFLECTION_MV:                   return kBufferTypeOutReflectionMv;
        case nrd::ResourceType::OUT_DELTA_MV:                   return kBufferTypeOutDeltaMv;
        case nrd::ResourceType::OUT_VALIDATION:                   return kBufferTypeOutValidation;
        }
        return (BufferType)-1;
    }

    struct NrdMethodInfo
    {
        nrd::Denoiser const method;
        const char* const name;
        DenoiserClass const denoiserClass;
        std::vector<ResourceTypeDesc> const& resourceTypeDescs;
    };

    std::vector<NrdMethodInfo> const kMethodInfos{
        { nrd::Denoiser::REBLUR_DIFFUSE,                      "REBLUR_DIFFUSE",                       DenoiserClass::eReblur,     kReblurDiffuseBuffers },
        { nrd::Denoiser::REBLUR_DIFFUSE_OCCLUSION,            "REBLUR_DIFFUSE_OCCLUSION",             DenoiserClass::eReblur,     kReblurDiffuseOcclusionBuffers },
        { nrd::Denoiser::REBLUR_DIFFUSE_SH,                   "REBLUR_DIFFUSE_SH",                    DenoiserClass::eReblur,     kReblurDiffuseShBuffers },
        { nrd::Denoiser::REBLUR_SPECULAR,                     "REBLUR_SPECULAR",                      DenoiserClass::eReblur,     kReblurSpecularBuffers },
        { nrd::Denoiser::REBLUR_SPECULAR_OCCLUSION,           "REBLUR_SPECULAR_OCCLUSION",            DenoiserClass::eReblur,     kReblurSpecularOcclusionBuffers },
        { nrd::Denoiser::REBLUR_SPECULAR_SH,                  "REBLUR_SPECULAR_SH",                   DenoiserClass::eReblur,     kReblurSpecularShBuffers },
        { nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR,             "REBLUR_DIFFUSE_SPECULAR",              DenoiserClass::eReblur,     kReblurDiffuseSpecularBuffers },
        { nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR_OCCLUSION,   "REBLUR_DIFFUSE_SPECULAR_OCCLUSION",    DenoiserClass::eReblur,     kReblurDiffuseSpecularOcclusionBuffers },
        { nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR_SH,          "REBLUR_DIFFUSE_SPECULAR_SH",           DenoiserClass::eReblur,     kReblurDiffuseSpecularShBuffers },
        { nrd::Denoiser::REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION,"REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION", DenoiserClass::eReblur,     kReblurDiffuseDirectionalOcclusionBuffers },
        { nrd::Denoiser::SIGMA_SHADOW,                        "SIGMA_SHADOW",                         DenoiserClass::eSigma,      kSigmaShadowBuffers },
        { nrd::Denoiser::SIGMA_SHADOW_TRANSLUCENCY,           "SIGMA_SHADOW_TRANSLUCENCY",            DenoiserClass::eSigma,      kSigmaShadowTransluscencyBuffers },
        { nrd::Denoiser::RELAX_DIFFUSE,                       "RELAX_DIFFUSE",                        DenoiserClass::eRelax,      kRelaxDiffuseBuffers },
        { nrd::Denoiser::RELAX_SPECULAR,                      "RELAX_SPECULAR",                       DenoiserClass::eRelax,      kRelaxSpecularBuffers },
        { nrd::Denoiser::RELAX_DIFFUSE_SPECULAR,              "RELAX_DIFFUSE_SPECULAR",               DenoiserClass::eRelax,      kRelaxDiffuseSpecularBuffers },
        { nrd::Denoiser::REFERENCE,                           "REFERENCE",                            DenoiserClass::eReference,  kReferenceBuffers },
        { nrd::Denoiser::SPECULAR_REFLECTION_MV,              "SPECULAR_REFLECTION_MV",               DenoiserClass::eMv,         kSpecularReflectionMvBuffers },
        { nrd::Denoiser::SPECULAR_DELTA_MV,                   "SPECULAR_DELTA_MV",                    DenoiserClass::eMv,         kSpecularDeltaMvBuffers },
    };
    std::vector<NrdMethodInfo> listMethodsFromMask(uint32_t mask)
    {
        std::vector<NrdMethodInfo> output;
        for (auto i = 0ul; mask; ++i, mask = mask >> 1)
            if (mask & 0x01)
                output.push_back(kMethodInfos[i]);
        return output;
    }

    enum class Encodable
    {
        eDiffuseRadianceHitDist = 0ul,
        eSpecularRadianceHitDist,
        eDiffuseDirectionHitDist,
        eDiffuseSh0,
        eDiffuseSh1,
        eSpecularSh0,
        eSpecularSh1,
        eShadowdata,
        eShadowTransluscency,
        eCount
    };

    enum class Decodable
    {
        eDiffuseRadianceHitDist = 0ul,
        eSpecularRadianceHitDist,
        eDiffuseDirectionHitDist,
        eDiffuseSh0,
        eDiffuseSh1,
        eSpecularSh0,
        eSpecularSh1,
        eShadowTransluscency,
        eCount
    };

    struct EncodableInfo
    {
        Encodable const encodable;
        chi::Format const format;
        const char* const debugName;
    };

    std::vector<EncodableInfo> const kEncodableInfos{
        { Encodable::eDiffuseRadianceHitDist,   chi::eFormatRGBA16F, "sl.ctx.DiffuseRadianceHitDist"},
        { Encodable::eSpecularRadianceHitDist,  chi::eFormatRGBA16F, "sl.ctx.SpecularRadianceHitDist"},
        { Encodable::eDiffuseDirectionHitDist,  chi::eFormatRGBA8UN, "sl.ctx.DiffuseDirectionHitDist"},
        { Encodable::eDiffuseSh0,               chi::eFormatRGBA16F, "sl.ctx.DiffuseSh0"},
        { Encodable::eDiffuseSh1,               chi::eFormatRGBA16F, "sl.ctx.DiffuseSh1"},
        { Encodable::eSpecularSh0,              chi::eFormatRGBA16F, "sl.ctx.SpecularSh0"},
        { Encodable::eSpecularSh1,              chi::eFormatRGBA16F, "sl.ctx.SpecularSh1"},
        { Encodable::eShadowdata,               chi::eFormatRGBA16F, "sl.ctx.Shadowdata"},
        { Encodable::eShadowTransluscency,      chi::eFormatRGBA8UN, "sl.ctx.ShadowTransluscency"},
    };

    Encodable cast2Encodable(nrd::ResourceType resourceType)
    {
        switch (resourceType)
        {
        case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:   return Encodable::eDiffuseRadianceHitDist;
        case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:   return Encodable::eSpecularRadianceHitDist;
        case nrd::ResourceType::IN_DIFF_DIRECTION_HITDIST:  return Encodable::eDiffuseDirectionHitDist;
        case nrd::ResourceType::IN_DIFF_SH0:                return Encodable::eDiffuseSh0;
        case nrd::ResourceType::IN_DIFF_SH1:                return Encodable::eDiffuseSh1;
        case nrd::ResourceType::IN_SPEC_SH0:                return Encodable::eSpecularSh0;
        case nrd::ResourceType::IN_SPEC_SH1:                return Encodable::eSpecularSh1;
        case nrd::ResourceType::IN_SHADOWDATA:              return Encodable::eShadowdata;
        case nrd::ResourceType::IN_SHADOW_TRANSLUCENCY:     return Encodable::eShadowTransluscency;
        }
        return Encodable::eCount;
    };

    nrd::ResourceType cast2ResourceType(Encodable encodable)
    {
        switch (encodable)
        {
        case Encodable::eDiffuseRadianceHitDist:    return nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST;
        case Encodable::eSpecularRadianceHitDist:   return nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST;
        case Encodable::eDiffuseDirectionHitDist:   return nrd::ResourceType::IN_DIFF_DIRECTION_HITDIST;
        case Encodable::eDiffuseSh0:                return nrd::ResourceType::IN_DIFF_SH0;
        case Encodable::eDiffuseSh1:                return nrd::ResourceType::IN_DIFF_SH1;
        case Encodable::eSpecularSh0:               return nrd::ResourceType::IN_SPEC_SH0;
        case Encodable::eSpecularSh1:               return nrd::ResourceType::IN_SPEC_SH1;
        case Encodable::eShadowdata:                return nrd::ResourceType::IN_SHADOWDATA;
        case Encodable::eShadowTransluscency:       return nrd::ResourceType::IN_SHADOW_TRANSLUCENCY;
        }
        return nrd::ResourceType::MAX_NUM;
    }

    Decodable selectDecodable(nrd::ResourceType resourceType)
    {
        switch (resourceType)
        {
        case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:   return Decodable::eDiffuseRadianceHitDist;
        case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:   return Decodable::eSpecularRadianceHitDist;
        case nrd::ResourceType::OUT_DIFF_DIRECTION_HITDIST:  return Decodable::eDiffuseDirectionHitDist;
        case nrd::ResourceType::OUT_DIFF_SH0:                return Decodable::eDiffuseSh0;
        case nrd::ResourceType::OUT_DIFF_SH1:                return Decodable::eDiffuseSh1;
        case nrd::ResourceType::OUT_SPEC_SH0:                return Decodable::eSpecularSh0;
        case nrd::ResourceType::OUT_SPEC_SH1:                return Decodable::eSpecularSh1;
        case nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY:     return Decodable::eShadowTransluscency;
        }
        return Decodable::eCount;
    };

    struct EncoderCB
    {
        uint32_t encodeDiffuseRadianceHitDist;
        uint32_t encodeSpecularRadianceHitDist;
        uint32_t encodeDiffuseDirectionHitDist;
        uint32_t encodeDiffuseSh0;
        uint32_t encodeDiffuseSh1;
        uint32_t encodeSpecularSh0;
        uint32_t encodeSpecularSh1;
        uint32_t encodeShadowdata;
        uint32_t encodeShadowTransluscency;
    };

    struct DecoderCB
    {
        uint32_t decodeDiffuseRadianceHitDist;
        uint32_t decodeSpecularRadianceHitDist;
        uint32_t decodeDiffuseDirectionHitDist;
        uint32_t decodeDiffuseSh0;
        uint32_t decodeDiffuseSh1;
        uint32_t decodeSpecularSh0;
        uint32_t decodeSpecularSh1;
        uint32_t decodeShadowTransluscency;
    };

using PFunCreateInstance = nrd::Result(const nrd::InstanceCreationDesc& denoiserCreationDesc, nrd::Instance*& denoiser);
using PFunDestroyInstance = void(nrd::Instance& denoiser);
using PFunGetLibraryDesc = const nrd::LibraryDesc& ();
using PFunGetInstanceDesc = const nrd::InstanceDesc& (const nrd::Instance& denoiser);
using PFunSetCommonSettings = nrd::Result(nrd::Instance& instance, nrd::CommonSettings const& commonSettings);
using PFunSetDenoiserSettings = nrd::Result(nrd::Instance& instance, nrd::Identifier identifier, const void* denoiserSettings);
using PFunGetComputeDispatches = nrd::Result(nrd::Instance& instance, const nrd::Identifier* identifiers, uint32_t identifiersNum, const nrd::DispatchDesc*& dispatchDescs, uint32_t& dispatchDescNum);

struct StateVector : std::vector<chi::ResourceState>
{
    chi::ResourceState initialState;

    StateVector()
        : std::vector<chi::ResourceState>{}
        , initialState{ chi::ResourceState::eUndefined }
    {}

    StateVector(chi::ResourceState initialState)
        : std::vector<chi::ResourceState>{}
        , initialState{ initialState }
    {}

    void reset()
    {
        for (auto& state : *this)
        {
            state = initialState;
        }
    }
};

struct NRDInstance
{
    nrd::CommonSettings prevCommonSettings{};
    std::vector<chi::Resource> permanentTextures;
    std::vector<chi::Resource> transientTextures;

    StateVector permanentTexturesStates{ chi::ResourceState::eStorageRW },
        transientTexturesStates{},
        taggedInputBuffersStates{ chi::ResourceState::eStorageRW },
        taggedOutputBuffersStates{ chi::ResourceState::eTextureRead };

    void resetStateVectors()
    {
        permanentTexturesStates.reset();
        transientTexturesStates.reset();
        taggedInputBuffersStates.reset();
        taggedOutputBuffersStates.reset();
    }

    bool setResourceState(chi::ResourceState resourceState, nrd::ResourceType resourceType, uint32_t indexInPool = 0)
    {
        if (resourceType == nrd::ResourceType::PERMANENT_POOL)
        {
            permanentTexturesStates[indexInPool] = resourceState;
            return true;
        }

        if (resourceType == nrd::ResourceType::TRANSIENT_POOL)
        {
            transientTexturesStates[indexInPool] = resourceState;
            return true;
        }

        indexInPool = static_cast<uint32_t>(resourceType);
        if (indexInPool < kNrdInputBufferTagCount)
        {
            taggedInputBuffersStates[indexInPool] = resourceState;
            return true;
        }

        indexInPool -= kNrdInputBufferTagCount;
        if (indexInPool < kNrdOutputBufferTagCount)
        {
            taggedOutputBuffersStates[indexInPool] = resourceState;
            return true;
        }

        return false;
    }

    chi::ResourceState getResourceState(nrd::ResourceType resourceType, uint32_t indexInPool = 0)
    {
        if (resourceType == nrd::ResourceType::PERMANENT_POOL)
            return permanentTexturesStates[indexInPool];

        if (resourceType == nrd::ResourceType::TRANSIENT_POOL)
            return transientTexturesStates[indexInPool];

        indexInPool = static_cast<uint32_t>(resourceType);
        if (indexInPool < kNrdInputBufferTagCount)
            return taggedInputBuffersStates[indexInPool];

        indexInPool -= kNrdInputBufferTagCount;
        if (indexInPool < kNrdOutputBufferTagCount)
            return taggedOutputBuffersStates[indexInPool];

        return chi::ResourceState::eUnknown;
    }
    std::vector<chi::Kernel> shaders;
    nrd::Instance* denoiser = {};
    uint32_t methodMask = 0;
    nrd::DenoiserDesc denoiserDescs[6];
    uint32_t denoiserCount = 0;
    std::array<bool, static_cast<uint32_t>(Encodable::eCount)> enabledInputResources{};
    bool relax = false;
};

struct NRDViewport
{
    uint32_t id{};
    uint32_t frameIndex{};
    uint32_t width{};
    uint32_t height{};
    std::map<uint32_t, NRDInstance*> instances{};
    NRDInstance* instance{};
    chi::Resource viewZ{};
    chi::Resource mvec{};

    std::array<chi::Resource, static_cast<uint32_t>(Encodable::eCount)> inputs{};
    std::string description = "";
};

namespace nrdsl
{
struct NRDContext
{
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(NRDContext);
    void onCreateContext() {};
    void onDestroyContext() {};

    HMODULE lib{};
    PFunCreateInstance* createInstance{};
    PFunDestroyInstance* destroyInstance{};
    PFunGetLibraryDesc* getLibraryDesc{};
    PFunGetInstanceDesc* getInstanceDesc{};
    PFunSetCommonSettings* setCommonSettings{};
    PFunSetDenoiserSettings* setDenoiserSettings{};
    PFunGetComputeDispatches* getComputeDispatches{};

    operator bool() const
    {
        return getLibraryDesc && createInstance && getInstanceDesc && setCommonSettings &&
            setDenoiserSettings && getComputeDispatches && destroyInstance;
    }

    chi::Kernel prepareDataKernel = {};
    chi::Kernel packDataKernel = {};

    std::map<chi::Resource, chi::ResourceState> cachedStates = {};

    std::map<uint32_t, NRDViewport*> viewports = {};
    NRDViewport* viewport = {};

    // Per evaluate extracted constants
    Constants* commonConsts{};
    NRDConstants* nrdConsts{};

    // We store incoming constants here
    common::ViewportIdFrameData<> constsPerViewport = { "nrd" };

    chi::ICompute* compute = {};

    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};

    void cacheState(chi::Resource res, uint32_t nativeState = 0)
    {
        if (cachedStates.find(res) == cachedStates.end())
        {
            chi::ResourceState state = chi::ResourceState::eGeneral;
            if (nativeState)
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
}

static std::string JSON = std::string(nrd_json, &nrd_json[nrd_json_len]);

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
        updateCommonEmbeddedJSONConfig(&config, info);
    }
}

SL_PLUGIN_DEFINE("sl.nrd", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON.c_str(), updateEmbeddedJSON, nrdsl, NRDContext)

void destroyNRDViewport(NRDViewport*);

Result slSetData(const BaseStructure* inputs, CommandBuffer* cmdBuffer)
{
    auto consts = findStruct<NRDConstants>(inputs);
    auto viewport = findStruct<ViewportHandle>(inputs);

    if (!consts || !viewport)
    {
        SL_LOG_ERROR("Invalid input data");
        return Result::eErrorMissingInputParameter;
    }

    auto& ctx = (*nrdsl::getContext());
    ctx.constsPerViewport.set(0, *viewport, consts);

    return Result::eOk;
}

sl::Result slNRDSetConstants(const sl::ViewportHandle& viewport, const sl::NRDConstants& constants)
{
    auto v = viewport;
    v.next = (sl::BaseStructure*)&constants;
    return slSetData(&v, nullptr);
}

bool nrdGetConstants(const common::EventData& data, NRDConstants** consts)
{
    return (*nrdsl::getContext()).constsPerViewport.get(data, consts);
}

chi::Format convertNRDFormat(nrd::Format format)
{
    switch (format)
    {
        case nrd::Format::R8_UNORM:        return chi::eFormatR8UN;
        case nrd::Format::R8_SNORM:        break;
        case nrd::Format::R8_UINT:        return chi::eFormatR8UI;
        case nrd::Format::R8_SINT:        break;
        case nrd::Format::RG8_UNORM:      return chi::eFormatRG8UN;
        case nrd::Format::RG8_SNORM:      break;
        case nrd::Format::RG8_UINT:        break;
        case nrd::Format::RG8_SINT:        break;
        case nrd::Format::RGBA8_UNORM:      return chi::eFormatRGBA8UN;
        case nrd::Format::RGBA8_SNORM:      break;
        case nrd::Format::RGBA8_UINT:      break;
        case nrd::Format::RGBA8_SINT:      break;
        case nrd::Format::RGBA8_SRGB:      break;
        case nrd::Format::R16_UNORM:      break;
        case nrd::Format::R16_SNORM:      break;
        case nrd::Format::R16_UINT:        return chi::eFormatR16UI;
        case nrd::Format::R16_SINT:        break;
        case nrd::Format::R16_SFLOAT:      return chi::eFormatR16F;
        case nrd::Format::RG16_UNORM:      return chi::eFormatRG16UN;
        case nrd::Format::RG16_SNORM:      break;
        case nrd::Format::RG16_UINT:      return chi::eFormatRG16UI;
        case nrd::Format::RG16_SINT:      break;
        case nrd::Format::RG16_SFLOAT:      return chi::eFormatRG16F;
        case nrd::Format::RGBA16_UNORM:      break;
        case nrd::Format::RGBA16_SNORM:      break;
        case nrd::Format::RGBA16_UINT:      break;
        case nrd::Format::RGBA16_SINT:      break;
        case nrd::Format::RGBA16_SFLOAT:    return chi::eFormatRGBA16F;
        case nrd::Format::R32_UINT:        return chi::eFormatR32UI;
        case nrd::Format::R32_SINT:        break;
        case nrd::Format::R32_SFLOAT:      return chi::eFormatR32F;
        case nrd::Format::RG32_UINT:      return chi::eFormatRG32UI;
        case nrd::Format::RG32_SINT:      break;
        case nrd::Format::RG32_SFLOAT:      return chi::eFormatRG32F;
        case nrd::Format::RGB32_UINT:      break;
        case nrd::Format::RGB32_SINT:      break;
        case nrd::Format::RGB32_SFLOAT:      break;
        case nrd::Format::RGBA32_UINT:      break;
        case nrd::Format::RGBA32_SINT:      break;
        case nrd::Format::RGBA32_SFLOAT:    return chi::eFormatRGBA32F;
        case nrd::Format::R10_G10_B10_A2_UNORM:  break;
        case nrd::Format::R10_G10_B10_A2_UINT:  break;
        case nrd::Format::R11_G11_B10_UFLOAT:  return chi::eFormatRGB11F;
        case nrd::Format::R9_G9_B9_E5_UFLOAT:  break;
        default:                break;
    }
    return chi::Format::eFormatINVALID;
}

void destroyNRDInstance(NRDInstance* inst)
{
    auto& ctx = (*nrdsl::getContext());
    SL_LOG_INFO("Destroying NRDContext instance with method mask %u", inst->methodMask);
    ctx.destroyInstance(*inst->denoiser);
    
    for (auto& shader : inst->shaders)
    {
        CHI_VALIDATE(ctx.compute->destroyKernel(shader));
    }
    inst->shaders.clear();
    for (auto& res : inst->permanentTextures)
    {
        CHI_VALIDATE(ctx.compute->destroyResource(res));
    }
    inst->permanentTextures.clear();
    for (auto& res : inst->transientTextures)
    {
        CHI_VALIDATE(ctx.compute->destroyResource(res));
    }
    inst->transientTextures.clear();
    delete inst;
}

void destroyNRDViewport(NRDViewport* viewport)
{
    auto& ctx = (*nrdsl::getContext());

    for (auto& inst : viewport->instances)
    {
        destroyNRDInstance(inst.second);
    }
    viewport->instances.clear();
    viewport->instance = {};

    viewport->description = "";

    CHI_VALIDATE(ctx.compute->destroyResource(viewport->viewZ));
    CHI_VALIDATE(ctx.compute->destroyResource(viewport->mvec));

    for (auto& resource : viewport->inputs)
    {
        CHI_VALIDATE(ctx.compute->destroyResource(resource));
    }

    ctx.cachedStates.clear();
}

void destroyNRD()
{
    auto& ctx = (*nrdsl::getContext());

    for (auto& v : ctx.viewports)
    {
        destroyNRDViewport(v.second);
        delete v.second;
    }
    ctx.viewports.clear();

    CHI_VALIDATE(ctx.compute->destroyKernel(ctx.prepareDataKernel));
    CHI_VALIDATE(ctx.compute->destroyKernel(ctx.packDataKernel));
}

Result initializeNRD(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*nrdsl::getContext());

    {
        auto it = ctx.viewports.find(data.id);
        if (it == ctx.viewports.end())
        {
            ctx.viewports[data.id] = new NRDViewport{};
        }
        ctx.viewport = ctx.viewports[data.id];
        ctx.viewport->id = data.id;
        ctx.viewport->frameIndex = data.frame;
    }

    if (!ctx.nrdConsts->methodMask)
    {
        SL_LOG_WARN("NRDContext disabled, if this is not intentional please update methodMask bit field.");
        return Result::eOk; // NRDContext disabled which is OK
    }

    ctx.cachedStates[nullptr] = chi::ResourceState::eGeneral;

    ctx.viewport->instance = {};
    auto it = ctx.viewport->instances.find(ctx.nrdConsts->methodMask);
    if (it != ctx.viewport->instances.end())
    {
        ctx.viewport->instance = (*it).second;
    }
    else
    {
        ctx.viewport->instance = new NRDInstance();
        *ctx.viewport->instance = {};
        ctx.viewport->instances[ctx.nrdConsts->methodMask] = ctx.viewport->instance;
    }

    auto parameters = api::getContext()->parameters;

    uint32_t mask = ctx.nrdConsts->methodMask;
    uint32_t denoiserCount = 0;

    auto methods = listMethodsFromMask(mask);
    for (auto const& method : methods)
    {
        BufferType pname = (BufferType)-1;

        if (method.denoiserClass == DenoiserClass::eRelax)
            ctx.viewport->instance->relax = true;

        for (auto const& resourceDesc : method.resourceTypeDescs)
        {
            if (!resourceDesc.isOptional)
            {
                pname = convert2BufferType(resourceDesc.resourceType);
                break;
            }
        }
        
        if (pname == (BufferType)-1)
            SL_LOG_ERROR("Unable to identify methods resources");

        for (auto const& resourceDesc : method.resourceTypeDescs)
        {
            auto encodable = cast2Encodable(resourceDesc.resourceType);
            if (encodable != Encodable::eCount)
                ctx.viewport->instance->enabledInputResources[static_cast<uint32_t>(encodable)] = true;
        }

        CommonResource res{};
        SL_CHECK(getTaggedResource(pname, res, ctx.viewport->id, false, inputs, numInputs));
        chi::ResourceDescription desc{};
        CHI_CHECK_RR(ctx.compute->getResourceState(res.getState(), desc.state));
        CHI_CHECK_RR(ctx.compute->getResourceDescription(res, desc));

        if (ctx.viewport->instance->denoiser && (ctx.viewport->width != desc.width || ctx.viewport->height != desc.height))
        {
            auto viewport = new NRDViewport{};
            viewport->id = ctx.viewport->id;
            auto instance = new NRDInstance{};
            instance->enabledInputResources = ctx.viewport->instance->enabledInputResources;
            destroyNRDViewport(ctx.viewport);
            ctx.viewports[viewport->id] = viewport;
            ctx.viewport = viewport;
            ctx.viewport->instances[ctx.nrdConsts->methodMask] = instance;
            ctx.viewport->instance = instance;
        }

        ctx.viewport->instance->denoiserDescs[denoiserCount++] = nrd::DenoiserDesc{ denoiserCount, method.method, (uint16_t)desc.width, (uint16_t)desc.height };
        ctx.viewport->width = desc.width;
        ctx.viewport->height = desc.height;
    }

    // Nothing to do, bail out
    if (ctx.viewport->instance->denoiser)
    {
        return Result::eOk;
    }

    if (!ctx.prepareDataKernel)
    {
        RenderAPI platform = RenderAPI::eD3D12;
        ctx.compute->getRenderAPI(platform);
        if (platform == RenderAPI::eVulkan)
        {
            CHI_CHECK_RR(ctx.compute->createKernel((void*)nrd_prep_spv, nrd_prep_spv_len, "nrd_prep.cs", "main", ctx.prepareDataKernel));
            CHI_CHECK_RR(ctx.compute->createKernel((void*)nrd_pack_spv, nrd_pack_spv_len, "nrd_pack.cs", "main", ctx.packDataKernel));
        }
        else
        {
            CHI_CHECK_RR(ctx.compute->createKernel((void*)nrd_prep_cs, nrd_prep_cs_len, "nrd_prep.cs", "main", ctx.prepareDataKernel));
            CHI_CHECK_RR(ctx.compute->createKernel((void*)nrd_pack_cs, nrd_pack_cs_len, "nrd_pack.cs", "main", ctx.packDataKernel));
        }
    }
#if 0
    for (uint32_t i = 0; i < denoiserCount; i++)
    {
        SL_LOG_HINT("Requested NRDContext method %s (%u,%u) for viewport %u", allMethodNames[(int)ctx.viewport->instance->denoiserDescs[i].method], ctx.viewport->instance->denoiserDescs[i].fullResolutionWidth, ctx.viewport->instance->denoiserDescs[i].fullResolutionHeight, ctx.viewport->id);
        ctx.viewport->description += extra::format(" - {} ({},{})", allMethodNames[(int)ctx.viewport->instance->denoiserDescs[i].method], ctx.viewport->instance->denoiserDescs[i].fullResolutionWidth, ctx.viewport->instance->denoiserDescs[i].fullResolutionHeight);
    }
#endif 
    ctx.viewport->instance->methodMask = ctx.nrdConsts->methodMask;
    ctx.viewport->instance->denoiserCount = denoiserCount;

    nrd::InstanceCreationDesc instanceCreationDesc = {};
    instanceCreationDesc.denoisers = ctx.viewport->instance->denoiserDescs;
    instanceCreationDesc.denoisersNum = ctx.viewport->instance->denoiserCount;
    if (ctx.createInstance(instanceCreationDesc, ctx.viewport->instance->denoiser) != nrd::Result::SUCCESS)
    {
        return Result::eErrorNRDAPI;
    }

    ctx.viewport->instance->taggedInputBuffersStates.resize(kNrdInputBufferTagCount, chi::ResourceState::eUndefined);
    ctx.viewport->instance->taggedOutputBuffersStates.resize(kNrdOutputBufferTagCount, chi::ResourceState::eUndefined);

    nrd::InstanceDesc instanceDesc = {};
    instanceDesc = ctx.getInstanceDesc(*ctx.viewport->instance->denoiser);

    auto convertNRDTextureDesc = [](const nrd::TextureDesc& nrdTexDesc) -> chi::ResourceDescription
    {
        chi::ResourceDescription texDesc = {};
        texDesc.format = convertNRDFormat(nrdTexDesc.format);
        texDesc.height = nrdTexDesc.height;
        texDesc.width = nrdTexDesc.width;
        texDesc.mips = nrdTexDesc.mipNum;
        texDesc.state = chi::ResourceState::eTextureRead;
        return texDesc;
    };

    ctx.viewport->instance->permanentTextures.resize(instanceDesc.permanentPoolSize);
    ctx.viewport->instance->permanentTexturesStates.resize(instanceDesc.permanentPoolSize, chi::ResourceState::eUndefined);

    for (uint32_t texID = 0; texID < instanceDesc.permanentPoolSize; ++texID)
    {
        char buffer[64];
        snprintf(buffer, 64, "sl.ctx.permanentTexture[%d]", texID);
        auto texDesc = convertNRDTextureDesc(instanceDesc.permanentPool[texID]);
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->instance->permanentTextures[texID], buffer));
    }

    ctx.viewport->instance->transientTextures.resize(instanceDesc.transientPoolSize);
    ctx.viewport->instance->transientTexturesStates.resize(instanceDesc.transientPoolSize, chi::ResourceState::eUndefined);

    for (uint32_t texID = 0; texID < instanceDesc.transientPoolSize; ++texID)
    {
        char buffer[64];
        snprintf(buffer, 64, "sl.ctx.transientTexture[%d]", texID);
        auto texDesc = convertNRDTextureDesc(instanceDesc.transientPool[texID]);
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->instance->transientTextures[texID], buffer));
    }

    RenderAPI platform = RenderAPI::eD3D12;
    ctx.compute->getRenderAPI(platform);
    
    ctx.viewport->instance->shaders.resize(instanceDesc.pipelinesNum);
    for (uint32_t shaderID = 0; shaderID < instanceDesc.pipelinesNum; ++shaderID)
    {
        const nrd::PipelineDesc& pipeline = instanceDesc.pipelines[shaderID];
        if (platform == RenderAPI::eVulkan)
        {
            CHI_VALIDATE(ctx.compute->createKernel((void*)pipeline.computeShaderSPIRV.bytecode, (uint32_t)pipeline.computeShaderSPIRV.size, pipeline.shaderFileName, pipeline.shaderEntryPointName, ctx.viewport->instance->shaders[shaderID]));
        }
        else
        {
            CHI_VALIDATE(ctx.compute->createKernel((void*)pipeline.computeShaderDXBC.bytecode, (uint32_t)pipeline.computeShaderDXBC.size, pipeline.shaderFileName, pipeline.shaderEntryPointName, ctx.viewport->instance->shaders[shaderID]));
        }
    }

    chi::ResourceDescription texDesc = {};
    texDesc.width = ctx.viewport->width;
    texDesc.height = ctx.viewport->height;
    texDesc.mips = 1;
    texDesc.state = chi::ResourceState::eTextureRead;

    texDesc.format = chi::eFormatR32F;
    if (!ctx.viewport->viewZ)
    {
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->viewZ, "sl.ctx.viewZ"));
    }

    texDesc.format = chi::eFormatRGBA16F;
    if (!ctx.viewport->mvec)
    {
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->mvec, "sl.ctx.mvec"));
    }

    for (auto i = static_cast<uint32_t>(Encodable::eDiffuseRadianceHitDist);
        i != static_cast<uint32_t>(Encodable::eCount); ++i)
    {
        if (ctx.viewport->instance->enabledInputResources[i] && !ctx.viewport->inputs[i])
        {
            EncodableInfo encodableInfo = kEncodableInfos[i];
            texDesc.format = encodableInfo.format;
            CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->inputs[i], encodableInfo.debugName));
        }
    }
    return Result::eOk;
}

Result nrdBeginEvent(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*nrdsl::getContext());

    if (!common::getConsts(data, &ctx.commonConsts))
    {
        return Result::eErrorMissingConstants;
    }

    if (!nrdGetConstants(data, &ctx.nrdConsts))
    {
        return Result::eErrorMissingConstants;
    }

    // Initialize or rebuild if resized
    initializeNRD(cmdList, data, inputs, numInputs);
    return Result::eOk;
}

struct CommonResourcePack
{
    CommonResource& mvec, &depth, &normalRoughness;
};

// Prepare data
sl::Result prepare_data(sl::nrdsl::NRDContext& ctx, CommonResourcePack pack, chi::CommandList cmdList)
{
    struct PrepareDataCB
    {
        float4x4 clipToPrevClip;
        float4x4 invProj;
        float4x4 screenToWorld;
        float4x4 screenToWorldPrev;
        float4 sizeAndInvSize;
        float4 hitDistParams;
        uint32_t frameId;
        uint32_t enableWorldMotion;
        uint32_t enableCheckerboard;
        uint32_t cameraMotionIncluded;
        uint32_t relax;

        static_assert(alignof(EncoderCB) == alignof(uint32_t));
        EncoderCB encoding;
    };
    PrepareDataCB cb;

    float w = ctx.viewport->width * ctx.nrdConsts->common.resolutionScale[0];
    float h = ctx.viewport->height * ctx.nrdConsts->common.resolutionScale[1];

    cb.clipToPrevClip = (ctx.commonConsts->clipToPrevClip);
    cb.invProj = (ctx.commonConsts->clipToCameraView);
    cb.screenToWorld = (ctx.nrdConsts->clipToWorld);
    cb.screenToWorldPrev = (ctx.nrdConsts->clipToWorldPrev);
    cb.sizeAndInvSize = { w, h, 1.0f / w, 1.0f / h };
    cb.hitDistParams = { ctx.nrdConsts->reblurSettings.hitDistanceParameters.A, ctx.nrdConsts->reblurSettings.hitDistanceParameters.B,ctx.nrdConsts->reblurSettings.hitDistanceParameters.C, ctx.nrdConsts->reblurSettings.hitDistanceParameters.D };
    cb.frameId = ctx.viewport->frameIndex;
    cb.enableWorldMotion = ctx.commonConsts->motionVectors3D;
    cb.enableCheckerboard = ctx.nrdConsts->reblurSettings.checkerboardMode != NRDCheckerboardMode::OFF;
    cb.cameraMotionIncluded = ctx.commonConsts->cameraMotionIncluded;
    cb.relax = ctx.viewport->instance->relax;

    for (auto i = static_cast<uint32_t>(Encodable::eDiffuseRadianceHitDist);
        i != static_cast<uint32_t>(Encodable::eCount); ++i)
    {
        reinterpret_cast<uint32_t*>(&cb.encoding)[i] = static_cast<uint32_t>(ctx.viewport->instance->enabledInputResources[i]);
    }

    // We can override to allow data pass through for testing
    json& config = *(json*)api::getContext()->extConfig;
    if (config.contains("relax"))
    {
        // Relax option is just passing through data as it is
        config.at("relax").get_to(cb.relax);
    }

    // Prepare
    {
        extra::ScopedTasks transitions;
        chi::ResourceTransition trans[] =
        {
            {pack.mvec, chi::ResourceState::eTextureRead, ctx.cachedStates[pack.mvec]},
            {pack.depth, chi::ResourceState::eTextureRead, ctx.cachedStates[pack.depth]},
            {ctx.viewport->mvec, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
            {ctx.viewport->viewZ, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
        };
        ctx.compute->transitionResources(cmdList, trans, (uint32_t)countof(trans), &transitions);

        CHI_VALIDATE(ctx.compute->bindKernel(ctx.prepareDataKernel));
        CHI_VALIDATE(ctx.compute->bindConsts(0, 0, &cb, sizeof(PrepareDataCB),
            3 * (uint32_t)ctx.viewport->instances.size() * (uint32_t)ctx.viewports.size()));
        CHI_VALIDATE(ctx.compute->bindSampler(1, 0, chi::eSamplerLinearClamp));
        CHI_VALIDATE(ctx.compute->bindTexture(2, 0, pack.depth));
        CHI_VALIDATE(ctx.compute->bindTexture(3, 1, pack.mvec));
        CHI_VALIDATE(ctx.compute->bindRWTexture(4, 0, ctx.viewport->mvec));
        CHI_VALIDATE(ctx.compute->bindRWTexture(5, 1, ctx.viewport->viewZ));
        uint32_t grid[] = { ((uint32_t)w + 16 - 1) / 16, ((uint32_t)h + 16 - 1) / 16, 1 };
        CHI_VALIDATE(ctx.compute->dispatch(grid[0], grid[1], grid[2]));
    }

    // Pack
    {
        extra::ScopedTasks transitions;
        std::vector<chi::ResourceTransition> trans{
            {pack.normalRoughness, chi::ResourceState::eTextureRead, ctx.cachedStates[pack.normalRoughness]},
        };

        for (auto& resource : ctx.viewport->inputs)
        {
            trans.push_back({ resource, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead });
        }

        ctx.compute->transitionResources(cmdList, trans.data(), static_cast<uint32_t>(trans.size()), &transitions);

        CHI_VALIDATE(ctx.compute->bindKernel(ctx.packDataKernel));
        CHI_VALIDATE(ctx.compute->bindConsts(0, 0, &cb, sizeof(PrepareDataCB),
            3 * (uint32_t)ctx.viewport->instances.size() * (uint32_t)ctx.viewports.size()));
        CHI_VALIDATE(ctx.compute->bindSampler(1, 0, chi::eSamplerLinearClamp));
        CHI_VALIDATE(ctx.compute->bindTexture(2, 0, ctx.viewport->viewZ));
        CHI_VALIDATE(ctx.compute->bindTexture(3, 1, pack.normalRoughness));

        std::vector<CommonResource> commonResources{ static_cast<uint32_t>(Encodable::eCount), CommonResource{} };
        for (auto i = static_cast<uint32_t>(Encodable::eDiffuseRadianceHitDist);
            i != static_cast<uint32_t>(Encodable::eCount); ++i)
        {
            if (ctx.viewport->instance->enabledInputResources[i])
            {
                auto resourceType = cast2ResourceType(static_cast<Encodable>(i));
                auto bufferType = convert2BufferType(resourceType);

                SL_CHECK(getTaggedResource(bufferType, commonResources[i], ctx.viewport->id));
                ctx.cacheState(commonResources[i], commonResources[i].getState());

                CHI_VALIDATE(ctx.compute->bindTexture(4 + i, 2 + i, commonResources[i]));
            }
        }
        for (auto i = static_cast<uint32_t>(Encodable::eDiffuseRadianceHitDist);
            i != static_cast<uint32_t>(Encodable::eCount); ++i)
        {
            CHI_VALIDATE(ctx.compute->bindRWTexture(13 + i, i, ctx.viewport->inputs[i]));
        }
        if (cb.enableCheckerboard)
        {
            w = w / 2.0f;
        }
        uint32_t grid[] = { ((uint32_t)w + 16 - 1) / 16, ((uint32_t)h + 16 - 1) / 16, 1 };
        CHI_VALIDATE(ctx.compute->dispatch(grid[0], grid[1], grid[2]));
        // transition back ????
    }

    return Result::eOk;
}

sl::Result get_resource_info(
    sl::nrdsl::NRDContext& ctx,
    nrd::ResourceDesc const& resource,
    const sl::BaseStructure** inputs,
    uint32_t numInputs,
    sl::CommonResource& outResource)
{
    switch (resource.type)
    {
    case nrd::ResourceType::IN_MV:
        SL_CHECK(getTaggedResource(kBufferTypeMotionVectors, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_NORMAL_ROUGHNESS:
        SL_CHECK(getTaggedResource(kBufferTypeNormalRoughness, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_VIEWZ:
        SL_CHECK(getTaggedResource(kBufferTypeDepth, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeInDiffuseRadianceHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeInSpecularRadianceHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DIFF_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeInDiffuseHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_SPEC_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeInSpecularHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DIFF_DIRECTION_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeInDiffuseDirectionHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DIFF_SH0:
        SL_CHECK(getTaggedResource(kBufferTypeInDiffuseSH0, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DIFF_SH1:
        SL_CHECK(getTaggedResource(kBufferTypeInDiffuseSH1, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_SPEC_SH0:
        SL_CHECK(getTaggedResource(kBufferTypeInSpecularSH0, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_SPEC_SH1:
        SL_CHECK(getTaggedResource(kBufferTypeInSpecularSH1, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DIFF_CONFIDENCE:
        SL_CHECK(getTaggedResource(kBufferTypeInDiffuseConfidence, outResource, ctx.viewport->id, true, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_SPEC_CONFIDENCE:
        SL_CHECK(getTaggedResource(kBufferTypeInSpecularConfidence, outResource, ctx.viewport->id, true, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DISOCCLUSION_THRESHOLD_MIX:
        SL_CHECK(getTaggedResource(kBufferTypeInDisocclusionThresholdMix, outResource, ctx.viewport->id, true, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_BASECOLOR_METALNESS:
        SL_CHECK(getTaggedResource(kBufferTypeInBasecolorMetalness, outResource, ctx.viewport->id, true, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_SHADOWDATA:
        SL_CHECK(getTaggedResource(kBufferTypeInShadowData, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_SHADOW_TRANSLUCENCY:
        SL_CHECK(getTaggedResource(kBufferTypeInShadowTransluscency, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_RADIANCE:
        SL_CHECK(getTaggedResource(kBufferTypeInRadiance, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DELTA_PRIMARY_POS:
        SL_CHECK(getTaggedResource(kBufferTypeInDeltaPrimaryPos, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::IN_DELTA_SECONDARY_POS:
        SL_CHECK(getTaggedResource(kBufferTypeInDeltaSecondaryPos, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    }

    switch (resource.type)
    {
    case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeOutDiffuseRadianceHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeOutSpecularRadianceHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_DIFF_SH0:
        SL_CHECK(getTaggedResource(kBufferTypeOutDiffuseSH0, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_DIFF_SH1:
        SL_CHECK(getTaggedResource(kBufferTypeOutDiffuseSH1, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_SPEC_SH0:
        SL_CHECK(getTaggedResource(kBufferTypeOutSpecularSH0, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_SPEC_SH1:
        SL_CHECK(getTaggedResource(kBufferTypeOutSpecularSH1, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_DIFF_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeOutDiffuseHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_SPEC_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeOutSpecularHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_DIFF_DIRECTION_HITDIST:
        SL_CHECK(getTaggedResource(kBufferTypeOutDiffuseDirectionHitDist, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY:
        SL_CHECK(getTaggedResource(kBufferTypeOutShadowTransluscency, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_RADIANCE:
        SL_CHECK(getTaggedResource(kBufferTypeOutRadiance, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_REFLECTION_MV:
        SL_CHECK(getTaggedResource(kBufferTypeOutReflectionMv, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_DELTA_MV:
        SL_CHECK(getTaggedResource(kBufferTypeOutDeltaMv, outResource, ctx.viewport->id, false, inputs, numInputs)); return sl::Result::eOk;
    case nrd::ResourceType::OUT_VALIDATION:
        SL_CHECK(getTaggedResource(kBufferTypeOutValidation, outResource, ctx.viewport->id, true, inputs, numInputs)); return sl::Result::eOk;
    }

    if (resource.type == nrd::ResourceType::TRANSIENT_POOL)
    {
        outResource = ctx.viewport->instance->transientTextures[resource.indexInPool];
        return sl::Result::eOk;
    }

    if (resource.type == nrd::ResourceType::PERMANENT_POOL)
    {
        outResource = ctx.viewport->instance->permanentTextures[resource.indexInPool];
        return sl::Result::eOk;
    }

    return sl::Result::eErrorInvalidParameter;
}

Result nrdDispatch(
    sl::nrdsl::NRDContext& ctx, 
    chi::CommandList cmdList,  
    const nrd::DispatchDesc& dispatch, 
    const sl::BaseStructure** inputs, uint32_t numInputs,
    uint32_t dispatchDescNum)
{
    const nrd::InstanceDesc& denoiserDesc = ctx.getInstanceDesc(*ctx.viewport->instance->denoiser);
    const nrd::PipelineDesc& pipeline = denoiserDesc.pipelines[dispatch.pipelineIndex];

    extra::ScopedTasks reverseTransitions;
    std::vector<chi::ResourceTransition> transitions;
    std::vector<CommonResource> commonResources;
    commonResources.reserve(32);

    CHI_VALIDATE(ctx.compute->bindKernel(ctx.viewport->instance->shaders[dispatch.pipelineIndex]));

    for (uint32_t samplerID = 0; samplerID < denoiserDesc.samplersNum; ++samplerID)
    {
        switch (denoiserDesc.samplers[samplerID])
        {
        case nrd::Sampler::NEAREST_CLAMP:
            CHI_VALIDATE(ctx.compute->bindSampler(samplerID, denoiserDesc.samplersBaseRegisterIndex + samplerID, chi::Sampler::eSamplerPointClamp)); break;
        case nrd::Sampler::NEAREST_MIRRORED_REPEAT:
            CHI_VALIDATE(ctx.compute->bindSampler(samplerID, denoiserDesc.samplersBaseRegisterIndex + samplerID, chi::Sampler::eSamplerPointMirror)); break;
        case nrd::Sampler::LINEAR_CLAMP:
            CHI_VALIDATE(ctx.compute->bindSampler(samplerID, denoiserDesc.samplersBaseRegisterIndex + samplerID, chi::Sampler::eSamplerLinearClamp)); break;
        case nrd::Sampler::LINEAR_MIRRORED_REPEAT:
            CHI_VALIDATE(ctx.compute->bindSampler(samplerID, denoiserDesc.samplersBaseRegisterIndex + samplerID, chi::Sampler::eSamplerLinearMirror)); break;
        default: SL_LOG_ERROR("Unknown sampler detected");
        }
    }

    uint32_t slot = 0, descriptorIdx = 0;
    for (uint32_t descriptorRangeID = 0; descriptorRangeID < pipeline.resourceRangesNum; ++descriptorRangeID)
    {
        const nrd::ResourceRangeDesc& descriptorRange = pipeline.resourceRanges[descriptorRangeID];

        for (uint32_t descriptorID = 0; descriptorID < descriptorRange.descriptorsNum; ++descriptorID)
        {
            if (slot >= dispatch.resourcesNum)
            {
                SL_LOG_ERROR("Mismatch slot and resourceNum");
            }

            const nrd::ResourceDesc& resourceDesc = dispatch.resources[slot++];
            if (resourceDesc.stateNeeded != descriptorRange.descriptorType)
            {
                SL_LOG_ERROR("Mismatch stateNeeded and descriptor type");
            }

            commonResources.push_back({});
            sl::CommonResource& resource = commonResources.back();
            if (get_resource_info(ctx, resourceDesc, inputs, numInputs, resource) != Result::eOk)
            {
                SL_LOG_ERROR("Unable to find texture for nrd::ResourceType %u", resourceDesc.type);
            }

            auto transition = [&ctx](nrd::ResourceDesc const& resourceDesc) -> chi::ResourceState {
                auto from = ctx.viewport->instance->getResourceState(resourceDesc.type, resourceDesc.indexInPool);
                auto to = resourceDesc.stateNeeded == nrd::DescriptorType::TEXTURE ? chi::ResourceState::eTextureRead : chi::ResourceState::eStorageRW;

                ctx.viewport->instance->setResourceState(to, resourceDesc.type, resourceDesc.indexInPool);
                return from;
            };

            chi::ResourceState resourceState = transition(resourceDesc);
            uint32_t bindingSlot = descriptorRange.baseRegisterIndex + descriptorID;
            if (descriptorRange.descriptorType == nrd::DescriptorType::TEXTURE)
            {
                // TODO: Fix binding pos for VK
                CHI_VALIDATE(ctx.compute->bindTexture(descriptorIdx++, bindingSlot, resource, resourceDesc.mipOffset, resourceDesc.mipNum));
                transitions.push_back(chi::ResourceTransition(resource, chi::ResourceState::eTextureRead, resourceState));
            }
            else
            {
                // TODO: Fix binding pos for VK
                CHI_VALIDATE(ctx.compute->bindRWTexture(descriptorIdx++, bindingSlot, resource, resourceDesc.mipOffset));
                transitions.push_back(chi::ResourceTransition(resource, chi::ResourceState::eStorageRW, resourceState));
            }
        }
    }

    CHI_VALIDATE(ctx.compute->bindConsts(descriptorIdx++, denoiserDesc.constantBufferRegisterIndex, (void*)dispatch.constantBufferData, denoiserDesc.constantBufferMaxDataSize, 3 * dispatchDescNum));
    CHI_VALIDATE(ctx.compute->transitionResources(cmdList, transitions.data(), (uint32_t)transitions.size(), nullptr));
    CHI_VALIDATE(ctx.compute->dispatch(dispatch.gridWidth, dispatch.gridHeight, 1));

    return sl::Result::eOk;
}

Result nrdEndEvent(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*nrdsl::getContext());

    if (!ctx.viewport || !ctx.viewport->instance) return Result::eErrorMissingInputParameter;

    auto parameters = api::getContext()->parameters;

    ctx.viewport->instance->resetStateVectors();
    
    {
        CHI_VALIDATE(ctx.compute->bindSharedState(cmdList));
#if 1
        CommonResource mvec{}, depth{}, aoIn{}, diffuseIn{}, specularIn{}, normalRoughness{};

        SL_CHECK(getTaggedResource(kBufferTypeDepth, depth, ctx.viewport->id, false, inputs, numInputs));
        SL_CHECK(getTaggedResource(kBufferTypeMotionVectors, mvec, ctx.viewport->id, false, inputs, numInputs));
        SL_CHECK(getTaggedResource(kBufferTypeNormalRoughness, normalRoughness, ctx.viewport->id, false, inputs, numInputs));

        ctx.cacheState(depth, depth.getState());
        ctx.cacheState(mvec, mvec.getState());
        ctx.cacheState(normalRoughness, normalRoughness.getState());

        //SL_LOG_HINT("-----------------------------------------------------------------------------------------------------------------------------------------");
        // prepare_data();

        nrd::CommonSettings commonSettings{};
        memcpy(&commonSettings, &ctx.nrdConsts->common, sizeof(nrd::CommonSettings));

        ctx.setCommonSettings(*ctx.viewport->instance->denoiser, commonSettings);
        ctx.viewport->instance->prevCommonSettings = commonSettings;

        std::vector<nrd::Identifier> identifiers;
        for (uint32_t i = 0; i < ctx.viewport->instance->denoiserCount; i++)
        {
            identifiers.push_back(ctx.viewport->instance->denoiserDescs[i].identifier);
            switch (ctx.viewport->instance->denoiserDescs[i].denoiser)
            {
            case nrd::Denoiser::REBLUR_DIFFUSE:
            case nrd::Denoiser::REBLUR_DIFFUSE_OCCLUSION:
            case nrd::Denoiser::REBLUR_DIFFUSE_SH:
            case nrd::Denoiser::REBLUR_SPECULAR:
            case nrd::Denoiser::REBLUR_SPECULAR_OCCLUSION:
            case nrd::Denoiser::REBLUR_SPECULAR_SH:
            case nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR:
            case nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR_OCCLUSION:
            case nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR_SH:
            case nrd::Denoiser::REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION:
                ctx.setDenoiserSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->denoiserDescs[i].identifier, &ctx.nrdConsts->reblurSettings); break;
            case nrd::Denoiser::RELAX_DIFFUSE_SPECULAR:
                ctx.setDenoiserSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->denoiserDescs[i].identifier, &ctx.nrdConsts->relaxDiffuseSpecular); break;
            case nrd::Denoiser::RELAX_DIFFUSE:
                ctx.setDenoiserSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->denoiserDescs[i].identifier, &ctx.nrdConsts->relaxDiffuse); break;
            case nrd::Denoiser::RELAX_SPECULAR:
                ctx.setDenoiserSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->denoiserDescs[i].identifier, &ctx.nrdConsts->relaxSpecular); break;
            case nrd::Denoiser::SIGMA_SHADOW:
            case nrd::Denoiser::SIGMA_SHADOW_TRANSLUCENCY:
                ctx.setDenoiserSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->denoiserDescs[i].identifier, &ctx.nrdConsts->sigmaShadow); break;
            case nrd::Denoiser::REFERENCE:
            case nrd::Denoiser::SPECULAR_REFLECTION_MV:
            case nrd::Denoiser::SPECULAR_DELTA_MV:
                break;
            default:
                SL_LOG_ERROR("Could not find appropriate settings for chosen denoisers.");
            }
        }

        const nrd::DispatchDesc* dispatchDescs = nullptr;
        uint32_t dispatchDescNum = 0;
        NRD_CHECK1(ctx.getComputeDispatches(*ctx.viewport->instance->denoiser, identifiers.data(), static_cast<uint32_t>(identifiers.size()), dispatchDescs, dispatchDescNum));

#if SL_ENABLE_TIMING
        CHI_VALIDATE(ctx.compute->beginPerfSection(cmdList, "sl.nrd"));
#endif

        for (uint32_t dispatchID = 0; dispatchID < dispatchDescNum; ++dispatchID)
        {
            const nrd::DispatchDesc& dispatch = dispatchDescs[dispatchID];

            nrdDispatch(ctx, cmdList, dispatch, inputs, numInputs, dispatchDescNum);
        }
        float ms = 0;
#if SL_ENABLE_TIMING
        CHI_VALIDATE(ctx.compute->endPerfSection(cmdList, "sl.nrd", ms));
#endif

        parameters->set(sl::param::nrd::kMVecBuffer, ctx.viewport->mvec);
        parameters->set(sl::param::nrd::kViewZBuffer, ctx.viewport->viewZ);

        {
            /*static std::string s_stats;
            auto v = api::getContext()->pluginVersion;
            std::string description{};
            for (auto& v : ctx.viewports)
            {
                description += v.second->description + " ";
            }
            s_stats = extra::format("sl.nrd {} - {}- {}ms", v.toStr() + "." + GIT_LAST_COMMIT_SHORT, description, ms);
            parameters->set(sl::param::nrd::kStats, (void*)s_stats.c_str());*/

            uint32_t frame = 0;
            ctx.compute->getFinishedFrameIndex(frame);
            parameters->set(sl::param::nrd::kCurrentFrame, frame + 1);
        }
#else
        chi::Resource specularIn, specularOut, normRough, mvec, viewZ;
        getPointerParam(parameters, param::global::kSpecularNoisyBuffer, &specularIn);
        getPointerParam(parameters, param::global::kSpecularDenoisedBuffer, &specularOut);
        getPointerParam(parameters, param::global::kDiffuseNoisyBuffer, &viewZ);
        getPointerParam(parameters, param::global::kDiffuseDenoisedBuffer, &mvec);
        getPointerParam(parameters, param::global::kNormalRoughnessBuffer, &normRough);
#endif

#ifndef SL_PRODUCTION
        //static bool _dump = false;
        //if (sl::extra::wasKeyPressed(sl::extra::VirtKey(VK_OEM_5, true, true))) // '\|' for US
        //{
        //    _dump = !_dump;
        //    SL_LOG_HINT("Dumping %s", _dump ? "on" : "off");
        //}
        //if (_dump && specularOut)
        //{
        //    auto idx = std::to_string(ctx.nrdConsts->common.frameIndex);
        //    //ctx.compute->dumpResource(cmdList, mvec, std::string("f:/tmp/correct/mvec" + idx).c_str());
        //    //ctx.compute->dumpResource(cmdList, viewZ, std::string("f:/tmp/correct/viewZ" + idx).c_str());
        //    //ctx.compute->dumpResource(cmdList, normRough, std::string("f:/tmp/correct/normRough" + idx).c_str());
        //    //ctx.compute->dumpResource(cmdList, specularOut, std::string("f:/tmp/correct/specularOut" + idx).c_str());
        //    //ctx.compute->dumpResource(cmdList, specularIn, std::string("f:/tmp/correct/specularIn" + idx).c_str());
        //    ctx.compute->dumpResource(cmdList, normalRoughness, std::string("f:/tmp/normalRoughness" + idx).c_str());
        //    ctx.compute->dumpResource(cmdList, ctx.viewport->mvec, std::string("f:/tmp/mvec" + idx).c_str());
        //    ctx.compute->dumpResource(cmdList, ctx.viewport->packedSpec, std::string("f:/tmp/inSpecHit" + idx).c_str());
        //    ctx.compute->dumpResource(cmdList, specularOut, std::string("f:/tmp/outSpecHit" + idx).c_str());
        //    ctx.compute->dumpResource(cmdList, ctx.viewport->viewZ, std::string("f:/tmp/viewZ" + idx).c_str());
        //}
#endif
    }
    return Result::eOk;
}

//! -------------------------------------------------------------------------------------------------
//! Required interface


Result slAllocateResources(sl::CommandBuffer* cmdBuffer, Feature feature, const sl::ViewportHandle& viewport)
{
    auto& ctx = (*nrdsl::getContext());
    common::EventData data{ viewport, 0 };
    nrdBeginEvent(cmdBuffer, data, nullptr, 0);
    auto it = ctx.viewports.find(viewport);
    return it != ctx.viewports.end() && !(*it).second->instances.empty() ? Result::eOk : Result::eErrorInvalidParameter;
}

Result slFreeResources(Feature feature, const sl::ViewportHandle& viewport)
{
    auto& ctx = (*nrdsl::getContext());
    auto it = ctx.viewports.find(viewport);
    if(it != ctx.viewports.end())
    {
        if ((*it).second->id == viewport)
        {
            destroyNRDViewport((*it).second);
            delete (*it).second;
            ctx.viewports.erase(it);
        }
        return Result::eOk;
    }
    return Result::eErrorInvalidParameter;
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

    auto& ctx = (*nrdsl::getContext());

    auto parameters = api::getContext()->parameters;

    if (!getPointerParam(parameters, param::common::kComputeAPI, &ctx.compute))
    {
        SL_LOG_ERROR( "Can't find %s", param::common::kComputeAPI);
        return false;
    }
    
    RenderAPI platform;
    ctx.compute->getRenderAPI(platform);

    // Set callbacks from the sl.common
    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &ctx.registerEvaluateCallbacks))
    {
        SL_LOG_ERROR( "Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        return false;
    }
    ctx.registerEvaluateCallbacks(kFeatureNRD, nrdBeginEvent, nrdEndEvent);

    json& config = *(json*)api::getContext()->loaderConfig;
    int appId = 0;
    config.at("appId").get_to(appId);

    // Path where our modules are located
    wchar_t *pluginPath = {};
    param::getPointerParam(parameters, param::global::kPluginPath, &pluginPath);
    if (!pluginPath)
    {
        SL_LOG_ERROR( "Cannot find path to plugins");
        return false;
    }
    // Now let's load our NRDContext module
    std::wstring path(pluginPath);
    path += L"/nrd.dll";
    ctx.lib = security::loadLibrary(path.c_str());
    if (!ctx.lib)
    {
        SL_LOG_ERROR( "Failed to load %S", path.c_str());
        return false;
    }

    ctx.createInstance = (PFunCreateInstance*)GetProcAddress(ctx.lib, "CreateInstance");
    ctx.destroyInstance = (PFunDestroyInstance*)GetProcAddress(ctx.lib, "DestroyInstance");
    ctx.getLibraryDesc = (PFunGetLibraryDesc*)GetProcAddress(ctx.lib, "GetLibraryDesc");
    ctx.getInstanceDesc = (PFunGetInstanceDesc*)GetProcAddress(ctx.lib, "GetInstanceDesc");
    ctx.setCommonSettings = (PFunSetCommonSettings*)GetProcAddress(ctx.lib, "SetCommonSettings");
    ctx.setDenoiserSettings = (PFunSetDenoiserSettings*)GetProcAddress(ctx.lib, "SetDenoiserSettings");
    ctx.getComputeDispatches = (PFunGetComputeDispatches*)GetProcAddress(ctx.lib, "GetComputeDispatches");

    if (!ctx)
    {
        SL_LOG_ERROR( "Failed to map NRDContext API in %S", path.c_str());
        return false;
    }

    // At this point we are good to go!
    return true;
}

//! Plugin shutdown
//!
//! Called by loader when unloading the plugin
void slOnPluginShutdown()
{
    destroyNRD();

    auto& ctx = (*nrdsl::getContext());
    FreeLibrary(ctx.lib);

    ctx.registerEvaluateCallbacks(kFeatureNRD, nullptr, nullptr);
    
    // Common shutdown
    plugin::onShutdown(api::getContext());
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
    SL_EXPORT_FUNCTION(slAllocateResources);
    SL_EXPORT_FUNCTION(slFreeResources);

    SL_EXPORT_FUNCTION(slNRDSetConstants);

    return nullptr;
}
}

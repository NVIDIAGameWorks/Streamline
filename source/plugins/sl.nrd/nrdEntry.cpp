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
#include "include/sl_nrd.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "external/json/include/nlohmann/json.hpp"

#include "source/platforms/sl.chi/d3d12.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "source/plugins/sl.nrd/versions.h"

#include "external/nrd/Include/NRD.h"
#include "shaders/nrd_prep_cs.h"
#include "shaders/nrd_pack_cs.h"
#include "shaders/nrd_prep_spv.h"
#include "shaders/nrd_pack_spv.h"
#include "_artifacts/gitVersion.h"

#define NRD_CHECK(f) {nrd::Result r = f;if(r != nrd::Result::SUCCESS) { SL_LOG_ERROR("%s failed error %u",#f,r); return false;}};
#define NRD_CHECK1(f) {nrd::Result r = f;if(r != nrd::Result::SUCCESS) { SL_LOG_ERROR("%s failed error %u",#f,r); return;}};

using json = nlohmann::json;
namespace sl
{

using PFunGetLibraryDesc = const nrd::LibraryDesc&();
using PFunCreateDenoiser = nrd::Result(const nrd::DenoiserCreationDesc& denoiserCreationDesc, nrd::Denoiser*& denoiser);
using PFunGetDenoiserDesc = const nrd::DenoiserDesc&(const nrd::Denoiser& denoiser);
using PFunSetMethodSettings = nrd::Result(nrd::Denoiser& denoiser, nrd::Method method, const void* methodSettings);
using PFunGetComputeDispatches = nrd::Result(nrd::Denoiser& denoiser, const nrd::CommonSettings& commonSettings, const nrd::DispatchDesc*& dispatchDescs, uint32_t& dispatchDescNum);
using PFunDestroyDenoiser = void(nrd::Denoiser& denoiser);

struct NRDInstance
{
    nrd::CommonSettings prevCommonSettings{};
    std::vector<chi::Resource> permanentTextures;
    std::vector<chi::Resource> transientTextures;
    std::vector<chi::Kernel> shaders;
    nrd::Denoiser* denoiser = {};
    uint32_t methodMask = 0;
    nrd::MethodDesc methodDescs[6];
    uint32_t methodCount = 0;
    bool enableAO = false;
    bool enableDiffuse = false;
    bool enableSpecular = false;
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
    chi::Resource packedAO{};
    chi::Resource packedSpec{};
    chi::Resource packedDiff{};
    std::string description = "";
};

struct NRD
{
    HMODULE lib{};
    PFunGetLibraryDesc* getLibraryDesc{};
    PFunCreateDenoiser* createDenoiser{};
    PFunGetDenoiserDesc* getDenoiserDesc{};
    PFunSetMethodSettings* setMethodSettings{};
    PFunGetComputeDispatches* getComputeDispatches{};
    PFunDestroyDenoiser* destroyDenoiser{};

    operator bool() const 
    { 
        return getLibraryDesc && createDenoiser && getDenoiserDesc && 
               setMethodSettings && getComputeDispatches && destroyDenoiser; 
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
    common::ViewportIdFrameData<> constsPerViewport = {"nrd"};

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
NRD s_nrd = {};

void destroyNRDViewport(NRDViewport*);

bool slSetConstants(const void* data, uint32_t frameIndex, uint32_t id)
{
    auto consts = (const sl::NRDConstants*)data;
    s_nrd.constsPerViewport.set(frameIndex, id, consts);
#ifndef SL_PRODUCTION
    if (consts->methodMask == 0 && !s_nrd.viewports.empty())
    {
        auto viewports = s_nrd.viewports;
        auto lambda = [viewports](void)->void
        {
            for (auto& v : viewports)
            {
                destroyNRDViewport(v.second);
                delete v.second;
            }
            s_nrd.viewports.clear();

            CHI_VALIDATE(s_nrd.compute->destroyKernel(s_nrd.prepareDataKernel));
            CHI_VALIDATE(s_nrd.compute->destroyKernel(s_nrd.packDataKernel));
        };
        CHI_VALIDATE(s_nrd.compute->destroy(lambda));
        s_nrd.viewports.clear();
    }
#endif
    return true;
}

bool nrdGetConstants(const common::EventData& data, NRDConstants** consts)
{
    return s_nrd.constsPerViewport.get(data, consts);
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
    SL_LOG_INFO("Destroying NRD instance with method mask %u", inst->methodMask);
    s_nrd.destroyDenoiser(*inst->denoiser);
    
    for (auto& shader : inst->shaders)
    {
        CHI_VALIDATE(s_nrd.compute->destroyKernel(shader));
    }
    inst->shaders.clear();
    for (auto& res : inst->permanentTextures)
    {
        CHI_VALIDATE(s_nrd.compute->destroyResource(res));
    }
    inst->permanentTextures.clear();
    for (auto& res : inst->transientTextures)
    {
        CHI_VALIDATE(s_nrd.compute->destroyResource(res));
    }
    inst->transientTextures.clear();
    delete inst;
}

void destroyNRDViewport(NRDViewport* viewport)
{
    for (auto& inst : viewport->instances)
    {
        destroyNRDInstance(inst.second);
    }
    viewport->instances.clear();
    viewport->instance = {};

    viewport->description = "";

    CHI_VALIDATE(s_nrd.compute->destroyResource(viewport->viewZ));
    CHI_VALIDATE(s_nrd.compute->destroyResource(viewport->mvec));
    CHI_VALIDATE(s_nrd.compute->destroyResource(viewport->packedAO));
    CHI_VALIDATE(s_nrd.compute->destroyResource(viewport->packedSpec));
    CHI_VALIDATE(s_nrd.compute->destroyResource(viewport->packedDiff));

    s_nrd.cachedStates.clear();
}

void destroyNRD()
{
    for (auto& v : s_nrd.viewports)
    {
        destroyNRDViewport(v.second);
        delete v.second;
    }
    s_nrd.viewports.clear();

    CHI_VALIDATE(s_nrd.compute->destroyKernel(s_nrd.prepareDataKernel));
    CHI_VALIDATE(s_nrd.compute->destroyKernel(s_nrd.packDataKernel));
}

bool initializeNRD(chi::CommandList cmdList, const common::EventData& data)
{
    {
        auto it = s_nrd.viewports.find(data.id);
        if (it == s_nrd.viewports.end())
        {
            s_nrd.viewports[data.id] = new NRDViewport{};
        }
        s_nrd.viewport = s_nrd.viewports[data.id];
        s_nrd.viewport->id = data.id;
        s_nrd.viewport->frameIndex = data.frame;
    }

    if (!s_nrd.nrdConsts->methodMask)
    {
        SL_LOG_WARN("NRD disabled, if this is not intentional please update methodMask bit field.");
        return true; // NRD disabled which is OK
    }

    s_nrd.cachedStates[nullptr] = chi::ResourceState::eGeneral;

    s_nrd.viewport->instance = {};
    auto it = s_nrd.viewport->instances.find(s_nrd.nrdConsts->methodMask);
    if (it != s_nrd.viewport->instances.end())
    {
        s_nrd.viewport->instance = (*it).second;
    }
    else
    {
        s_nrd.viewport->instance = new NRDInstance();
        *s_nrd.viewport->instance = {};
        s_nrd.viewport->instances[s_nrd.nrdConsts->methodMask] = s_nrd.viewport->instance;
    }

    auto parameters = api::getContext()->parameters;

    uint32_t mask = s_nrd.nrdConsts->methodMask;
    uint32_t i = 0;
    nrd::Method allMethods[] = 
    { 
        nrd::Method::REBLUR_DIFFUSE,
        nrd::Method::REBLUR_DIFFUSE_OCCLUSION,
        nrd::Method::REBLUR_SPECULAR,
        nrd::Method::REBLUR_SPECULAR_OCCLUSION,
        nrd::Method::REBLUR_DIFFUSE_SPECULAR,
        nrd::Method::REBLUR_DIFFUSE_SPECULAR_OCCLUSION,
        nrd::Method::REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION,
        nrd::Method::SIGMA_SHADOW,
        nrd::Method::SIGMA_SHADOW_TRANSLUCENCY,
        nrd::Method::RELAX_DIFFUSE,
        nrd::Method::RELAX_SPECULAR,
        nrd::Method::RELAX_DIFFUSE_SPECULAR,
        nrd::Method::REFERENCE,
        nrd::Method::SPECULAR_REFLECTION_MV,
        nrd::Method::SPECULAR_DELTA_MV,
    };
    const char* allMethodNames[] =
    {
        "REBLUR_DIFFUSE",
        "REBLUR_DIFFUSE_OCCLUSION",
        "REBLUR_SPECULAR",
        "REBLUR_SPECULAR_OCCLUSION",
        "REBLUR_DIFFUSE_SPECULAR",
        "REBLUR_DIFFUSE_SPECULAR_OCCLUSION",
        "REBLUR_DIFFUSE_DIRECTIONAL_OCCLUSION",
        "SIGMA_SHADOW",
        "SIGMA_SHADOW_TRANSLUCENCY",
        "RELAX_DIFFUSE",
        "RELAX_SPECULAR",
        "RELAX_DIFFUSE_SPECULAR",
        "REFERENCE",
        "SPECULAR_REFLECTION_MV",
        "SPECULAR_DELTA_MV",
    };

    uint32_t methodCount = 0;
    // In SL we have off method at index 0 so skip that
    mask = mask >> 1;
    while (mask)
    { 
        if (mask & 0x01)
        {
            BufferType pname = (BufferType)-1;
            switch(allMethods[i])
            {
                case nrd::Method::RELAX_DIFFUSE_SPECULAR:
                    s_nrd.viewport->instance->relax = true;
                case nrd::Method::REBLUR_DIFFUSE_SPECULAR:
                    s_nrd.viewport->instance->enableSpecular = true;
                    s_nrd.viewport->instance->enableDiffuse = true;
                    pname = eBufferTypeDiffuseHitNoisy;
                    break;
                case nrd::Method::RELAX_DIFFUSE: 
                    s_nrd.viewport->instance->relax = true;
                case nrd::Method::REBLUR_DIFFUSE: 
                    pname = eBufferTypeDiffuseHitNoisy;
                    s_nrd.viewport->instance->enableDiffuse = true;
                    break;
                case nrd::Method::RELAX_SPECULAR:
                    s_nrd.viewport->instance->relax = true;
                case nrd::Method::REBLUR_SPECULAR:
                    pname = eBufferTypeSpecularHitNoisy;
                    s_nrd.viewport->instance->enableSpecular = true;
                    break;
                case nrd::Method::SIGMA_SHADOW_TRANSLUCENCY:
                case nrd::Method::SIGMA_SHADOW:
                    pname = eBufferTypeShadowNoisy; 
                    break;
                case nrd::Method::REBLUR_DIFFUSE_OCCLUSION:
                    pname = eBufferTypeAmbientOcclusionNoisy;
                    s_nrd.viewport->instance->enableAO = true;
                    break;
            };
 
            chi::Resource res = {};
            if (!getTaggedResource(pname, res, s_nrd.viewport->id))
            {
                SL_LOG_ERROR("Failed to find resource '%s', please make sure to tag all NRD resources", pname);
                return false;
            }
            chi::ResourceDescription desc = {};
            CHI_CHECK_RF(s_nrd.compute->getResourceDescription(res, desc));
            
            if (s_nrd.viewport->instance->denoiser && (s_nrd.viewport->width != desc.width || s_nrd.viewport->height != desc.height))
            {
                auto viewport = new NRDViewport{};
                viewport->id = s_nrd.viewport->id;
                auto instance = new NRDInstance{};
                instance->enableAO = s_nrd.viewport->instance->enableAO;
                instance->enableSpecular = s_nrd.viewport->instance->enableSpecular;
                instance->enableDiffuse = s_nrd.viewport->instance->enableDiffuse;
                destroyNRDViewport(s_nrd.viewport);
                s_nrd.viewports[viewport->id] = viewport;
                s_nrd.viewport = viewport;
                s_nrd.viewport->instances[s_nrd.nrdConsts->methodMask] = instance;
                s_nrd.viewport->instance = instance;
            }

            s_nrd.viewport->instance->methodDescs[methodCount++] = nrd::MethodDesc{ allMethods[i], (uint16_t)desc.width, (uint16_t)desc.height };
            s_nrd.viewport->width = desc.width;
            s_nrd.viewport->height = desc.height;
        }
        mask = mask >> 1;
        i++;
    }

    // Nothing to do, bail out
    if (s_nrd.viewport->instance->denoiser)
    {
        return true;
    }

    if (!s_nrd.prepareDataKernel)
    {
        chi::PlatformType platform = chi::ePlatformTypeD3D12;
        s_nrd.compute->getPlatformType(platform);
        if (platform == chi::ePlatformTypeVK)
        {
            CHI_CHECK_RF(s_nrd.compute->createKernel((void*)nrd_prep_spv, nrd_prep_spv_len, "nrd_prep.cs", "main", s_nrd.prepareDataKernel));
            CHI_CHECK_RF(s_nrd.compute->createKernel((void*)nrd_pack_spv, nrd_pack_spv_len, "nrd_pack.cs", "main", s_nrd.packDataKernel));
        }
        else
        {
            CHI_CHECK_RF(s_nrd.compute->createKernel((void*)nrd_prep_cs, nrd_prep_cs_len, "nrd_prep.cs", "main", s_nrd.prepareDataKernel));
            CHI_CHECK_RF(s_nrd.compute->createKernel((void*)nrd_pack_cs, nrd_pack_cs_len, "nrd_pack.cs", "main", s_nrd.packDataKernel));
        }
    }

    for (uint32_t i = 0; i < methodCount; i++)
    {
        SL_LOG_HINT("Requested NRD method %s (%u,%u) for viewport %u", allMethodNames[(int)s_nrd.viewport->instance->methodDescs[i].method], s_nrd.viewport->instance->methodDescs[i].fullResolutionWidth, s_nrd.viewport->instance->methodDescs[i].fullResolutionHeight, s_nrd.viewport->id);
        s_nrd.viewport->description += extra::format(" - {} ({},{})", allMethodNames[(int)s_nrd.viewport->instance->methodDescs[i].method], s_nrd.viewport->instance->methodDescs[i].fullResolutionWidth, s_nrd.viewport->instance->methodDescs[i].fullResolutionHeight);
    }

    s_nrd.viewport->instance->methodMask = s_nrd.nrdConsts->methodMask;
    s_nrd.viewport->instance->methodCount = methodCount;

    nrd::DenoiserCreationDesc denoiserCreationDesc = {};
    denoiserCreationDesc.requestedMethods = s_nrd.viewport->instance->methodDescs;
    denoiserCreationDesc.requestedMethodNum = methodCount;
    NRD_CHECK(s_nrd.createDenoiser(denoiserCreationDesc, s_nrd.viewport->instance->denoiser));

    nrd::DenoiserDesc denoiserDesc = {};
    denoiserDesc = s_nrd.getDenoiserDesc(*s_nrd.viewport->instance->denoiser);

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

    s_nrd.viewport->instance->permanentTextures.resize(denoiserDesc.permanentPoolSize);
    for (uint32_t texID = 0; texID < denoiserDesc.permanentPoolSize; ++texID)
    {
        char buffer[64];
        snprintf(buffer, 64, "sl.s_nrd.permanentTexture[%d]", texID);
        auto texDesc = convertNRDTextureDesc(denoiserDesc.permanentPool[texID]);
        CHI_VALIDATE(s_nrd.compute->createTexture2D(texDesc, s_nrd.viewport->instance->permanentTextures[texID], buffer));
    }

    s_nrd.viewport->instance->transientTextures.resize(denoiserDesc.transientPoolSize);
    for (uint32_t texID = 0; texID < denoiserDesc.transientPoolSize; ++texID)
    {
        char buffer[64];
        snprintf(buffer, 64, "sl.s_nrd.transientTexture[%d]", texID);
        auto texDesc = convertNRDTextureDesc(denoiserDesc.transientPool[texID]);
        CHI_VALIDATE(s_nrd.compute->createTexture2D(texDesc, s_nrd.viewport->instance->transientTextures[texID], buffer));
    }

    chi::PlatformType platform = chi::ePlatformTypeD3D12;
    s_nrd.compute->getPlatformType(platform);
    
    s_nrd.viewport->instance->shaders.resize(denoiserDesc.pipelineNum);
    for (uint32_t shaderID = 0; shaderID < denoiserDesc.pipelineNum; ++shaderID)
    {
        const nrd::PipelineDesc& pipeline = denoiserDesc.pipelines[shaderID];
        if (platform == chi::ePlatformTypeVK)
        {
            CHI_VALIDATE(s_nrd.compute->createKernel((void*)pipeline.computeShaderSPIRV.bytecode, (uint32_t)pipeline.computeShaderSPIRV.size, pipeline.shaderFileName, pipeline.shaderEntryPointName, s_nrd.viewport->instance->shaders[shaderID]));
        }
        else
        {
            CHI_VALIDATE(s_nrd.compute->createKernel((void*)pipeline.computeShaderDXBC.bytecode, (uint32_t)pipeline.computeShaderDXBC.size, pipeline.shaderFileName, pipeline.shaderEntryPointName, s_nrd.viewport->instance->shaders[shaderID]));
        }
    }

    chi::ResourceDescription texDesc = {};
    texDesc.width = s_nrd.viewport->width;
    texDesc.height = s_nrd.viewport->height;
    texDesc.mips = 1;
    texDesc.state = chi::ResourceState::eTextureRead;

    texDesc.format = chi::eFormatR32F;
    if (!s_nrd.viewport->viewZ)
    {
        CHI_VALIDATE(s_nrd.compute->createTexture2D(texDesc, s_nrd.viewport->viewZ, "sl.s_nrd.viewZ"));
    }

    texDesc.format = chi::eFormatRGBA16F;
    if (!s_nrd.viewport->mvec)
    {
        CHI_VALIDATE(s_nrd.compute->createTexture2D(texDesc, s_nrd.viewport->mvec, "sl.s_nrd.mvec"));
    }
    if (s_nrd.viewport->instance->enableSpecular && !s_nrd.viewport->packedSpec)
    {
        CHI_VALIDATE(s_nrd.compute->createTexture2D(texDesc, s_nrd.viewport->packedSpec, "sl.s_nrd.packedSpec"));
    }
    if (s_nrd.viewport->instance->enableDiffuse && !s_nrd.viewport->packedDiff)
    {
        CHI_VALIDATE(s_nrd.compute->createTexture2D(texDesc, s_nrd.viewport->packedDiff, "sl.s_nrd.packedDiff"));
    }

    texDesc.format = chi::eFormatR16F;
    if (s_nrd.viewport->instance->enableAO && !s_nrd.viewport->packedAO)
    {
        CHI_VALIDATE(s_nrd.compute->createTexture2D(texDesc, s_nrd.viewport->packedAO, "sl.s_nrd.packedAO"));
    }

    return true;
}

void nrdBeginEvent(chi::CommandList cmdList, const common::EventData& data)
{
    if (!common::getConsts(data, &s_nrd.commonConsts))
    {
        return;
    }

    if (!nrdGetConstants(data, &s_nrd.nrdConsts))
    {
        return;
    }

    // Initialize or rebuild if resized
    initializeNRD(cmdList, data);
}

void nrdEndEvent(chi::CommandList cmdList)
{
    if (!s_nrd.viewport || !s_nrd.viewport->instance) return;

    auto parameters = api::getContext()->parameters;
    
    {
        CHI_VALIDATE(s_nrd.compute->bindSharedState(cmdList));
#if 1
        chi::Resource mvec{}, depth{}, aoIn{}, diffuseIn{}, specularIn{}, normalRoughness{};
        chi::Resource diffuseOut{}, specularOut{}, aoOut{};

        uint32_t mvecState{}, depthState{}, aoInState{}, diffuseInState{}, specularInState{}, normalRoughnessState{};
        uint32_t diffuseOutState{}, specularOutState{}, aoOutState{};

        Extent depthExt{};

        if (!getTaggedResource(eBufferTypeDepth, depth, s_nrd.viewport->id, &depthExt, &depthState)) return;
        if (!getTaggedResource(eBufferTypeMVec, mvec, s_nrd.viewport->id, nullptr, &mvecState)) return;
        if (!getTaggedResource(eBufferTypeNormalRoughness, normalRoughness, s_nrd.viewport->id, nullptr, &normalRoughnessState)) return;

        s_nrd.cacheState(depth, depthState);
        s_nrd.cacheState(mvec, mvecState);
        s_nrd.cacheState(normalRoughness, normalRoughnessState);

        if (s_nrd.viewport->instance->enableAO)
        {
            if (!getTaggedResource(eBufferTypeAmbientOcclusionNoisy, aoIn, s_nrd.viewport->id)) return;
            if (!getTaggedResource(eBufferTypeAmbientOcclusionDenoised, aoOut, s_nrd.viewport->id)) return;
            s_nrd.cacheState(aoIn, aoInState);
            s_nrd.cacheState(aoOut, aoOutState);
        }
        if (s_nrd.viewport->instance->enableDiffuse)
        {
            if (!getTaggedResource(eBufferTypeDiffuseHitNoisy, diffuseIn, s_nrd.viewport->id)) return;
            if (!getTaggedResource(eBufferTypeDiffuseHitDenoised, diffuseOut, s_nrd.viewport->id)) return;
            s_nrd.cacheState(diffuseIn, diffuseInState);
            s_nrd.cacheState(diffuseOut, diffuseOutState);
        }
        if (s_nrd.viewport->instance->enableSpecular)
        {
            if (!getTaggedResource(eBufferTypeSpecularHitNoisy, specularIn, s_nrd.viewport->id)) return;
            if (!getTaggedResource(eBufferTypeSpecularHitDenoised, specularOut, s_nrd.viewport->id)) return;
            s_nrd.cacheState(specularIn, specularInState);
            s_nrd.cacheState(specularOut, specularOutState);
        }

        // Prepare data
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
                uint32_t enableAO;
                uint32_t enableSpecular;
                uint32_t enableDiffuse;
                uint32_t enableWorldMotion;
                uint32_t enableCheckerboard;
                uint32_t cameraMotionIncluded;
                uint32_t relax;
            };
            PrepareDataCB cb;

            float w = s_nrd.viewport->width * s_nrd.nrdConsts->common.resolutionScale[0];
            float h = s_nrd.viewport->height * s_nrd.nrdConsts->common.resolutionScale[1];


            cb.clipToPrevClip = (s_nrd.commonConsts->clipToPrevClip);
            cb.invProj = (s_nrd.commonConsts->clipToCameraView);
            cb.screenToWorld = (s_nrd.nrdConsts->clipToWorld);
            cb.screenToWorldPrev = (s_nrd.nrdConsts->clipToWorldPrev);
            cb.sizeAndInvSize = { w, h, 1.0f / w, 1.0f / h };
            cb.hitDistParams = { s_nrd.nrdConsts->reblurSettings.hitDistanceParameters.A, s_nrd.nrdConsts->reblurSettings.hitDistanceParameters.B,s_nrd.nrdConsts->reblurSettings.hitDistanceParameters.C, s_nrd.nrdConsts->reblurSettings.hitDistanceParameters.D };
            cb.frameId = s_nrd.viewport->frameIndex;
            cb.enableAO = aoIn != nullptr;
            cb.enableSpecular = specularIn != nullptr;
            cb.enableDiffuse = diffuseIn != nullptr;
            cb.enableWorldMotion = s_nrd.commonConsts->motionVectors3D;
            cb.enableCheckerboard = s_nrd.nrdConsts->reblurSettings.checkerboardMode != NRDCheckerboardMode::OFF;
            cb.cameraMotionIncluded = s_nrd.commonConsts->cameraMotionIncluded;
            cb.relax = s_nrd.viewport->instance->relax;

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
                    {mvec, chi::ResourceState::eTextureRead, s_nrd.cachedStates[mvec]},
                    {depth, chi::ResourceState::eTextureRead, s_nrd.cachedStates[depth]},
                    {s_nrd.viewport->mvec, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
                    {s_nrd.viewport->viewZ, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
                };
                s_nrd.compute->transitionResources(cmdList, trans, (uint32_t)countof(trans), &transitions);

                CHI_VALIDATE(s_nrd.compute->bindKernel(s_nrd.prepareDataKernel));
                CHI_VALIDATE(s_nrd.compute->bindSampler(1, 0, chi::eSamplerLinearClamp));
                CHI_VALIDATE(s_nrd.compute->bindTexture(2, 0, depth));
                CHI_VALIDATE(s_nrd.compute->bindTexture(3, 1, mvec));
                CHI_VALIDATE(s_nrd.compute->bindRWTexture(4, 0, s_nrd.viewport->mvec));
                CHI_VALIDATE(s_nrd.compute->bindRWTexture(5, 1, s_nrd.viewport->viewZ));
                CHI_VALIDATE(s_nrd.compute->bindConsts(0, 0, &cb, sizeof(PrepareDataCB), 3 * (uint32_t)s_nrd.viewport->instances.size() * (uint32_t)s_nrd.viewports.size()));
                uint32_t grid[] = { ((uint32_t)w + 16 - 1) / 16, ((uint32_t)h + 16 - 1) / 16, 1 };
                CHI_VALIDATE(s_nrd.compute->dispatch(grid[0], grid[1], grid[2]));
            }

            // Pack
            {
                extra::ScopedTasks transitions;
                chi::ResourceTransition trans[] =
                {
                    {aoIn, chi::ResourceState::eTextureRead, s_nrd.cachedStates[aoIn]},
                    {diffuseIn, chi::ResourceState::eTextureRead, s_nrd.cachedStates[diffuseIn]},
                    {specularIn, chi::ResourceState::eTextureRead, s_nrd.cachedStates[specularIn]},
                    {normalRoughness, chi::ResourceState::eTextureRead, s_nrd.cachedStates[normalRoughness]},
                    {s_nrd.viewport->packedSpec, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
                    {s_nrd.viewport->packedDiff, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
                    {s_nrd.viewport->packedAO, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead}
                };
                s_nrd.compute->transitionResources(cmdList, trans, (uint32_t)countof(trans), &transitions);

                CHI_VALIDATE(s_nrd.compute->bindKernel(s_nrd.packDataKernel));
                CHI_VALIDATE(s_nrd.compute->bindSampler(1, 0, chi::eSamplerLinearClamp));
                CHI_VALIDATE(s_nrd.compute->bindTexture(2, 0, s_nrd.viewport->viewZ));
                CHI_VALIDATE(s_nrd.compute->bindTexture(3, 1, normalRoughness));
                CHI_VALIDATE(s_nrd.compute->bindTexture(4, 2, diffuseIn));
                CHI_VALIDATE(s_nrd.compute->bindTexture(5, 3, specularIn));
                CHI_VALIDATE(s_nrd.compute->bindTexture(6, 4, aoIn));
                CHI_VALIDATE(s_nrd.compute->bindRWTexture(7, 0, s_nrd.viewport->packedDiff));
                CHI_VALIDATE(s_nrd.compute->bindRWTexture(8, 1, s_nrd.viewport->packedSpec));
                CHI_VALIDATE(s_nrd.compute->bindRWTexture(9, 2, s_nrd.viewport->packedAO));
                CHI_VALIDATE(s_nrd.compute->bindConsts(0, 0, &cb, sizeof(PrepareDataCB), 3 * (uint32_t)s_nrd.viewport->instances.size() * (uint32_t)s_nrd.viewports.size()));
                if (cb.enableCheckerboard)
                {
                    w = w / 2.0f;
                }
                uint32_t grid[] = { ((uint32_t)w + 16 - 1) / 16, ((uint32_t)h + 16 - 1) / 16, 1 };
                CHI_VALIDATE(s_nrd.compute->dispatch(grid[0], grid[1], grid[2]));
            }
        }

        //SL_LOG_HINT("-----------------------------------------------------------------------------------------------------------------------------------------");

        // Now run NRD

        // NRD SDK states that matrices are column major
        auto transpose = [](const float4x4& m)->float4x4
        {
            float4x4 r;
            r[0] = { m[0].x, m[1].x, m[2].x, m[3].x };
            r[1] = { m[0].y, m[1].y, m[2].y, m[3].y };
            r[2] = { m[0].z, m[1].z, m[2].z, m[3].z };
            r[3] = { m[0].w, m[1].w, m[2].w, m[3].w };
            return r;
        };

        nrd::CommonSettings commonSettings{};
        memcpy(commonSettings.viewToClipMatrix, &transpose(s_nrd.commonConsts->cameraViewToClip), sizeof(float4x4));
        memcpy(commonSettings.viewToClipMatrixPrev, s_nrd.viewport->instance->prevCommonSettings.viewToClipMatrix, sizeof(float4x4));
        memcpy(commonSettings.worldToViewMatrix, &transpose(s_nrd.nrdConsts->common.worldToViewMatrix), sizeof(float4x4));
        memcpy(commonSettings.worldToViewMatrixPrev, s_nrd.viewport->instance->prevCommonSettings.worldToViewMatrix, sizeof(float4x4));
        memcpy(commonSettings.cameraJitter, &s_nrd.commonConsts->jitterOffset, sizeof(float2));

        memcpy(commonSettings.motionVectorScale, &s_nrd.nrdConsts->common.motionVectorScale, sizeof(float2));
        memcpy(commonSettings.resolutionScale, &s_nrd.nrdConsts->common.resolutionScale, sizeof(float2));
        commonSettings.timeDeltaBetweenFrames = s_nrd.nrdConsts->common.timeDeltaBetweenFrames;
        commonSettings.denoisingRange = s_nrd.nrdConsts->common.denoisingRange;
        commonSettings.disocclusionThreshold = s_nrd.nrdConsts->common.disocclusionThreshold;
        commonSettings.splitScreen = s_nrd.nrdConsts->common.splitScreen;
        commonSettings.inputSubrectOrigin[0] = depthExt.left;
        commonSettings.inputSubrectOrigin[1] = depthExt.top;
        commonSettings.frameIndex = s_nrd.viewport->frameIndex;
        commonSettings.accumulationMode = (nrd::AccumulationMode)s_nrd.nrdConsts->common.accumulationMode;
        commonSettings.isMotionVectorInWorldSpace = s_nrd.commonConsts->motionVectors3D == Boolean::eTrue;
        commonSettings.isHistoryConfidenceInputsAvailable = s_nrd.nrdConsts->common.isHistoryConfidenceInputsAvailable;

        s_nrd.viewport->instance->prevCommonSettings = commonSettings;

        for (uint32_t i = 0; i < s_nrd.viewport->instance->methodCount; i++)
        {
            if (s_nrd.viewport->instance->methodDescs[i].method == nrd::Method::REBLUR_DIFFUSE_SPECULAR)
            {
                s_nrd.setMethodSettings(*s_nrd.viewport->instance->denoiser, s_nrd.viewport->instance->methodDescs[i].method, &s_nrd.nrdConsts->reblurSettings);
            }
            else if (s_nrd.viewport->instance->methodDescs[i].method == nrd::Method::REBLUR_DIFFUSE ||
                s_nrd.viewport->instance->methodDescs[i].method == nrd::Method::REBLUR_DIFFUSE_OCCLUSION)
            {
                s_nrd.setMethodSettings(*s_nrd.viewport->instance->denoiser, s_nrd.viewport->instance->methodDescs[i].method, &s_nrd.nrdConsts->reblurSettings);
            }
            else if (s_nrd.viewport->instance->methodDescs[i].method == nrd::Method::REBLUR_SPECULAR)
            {
                s_nrd.setMethodSettings(*s_nrd.viewport->instance->denoiser, s_nrd.viewport->instance->methodDescs[i].method, &s_nrd.nrdConsts->reblurSettings);
            }
            else if (s_nrd.viewport->instance->methodDescs[i].method == nrd::Method::RELAX_DIFFUSE_SPECULAR)
            {
                s_nrd.setMethodSettings(*s_nrd.viewport->instance->denoiser, s_nrd.viewport->instance->methodDescs[i].method, &s_nrd.nrdConsts->relaxDiffuseSpecular);
            }
            else if (s_nrd.viewport->instance->methodDescs[i].method == nrd::Method::RELAX_DIFFUSE)
            {
                s_nrd.setMethodSettings(*s_nrd.viewport->instance->denoiser, s_nrd.viewport->instance->methodDescs[i].method, &s_nrd.nrdConsts->relaxDiffuse);
            }
            else if (s_nrd.viewport->instance->methodDescs[i].method == nrd::Method::RELAX_SPECULAR)
            {
                s_nrd.setMethodSettings(*s_nrd.viewport->instance->denoiser, s_nrd.viewport->instance->methodDescs[i].method, &s_nrd.nrdConsts->relaxSpecular);
            }
            else
            {
                s_nrd.setMethodSettings(*s_nrd.viewport->instance->denoiser, s_nrd.viewport->instance->methodDescs[i].method, &s_nrd.nrdConsts->sigmaShadow);
            }
        }

        const nrd::DispatchDesc* dispatchDescs = nullptr;
        uint32_t dispatchDescNum = 0;
        NRD_CHECK1(s_nrd.getComputeDispatches(*s_nrd.viewport->instance->denoiser, commonSettings, dispatchDescs, dispatchDescNum));

        CHI_VALIDATE(s_nrd.compute->beginPerfSection(cmdList, "sl.nrd"));

        const nrd::DenoiserDesc& denoiserDesc = s_nrd.getDenoiserDesc(*s_nrd.viewport->instance->denoiser);
        for (uint32_t dispatchID = 0; dispatchID < dispatchDescNum; ++dispatchID)
        {
            const nrd::DispatchDesc& dispatch = dispatchDescs[dispatchID];
            const nrd::PipelineDesc& pipeline = denoiserDesc.pipelines[dispatch.pipelineIndex];

            extra::ScopedTasks reverseTransitions;
            std::vector<chi::ResourceTransition> transitions;
            CHI_VALIDATE(s_nrd.compute->bindKernel(s_nrd.viewport->instance->shaders[dispatch.pipelineIndex]));

            for (uint32_t samplerID = 0; samplerID < denoiserDesc.staticSamplerNum; ++samplerID)
            {
                const nrd::StaticSamplerDesc& samplerDesc = denoiserDesc.staticSamplers[samplerID];
                switch (denoiserDesc.staticSamplers[samplerID].sampler)
                {
                case nrd::Sampler::NEAREST_CLAMP:
                    CHI_VALIDATE(s_nrd.compute->bindSampler(samplerID, samplerDesc.registerIndex, chi::Sampler::eSamplerPointClamp)); break;
                case nrd::Sampler::NEAREST_MIRRORED_REPEAT:
                    CHI_VALIDATE(s_nrd.compute->bindSampler(samplerID, samplerDesc.registerIndex, chi::Sampler::eSamplerPointMirror)); break;
                case nrd::Sampler::LINEAR_CLAMP:
                    CHI_VALIDATE(s_nrd.compute->bindSampler(samplerID, samplerDesc.registerIndex, chi::Sampler::eSamplerLinearClamp)); break;
                case nrd::Sampler::LINEAR_MIRRORED_REPEAT:
                    CHI_VALIDATE(s_nrd.compute->bindSampler(samplerID, samplerDesc.registerIndex, chi::Sampler::eSamplerLinearMirror)); break;
                default: SL_LOG_ERROR("Unknown sampler detected");
                }
            }

            uint32_t slot = 0;
            for (uint32_t descriptorRangeID = 0; descriptorRangeID < pipeline.descriptorRangeNum; ++descriptorRangeID)
            {
                const nrd::DescriptorRangeDesc& descriptorRange = pipeline.descriptorRanges[descriptorRangeID];

                for (uint32_t descriptorID = 0; descriptorID < descriptorRange.descriptorNum; ++descriptorID)
                {
                    if (slot >= dispatch.resourceNum)
                    {
                        SL_LOG_ERROR("Mismatch slot and resourceNum");
                    }
                    chi::Resource texture = {};
                    auto state = chi::ResourceState::eUnknown;
                    const nrd::Resource& resource = dispatch.resources[slot++];
                    if (resource.stateNeeded != descriptorRange.descriptorType)
                    {
                        SL_LOG_ERROR("Mismatch stateNeeded and descriptor type");
                    }
                    switch (resource.type)
                    {
                    case nrd::ResourceType::IN_MV:
                        texture = s_nrd.viewport->mvec;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_NORMAL_ROUGHNESS:
                        texture = normalRoughness;
                        state = s_nrd.cachedStates[texture];
                        break;
                    case nrd::ResourceType::IN_VIEWZ:
                        texture = s_nrd.viewport->viewZ;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_DIFF_HITDIST:
                        texture = s_nrd.viewport->packedAO;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:
                        texture = s_nrd.viewport->packedDiff;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:
                        texture = s_nrd.viewport->packedSpec;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_SHADOWDATA:
                        /**/
                        break;
                    case nrd::ResourceType::IN_SHADOW_TRANSLUCENCY:
                        /**/
                        break;
                    case nrd::ResourceType::OUT_DIFF_HITDIST:
                        texture = aoOut;
                        state = s_nrd.cachedStates[texture];
                        break;
                    case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:
                        texture = diffuseOut;
                        state = s_nrd.cachedStates[texture];
                        break;
                    case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:
                        texture = specularOut;
                        state = s_nrd.cachedStates[texture];
                        break;
                    case nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY:
                        /**/
                        break;
                    case nrd::ResourceType::TRANSIENT_POOL:
                        texture = s_nrd.viewport->instance->transientTextures[resource.indexInPool];
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::PERMANENT_POOL:
                        texture = s_nrd.viewport->instance->permanentTextures[resource.indexInPool];
                        state = chi::ResourceState::eTextureRead;
                        break;
                    };
                    if (!texture)
                    {
                        SL_LOG_ERROR("Unable to find texture for nrd::ResourceType %u", resource.type);
                    }
                    uint32_t bindingSlot = descriptorRange.baseRegisterIndex + descriptorID;
                    if (descriptorRange.descriptorType == nrd::DescriptorType::TEXTURE)
                    {
                        // TODO: Fix binding pos for VK
                        CHI_VALIDATE(s_nrd.compute->bindTexture(descriptorRange.descriptorNum, bindingSlot, texture, resource.mipOffset, resource.mipNum));
                        transitions.push_back(chi::ResourceTransition(texture, chi::ResourceState::eTextureRead, state));
                    }
                    else
                    {
                        // TODO: Fix binding pos for VK
                        CHI_VALIDATE(s_nrd.compute->bindRWTexture(descriptorRange.descriptorNum, bindingSlot, texture, resource.mipOffset));
                        transitions.push_back(chi::ResourceTransition(texture, chi::ResourceState::eStorageRW, state));
                    }
                }
            }

            // TODO: Fix binding pos for VK
            CHI_VALIDATE(s_nrd.compute->bindConsts(0, denoiserDesc.constantBufferDesc.registerIndex, (void*)dispatch.constantBufferData, denoiserDesc.constantBufferDesc.maxDataSize, 3 * dispatchDescNum));
            CHI_VALIDATE(s_nrd.compute->transitionResources(cmdList, transitions.data(), (uint32_t)transitions.size(), &reverseTransitions));
            CHI_VALIDATE(s_nrd.compute->dispatch(dispatch.gridWidth, dispatch.gridHeight, 1));
            //SL_LOG_HINT("Dispatch %s", dispatch.name);
        }
        float ms = 0;
        CHI_VALIDATE(s_nrd.compute->endPerfSection(cmdList, "sl.nrd", ms));

        parameters->set(sl::param::nrd::kMVecBuffer, s_nrd.viewport->mvec);
        parameters->set(sl::param::nrd::kViewZBuffer, s_nrd.viewport->viewZ);

        {
            static std::string s_stats;
            auto v = api::getContext()->pluginVersion;
            std::string description{};
            for (auto& v : s_nrd.viewports)
            {
                description += v.second->description + " ";
            }
            s_stats = extra::format("sl.nrd {} - {}- {}ms - {}", v.toStr(), description, ms, GIT_LAST_COMMIT);
            parameters->set(sl::param::nrd::kStats, (void*)s_stats.c_str());
            uint32_t frame = 0;
            s_nrd.compute->getFinishedFrameIndex(frame);
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
        //    auto idx = std::to_string(s_nrd.nrdConsts->common.frameIndex);
        //    //s_nrd.compute->dumpResource(cmdList, mvec, std::string("f:/tmp/correct/mvec" + idx).c_str());
        //    //s_nrd.compute->dumpResource(cmdList, viewZ, std::string("f:/tmp/correct/viewZ" + idx).c_str());
        //    //s_nrd.compute->dumpResource(cmdList, normRough, std::string("f:/tmp/correct/normRough" + idx).c_str());
        //    //s_nrd.compute->dumpResource(cmdList, specularOut, std::string("f:/tmp/correct/specularOut" + idx).c_str());
        //    //s_nrd.compute->dumpResource(cmdList, specularIn, std::string("f:/tmp/correct/specularIn" + idx).c_str());
        //    s_nrd.compute->dumpResource(cmdList, normalRoughness, std::string("f:/tmp/normalRoughness" + idx).c_str());
        //    s_nrd.compute->dumpResource(cmdList, s_nrd.viewport->mvec, std::string("f:/tmp/mvec" + idx).c_str());
        //    s_nrd.compute->dumpResource(cmdList, s_nrd.viewport->packedSpec, std::string("f:/tmp/inSpecHit" + idx).c_str());
        //    s_nrd.compute->dumpResource(cmdList, specularOut, std::string("f:/tmp/outSpecHit" + idx).c_str());
        //    s_nrd.compute->dumpResource(cmdList, s_nrd.viewport->viewZ, std::string("f:/tmp/viewZ" + idx).c_str());
        //}
#endif
    }
}

//! -------------------------------------------------------------------------------------------------
//! Required interface


bool slAllocateResources(sl::CommandBuffer* cmdBuffer, Feature feature, uint32_t id)
{
    common::EventData data{ id, 0 };
    nrdBeginEvent(cmdBuffer, data);
    auto it = s_nrd.viewports.find(id);
    return it != s_nrd.viewports.end() && !(*it).second->instances.empty();
}

bool slFreeResources(Feature feature, uint32_t id)
{
    auto it = s_nrd.viewports.find(id);
    if(it != s_nrd.viewports.end())
    {
        if ((*it).second->id == id)
        {
            destroyNRDViewport((*it).second);
            delete (*it).second;
            s_nrd.viewports.erase(it);
        }
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

    if (!getPointerParam(parameters, param::common::kComputeAPI, &s_nrd.compute))
    {
        SL_LOG_ERROR("Can't find %s", param::common::kComputeAPI);
        return false;
    }
    
    chi::PlatformType platform;
    s_nrd.compute->getPlatformType(platform);

    // Set callbacks from the sl.common
    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &s_nrd.registerEvaluateCallbacks))
    {
        SL_LOG_ERROR("Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        return false;
    }
    s_nrd.registerEvaluateCallbacks(eFeatureNRD, nrdBeginEvent, nrdEndEvent);

    json& config = *(json*)api::getContext()->loaderConfig;
    int appId = 0;
    bool ota = false;
    config.at("appId").get_to(appId);
    if (config.contains("ota"))
    {
        config.at("ota").get_to(ota);
    }

    // Path where our modules are located
    wchar_t *pluginPath = {};
    param::getPointerParam(parameters, param::global::kPluginPath, &pluginPath);
    if (!pluginPath)
    {
        SL_LOG_ERROR("Cannot find path to plugins");
        return false;
    }
    // Now let's load our NRD module
    std::wstring path(pluginPath);
    path += L"/nrd.dll";
    s_nrd.lib = security::loadLibrary(path.c_str());
    if (!s_nrd.lib)
    {
        SL_LOG_ERROR("Failed to load %S", path.c_str());
        return false;
    }

    s_nrd.getLibraryDesc = (PFunGetLibraryDesc*)GetProcAddress(s_nrd.lib, "GetLibraryDesc");
    s_nrd.createDenoiser = (PFunCreateDenoiser*)GetProcAddress(s_nrd.lib, "CreateDenoiser");
    s_nrd.getDenoiserDesc = (PFunGetDenoiserDesc*)GetProcAddress(s_nrd.lib, "GetDenoiserDesc");
    s_nrd.setMethodSettings = (PFunSetMethodSettings*)GetProcAddress(s_nrd.lib, "SetMethodSettings");
    s_nrd.getComputeDispatches = (PFunGetComputeDispatches*)GetProcAddress(s_nrd.lib, "GetComputeDispatches");
    s_nrd.destroyDenoiser = (PFunDestroyDenoiser*)GetProcAddress(s_nrd.lib, "DestroyDenoiser");

    if (!s_nrd)
    {
        SL_LOG_ERROR("Failed to map NRD API in %S", path.c_str());
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

    s_nrd.registerEvaluateCallbacks(eFeatureNRD, nullptr, nullptr);
    
    // Common shutdown, if we loaded an OTA
    // it will shutdown it down automatically
    plugin::onShutdown(api::getContext());
}

const char *JSON = R"json(
{
    "id" : 1,
    "priority" : 1,
    "namespace" : "nrd",
    "hooks" :
    [
    ]
}
)json";

uint32_t getSupportedAdapterMask()
{
    // Always supported on any adapter
    return ~0;
}

SL_PLUGIN_DEFINE("sl.nrd", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON, getSupportedAdapterMask())

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
    SL_EXPORT_FUNCTION(slAllocateResources);
    SL_EXPORT_FUNCTION(slFreeResources);

    return nullptr;
}
}
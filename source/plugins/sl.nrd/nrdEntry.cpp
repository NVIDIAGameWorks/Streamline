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
#include "_artifacts/gitVersion.h"

#define NRD_CHECK(f) {nrd::Result r = f;if(r != nrd::Result::SUCCESS) { SL_LOG_ERROR( "%s failed error %u",#f,r); return false;}};
#define NRD_CHECK1(f) {nrd::Result r = f;if(r != nrd::Result::SUCCESS) { SL_LOG_ERROR( "%s failed error %u",#f,r); return Result::eErrorNRDAPI;}};

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

namespace nrdsl
{
struct NRDContext
{
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(NRDContext);
    void onDestroyContext() {};

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

const char* JSON = R"json(
{
    "id" : 1,
    "priority" : 100,
    "namespace" : "nrd",
    "required_plugins" : ["sl.common"],
    "rhi" : ["d3d11", "d3d12", "vk"],
    "hooks" :
    [
    ]
}
)json";

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

SL_PLUGIN_DEFINE("sl.nrd", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON, updateEmbeddedJSON, nrdsl, NRDContext)

void destroyNRDViewport(NRDViewport*);

bool slSetConstants(const void* data, uint32_t frameIndex, uint32_t id)
{
    auto& ctx = (*nrdsl::getContext());
    auto consts = (const sl::NRDConstants*)data;
    ctx.constsPerViewport.set(frameIndex, id, consts);
#ifndef SL_PRODUCTION
    if (consts->methodMask == 0 && !ctx.viewports.empty())
    {
        auto viewports = ctx.viewports;
        auto lambda = [&ctx, viewports](void)->void
        {
            for (auto& v : viewports)
            {
                destroyNRDViewport(v.second);
                delete v.second;
            }
            ctx.viewports.clear();

            CHI_VALIDATE(ctx.compute->destroyKernel(ctx.prepareDataKernel));
            CHI_VALIDATE(ctx.compute->destroyKernel(ctx.packDataKernel));
        };
        CHI_VALIDATE(ctx.compute->destroy(lambda));
        ctx.viewports.clear();
    }
#endif
    return true;
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
    ctx.destroyDenoiser(*inst->denoiser);
    
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
    CHI_VALIDATE(ctx.compute->destroyResource(viewport->packedAO));
    CHI_VALIDATE(ctx.compute->destroyResource(viewport->packedSpec));
    CHI_VALIDATE(ctx.compute->destroyResource(viewport->packedDiff));

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
                    ctx.viewport->instance->relax = true;
                case nrd::Method::REBLUR_DIFFUSE_SPECULAR:
                    ctx.viewport->instance->enableSpecular = true;
                    ctx.viewport->instance->enableDiffuse = true;
                    pname = kBufferTypeDiffuseHitNoisy;
                    break;
                case nrd::Method::RELAX_DIFFUSE: 
                    ctx.viewport->instance->relax = true;
                case nrd::Method::REBLUR_DIFFUSE: 
                    pname = kBufferTypeDiffuseHitNoisy;
                    ctx.viewport->instance->enableDiffuse = true;
                    break;
                case nrd::Method::RELAX_SPECULAR:
                    ctx.viewport->instance->relax = true;
                case nrd::Method::REBLUR_SPECULAR:
                    pname = kBufferTypeSpecularHitNoisy;
                    ctx.viewport->instance->enableSpecular = true;
                    break;
                case nrd::Method::SIGMA_SHADOW_TRANSLUCENCY:
                case nrd::Method::SIGMA_SHADOW:
                    pname = kBufferTypeShadowNoisy;
                    break;
                case nrd::Method::REBLUR_DIFFUSE_OCCLUSION:
                    pname = kBufferTypeAmbientOcclusionNoisy;
                    ctx.viewport->instance->enableAO = true;
                    break;
            };
 
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
                instance->enableAO = ctx.viewport->instance->enableAO;
                instance->enableSpecular = ctx.viewport->instance->enableSpecular;
                instance->enableDiffuse = ctx.viewport->instance->enableDiffuse;
                destroyNRDViewport(ctx.viewport);
                ctx.viewports[viewport->id] = viewport;
                ctx.viewport = viewport;
                ctx.viewport->instances[ctx.nrdConsts->methodMask] = instance;
                ctx.viewport->instance = instance;
            }

            ctx.viewport->instance->methodDescs[methodCount++] = nrd::MethodDesc{ allMethods[i], (uint16_t)desc.width, (uint16_t)desc.height };
            ctx.viewport->width = desc.width;
            ctx.viewport->height = desc.height;
        }
        mask = mask >> 1;
        i++;
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

    for (uint32_t i = 0; i < methodCount; i++)
    {
        SL_LOG_HINT("Requested NRDContext method %s (%u,%u) for viewport %u", allMethodNames[(int)ctx.viewport->instance->methodDescs[i].method], ctx.viewport->instance->methodDescs[i].fullResolutionWidth, ctx.viewport->instance->methodDescs[i].fullResolutionHeight, ctx.viewport->id);
        ctx.viewport->description += extra::format(" - {} ({},{})", allMethodNames[(int)ctx.viewport->instance->methodDescs[i].method], ctx.viewport->instance->methodDescs[i].fullResolutionWidth, ctx.viewport->instance->methodDescs[i].fullResolutionHeight);
    }

    ctx.viewport->instance->methodMask = ctx.nrdConsts->methodMask;
    ctx.viewport->instance->methodCount = methodCount;

    nrd::DenoiserCreationDesc denoiserCreationDesc = {};
    denoiserCreationDesc.requestedMethods = ctx.viewport->instance->methodDescs;
    denoiserCreationDesc.requestedMethodNum = methodCount;
    if (ctx.createDenoiser(denoiserCreationDesc, ctx.viewport->instance->denoiser) != nrd::Result::SUCCESS)
    {
        return Result::eErrorNRDAPI;
    }

    nrd::DenoiserDesc denoiserDesc = {};
    denoiserDesc = ctx.getDenoiserDesc(*ctx.viewport->instance->denoiser);

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

    ctx.viewport->instance->permanentTextures.resize(denoiserDesc.permanentPoolSize);
    for (uint32_t texID = 0; texID < denoiserDesc.permanentPoolSize; ++texID)
    {
        char buffer[64];
        snprintf(buffer, 64, "sl.ctx.permanentTexture[%d]", texID);
        auto texDesc = convertNRDTextureDesc(denoiserDesc.permanentPool[texID]);
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->instance->permanentTextures[texID], buffer));
    }

    ctx.viewport->instance->transientTextures.resize(denoiserDesc.transientPoolSize);
    for (uint32_t texID = 0; texID < denoiserDesc.transientPoolSize; ++texID)
    {
        char buffer[64];
        snprintf(buffer, 64, "sl.ctx.transientTexture[%d]", texID);
        auto texDesc = convertNRDTextureDesc(denoiserDesc.transientPool[texID]);
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->instance->transientTextures[texID], buffer));
    }

    RenderAPI platform = RenderAPI::eD3D12;
    ctx.compute->getRenderAPI(platform);
    
    ctx.viewport->instance->shaders.resize(denoiserDesc.pipelineNum);
    for (uint32_t shaderID = 0; shaderID < denoiserDesc.pipelineNum; ++shaderID)
    {
        const nrd::PipelineDesc& pipeline = denoiserDesc.pipelines[shaderID];
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
    if (ctx.viewport->instance->enableSpecular && !ctx.viewport->packedSpec)
    {
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->packedSpec, "sl.ctx.packedSpec"));
    }
    if (ctx.viewport->instance->enableDiffuse && !ctx.viewport->packedDiff)
    {
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->packedDiff, "sl.ctx.packedDiff"));
    }

    texDesc.format = chi::eFormatR16F;
    if (ctx.viewport->instance->enableAO && !ctx.viewport->packedAO)
    {
        CHI_VALIDATE(ctx.compute->createTexture2D(texDesc, ctx.viewport->packedAO, "sl.ctx.packedAO"));
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

Result nrdEndEvent(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*nrdsl::getContext());

    if (!ctx.viewport || !ctx.viewport->instance) return Result::eErrorMissingInputParameter;

    auto parameters = api::getContext()->parameters;
    
    {
        CHI_VALIDATE(ctx.compute->bindSharedState(cmdList));
#if 1
        CommonResource mvec{}, depth{}, aoIn{}, diffuseIn{}, specularIn{}, normalRoughness{};
        CommonResource diffuseOut{}, specularOut{}, aoOut{};

        SL_CHECK(getTaggedResource(kBufferTypeDepth, depth, ctx.viewport->id, false, inputs, numInputs));
        SL_CHECK(getTaggedResource(kBufferTypeMotionVectors, mvec, ctx.viewport->id, false, inputs, numInputs));
        SL_CHECK(getTaggedResource(kBufferTypeNormalRoughness, normalRoughness, ctx.viewport->id, false, inputs, numInputs));

        ctx.cacheState(depth, depth.getState());
        ctx.cacheState(mvec, mvec.getState());
        ctx.cacheState(normalRoughness, normalRoughness.getState());

        if (ctx.viewport->instance->enableAO)
        {
            SL_CHECK(getTaggedResource(kBufferTypeAmbientOcclusionNoisy, aoIn, ctx.viewport->id, false, inputs, numInputs));
            SL_CHECK(getTaggedResource(kBufferTypeAmbientOcclusionDenoised, aoOut, ctx.viewport->id, false, inputs, numInputs));
            ctx.cacheState(aoIn, aoIn.getState());
            ctx.cacheState(aoOut, aoOut.getState());
        }
        if (ctx.viewport->instance->enableDiffuse)
        {
            SL_CHECK(getTaggedResource(kBufferTypeDiffuseHitNoisy, diffuseIn, ctx.viewport->id, false, inputs, numInputs));
            SL_CHECK(getTaggedResource(kBufferTypeDiffuseHitDenoised, diffuseOut, ctx.viewport->id, false, inputs, numInputs));
            ctx.cacheState(diffuseIn, diffuseIn.getState());
            ctx.cacheState(diffuseOut, diffuseOut.getState());
        }
        if (ctx.viewport->instance->enableSpecular)
        {
            SL_CHECK(getTaggedResource(kBufferTypeSpecularHitNoisy, specularIn, ctx.viewport->id, false, inputs, numInputs));
            SL_CHECK(getTaggedResource(kBufferTypeSpecularHitDenoised, specularOut, ctx.viewport->id, false, inputs, numInputs));
            ctx.cacheState(specularIn, specularIn.getState());
            ctx.cacheState(specularOut, specularOut.getState());
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

            float w = ctx.viewport->width * ctx.nrdConsts->common.resolutionScale[0];
            float h = ctx.viewport->height * ctx.nrdConsts->common.resolutionScale[1];


            cb.clipToPrevClip = (ctx.commonConsts->clipToPrevClip);
            cb.invProj = (ctx.commonConsts->clipToCameraView);
            cb.screenToWorld = (ctx.nrdConsts->clipToWorld);
            cb.screenToWorldPrev = (ctx.nrdConsts->clipToWorldPrev);
            cb.sizeAndInvSize = { w, h, 1.0f / w, 1.0f / h };
            cb.hitDistParams = { ctx.nrdConsts->reblurSettings.hitDistanceParameters.A, ctx.nrdConsts->reblurSettings.hitDistanceParameters.B,ctx.nrdConsts->reblurSettings.hitDistanceParameters.C, ctx.nrdConsts->reblurSettings.hitDistanceParameters.D };
            cb.frameId = ctx.viewport->frameIndex;
            cb.enableAO = aoIn;
            cb.enableSpecular = specularIn;
            cb.enableDiffuse = diffuseIn;
            cb.enableWorldMotion = ctx.commonConsts->motionVectors3D;
            cb.enableCheckerboard = ctx.nrdConsts->reblurSettings.checkerboardMode != NRDCheckerboardMode::OFF;
            cb.cameraMotionIncluded = ctx.commonConsts->cameraMotionIncluded;
            cb.relax = ctx.viewport->instance->relax;

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
                    {mvec, chi::ResourceState::eTextureRead, ctx.cachedStates[mvec]},
                    {depth, chi::ResourceState::eTextureRead, ctx.cachedStates[depth]},
                    {ctx.viewport->mvec, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
                    {ctx.viewport->viewZ, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
                };
                ctx.compute->transitionResources(cmdList, trans, (uint32_t)countof(trans), &transitions);

                CHI_VALIDATE(ctx.compute->bindKernel(ctx.prepareDataKernel));
                CHI_VALIDATE(ctx.compute->bindSampler(1, 0, chi::eSamplerLinearClamp));
                CHI_VALIDATE(ctx.compute->bindTexture(2, 0, depth));
                CHI_VALIDATE(ctx.compute->bindTexture(3, 1, mvec));
                CHI_VALIDATE(ctx.compute->bindRWTexture(4, 0, ctx.viewport->mvec));
                CHI_VALIDATE(ctx.compute->bindRWTexture(5, 1, ctx.viewport->viewZ));
                CHI_VALIDATE(ctx.compute->bindConsts(0, 0, &cb, sizeof(PrepareDataCB), 3 * (uint32_t)ctx.viewport->instances.size() * (uint32_t)ctx.viewports.size()));
                uint32_t grid[] = { ((uint32_t)w + 16 - 1) / 16, ((uint32_t)h + 16 - 1) / 16, 1 };
                CHI_VALIDATE(ctx.compute->dispatch(grid[0], grid[1], grid[2]));
            }

            // Pack
            {
                extra::ScopedTasks transitions;
                chi::ResourceTransition trans[] =
                {
                    {aoIn, chi::ResourceState::eTextureRead, ctx.cachedStates[aoIn]},
                    {diffuseIn, chi::ResourceState::eTextureRead, ctx.cachedStates[diffuseIn]},
                    {specularIn, chi::ResourceState::eTextureRead, ctx.cachedStates[specularIn]},
                    {normalRoughness, chi::ResourceState::eTextureRead, ctx.cachedStates[normalRoughness]},
                    {ctx.viewport->packedSpec, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
                    {ctx.viewport->packedDiff, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead},
                    {ctx.viewport->packedAO, chi::ResourceState::eStorageRW, chi::ResourceState::eTextureRead}
                };
                ctx.compute->transitionResources(cmdList, trans, (uint32_t)countof(trans), &transitions);

                CHI_VALIDATE(ctx.compute->bindKernel(ctx.packDataKernel));
                CHI_VALIDATE(ctx.compute->bindSampler(1, 0, chi::eSamplerLinearClamp));
                CHI_VALIDATE(ctx.compute->bindTexture(2, 0, ctx.viewport->viewZ));
                CHI_VALIDATE(ctx.compute->bindTexture(3, 1, normalRoughness));
                CHI_VALIDATE(ctx.compute->bindTexture(4, 2, diffuseIn));
                CHI_VALIDATE(ctx.compute->bindTexture(5, 3, specularIn));
                CHI_VALIDATE(ctx.compute->bindTexture(6, 4, aoIn));
                CHI_VALIDATE(ctx.compute->bindRWTexture(7, 0, ctx.viewport->packedDiff));
                CHI_VALIDATE(ctx.compute->bindRWTexture(8, 1, ctx.viewport->packedSpec));
                CHI_VALIDATE(ctx.compute->bindRWTexture(9, 2, ctx.viewport->packedAO));
                CHI_VALIDATE(ctx.compute->bindConsts(0, 0, &cb, sizeof(PrepareDataCB), 3 * (uint32_t)ctx.viewport->instances.size() * (uint32_t)ctx.viewports.size()));
                if (cb.enableCheckerboard)
                {
                    w = w / 2.0f;
                }
                uint32_t grid[] = { ((uint32_t)w + 16 - 1) / 16, ((uint32_t)h + 16 - 1) / 16, 1 };
                CHI_VALIDATE(ctx.compute->dispatch(grid[0], grid[1], grid[2]));
            }
        }

        //SL_LOG_HINT("-----------------------------------------------------------------------------------------------------------------------------------------");

        nrd::CommonSettings commonSettings{};
        auto tmp = transpose(ctx.commonConsts->cameraViewToClip);
        memcpy(commonSettings.viewToClipMatrix, &tmp, sizeof(float4x4));
        memcpy(commonSettings.viewToClipMatrixPrev, ctx.viewport->instance->prevCommonSettings.viewToClipMatrix, sizeof(float4x4));
        tmp = transpose(ctx.nrdConsts->common.worldToViewMatrix);
        memcpy(commonSettings.worldToViewMatrix, &tmp, sizeof(float4x4));
        memcpy(commonSettings.worldToViewMatrixPrev, ctx.viewport->instance->prevCommonSettings.worldToViewMatrix, sizeof(float4x4));
        memcpy(commonSettings.cameraJitter, &ctx.commonConsts->jitterOffset, sizeof(float2));

        memcpy(commonSettings.motionVectorScale, &ctx.nrdConsts->common.motionVectorScale, sizeof(float2));
        memcpy(commonSettings.resolutionScale, &ctx.nrdConsts->common.resolutionScale, sizeof(float2));
        commonSettings.timeDeltaBetweenFrames = ctx.nrdConsts->common.timeDeltaBetweenFrames;
        commonSettings.denoisingRange = ctx.nrdConsts->common.denoisingRange;
        commonSettings.disocclusionThreshold = ctx.nrdConsts->common.disocclusionThreshold;
        commonSettings.splitScreen = ctx.nrdConsts->common.splitScreen;
        commonSettings.inputSubrectOrigin[0] = depth.getExtent().left;
        commonSettings.inputSubrectOrigin[1] = depth.getExtent().top;
        commonSettings.frameIndex = ctx.viewport->frameIndex;
        commonSettings.accumulationMode = (nrd::AccumulationMode)ctx.nrdConsts->common.accumulationMode;
        commonSettings.isMotionVectorInWorldSpace = ctx.commonConsts->motionVectors3D == Boolean::eTrue;
        commonSettings.isHistoryConfidenceInputsAvailable = ctx.nrdConsts->common.isHistoryConfidenceInputsAvailable;

        ctx.viewport->instance->prevCommonSettings = commonSettings;

        for (uint32_t i = 0; i < ctx.viewport->instance->methodCount; i++)
        {
            if (ctx.viewport->instance->methodDescs[i].method == nrd::Method::REBLUR_DIFFUSE_SPECULAR)
            {
                ctx.setMethodSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->methodDescs[i].method, &ctx.nrdConsts->reblurSettings);
            }
            else if (ctx.viewport->instance->methodDescs[i].method == nrd::Method::REBLUR_DIFFUSE ||
                ctx.viewport->instance->methodDescs[i].method == nrd::Method::REBLUR_DIFFUSE_OCCLUSION)
            {
                ctx.setMethodSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->methodDescs[i].method, &ctx.nrdConsts->reblurSettings);
            }
            else if (ctx.viewport->instance->methodDescs[i].method == nrd::Method::REBLUR_SPECULAR)
            {
                ctx.setMethodSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->methodDescs[i].method, &ctx.nrdConsts->reblurSettings);
            }
            else if (ctx.viewport->instance->methodDescs[i].method == nrd::Method::RELAX_DIFFUSE_SPECULAR)
            {
                ctx.setMethodSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->methodDescs[i].method, &ctx.nrdConsts->relaxDiffuseSpecular);
            }
            else if (ctx.viewport->instance->methodDescs[i].method == nrd::Method::RELAX_DIFFUSE)
            {
                ctx.setMethodSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->methodDescs[i].method, &ctx.nrdConsts->relaxDiffuse);
            }
            else if (ctx.viewport->instance->methodDescs[i].method == nrd::Method::RELAX_SPECULAR)
            {
                ctx.setMethodSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->methodDescs[i].method, &ctx.nrdConsts->relaxSpecular);
            }
            else
            {
                ctx.setMethodSettings(*ctx.viewport->instance->denoiser, ctx.viewport->instance->methodDescs[i].method, &ctx.nrdConsts->sigmaShadow);
            }
        }

        const nrd::DispatchDesc* dispatchDescs = nullptr;
        uint32_t dispatchDescNum = 0;
        NRD_CHECK1(ctx.getComputeDispatches(*ctx.viewport->instance->denoiser, commonSettings, dispatchDescs, dispatchDescNum));

        CHI_VALIDATE(ctx.compute->beginPerfSection(cmdList, "sl.nrd"));

        const nrd::DenoiserDesc& denoiserDesc = ctx.getDenoiserDesc(*ctx.viewport->instance->denoiser);
        for (uint32_t dispatchID = 0; dispatchID < dispatchDescNum; ++dispatchID)
        {
            const nrd::DispatchDesc& dispatch = dispatchDescs[dispatchID];
            const nrd::PipelineDesc& pipeline = denoiserDesc.pipelines[dispatch.pipelineIndex];

            extra::ScopedTasks reverseTransitions;
            std::vector<chi::ResourceTransition> transitions;
            CHI_VALIDATE(ctx.compute->bindKernel(ctx.viewport->instance->shaders[dispatch.pipelineIndex]));

            for (uint32_t samplerID = 0; samplerID < denoiserDesc.staticSamplerNum; ++samplerID)
            {
                const nrd::StaticSamplerDesc& samplerDesc = denoiserDesc.staticSamplers[samplerID];
                switch (denoiserDesc.staticSamplers[samplerID].sampler)
                {
                case nrd::Sampler::NEAREST_CLAMP:
                    CHI_VALIDATE(ctx.compute->bindSampler(samplerID, samplerDesc.registerIndex, chi::Sampler::eSamplerPointClamp)); break;
                case nrd::Sampler::NEAREST_MIRRORED_REPEAT:
                    CHI_VALIDATE(ctx.compute->bindSampler(samplerID, samplerDesc.registerIndex, chi::Sampler::eSamplerPointMirror)); break;
                case nrd::Sampler::LINEAR_CLAMP:
                    CHI_VALIDATE(ctx.compute->bindSampler(samplerID, samplerDesc.registerIndex, chi::Sampler::eSamplerLinearClamp)); break;
                case nrd::Sampler::LINEAR_MIRRORED_REPEAT:
                    CHI_VALIDATE(ctx.compute->bindSampler(samplerID, samplerDesc.registerIndex, chi::Sampler::eSamplerLinearMirror)); break;
                default: SL_LOG_ERROR( "Unknown sampler detected");
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
                        SL_LOG_ERROR( "Mismatch slot and resourceNum");
                    }
                    chi::Resource texture = {};
                    auto state = chi::ResourceState::eUnknown;
                    const nrd::Resource& resource = dispatch.resources[slot++];
                    if (resource.stateNeeded != descriptorRange.descriptorType)
                    {
                        SL_LOG_ERROR( "Mismatch stateNeeded and descriptor type");
                    }
                    switch (resource.type)
                    {
                    case nrd::ResourceType::IN_MV:
                        texture = ctx.viewport->mvec;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_NORMAL_ROUGHNESS:
                        texture = normalRoughness;
                        state = ctx.cachedStates[texture];
                        break;
                    case nrd::ResourceType::IN_VIEWZ:
                        texture = ctx.viewport->viewZ;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_DIFF_HITDIST:
                        texture = ctx.viewport->packedAO;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:
                        texture = ctx.viewport->packedDiff;
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:
                        texture = ctx.viewport->packedSpec;
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
                        state = ctx.cachedStates[texture];
                        break;
                    case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:
                        texture = diffuseOut;
                        state = ctx.cachedStates[texture];
                        break;
                    case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:
                        texture = specularOut;
                        state = ctx.cachedStates[texture];
                        break;
                    case nrd::ResourceType::OUT_SHADOW_TRANSLUCENCY:
                        /**/
                        break;
                    case nrd::ResourceType::TRANSIENT_POOL:
                        texture = ctx.viewport->instance->transientTextures[resource.indexInPool];
                        state = chi::ResourceState::eTextureRead;
                        break;
                    case nrd::ResourceType::PERMANENT_POOL:
                        texture = ctx.viewport->instance->permanentTextures[resource.indexInPool];
                        state = chi::ResourceState::eTextureRead;
                        break;
                    };
                    if (!texture)
                    {
                        SL_LOG_ERROR( "Unable to find texture for nrd::ResourceType %u", resource.type);
                    }
                    uint32_t bindingSlot = descriptorRange.baseRegisterIndex + descriptorID;
                    if (descriptorRange.descriptorType == nrd::DescriptorType::TEXTURE)
                    {
                        // TODO: Fix binding pos for VK
                        CHI_VALIDATE(ctx.compute->bindTexture(descriptorRange.descriptorNum, bindingSlot, texture, resource.mipOffset, resource.mipNum));
                        transitions.push_back(chi::ResourceTransition(texture, chi::ResourceState::eTextureRead, state));
                    }
                    else
                    {
                        // TODO: Fix binding pos for VK
                        CHI_VALIDATE(ctx.compute->bindRWTexture(descriptorRange.descriptorNum, bindingSlot, texture, resource.mipOffset));
                        transitions.push_back(chi::ResourceTransition(texture, chi::ResourceState::eStorageRW, state));
                    }
                }
            }

            // TODO: Fix binding pos for VK
            CHI_VALIDATE(ctx.compute->bindConsts(0, denoiserDesc.constantBufferDesc.registerIndex, (void*)dispatch.constantBufferData, denoiserDesc.constantBufferDesc.maxDataSize, 3 * dispatchDescNum));
            CHI_VALIDATE(ctx.compute->transitionResources(cmdList, transitions.data(), (uint32_t)transitions.size(), &reverseTransitions));
            CHI_VALIDATE(ctx.compute->dispatch(dispatch.gridWidth, dispatch.gridHeight, 1));
            //SL_LOG_HINT("Dispatch %s", dispatch.name);
        }
        float ms = 0;
        CHI_VALIDATE(ctx.compute->endPerfSection(cmdList, "sl.nrd", ms));

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

    ctx.getLibraryDesc = (PFunGetLibraryDesc*)GetProcAddress(ctx.lib, "GetLibraryDesc");
    ctx.createDenoiser = (PFunCreateDenoiser*)GetProcAddress(ctx.lib, "CreateDenoiser");
    ctx.getDenoiserDesc = (PFunGetDenoiserDesc*)GetProcAddress(ctx.lib, "GetDenoiserDesc");
    ctx.setMethodSettings = (PFunSetMethodSettings*)GetProcAddress(ctx.lib, "SetMethodSettings");
    ctx.getComputeDispatches = (PFunGetComputeDispatches*)GetProcAddress(ctx.lib, "GetComputeDispatches");
    ctx.destroyDenoiser = (PFunDestroyDenoiser*)GetProcAddress(ctx.lib, "DestroyDenoiser");

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
    SL_EXPORT_FUNCTION(slSetConstants);
    SL_EXPORT_FUNCTION(slAllocateResources);
    SL_EXPORT_FUNCTION(slFreeResources);

    return nullptr;
}
}
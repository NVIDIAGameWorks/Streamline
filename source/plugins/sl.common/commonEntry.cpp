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

#include <dxgi1_6.h>
#include <d3d11_4.h>

#include <algorithm>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

#include "include/sl.h"
#include "source/core/sl.api/internal.h"
#include "include/sl_consts.h"
#include "include/sl_helpers.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.interposer/d3d12/d3d12.h"
#include "source/core/sl.interposer/vulkan/layer.h"
#include "source/plugins/sl.common/versions.h"
#include "source/platforms/sl.chi/d3d12.h"
#include "source/platforms/sl.chi/vulkan.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "source/plugins/sl.common/resourceTaggingForFrame.h"
#include "source/plugins/sl.common/commonDRSInterface.h"
#include "source/plugins/sl.common/drs.h"
#include "source/plugins/sl.pcl/pclImpl.h"
#include "_artifacts/gitVersion.h"
#include "_artifacts/json/common_json.h"

#ifdef SL_WINDOWS
#define NV_WINDOWS
// NT_SUCCESS macro
#include <winternl.h>
// Needed for SHGetKnownFolderPath
#include <ShlObj.h>
#pragma comment(lib,"shlwapi.lib")
#endif

#include "external/ngx-sdk/include/nvsdk_ngx.h"
#include "external/ngx-sdk/include/nvsdk_ngx_helpers.h"
#include "external/ngx-sdk/include/nvsdk_ngx_helpers_vk.h"
#include "external/ngx-sdk/include/nvsdk_ngx_defs.h"
#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

PCLSTATS_DEFINE()

#define SL_TAG_LOG_ENABLE 0

namespace sl
{

// Implemented in the common interface
extern HRESULT slHookCreateCommittedResource(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pResourceDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource, void** ppvResource);
extern HRESULT slHookCreatePlacedResource(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource);
extern HRESULT slHookCreateReservedResource(const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource);
extern HRESULT slHookPresent(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, bool& Skip);
extern HRESULT slHookAfterPresent(UINT Flags);
extern HRESULT slHookPresent1(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, DXGI_PRESENT_PARAMETERS* params, bool& Skip);
extern HRESULT slHookResizeSwapChainPre(IDXGISwapChain* swapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags, bool& Skip);

extern HRESULT slHookCreateSwapChain(IDXGIFactory* pFactory, IUnknown* pDevice, DXGI_SWAP_CHAIN_DESC* pDesc, IDXGISwapChain** ppSwapChain, bool& Skip);
extern HRESULT slHookCreateSwapChainForHwnd(IDXGIFactory2* pFactory, IUnknown* pDevice, HWND hWnd, const DXGI_SWAP_CHAIN_DESC1* pDesc, const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* pFullscreenDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain, bool& Skip);
extern HRESULT slHookCreateSwapChainForCoreWindow(IDXGIFactory2* pFactory, IUnknown* pDevice, IUnknown* pWindow, const DXGI_SWAP_CHAIN_DESC1* pDesc, IDXGIOutput* pRestrictToOutput, IDXGISwapChain1** ppSwapChain, bool& Skip);

// VULKAN
extern VkResult slHookVkPresent(VkQueue Queue, const VkPresentInfoKHR* PresentInfo, bool& Skip);
extern VkResult slHookVkAfterPresent();
extern void slHookVkCmdBindPipeline(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipeline Pipeline);
extern void slHookVkCmdBindDescriptorSets(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipelineLayout Layout, uint32_t FirstSet, uint32_t DescriptorSetCount, const VkDescriptorSet* DescriptorSets, uint32_t DynamicOffsetCount, const uint32_t* DynamicOffsets);
extern void slHookVkBeginCommandBuffer(VkCommandBuffer CommandBuffer, const VkCommandBufferBeginInfo* BeginInfo);

extern bool getSystemCaps(common::SystemCaps*& info);
extern sl::Result slEvaluateFeatureInternal(sl::Feature feature, const sl::FrameToken& frame, const sl::BaseStructure** inputs, uint32_t numInputs, sl::CommandBuffer* cmdBuffer);

struct NGXContextStandard : public common::NGXContext
{
};

struct NGXContextD3D12 : public common::NGXContext
{
};

namespace common
{
//! Our common context
//! 
//! Here we keep tagged resources, NGX context
//! and other common stuff that comes along
//! and can be shared with other plugins.
//! 
struct CommonEntryContext
{
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(CommonEntryContext);

    void onCreateContext() {};
    void onDestroyContext() {};

    bool needNGX = false;
    bool needDX11On12 = false;
    bool needDRS = false;

    NGXContextStandard ngxContext{};    // Regular context based on requested API from the host
    NGXContextD3D12 ngxContextD3D12{};  // Special context for plugins which run d3d11 on d3d12

    DRSContext drsContext{};            // DRS context for plugins which use d3dreg keys

    common::SystemCaps* caps{};

    chi::IResourcePool* pool{};
    chi::ICompute* compute{};
    chi::ICompute* computeD3D12{}; // Only valid when running D3D11 and some features require "d3d11 on 12"
    RenderAPI platform = RenderAPI::eD3D12;

    std::unique_ptr<ResourceTaggingBase> pBaseResourceTagging{};
    // Common constants must be set every frame, we allow up to 3 frames in flight
    common::ViewportIdFrameData<3, true> constants = { "common" };
    std::unordered_map<Feature, std::vector<uint32_t>> featureViewportIds;
    std::mutex featureViewportIdsMutex;

    // WAR for interposer < 2.3 which don't load PCL plugin.  PCL functionality will instead be handled in sl.common.
    PCLOptions pclOptions{};
    bool enablePCLPluginInCommonWAR = false;
    // Updated from sl::Preferences flag for the same to track whether resource tagging is frame-based.
    // In absence of common implementation or host SL SDK version check, this is necessary because legacy tagging API doesn't support
    // frame-based resource-tagging and thereby resource-tagging for multiple frames in the same frame as required by some SL clients
    // and is therefore not frame-aware.
    bool useResourceTaggingForFrame = false;


};
}

//! Embedded JSON, containing information about the plugin and the hooks it requires.
static std::string JSON = std::string(common_json, &common_json[common_json_len]);

void updateEmbeddedJSON(json& config);

//! Define our plugin
SL_PLUGIN_DEFINE("sl.common", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON.c_str(), updateEmbeddedJSON, common, CommonEntryContext)

extern uint64_t getCurrentFrame();

//! Thread safe get/set resource tag
//! 
void getCommonTag(BufferType tagType, uint32_t frameId, uint32_t viewportId, CommonResource& res, const sl::BaseStructure** inputs, uint32_t numInputs, bool optional)
{
    auto& ctx = (*common::getContext());

    if (ctx.pBaseResourceTagging == nullptr)
    {
        SL_LOG_ERROR("SL Resource tagging not properly initialized!");
        assert("SL Resource tagging not properly initialized!" && false);
        return;
    }

    ctx.pBaseResourceTagging->getTag(tagType, frameId, viewportId, res, inputs, numInputs, optional);
}

void recycleFramePresentEndResourceTags()
{
    auto& ctx = (*common::getContext());

    if (ctx.pBaseResourceTagging == nullptr)
    {
        SL_LOG_ERROR("SL Resource tagging not properly initialized!");
        assert("SL Resource tagging not properly initialized!" && false);
        return;
    }

    if (ctx.useResourceTaggingForFrame)
    {
        auto pResourceTaggingForFrame = dynamic_cast<common::ResourceTaggingForFrame*>(ctx.pBaseResourceTagging.get());
        assert(pResourceTaggingForFrame != nullptr);
        pResourceTaggingForFrame->recycleTags();
    }
}

sl::Result common::ResourceTaggingGeneral::setTag(const sl::Resource* resource,
    BufferType tag,
    uint32_t id,
    const Extent* ext,
    ResourceLifecycle lifecycle,
    CommandBuffer* cmdBuffer,
    bool localTag,
    const PrecisionInfo* pi,
    const sl::FrameToken& frame)
{
    auto& ctx = (*common::getContext());
    uint64_t uid = ((uint64_t)tag << 32) | (uint64_t)id;
    CommonResource cr{};
    cr.uFrameWhenTagged = getCurrentFrame();
    if (resource && resource->native)
    {
        cr.res = *(sl::Resource*)resource;
        if (ctx.platform == RenderAPI::eD3D11)
        {
            // Force common state for d3d11 in case engine is providing something that won't work on compute queue
            cr.res.state = 0;
        }
#if defined(SL_PRODUCTION) || defined(SL_DEVELOP)
        // Check if state is provided but only if not running on D3D11
        if (ctx.platform != RenderAPI::eD3D11 && resource->state == UINT_MAX)
        {
            SL_LOG_ERROR("Resource state must be provided");
            return sl::Result::eErrorMissingResourceState;
        }
#endif
        //! Check for volatile tags
        //! 
        //! Note that tagging outputs as volatile is ignored, we need to write output into the engine's resource
        //!
        bool writeTag = tag == kBufferTypeScalingOutputColor || tag == kBufferTypeAmbientOcclusionDenoised ||
            tag == kBufferTypeShadowDenoised || tag == kBufferTypeSpecularHitDenoised || tag == kBufferTypeDiffuseHitDenoised ||
            tag == kBufferTypeBackbuffer;
        if (!writeTag && lifecycle != ResourceLifecycle::eValidUntilPresent)
        {
            //! Only make a copy if this tag is required by at least one loaded and supported plugin on the same viewport and with immutable life-cycle.
            //! 
            //! If tag is required on present we have to make a copy always, if tag is required on evaluate
            //! we make a copy only if buffer is tagged as "valid only now" and this is not a local tag.
            resourceTagMutex.lock();
            auto requiredOnPresent = requiredTags.find({ id, tag, ResourceLifecycle::eValidUntilPresent }) != requiredTags.end();
            auto requiredOnEvaluate = requiredTags.find({ id, tag, ResourceLifecycle::eValidUntilEvaluate }) != requiredTags.end();
            resourceTagMutex.unlock();
            auto makeCopy = requiredOnPresent || (requiredOnEvaluate && lifecycle == ResourceLifecycle::eOnlyValidNow && !localTag);

            if (makeCopy)
            {
                if (!cmdBuffer)
                {
                    SL_LOG_ERROR("Valid command buffer is required when tagging resources");
                    return Result::eErrorMissingInputParameter;
                }
                cmdBuffer = common::getNativeCommandBuffer(cmdBuffer);

                // Actual resource to use
                auto actualResource = (chi::Resource)resource;

                // Quick peek at the previous tag with the same id
                resourceTagMutex.lock();
                auto prevTag = idToResourceMap[uid];
                resourceTagMutex.unlock();

                if (prevTag.clone)
                {
                    ctx.pool->recycle(prevTag.clone);
                }
                else
                {
                    ctx.compute->stopTrackingResource(uid, &prevTag.res);
                }

                // Defaults to eCopyDestination state 
                cr.clone = ctx.pool->allocate(actualResource, (SL_RESOURCE_NAME("clone.tagVolatile.") + sl::getBufferTypeAsStr(tag) + "." + std::to_string(id)).c_str());

                // Get tagged resource's state
                chi::ResourceState state{};
                ctx.compute->getResourceState(cr.res.state, state);
                // Now store clone's state for further use in SL
                ctx.compute->getNativeResourceState(chi::ResourceState::eCopyDestination, cr.res.state);
                extra::ScopedTasks revTransitions;
                chi::ResourceTransition transitions[] =
                {
                    {actualResource, chi::ResourceState::eCopySource, state},
                };
                CHI_CHECK_RR(ctx.compute->transitionResources(cmdBuffer, transitions, (uint32_t)countof(transitions), &revTransitions));
                CHI_CHECK_RR(ctx.compute->copyResource(cmdBuffer, cr.clone, actualResource));

                // We've made a copy of the original resource and we're not doing AddRef() on the original. So set the
                // original to nullptr - that way nobody can access it (it may become invalid at some point).
                cr.res.native = nullptr;
            }
        }
    }

    if (ext)
    {
        cr.extent = *ext;
    }

    if (pi)
    {
        cr.pi = *pi;
    }

    // No need to track volatile resources since we keep a copy
    if (cr.clone == nullptr)
    {
        // If this is a local tag but it was not copied there is nothing more to do, bail out
        if (localTag) return Result::eOk;

        if (cr.res.native)
        {
            ctx.compute->startTrackingResource(uid, &cr.res);
        }
        else
        {
            ctx.compute->stopTrackingResource(uid, &cr.res);
        }
    }

    std::lock_guard<std::mutex> lock(resourceTagMutex);
    auto& prevTag = idToResourceMap[uid];
    if (prevTag.clone && !cr.clone)
    {
        // Host can set null as a tag or even change the life-cycle of a tag, in that case any previously allocated copies must be recycled
        ctx.pool->recycle(prevTag.clone);
    }
    prevTag = cr;
    return Result::eOk;
}

//! Thread safe get/set resource tag
//! 
void common::ResourceTaggingGeneral::getTag(BufferType tagType, uint32_t frameId, uint32_t viewportId, CommonResource& res, const sl::BaseStructure** inputs, uint32_t numInputs, bool optional)
{
    auto& ctx = (*common::getContext());

    //! First look for local tags
    if (inputs)
    {
        std::vector<ResourceTag*> tags;
        if (findStructs<ResourceTag>((const void**)inputs, numInputs, tags))
        {
            for (auto& tag : tags)
            {
                if (tag->type == tagType)
                {
                    res.extent = tag->extent;
                    res.res = *tag->resource;

                    // Optional extensions are chained after the tag they belong to
                    PrecisionInfo* optPi = findStruct<PrecisionInfo>(tag->next);
                    res.pi = optPi ? *optPi : PrecisionInfo{};

                    //! Keep track of what tags are requested for what viewport (unique insert)
                    //! 
                    //! Note that the presence of a valid pointer to 'inputs' 
                    //! indicates that we are called during the evaluate feature call.
                    std::lock_guard<std::mutex> lock(resourceTagMutex);
                    requiredTags.insert({ viewportId, tagType, ResourceLifecycle::eValidUntilEvaluate });

                    return;
                }
            }
        }
    }

    //! Now let's check the global ones
    uint64_t uid = ((uint64_t)tagType << 32) | (uint64_t)viewportId;
    std::lock_guard<std::mutex> lock(resourceTagMutex);
    // legacy tag-retrieval implementation
    CommonResource& resTmp = idToResourceMap[uid];
    uint64_t uCurFrame = getCurrentFrame();
    if (resTmp.uFrameWhenTagged != ~0ull && uCurFrame > resTmp.uFrameWhenTagged + 1)
    {
        if (resTmp)
        {
            SL_LOG_WARN("Tags are valid only until Present(). Invalidating the hanging tag %d for viewport %d (created at frame: %d, current frame: %d)", tagType, viewportId, resTmp.uFrameWhenTagged, uCurFrame);
            if (resTmp.clone)
            {
                ctx.pool->recycle(resTmp.clone);
            }
            else
            {
                ctx.compute->stopTrackingResource(uid, &resTmp.res);
            }
        }
        resTmp = CommonResource();
    }
    res = resTmp;

    //! Keep track of what tags are requested for what viewport (unique insert)
    //! 
    //! Note that the presence of a valid pointer to 'inputs' indicates that we are called
    //! during the evaluate feature call, otherwise tag was requested from a hook (present etc.)
    requiredTags.insert({ viewportId, tagType, inputs ? ResourceLifecycle::eValidUntilEvaluate : ResourceLifecycle::eValidUntilPresent });
}

common::ResourceTaggingGeneral::~ResourceTaggingGeneral()
{
    auto& ctx = (*common::getContext());

    assert(ctx.pool != nullptr);
    assert(ctx.compute != nullptr);

    std::lock_guard<std::mutex> lock(resourceTagMutex);
    requiredTags.clear();

    for (auto&[resourceTagId, resourceTag] : idToResourceMap)
    {
        ctx.pool->recycle(resourceTag.clone);
        ctx.compute->stopTrackingResource(resourceTagId, &resourceTag.res);
        resourceTag = {};
    }

    idToResourceMap.clear();
}

sl::Result setTagCommon(const sl::ViewportHandle& viewport, const sl::ResourceTag* resources, uint32_t numResources, sl::CommandBuffer* cmdBuffer, const sl::FrameToken& frame)
{
    auto& ctx = (*common::getContext());

    if (ctx.pBaseResourceTagging == nullptr)
    {
        SL_LOG_ERROR("SL Resource tagging not properly initialized!");
        assert("SL Resource tagging not properly initialized!" && false);
        return Result::eErrorNotInitialized;
    }

    for (uint32_t i = 0; i < numResources; i++)
    {
        auto tag = &resources[i];
        while (tag != nullptr)
        {
            // Find the optional extension PrecisionInfo, until we see a ResourceTag (or nullptr) in the linked list
            PrecisionInfo* optPi = findStruct<PrecisionInfo, ResourceTag>(tag->next);
            SL_CHECK(ctx.pBaseResourceTagging->setTag(tag->resource, tag->type, viewport, &tag->extent, tag->lifecycle, cmdBuffer, false, optPi, frame));

            tag = findStruct<ResourceTag>(tag->next);
        }
    }

    return sl::Result::eOk;
}

sl::Result slSetTag(const sl::ViewportHandle& viewport, const sl::ResourceTag* resources, uint32_t numResources, sl::CommandBuffer* cmdBuffer)
{
    auto& ctx = (*common::getContext());
    chi::ScopedProfilingSection ScopedSection(ctx.compute, cmdBuffer, __FUNCTION__, resources, numResources);
    if (ctx.useResourceTaggingForFrame)
    {
        SL_LOG_ERROR("Deprecated SL API slSetTag and the new API slSetTagForFrame are both being invalidly used in the same application! Please switch to slSetTagForFrame API.");
        return Result::eErrorInvalidIntegration;
    }

    return setTagCommon(viewport, resources, numResources, cmdBuffer, FrameHandleImplementation{});
}

sl::Result slSetTagForFrame(const sl::FrameToken& frame, const sl::ViewportHandle& viewport, const sl::ResourceTag* resources, uint32_t numResources, sl::CommandBuffer* cmdBuffer)
{
    chi::ScopedProfilingSection ScopedSection((*common::getContext()).compute, cmdBuffer, __FUNCTION__, resources, numResources);
    return setTagCommon(viewport, resources, numResources, cmdBuffer, frame);
}

//! Make sure host has provided common constants and
//! has not left something as an invalid value
void validateCommonConstants(const Constants& consts)
{
#define SL_VALIDATE_FLOAT4x4(v) if(v[0].x == INVALID_FLOAT) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}
    SL_VALIDATE_FLOAT4x4(consts.cameraViewToClip);
    SL_VALIDATE_FLOAT4x4(consts.clipToCameraView);
    SL_VALIDATE_FLOAT4x4(consts.clipToPrevClip);
    SL_VALIDATE_FLOAT4x4(consts.prevClipToClip);

#define SL_VALIDATE_FLOAT2(v) if(v.x == INVALID_FLOAT || v.y == INVALID_FLOAT) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}
    SL_VALIDATE_FLOAT2(consts.jitterOffset);
    SL_VALIDATE_FLOAT2(consts.mvecScale);
    SL_VALIDATE_FLOAT2(consts.cameraPinholeOffset);

#define SL_VALIDATE_FLOAT3(v) if(v.x == INVALID_FLOAT || v.y == INVALID_FLOAT || v.z == INVALID_FLOAT) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}

    SL_VALIDATE_FLOAT3(consts.cameraPos);
    SL_VALIDATE_FLOAT3(consts.cameraUp);
    SL_VALIDATE_FLOAT3(consts.cameraRight);
    SL_VALIDATE_FLOAT3(consts.cameraFwd);

#define SL_VALIDATE_FLOAT(v) if(v == INVALID_FLOAT) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}

    SL_VALIDATE_FLOAT(consts.cameraNear);
    SL_VALIDATE_FLOAT(consts.cameraFar);
    SL_VALIDATE_FLOAT(consts.cameraFOV);
    SL_VALIDATE_FLOAT(consts.cameraAspectRatio);
    
    // By the spec (sl_consts.h), motionVectorsInvalidValue only needs to be set when cameraMotionIncluded is set to false.
    // So if the app has set cameraMotionIncluded to true explicitly, then we do not check this value.
    if (consts.cameraMotionIncluded != sl::eTrue)
    {
        SL_VALIDATE_FLOAT(consts.motionVectorsInvalidValue);
    }

#define SL_VALIDATE_BOOL(v) if(v == Boolean::eInvalid) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}

    SL_VALIDATE_BOOL(consts.depthInverted);
    SL_VALIDATE_BOOL(consts.cameraMotionIncluded);
    SL_VALIDATE_BOOL(consts.motionVectors3D);
    SL_VALIDATE_BOOL(consts.reset);
    SL_VALIDATE_BOOL(consts.orthographicProjection);
    SL_VALIDATE_BOOL(consts.motionVectorsDilated);
    SL_VALIDATE_BOOL(consts.motionVectorsJittered);

    // minRelativeLinearDepthObjectSeparation does not need to be validated. It's entirely optional.
}

//! Thread safe get/set common constants
sl::Result slSetConstants(const sl::Constants& consts, const sl::FrameToken& frame, const sl::ViewportHandle& viewport)
{
    SL_RUN_ONCE
    {
        validateCommonConstants(consts);
    }
    // Common constants are per frame, per special id (viewport, instance etc)
    if (!(*common::getContext()).constants.set(frame, viewport, &consts))
    {
        return sl::Result::eErrorDuplicatedConstants;
    }
    return sl::Result::eOk;
}

common::GetDataResult getCommonConstants(const common::EventData& ev, Constants** consts)
{
    return (*common::getContext()).constants.get(ev, consts);
}

void registerCommonFeatureViewport(Feature feature, uint32_t viewportId)
{
    auto& ctx = (*common::getContext());
    std::lock_guard<std::mutex> lock(ctx.featureViewportIdsMutex);
    auto& viewportIds = ctx.featureViewportIds[feature];
    if (std::find(viewportIds.begin(), viewportIds.end(), viewportId) == viewportIds.end())
    {
        viewportIds.push_back(viewportId);
    }
}

void unregisterCommonFeatureViewport(Feature feature, uint32_t viewportId)
{
    auto& ctx = (*common::getContext());
    std::lock_guard<std::mutex> lock(ctx.featureViewportIdsMutex);
    auto it = ctx.featureViewportIds.find(feature);
    if (it != ctx.featureViewportIds.end())
    {
        auto& viewportIds = it->second;
        viewportIds.erase(std::remove(viewportIds.begin(), viewportIds.end(), viewportId), viewportIds.end());
        if (viewportIds.empty())
        {
            ctx.featureViewportIds.erase(it);
        }
    }
}

namespace common
{
bool getFeatureViewports(Feature feature, std::vector<uint32_t>& viewportIds)
{
    auto& ctx = (*common::getContext());
    std::lock_guard<std::mutex> lock(ctx.featureViewportIdsMutex);
    auto it = ctx.featureViewportIds.find(feature);
    if (it == ctx.featureViewportIds.end())
    {
        viewportIds.clear();
        return false;
    }
    viewportIds = it->second;
    return !viewportIds.empty();
}
}

sl::Result slEvaluateFeature(sl::Feature feature, const sl::FrameToken& frame, const sl::BaseStructure** inputs, uint32_t numInputs, sl::CommandBuffer* cmdBuffer)
{

    chi::ScopedProfilingSection ScopedSection((*common::getContext()).compute, cmdBuffer, __FUNCTION__, feature);
    // Check if host provided tags or constants in the eval call

    auto viewport = findStruct<ViewportHandle>((const void**)inputs, numInputs);
    if (!viewport)
    {
        SL_LOG_ERROR("Missing viewport handle, did you forget to chain it up in the slEvaluateFeature inputs?");
        return Result::eErrorMissingInputParameter;
    }

    //! Look for local tags that won't be valid later on
    if (inputs)
    {
        auto& ctx = (*common::getContext());

        if (ctx.pBaseResourceTagging == nullptr)
        {
            SL_LOG_ERROR("SL Resource tagging not properly initialized!");
            assert("SL Resource tagging not properly initialized!" && false);
            return Result::eErrorNotInitialized;
        }

        std::vector<ResourceTag*> tags{};
        if (findStructs<ResourceTag>((const void**)inputs, numInputs, tags))
        {
            for (auto& tag : tags)
            {
                if (tag->lifecycle != ResourceLifecycle::eValidUntilPresent)
                {
                    // Optional extensions are chained after the tag they belong to
                    PrecisionInfo* optPi = findStruct<PrecisionInfo>(tag->next);

                    //! Temporary tag, hence passing true
                    SL_CHECK(ctx.pBaseResourceTagging->setTag(tag->resource, tag->type, *viewport, &tag->extent, tag->lifecycle, cmdBuffer, true, optPi, frame));
                }
            }
        }
    }

    return slEvaluateFeatureInternal(feature, frame, inputs, numInputs, cmdBuffer);
}

bool getStringFromModule(const char* moduleName, const char* stringName, std::string& value)
{
    TCHAR filename[MAX_PATH + 1]{};
    //if (GetModuleFileName(GetModuleHandle(moduleName), filename, MAX_PATH) == 0)
    {
        wchar_t* slPluginPathUtf16{};
        param::getPointerParam(api::getContext()->parameters, param::global::kPluginPath, &slPluginPathUtf16);
        if (!slPluginPathUtf16) return false;
        std::string path = extra::utf16ToUtf8(slPluginPathUtf16) + std::string("\\") + moduleName;
        strcpy_s(filename, MAX_PATH + 1, path.c_str());
    }

    DWORD dummy;
    auto size = GetFileVersionInfoSize(filename, &dummy);
    if (size == 0)
    {
        return false;
    }
    std::vector<BYTE> data(size);

    if (!GetFileVersionInfo(filename, NULL, size, &data[0]))
    {
        return false;
    }

    LPVOID stringValue = NULL;
    uint32_t stringValueLen = 0;
    if (!VerQueryValueA(&data[0], (std::string("\\StringFileInfo\\040904e4\\") + stringName).c_str(), &stringValue, &stringValueLen))
    {
        return false;
    }
    value = (const char*)stringValue;
    return true;
}

namespace ngx
{

//! NGX management
//! 
//! Common spot for all NGX functionality, create/eval/release feature
//! 
//! Shared with all other plugins as NGXContext
//!

// Workaround (bug 5911849): nvngx_dlssg.dll versions prior to 310.2.2 will
// crash if NVSDK_NGX_VULKAN_CreateFeature1 is used. Query the DLL's FileVersion
// on first call and cache the result so we only pay the lookup cost once.
bool shouldSkipVulkanCreateFeature1(NVSDK_NGX_Feature feature)
{
    if (feature != NVSDK_NGX_Feature_FrameGeneration)
    {
        return false;
    }

    static std::optional<bool> cachedShouldSkipCreateFeature1 = std::nullopt;
    if (cachedShouldSkipCreateFeature1.has_value())
    {
        return cachedShouldSkipCreateFeature1.value();
    }

    std::string productNameStr;
    if (!getStringFromModule("nvngx_dlssg.dll", "ProductName", productNameStr))
    {
        // Can't access nvngx_dlssg.dll, assume CreateFeature1 is supported
        cachedShouldSkipCreateFeature1 = false;
        return false;
    }

    if (productNameStr == "NVIDIA DLSS-G")
    {
        // DLLs with product name exactly equal to "NVIDIA DLSS-G" don't have
        // the bug that causes the crash, so we can use CreateFeature1
        cachedShouldSkipCreateFeature1 = false;
        return false;
    }

    // Version 310.2.2 and above support CreateFeature1
    const Version kMinDlssgVersionForCreateFeature1(310, 2, 2);

    std::string versionStr;
    if (getStringFromModule("nvngx_dlssg.dll", "FileVersion", versionStr))
    {
        std::replace(versionStr.begin(), versionStr.end(), ',', '.');
        Version dllVersion;
        if (sscanf_s(versionStr.c_str(), "%u.%u.%u", &dllVersion.major, &dllVersion.minor, &dllVersion.build) == 3)
        {
            if (dllVersion < kMinDlssgVersionForCreateFeature1)
            {
                SL_LOG_INFO("nvngx_dlssg.dll version %s < %s - skipping VULKAN_CreateFeature1 for FrameGeneration",
                    dllVersion.toStr().c_str(), kMinDlssgVersionForCreateFeature1.toStr().c_str());
                cachedShouldSkipCreateFeature1 = true;
                return true;
            }
        }
    }

    cachedShouldSkipCreateFeature1 = false;
    return false;
}

bool createNGXFeature(void* cmdList, NVSDK_NGX_Feature feature, NVSDK_NGX_Handle** handle, const char* id)
{
    auto& ctx = (*common::getContext());

    extra::ScopedTasks vram([&ctx, id]()->void {ctx.compute->beginVRAMSegment(id); }, [&ctx]()->void {ctx.compute->endVRAMSegment(); });

    NVSDK_NGX_Result status{};
    if (ctx.platform == RenderAPI::eD3D11)
    {
        status = NVSDK_NGX_D3D11_CreateFeature((ID3D11DeviceContext*)cmdList, feature, ctx.ngxContext.params, handle);
    }
    else if (ctx.platform == RenderAPI::eD3D12)
    {
        status = NVSDK_NGX_D3D12_CreateFeature((ID3D12GraphicsCommandList*)cmdList, feature, ctx.ngxContext.params, handle);
    }
    else
    {
        status = NVSDK_NGX_Result_FAIL_NotImplemented;

        if (!shouldSkipVulkanCreateFeature1(feature))
        {
            // Vulkan can't query device from command buffer, so we provide it ourselves.
            chi::Device device;
            CHI_CHECK_RF(ctx.compute->getDevice(device));
            status = NVSDK_NGX_VULKAN_CreateFeature1((VkDevice)device, (VkCommandBuffer)cmdList, feature, ctx.ngxContext.params, handle);
        }

        // Some old features don't support CreateFeature1, so fall back to CreateFeature.
        if (status == NVSDK_NGX_Result_FAIL_NotImplemented || status == NVSDK_NGX_Result_FAIL_UnableToInitializeFeature)
        {
            status = NVSDK_NGX_VULKAN_CreateFeature((VkCommandBuffer)cmdList, feature, ctx.ngxContext.params, handle);
        }
    }

    if (status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("[%s] NGX create feature failed 0x%x", id, status); return false; }
    return true;
}

bool evaluateNGXFeature(void* cmdList, NVSDK_NGX_Handle* handle, const char* id)
{
    auto& ctx = (*common::getContext());

    extra::ScopedTasks vram([&ctx, id]()->void {ctx.compute->beginVRAMSegment(id); }, [&ctx]()->void {ctx.compute->endVRAMSegment(); });

    NVSDK_NGX_Result status{};
    if (ctx.platform == RenderAPI::eD3D11)
    {
        status = NVSDK_NGX_D3D11_EvaluateFeature((ID3D11DeviceContext*)cmdList, handle, ctx.ngxContext.params, nullptr);
    }
    else if (ctx.platform == RenderAPI::eD3D12)
    {
        status = NVSDK_NGX_D3D12_EvaluateFeature((ID3D12GraphicsCommandList*)cmdList, handle, ctx.ngxContext.params, nullptr);
    }
    else
    {
        status = NVSDK_NGX_VULKAN_EvaluateFeature((VkCommandBuffer)cmdList, handle, ctx.ngxContext.params, nullptr);
    }

    if (status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("[%s] NGX evaluate feature failed 0x%x", id, status); return false; }
    return true;
}

bool releaseNGXFeature(NVSDK_NGX_Handle* handle, const char* id)
{
    auto& ctx = (*common::getContext());

    extra::ScopedTasks vram([&ctx, id]()->void {ctx.compute->beginVRAMSegment(id); }, [&ctx]()->void {ctx.compute->endVRAMSegment(); });

    NVSDK_NGX_Result status{};
    if (ctx.platform == RenderAPI::eD3D11)
    {
        status = NVSDK_NGX_D3D11_ReleaseFeature(handle);
    }
    else if (ctx.platform == RenderAPI::eD3D12)
    {
        status = NVSDK_NGX_D3D12_ReleaseFeature(handle);
    }
    else
    {
        status = NVSDK_NGX_VULKAN_ReleaseFeature(handle);
    }

    if (status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("[%s] NGX release feature failed 0x%x", id, status); return false; }
    return true;
}

void updateNGXFeature(NVSDK_NGX_Feature feature)
{
    auto& ctx = (*common::getContext());

    PreferenceFlags preferenceFlags{};
    api::getContext()->parameters->get(sl::param::global::kPreferenceFlags, (uint64_t*)&preferenceFlags);
    if (preferenceFlags & PreferenceFlags::eAllowOTA)
    {
        //! Plugin manager gives us the device type and the application id
        json& config = *(json*)api::getContext()->loaderConfig;
        int appId = config.at("appId");
        EngineType engineType{};
        std::string engineVersion{};
        std::string projectId{};
        if (config.contains("ngx"))
        {
            engineType = config.at("ngx").at("engineType");
            engineVersion = config.at("ngx").at("engineVersion");
            projectId = config.at("ngx").at("projectId");
        }

        NVSDK_NGX_Application_Identifier applicationId;
        if (projectId.empty() && engineVersion.empty())
        {
            applicationId.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id;
            applicationId.v.ApplicationId = appId;
        }
        else
        {
            applicationId.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
            applicationId.v.ProjectDesc.EngineType = (NVSDK_NGX_EngineType)engineType;
            applicationId.v.ProjectDesc.EngineVersion = engineVersion.c_str();
            applicationId.v.ProjectDesc.ProjectId = projectId.c_str();
        }
        CHECK_NGX(NVSDK_NGX_UpdateFeature(&applicationId, feature));
    }
}

bool getNGXFeatureRequirements(NVSDK_NGX_Feature feature, common::PluginInfo& pluginInfo)
{
    auto& ctx = (*common::getContext());

    //! IMPORTANT: The logic here is as follows:
    //! 
    //! # SL sets min HW, OS, driver specs in the incoming plugin info structure
    //! # We ask NGX for up to date info about the same
    //! # If any NGX call fails for whatever reason we use SL defaults
    //! # If all NGX calls succeed we overwrite SL defaults as appropriate
    //! 
    NVSDK_NGX_FeatureRequirement result{};

    for (uint32_t i = 0; i < common::kMaxNumSupportedGPUs; i++)
    {
        // Find first NVDA adapter and get the info we need
        if (isVendorNvidia(ctx.caps->adapters[i].vendor))
        {
            auto adapter = (IDXGIAdapter*)ctx.caps->adapters[i].nativeInterface;

            //! Plugin manager gives us the info we need
            json& config = *(json*)api::getContext()->loaderConfig;
            int appId = config.at("appId");
            RenderAPI deviceType = (RenderAPI)config.at("deviceType");
            EngineType engineType{};
            std::string engineVersion{};
            std::string projectId{};
            if (config.contains("ngx"))
            {
                engineType = config.at("ngx").at("engineType");
                engineVersion = config.at("ngx").at("engineVersion");
                projectId = config.at("ngx").at("projectId");
            }

            NVSDK_NGX_Application_Identifier applicationId;
            if (projectId.empty() && engineVersion.empty())
            {
                applicationId.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id;
                applicationId.v.ApplicationId = appId;
            }
            else
            {
                applicationId.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
                applicationId.v.ProjectDesc.EngineType = (NVSDK_NGX_EngineType)engineType;
                applicationId.v.ProjectDesc.EngineVersion = engineVersion.c_str();
                applicationId.v.ProjectDesc.ProjectId = projectId.c_str();
            }

            wchar_t* pluginPath = {};
            param::getPointerParam(api::getContext()->parameters, param::global::kPluginPath, &pluginPath);

            NVSDK_NGX_FeatureCommonInfo featureInfo{};
            featureInfo.PathListInfo.Path = &pluginPath;
            featureInfo.PathListInfo.Length = 1;

            NVSDK_NGX_FeatureDiscoveryInfo info{};
            info.FeatureID = feature;
            info.SDKVersion = NVSDK_NGX_Version_API;
            info.ApplicationDataPath = file::getTmpPath();
            info.Identifier = applicationId;
            info.FeatureInfo = &featureInfo;
            NVSDK_NGX_Result ngxResult{};
            if (deviceType == RenderAPI::eD3D11)
            {
                ngxResult = NVSDK_NGX_D3D11_GetFeatureRequirements(adapter, &info, &result);
            }
            else if(deviceType == RenderAPI::eVulkan)
            {
                chi::Instance instance;
                chi::PhysicalDevice physicalDevice;
                CHI_CHECK_RF(chi::Vulkan::createInstanceAndFindPhysicalDevice(ctx.caps->adapters[i].deviceId, instance, physicalDevice));

                pluginInfo.opticalFlowInfo.nativeHWSupport = (chi::Vulkan::getOpticalFlowQueueInfo(physicalDevice, pluginInfo.opticalFlowInfo.queueFamily, pluginInfo.opticalFlowInfo.queueIndex) == sl::chi::ComputeStatus::eOk);
                if (pluginInfo.opticalFlowInfo.nativeHWSupport)
                {
                    // Native OFA always runs on the very first queue of the very first optical flow-capable queue family.
                    assert(pluginInfo.opticalFlowInfo.queueIndex == 0);
                    SL_LOG_INFO("Native VK OFA feature supported on this device!");

                    pluginInfo.minDriver = Version(527, 64, 0);
                    pluginInfo.minVkAPIVersion = VK_API_VERSION_1_1;
                }
                else
                {
                    SL_LOG_WARN("Native VK OFA feature not supported on this device! Falling back to OFA VK-CUDA interop feature.");
                }

                // If NGX fails and we have an early return clean up properly
                extra::ScopedTasks cleanup([&instance]()->void {CHI_VALIDATE(chi::Vulkan::destroyInstance(instance)); });

                ngxResult = NVSDK_NGX_VULKAN_GetFeatureRequirements((VkInstance)instance, (VkPhysicalDevice)physicalDevice, &info, &result);
                if (ngxResult == NVSDK_NGX_Result_Success)
                {
                    {
                        uint32_t count{};
                        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements((VkInstance)instance, (VkPhysicalDevice)physicalDevice, &info, &count, nullptr));
                        pluginInfo.vkDeviceExtensions.reserve(count);
                        std::vector<VkExtensionProperties> tmp(count);
                        auto extensions = tmp.data();
                        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements((VkInstance)instance, (VkPhysicalDevice)physicalDevice, &info, &count, &extensions));
                        for (uint32_t i = 0; i < count; i++) pluginInfo.vkDeviceExtensions.push_back(extensions[i].extensionName);
                    }
                    {
                        uint32_t count{};
                        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, nullptr));
                        std::vector<VkExtensionProperties> tmp(count);
                        auto extensions = tmp.data();
                        pluginInfo.vkInstanceExtensions.reserve(count);
                        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&info, &count, &extensions));
                        for (uint32_t i = 0; i < count; i++) pluginInfo.vkInstanceExtensions.push_back(extensions[i].extensionName);
                    }
                }
            }
            else
            {
                ngxResult = NVSDK_NGX_D3D12_GetFeatureRequirements(adapter, &info, &result);
            }

            // Check if the main NGX API call failed due to unsupported feature
            if (ngxResult == NVSDK_NGX_Result_FAIL_FeatureNotSupported || ngxResult == NVSDK_NGX_Result_FAIL_FeatureNotFound)
            {
                // This will ensure to trigger "no adapter found" error if no other compatible adapter is found
                pluginInfo.minGPUArchitecture = UINT_MAX;
                // Maybe there is another NVDA adapter, move on
                SL_LOG_INFO("NGX_*_GetFeatureRequirements returned 0x%x for adapter: %d feature: %u", ngxResult, i, feature);
                continue;
            }

            CHECK_NGX_RETURN_ON_ERROR(ngxResult);

            // At this point NGX calls have succeeded!

            if (result.FeatureSupported == NVSDK_NGX_FeatureSupportResult_Supported)
            {
                pluginInfo.minGPUArchitecture = result.MinHWArchitecture; // Identical NVAPI enumeration
                SL_LOG_INFO("NGX feature %u requirements - minOS %s minHW 0x%x", feature, pluginInfo.minOS.toStr().c_str(), pluginInfo.minGPUArchitecture);
                return true;
            }

            if (result.FeatureSupported & NVSDK_NGX_FeatureSupportResult_NotImplemented)
            {
                // Just use SL defaults, NGX is out of date
                SL_LOG_INFO("NGX_*_GetFeatureRequirements feature: %u FeatureSupported == NotImplemented", feature);
                return false;
            }
            if (result.FeatureSupported & NVSDK_NGX_FeatureSupportResult_AdapterUnsupported)
            {
                // This will ensure to trigger "no adapter found" error if no other compatible adapter is found
                pluginInfo.minGPUArchitecture = UINT_MAX;
                // Maybe there is another NVDA adapter, move on
                SL_LOG_INFO("NGX_*_GetFeatureRequirements feature: %u FeatureSupported == AdapterUnsupported", feature);
                continue;
            }
            if (result.FeatureSupported & NVSDK_NGX_FeatureSupportResult_OSVersionBelowMinimumSupported)
            {
                // Increment detected OS version to trigger "OS out of date" error
                pluginInfo.minOS = Version(ctx.caps->osVersionMajor, ctx.caps->osVersionMinor, ctx.caps->osVersionBuild + 1);
                SL_LOG_INFO("NGX_*_GetFeatureRequirements feature: %u FeatureSupported == OSVersionBelowMinimumSupported", feature);
            }
            if (result.FeatureSupported & NVSDK_NGX_FeatureSupportResult_DriverVersionUnsupported)
            {
                // Unfortunately NGX does not tell us which driver it needs so we just increment 
                // detected version to trigger "driver out of date" error
                pluginInfo.minDriver = Version(ctx.caps->driverVersionMajor, ctx.caps->driverVersionMinor + 1, 0);
                SL_LOG_INFO("NGX_*_GetFeatureRequirements feature: %u FeatureSupported == DriverVersionUnsupported", feature);
            }
            
        }
    }

    // Did not find any NVDA adapters
    return false;
}

//! Special case when running d3d11 on d3d12, we have an additional context which is d3d12 exclusive
bool createNGXFeatureD3D12(void* cmdList, NVSDK_NGX_Feature feature, NVSDK_NGX_Handle** handle, const char* id)
{
    auto& ctx = (*common::getContext());
    extra::ScopedTasks vram([&ctx, id]()->void {ctx.computeD3D12->beginVRAMSegment(id); }, [&ctx]()->void {ctx.computeD3D12->endVRAMSegment(); });
    NVSDK_NGX_Result status = NVSDK_NGX_D3D12_CreateFeature((ID3D12GraphicsCommandList*)cmdList, feature, ctx.ngxContextD3D12.params, handle);
    if (status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("[%s] NGX create feature (D3D12) failed 0x%x", id, status); return false; }
    return true;
}

bool evaluateNGXFeatureD3D12(void* cmdList, NVSDK_NGX_Handle* handle, const char* id)
{
    auto& ctx = (*common::getContext());
    extra::ScopedTasks vram([&ctx, id]()->void {ctx.computeD3D12->beginVRAMSegment(id); }, [&ctx]()->void {ctx.computeD3D12->endVRAMSegment(); });
    NVSDK_NGX_Result status = NVSDK_NGX_D3D12_EvaluateFeature((ID3D12GraphicsCommandList*)cmdList, handle, ctx.ngxContextD3D12.params, nullptr);
    if (status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("[%s] NGX evaluate feature (D3D12) failed 0x%x", id, status); return false; }
    return true;
}

bool releaseNGXFeatureD3D12(NVSDK_NGX_Handle* handle, const char* id)
{
    auto& ctx = (*common::getContext());
    extra::ScopedTasks vram([&ctx, id]()->void {ctx.computeD3D12->beginVRAMSegment(id); }, [&ctx]()->void {ctx.computeD3D12->endVRAMSegment(); });
    NVSDK_NGX_Result status = NVSDK_NGX_D3D12_ReleaseFeature(handle);
    if (status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("[%s] NGX release feature (D3D12) failed 0x%x", id, status); return false; }
    return true;
}

//! Managing allocations coming from NGX
void allocateNGXBufferCallback(D3D11_BUFFER_DESC* desc, ID3D11Buffer** resource)
{
    auto& ctx = (*common::getContext());
    
    chi::Resource res{};
    chi::ResourceDescription resDesc{};
    resDesc.width = (uint32_t)desc->ByteWidth;
    resDesc.height = 1;
    resDesc.nativeFormat = DXGI_FORMAT_UNKNOWN;
    resDesc.format = chi::eFormatINVALID;
    if ((desc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0)
    {
        resDesc.heapType = chi::HeapType::eHeapTypeUpload;
    }
    else if ((desc->CPUAccessFlags & D3D11_CPU_ACCESS_READ) != 0)
    {
        resDesc.heapType = chi::HeapType::eHeapTypeReadback;
    }
    else
    {
        resDesc.heapType = chi::HeapType::eHeapTypeDefault;
    }

    const auto resourceName{ SL_COMPOSE_NAME("sl", "buf.resource") };
    if (sl::chi::ComputeStatus::eOk == ctx.compute->createBuffer(resDesc, res, resourceName.c_str()))
    {
        SL_LOG_VERBOSE("Allocated NGX buffer '%s' of width %d", resourceName.c_str(), resDesc.width);
        *resource = (ID3D11Buffer*)(res->native);
        delete res;
    }
    else
    {
        SL_LOG_ERROR("Creating an NGX buffer failed: width=%d", resDesc.width);
        // make sure to return nullptr to the caller.
        *resource = nullptr;
    }
}

void allocateNGXTex2dCallback(D3D11_TEXTURE2D_DESC* desc, ID3D11Texture2D** resource)
{
    auto& ctx = (*common::getContext());

    chi::Resource res{};
    chi::ResourceDescription resDesc{};
    resDesc.width = (uint32_t)desc->Width;
    resDesc.height = desc->Height;
    resDesc.nativeFormat = desc->Format;
    resDesc.mips = desc->MipLevels;
    resDesc.format = chi::eFormatINVALID;
    if ((desc->CPUAccessFlags & D3D11_CPU_ACCESS_WRITE) != 0)
    {
        resDesc.heapType = chi::HeapType::eHeapTypeUpload;
    }
    else if ((desc->CPUAccessFlags & D3D11_CPU_ACCESS_READ) != 0)
    {
        resDesc.heapType = chi::HeapType::eHeapTypeReadback;
    }
    else
    {
        resDesc.heapType = chi::HeapType::eHeapTypeDefault;
    }

    const auto resourceName{ SL_COMPOSE_NAME("sl", "tex2d.resource") };
    if (sl::chi::ComputeStatus::eOk == ctx.compute->createTexture2D(resDesc, res, resourceName.c_str()))
    {
        SL_LOG_VERBOSE("Allocated NGX resource '%s'", resourceName.c_str());
        *resource = (ID3D11Texture2D*)(res->native);
        delete res;
    }
    else
    {
        SL_LOG_ERROR("Creating an NGX 2d texture failed: width=%d, height=%d", resDesc.width, resDesc.height);
        // make sure to return nullptr to the caller.
        *resource = nullptr;
    }
}

void allocateNGXResourceCallback(D3D12_RESOURCE_DESC* desc, int state, CD3DX12_HEAP_PROPERTIES* heap, ID3D12Resource** resource)
{
    auto& ctx = (*common::getContext());
    chi::Resource res = {};
    chi::ResourceDescription resDesc = {};
    resDesc.width = (uint32_t)desc->Width;
    resDesc.height = desc->Height;
    resDesc.mips = desc->MipLevels;
    ctx.compute->getResourceState(state, resDesc.state);
    resDesc.nativeFormat = desc->Format;
    resDesc.format = chi::eFormatINVALID;
    resDesc.heapType = (chi::HeapType)heap->Type;


    auto compute = ctx.computeD3D12 ? ctx.computeD3D12 : ctx.compute;

    sl::chi::ComputeStatus status;
    std::string resourceName{};
    //! Redirecting to host app if allocate callback is specified in sl::Preferences
    if (desc->Dimension == D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        resourceName = SL_COMPOSE_NAME("sl", "buf.resource");
        status = compute->createBuffer(resDesc, res, resourceName.c_str());
    }
    else
    {
        resourceName = SL_COMPOSE_NAME("sl", "tex2d.resource");
        status = compute->createTexture2D(resDesc, res, resourceName.c_str());
    }
    if (status == sl::chi::ComputeStatus::eOk)
    {
        SL_LOG_VERBOSE("Allocated NGX resource '%s'", resourceName.c_str());
        *resource = (ID3D12Resource*)(res->native);
        delete res;
    }
    else
    {
        SL_LOG_ERROR("Creating an NGX resource failed: width=%d, height=%d", resDesc.width, resDesc.height);
        // make sure to return nullptr to the caller.
        *resource = nullptr;
    }
}

//! Managing deallocations coming from NGX
void releaseNGXResourceCallback(IUnknown* resource)
{
    auto& ctx = (*common::getContext());
    if (resource)
    {
        //! Redirecting to host app if deallocate callback is specified in sl::Preferences
        //! 
        //! Assuming texture but it can be a buffer, not critical since type is not used to decrement reference count
        auto res = new sl::Resource{ ResourceType::eTex2d, resource };

        ID3D12Resource* d3d12Resource{};
        resource->QueryInterface(&d3d12Resource);
        if (d3d12Resource)
        {
            d3d12Resource->Release();
            // If DX11 on 12 select correct compute to release for the accurate VRAM tracking
            auto compute = ctx.computeD3D12 ? ctx.computeD3D12 : ctx.compute;
            compute->destroyResource(res);
        }
        else
        {
            ID3D11Resource* d3d11Resource{};
            resource->QueryInterface(&d3d11Resource);
            if (d3d11Resource)
            {
                d3d11Resource->Release();
            }
            else
            {
                res->type = ResourceType::eUnknown;
            }
            ctx.compute->destroyResource(res);
        }
    }
}

std::string getNGXFeatureAsStr(NVSDK_NGX_Feature feature)
{
    switch (feature)
    {
        case NVSDK_NGX_Feature_SuperSampling:        return "SuperSampling";
        case NVSDK_NGX_Feature_InPainting:           return "InPainting";
        case NVSDK_NGX_Feature_ImageSuperResolution: return "ImageSuperResolution";
        case NVSDK_NGX_Feature_SlowMotion:           return "SlowMotion";
        case NVSDK_NGX_Feature_VideoSuperResolution: return "VideoSuperResolution";
        case NVSDK_NGX_Feature_ImageSignalProcessing:return "ImageSignalProcessing";
        case NVSDK_NGX_Feature_DeepResolve:          return "DeepResolve";
        case NVSDK_NGX_Feature_FrameGeneration:      return "FrameGeneration";
        case NVSDK_NGX_Feature_DeepDVC:              return "DeepDVC";
        case NVSDK_NGX_Feature_RayReconstruction:    return "RayReconstruction";
        default:                                     return "Unknown(" + std::to_string(static_cast<int>(feature)) + ")";
    }
}

void ngxLog(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent)
{
    std::string sFeatureName = getNGXFeatureAsStr(sourceComponent);
    switch (loggingLevel)
    {
        case NVSDK_NGX_LOGGING_LEVEL_ON:      SL_LOG_INFO("[%s] %s", sFeatureName.c_str(), message); break;
        case NVSDK_NGX_LOGGING_LEVEL_VERBOSE:  SL_LOG_VERBOSE("[%s] %s", sFeatureName.c_str(), message); break;
    }
};

} // namespace ngx

//! Common JSON configuration containing OS version, driver version, GPU architecture, supported adapters, plugin's SHA etc.
//! 
//! Each plugin calls this first then adds any additional information which is specific to it.
//! 
void updateCommonEmbeddedJSONConfig(void* jsonConfig, const common::PluginInfo& info)
{
    auto& ctx = (*common::getContext());

    // Bit hacky but better than including big json header in common header
    json& config = *(json*)jsonConfig;

    // SL does not work on Win7, only Win10+
    sl::Version minOS(10, 0, 0);
    // Also some reasonably old min driver
    sl::Version minDriver(512, 15, 0);

    if (info.minOS)
    {
        minOS = info.minOS;
    }

    if (info.minDriver)
    {
        minDriver = info.minDriver;
    }

    PreferenceFlags preferenceFlags{};
    api::getContext()->parameters->get(sl::param::global::kPreferenceFlags, (uint64_t*)&preferenceFlags);

    // Provided to host
    Version detectedOS(ctx.caps->osVersionMajor, ctx.caps->osVersionMinor, ctx.caps->osVersionBuild);
    auto osSupported = minOS <= detectedOS;
    // Check if host wants us to bypass the OS check
    if (!osSupported && ((preferenceFlags & PreferenceFlags::eBypassOSVersionCheck) != 0))
    {
        osSupported = true;
        SL_LOG_WARN("Bypassing OS version check - detected OS v%s - required OS v%s", detectedOS.toStr().c_str(), minOS.toStr().c_str());
    }

    config["external"]["os"]["detected"] = detectedOS.toStr();
    config["external"]["os"]["required"] = minOS.toStr();
    config["external"]["os"]["supported"] = osSupported;

    // Detected driver version must be valid in order for us to test, but only if min driver is provided by the feature, if not any driver would work
    sl::Version detectedDriver(ctx.caps->driverVersionMajor, ctx.caps->driverVersionMinor, 0);
    auto driverSupported = minDriver <= detectedDriver || (!info.minDriver && !detectedDriver);
    config["external"]["driver"]["detected"] = detectedDriver.toStr();
    config["external"]["driver"]["required"] = minDriver.toStr();
    config["external"]["driver"]["supported"] = driverSupported;

    // Not supported on any adapter by default
    uint32_t adapterMask = 0;

    for (uint32_t i = 0; i < ctx.caps->gpuCount; i++)
    {
        std::string adapter = "gpu" + std::to_string(i);
        config["external"]["adapters"][adapter]["detected"] = ctx.caps->adapters[i].architecture;
        config["external"]["adapters"][adapter]["required"] = info.minGPUArchitecture;
        config["external"]["adapters"][adapter]["supported"] = info.minGPUArchitecture <= ctx.caps->adapters[i].architecture;

        // Check OS always, driver only on NVDA architecture and for generic plugins min architecture will be 0 which will always pass
        if (osSupported && (driverSupported || ctx.caps->adapters[i].architecture == 0) && ctx.caps->adapters[i].architecture >= info.minGPUArchitecture)
        {
            adapterMask |= 1 << i;
        }
    }

    bool supported = adapterMask != 0;

    // Internal bits
    config["supportedAdapters"] = adapterMask;
    config["sha"] = info.SHA;

    std::vector<BufferType> tags;
    if (supported)
    {
        //! Report to host and log required tags if feature is supported
        //! 
        Feature feature = config["id"];
        for (auto& t : info.requiredTags)
        {
            tags.push_back(t.first);
            SL_LOG_VERBOSE("Registering required tag '%s' life-cycle '%s' for feature `%s`", getBufferTypeAsStr(t.first), getResourceLifecycleAsStr(t.second), getFeatureAsStr(feature));
        }
    }

    config["external"]["feature"]["tags"] = tags;
    
    // Only if feature is supported on at least one adapter
    if (supported)
    {
        ctx.needNGX |= info.needsNGX;
        ctx.needDX11On12 |= info.needsDX11On12;
        ctx.needDRS |= info.needsDRS;
    }
}

Result pclSetData(const BaseStructure* inputs, CommandBuffer* cmdBuffer)
{
    auto& ctx = (*common::getContext());
    return sl::pcl::implSetData(inputs, ctx.pclOptions);
}

Result pclGetData(const BaseStructure*, BaseStructure* outputs, CommandBuffer*)
{
    return sl::pcl::implGetData(outputs);
}


//! Main entry point - starting our plugin
//! 
bool slOnPluginStartup(const char* jsonConfig, void* device)
{
    SL_PLUGIN_COMMON_STARTUP();

    auto& ctx = (*common::getContext());

    auto parameters = api::getContext()->parameters;

    //! We handle all common functionality - common constants, tagging, evaluate and provide various helpers
    parameters->set(param::global::kPFunGetConsts, getCommonConstants);
    parameters->set(param::global::kPFunGetTag, getCommonTag);
    parameters->set(param::common::kPFunRegisterEvaluateCallbacks, common::registerEvaluateCallbacks);
    parameters->set(param::common::kPFunRegisterFeatureViewport, registerCommonFeatureViewport);
    parameters->set(param::common::kPFunUnregisterFeatureViewport, unregisterCommonFeatureViewport);

    //! Plugin manager gives us the device type and the application id
    json& config = *(json*)api::getContext()->loaderConfig;

    auto deviceType = RenderAPI::eD3D12;
    int appId = 0;
    config.at("appId").get_to(appId);
    config.at("deviceType").get_to(deviceType);
    EngineType engine{};
    std::string engineVersion{};
    std::string projectId{};
    if (config.contains("ngx"))
    {
        config.at("ngx").at("engineType").get_to(engine);
        config.at("ngx").at("engineVersion").get_to(engineVersion);
        config.at("ngx").at("projectId").get_to(projectId);
    }

    //! Some optional tweaks, NGX logging included in SL logging 
    LogLevel logLevelNGX = log::getInterface()->getLogLevel();
    //! Extra config is always `sl.plugin_name.json` so in our case `sl.common.json`
    json& extraConfig = *(json*)api::getContext()->extConfig;
    if (extraConfig.contains("logLevelNGX"))
    {
        extraConfig.at("logLevelNGX").get_to(logLevelNGX);
        SL_LOG_HINT("Overriding NGX logging level to %u'", logLevelNGX);
    }
    //! Optional hot-key bindings
    if (extraConfig.contains("keys"))
    {
        auto keys = extraConfig.at("keys");
        for (auto& key : keys)
        {
            extra::keyboard::VirtKey vk;
            std::string id;
            key.at("alt").get_to(vk.m_bAlt);
            key.at("ctrl").get_to(vk.m_bControl);
            key.at("shift").get_to(vk.m_bShift);
            key.at("key").get_to(vk.m_mainKey);
            key.at("id").get_to(id);
            extra::keyboard::getInterface()->registerKey(id.c_str(), vk);
            SL_LOG_HINT("Overriding key combo for '%s'", id.c_str());
        }
    }

    // Now let's create our compute interface
    ctx.platform = (RenderAPI)deviceType;
    auto [compute, computeD3D12] = common::createCompute(device, ctx.platform, ctx.needDX11On12);
    if (!compute)
    {
        return false;
    }

    // Note that normally we get one compute interface except when running d3d11 and at least one of the plugins requires "dx11 on 12"
    ctx.compute = compute;
    ctx.computeD3D12 = computeD3D12;

    CHI_CHECK_RF(ctx.compute->createResourcePool(&ctx.pool, api::getPluginName()));

    // Allow lower level common interface to read config items etc.
    if (!common::onLoad(&config, &extraConfig, ctx.pool))
    {
        return false;
    }

    if (ctx.needNGX)
    {
        // NGX initialization
        SL_LOG_INFO("At least one plugin requires NGX, trying to initialize ...");

        // Reset our flag until we see if NGX can be initialized correctly
        ctx.needNGX = false;

        // We also need to provide path for logging
        PWSTR documentsDataPath = (PWSTR)file::getTmpPath();
        if (!documentsDataPath)
        {
            SL_LOG_ERROR( "Failed to obtain path to documents");
        }

        // We need to provide path to the NGX modules
        wchar_t* slPluginPathUtf16 = {};
        param::getPointerParam(parameters, param::global::kPluginPath, &slPluginPathUtf16);
        // Always check first where our plugins are then the other paths
        std::vector<std::wstring> ngxPathsTmp = {slPluginPathUtf16 };
        std::vector<wchar_t*> ngxPaths = { slPluginPathUtf16 };
        auto& paths = config.at("paths");
        for (auto& p : paths)
        {
            std::string s;
            p.get_to(s);
            auto ws = extra::utf8ToUtf16(s.c_str());
            if (std::find(ngxPathsTmp.begin(), ngxPathsTmp.end(), ws) == ngxPathsTmp.end())
            {
                ngxPathsTmp.push_back(ws);
                ngxPaths.push_back((wchar_t*)ngxPathsTmp.back().c_str());
            }
        }

        NVSDK_NGX_FeatureCommonInfo info = {};
        info.PathListInfo.Length = (uint32_t)ngxPaths.size();
        info.PathListInfo.Path = ngxPaths.data();
        {
            // We can control NXG logging as well
            info.LoggingInfo.LoggingCallback = ngx::ngxLog;
            info.LoggingInfo.DisableOtherLoggingSinks = true;
            switch (logLevelNGX)
            {
                case LogLevel::eOff:
                    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_OFF;
                    break;
                case LogLevel::eDefault:
                    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
                    break;
                case LogLevel::eVerbose:
                    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE;
                    break;
            }
        }

        NVSDK_NGX_Result ngxStatus{};
        
        bool hasProjectId = !engineVersion.empty() && !projectId.empty();
        if (hasProjectId)
        {
            // Engine data provided, no need for the application id
            if (deviceType == RenderAPI::eD3D11)
            {
                ngxStatus = NVSDK_NGX_D3D11_Init_with_ProjectID(projectId.c_str(), (NVSDK_NGX_EngineType)engine, engineVersion.c_str(), documentsDataPath, (ID3D11Device*)device, &info, NVSDK_NGX_Version_API);
                ngxStatus = NVSDK_NGX_D3D11_GetCapabilityParameters(&ctx.ngxContext.params);

                if (ctx.computeD3D12)
                {
                    chi::Device deviceD3D12;
                    CHI_CHECK_RF(ctx.computeD3D12->getDevice(deviceD3D12));
                    ngxStatus = NVSDK_NGX_D3D12_Init_with_ProjectID(projectId.c_str(), (NVSDK_NGX_EngineType)engine, engineVersion.c_str(), documentsDataPath, (ID3D12Device*)deviceD3D12, &info, NVSDK_NGX_Version_API);
                    ngxStatus = NVSDK_NGX_D3D12_GetCapabilityParameters(&ctx.ngxContextD3D12.params);
                }
            }
            else if (deviceType == RenderAPI::eD3D12)
            {
                ngxStatus = NVSDK_NGX_D3D12_Init_with_ProjectID(projectId.c_str(), (NVSDK_NGX_EngineType)engine, engineVersion.c_str(), documentsDataPath, (ID3D12Device*)device, &info, NVSDK_NGX_Version_API);
                ngxStatus = NVSDK_NGX_D3D12_GetCapabilityParameters(&ctx.ngxContext.params);
            }
            else
            {
                VkDevices* slVkDevices = (VkDevices*)device;

                ngxStatus = NVSDK_NGX_VULKAN_Init_with_ProjectID(projectId.c_str(), (NVSDK_NGX_EngineType)engine, engineVersion.c_str(), documentsDataPath, slVkDevices->instance, slVkDevices->physical, slVkDevices->device, nullptr/* TODO TBD plumb in vkGetInstanceProcAddr*/, nullptr/* TODO TBD plumb in vkGetDeviceProcAddr*/, &info, NVSDK_NGX_Version_API);
                ngxStatus = NVSDK_NGX_VULKAN_GetCapabilityParameters(&ctx.ngxContext.params);
            }
        }
        else
        {
            // Engine data NOT provided, application id must be valid
#if defined SL_PRODUCTION
            // Allowing in SL_REL_EXT_DEV to show warning on screen
            if (appId == kTemporaryAppId)
            {
                SL_LOG_ERROR("Please provide correct application id when calling slInit - NGX based features will be disabled");
                // Skip the code below
                ctx.needNGX = false;
                ngxStatus = NVSDK_NGX_Result_FAIL_InvalidParameter;
            }
            else
#endif
            if (deviceType == RenderAPI::eD3D11)
            {
                ngxStatus = NVSDK_NGX_D3D11_Init(appId, documentsDataPath, (ID3D11Device*)device, &info, NVSDK_NGX_Version_API);
                ngxStatus = NVSDK_NGX_D3D11_GetCapabilityParameters(&ctx.ngxContext.params);

                if (ctx.computeD3D12)
                {
                    chi::Device deviceD3D12;
                    CHI_CHECK_RF(ctx.computeD3D12->getDevice(deviceD3D12));
                    ngxStatus = NVSDK_NGX_D3D12_Init(appId, documentsDataPath, (ID3D12Device*)deviceD3D12, &info, NVSDK_NGX_Version_API);
                    ngxStatus = NVSDK_NGX_D3D12_GetCapabilityParameters(&ctx.ngxContextD3D12.params);
                }
            }
            else if (deviceType == RenderAPI::eD3D12)
            {
                ngxStatus = NVSDK_NGX_D3D12_Init(appId, documentsDataPath, (ID3D12Device*)device, &info, NVSDK_NGX_Version_API);
                ngxStatus = NVSDK_NGX_D3D12_GetCapabilityParameters(&ctx.ngxContext.params);
            }
            else
            {
                VkDevices* slVkDevices = (VkDevices*)device;

                ngxStatus = NVSDK_NGX_VULKAN_Init(appId, documentsDataPath, slVkDevices->instance, slVkDevices->physical, slVkDevices->device, nullptr/* TODO TBD plumb in vkGetInstanceProcAddr*/, nullptr/* TODO TBD plumb in vkGetDeviceProcAddr*/, &info, NVSDK_NGX_Version_API);
                ngxStatus = NVSDK_NGX_VULKAN_GetCapabilityParameters(&ctx.ngxContext.params);
            }
        }

        if (ngxStatus == NVSDK_NGX_Result_Success)
        {
            SL_LOG_HINT("NGX loaded - app id %u - application data path %S", appId, documentsDataPath);

            if (!hasProjectId && appId == kTemporaryAppId)
            {
                SL_LOG_WARN("Production builds require a valid application ID or project ID. Please provide one of these to ensure proper operation in production.");
            }

            ctx.needNGX = true;

            // Register callbacks so we can manage memory for NGX
            {
                if (deviceType == RenderAPI::eD3D12)
                {
                    ctx.ngxContext.params->Set(NVSDK_NGX_Parameter_ResourceAllocCallback, ngx::allocateNGXResourceCallback);
                    ctx.ngxContext.params->Set(NVSDK_NGX_Parameter_ResourceReleaseCallback, ngx::releaseNGXResourceCallback);
                }
                else if (deviceType == RenderAPI::eD3D11)
                {
                    ctx.ngxContext.params->Set(NVSDK_NGX_Parameter_BufferAllocCallback, ngx::allocateNGXBufferCallback);
                    ctx.ngxContext.params->Set(NVSDK_NGX_Parameter_Tex2DAllocCallback, ngx::allocateNGXTex2dCallback);
                    ctx.ngxContext.params->Set(NVSDK_NGX_Parameter_ResourceReleaseCallback, ngx::releaseNGXResourceCallback);
                }
                else
                {
                    // TODO: NGX does not provide VK memory hooking
                }

                // Special case for d3d11 on 12
                if (ctx.computeD3D12)
                {
                    ctx.ngxContextD3D12.params->Set(NVSDK_NGX_Parameter_ResourceAllocCallback, ngx::allocateNGXResourceCallback);
                    ctx.ngxContextD3D12.params->Set(NVSDK_NGX_Parameter_ResourceReleaseCallback, ngx::releaseNGXResourceCallback);
                }
            }

            // Provide NGX context to other plugins
            ctx.ngxContext.createFeature = ngx::createNGXFeature;
            ctx.ngxContext.releaseFeature = ngx::releaseNGXFeature;
            ctx.ngxContext.evaluateFeature = ngx::evaluateNGXFeature;
            ctx.ngxContext.updateFeature = ngx::updateNGXFeature;
            parameters->set(param::global::kNGXContext, &ctx.ngxContext);

            // Special context for plugins running d3d11 on d3d12
            if (ctx.computeD3D12)
            {
                ctx.ngxContextD3D12.createFeature = ngx::createNGXFeatureD3D12;
                ctx.ngxContextD3D12.releaseFeature = ngx::releaseNGXFeatureD3D12;
                ctx.ngxContextD3D12.evaluateFeature = ngx::evaluateNGXFeatureD3D12;
                ctx.ngxContextD3D12.updateFeature = ngx::updateNGXFeature;
                parameters->set(param::global::kNGXContextD3D12, &ctx.ngxContextD3D12);
            }
        }
        else
        {
            SL_LOG_WARN("Failed to initialize NGX, any SL feature requiring NGX will be unloaded and disabled");
        }
    }
    if (ctx.needDRS)
    {
        // DRS initialization
        SL_LOG_INFO("At least one plugin requires DRS, trying to initialize ...");

        // Reset our flag until we see if DRS can be initialized correctly
        ctx.needDRS = false;
        bool drsStatus = drs::drsInit();
        if (drsStatus)
        {
            ctx.needDRS = true;
            SL_LOG_HINT("DRS loaded - app id %u", appId);

            // Provide DRS context to other plugins
            ctx.drsContext.drsReadKey = drs::drsReadKey;
            ctx.drsContext.drsReadKeyFromProfile = drs::drsReadKeyFromProfile;
            ctx.drsContext.drsReadKeyString = drs::drsReadKeyString;
            ctx.drsContext.drsReadKeyStringFromProfile = drs::drsReadKeyStringFromProfile;
            ctx.drsContext.drsReadKeyFromProfileNoGlobal = drs::drsReadKeyFromProfileNoGlobal;
            ctx.drsContext.drsReadKeyStringFromProfileNoGlobal = drs::drsReadKeyStringFromProfileNoGlobal;
            parameters->set(param::global::kDRSContext, &ctx.drsContext);
        }
        else
        {
            SL_LOG_WARN("Failed to initialize DRS");
        }
    }

    if (Version(config["version"]["major"],
                config["version"]["minor"],
                config["version"]["build"]) < Version(2, 3, 0))
    {
        ctx.enablePCLPluginInCommonWAR = true;
        SL_LOG_INFO("Enabling WAR for PCLPluginInCommon");
    }
    if (ctx.enablePCLPluginInCommonWAR)
    {
        sl::pcl::implOnPluginStartup(parameters, &pclGetData, &pclSetData);
    }

    PreferenceFlags preferenceFlags{};
    api::getContext()->parameters->get(sl::param::global::kPreferenceFlags, (uint64_t*)&preferenceFlags);

    ctx.useResourceTaggingForFrame = ((preferenceFlags & PreferenceFlags::eUseFrameBasedResourceTagging) != 0);
    if (ctx.useResourceTaggingForFrame)
    {
        ctx.pBaseResourceTagging = std::make_unique<common::ResourceTaggingForFrame>(ctx.compute, ctx.pool);
    }
    else
    {
        ctx.pBaseResourceTagging = std::make_unique<common::ResourceTaggingGeneral>();
    }

    return true;
}

//! Main exit point - shutting down our plugin
//! 
void slOnPluginShutdown()
{
    auto parameters = api::getContext()->parameters;
    auto& ctx = (*common::getContext());

    // Remove all provided common interfaces
    parameters->set(param::global::kPFunGetConsts, nullptr);
    parameters->set(param::global::kPFunGetTag, nullptr);
    parameters->set(param::common::kPFunRegisterEvaluateCallbacks, nullptr);
    parameters->set(param::common::kPFunRegisterFeatureViewport, nullptr);
    parameters->set(param::common::kPFunUnregisterFeatureViewport, nullptr);
    parameters->set(param::common::kPFunGetStringFromModule, nullptr);
    parameters->set(param::common::kPFunUpdateCommonEmbeddedJSONConfig, nullptr);
    parameters->set(param::common::kPFunNGXGetFeatureRequirements, nullptr);
    parameters->set(param::common::kSystemCaps, nullptr);
    parameters->set(param::common::kPFunFindAdapter, nullptr);
    parameters->set(param::common::kKeyboardAPI, nullptr);
    parameters->set(param::common::kComputeAPI, nullptr);

    {
        std::lock_guard<std::mutex> lock(ctx.featureViewportIdsMutex);
        ctx.featureViewportIds.clear();
    }

    if (ctx.pBaseResourceTagging != nullptr)
    {
        ctx.pBaseResourceTagging.reset(nullptr);
    }


    if (ctx.enablePCLPluginInCommonWAR)
    {
        sl::pcl::implOnPluginShutdown(parameters);
    }

    ctx.compute->destroyResourcePool(ctx.pool);
    ctx.pool = {};

    for (uint32_t i = 0; i < common::kMaxNumSupportedGPUs; i++)
    {
        auto adapter = (IDXGIAdapter*)ctx.caps->adapters[i].nativeInterface;
        if(adapter) adapter->Release();
    }


    if (ctx.needNGX)
    {
        SL_LOG_INFO("Shutting down NGX");
        if (ctx.platform == RenderAPI::eD3D11)
        {
            NVSDK_NGX_D3D11_Shutdown1(nullptr);
            if (ctx.computeD3D12)
            {
                NVSDK_NGX_D3D12_Shutdown1(nullptr);
            }
        }
        else if (ctx.platform == RenderAPI::eD3D12)
        {
            NVSDK_NGX_D3D12_Shutdown1(nullptr);
        }
        else
        {
            NVSDK_NGX_VULKAN_Shutdown1(nullptr);
        }
        ctx.needNGX = false;
    }

    if (ctx.needDRS)
    {
        drs::drsShutdown();
        ctx.needDRS = false;
    }

    // Common shutdown
    plugin::onShutdown(api::getContext());

    common::destroyCompute();
}

using PFunRtlGetVersion = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);
using PFunNtSetTimerResolution = NTSTATUS(NTAPI*)(ULONG DesiredResolution, BOOLEAN SetResolution, PULONG CurrentResolution);
using PFunNtQueryTimerResolution = NTSTATUS(NTAPI*)(PULONG MinimumResolution, PULONG MaximumResolution, PULONG CurrentResolution);

bool getOSVersionAndUpdateTimerResolution(common::SystemCaps* caps)
{
    // In Win8, the GetVersion[Ex][AW]() functions were all deprecated in favour of using
    // other more dumbed down functions such as IsWin10OrGreater(), isWinVersionOrGreater(),
    // VerifyVersionInfo(), etc.  Unfortunately these have a couple huge issues:
    //   * they cannot retrieve the actual version information, just guess at it based on
    //     boolean return values.
    //   * in order to report anything above Win8.0, the app calling the function must be
    //     manifested as a Win10 app.
    //
    // Since we can't guarantee that all host apps will be properly manifested, we can't
    // rely on any of the new version verification functions or on GetVersionEx().
    //
    // However, all of the manifest checking is done at the kernel32 level.  If we go
    // directly to the ntdll level, we do actually get accurate information about the
    // current OS version.
    
    Version vKernel{}, vNT{};

    caps->osVersionMajor = 0;
    caps->osVersionMinor = 0;
    caps->osVersionBuild = 0;

    // Fist we try kernel32.dll
    TCHAR filename[MAX_PATH]{};
    if (GetModuleFileName(GetModuleHandle("kernel32.dll"), filename, MAX_PATH))
    {
        DWORD verHandle{};
        DWORD verSize = GetFileVersionInfoSize(filename, &verHandle);
        if (verSize != 0)
        {
            LPSTR verData = new char[verSize];
            if (GetFileVersionInfo(filename, verHandle, verSize, verData))
            {
                LPBYTE lpBuffer{};
                UINT size{};
                if (VerQueryValue(verData, "\\", (VOID FAR * FAR*) & lpBuffer, &size))
                {
                    if (size)
                    {
                        VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)lpBuffer;
                        if (verInfo->dwSignature == 0xfeef04bd)
                        {
                            vKernel.major = (verInfo->dwProductVersionMS >> 16) & 0xffff;
                            vKernel.minor = (verInfo->dwProductVersionMS >> 0) & 0xffff;
                            vKernel.build = (verInfo->dwProductVersionLS >> 16) & 0xffff;
                        }
                    }
                }
                else
                {
                    SL_LOG_ERROR("VerQueryValue failed - last error %s", std::system_category().message(GetLastError()).c_str());
                }
            }
            else
            {
                SL_LOG_ERROR("GetFileVersionInfo failed - last error %s", std::system_category().message(GetLastError()).c_str());
            }
            delete[] verData;
        }
        else
        {
            SL_LOG_ERROR("GetFileVersionInfoSize failed - last error %s", std::system_category().message(GetLastError()).c_str());
        }
    }
    else
    {
        SL_LOG_ERROR("GetModuleFileName failed - last error %s", std::system_category().message(GetLastError()).c_str());
    }

    bool res = false;
    RTL_OSVERSIONINFOW osVer{};
    auto handle = GetModuleHandleW(L"ntdll");
    auto rtlGetVersion = reinterpret_cast<PFunRtlGetVersion>(GetProcAddress(handle, "RtlGetVersion"));
    if (rtlGetVersion)
    {
        osVer.dwOSVersionInfoSize = sizeof(osVer);
        if (res = !rtlGetVersion(&osVer))
        {
            vNT.major = osVer.dwMajorVersion;
            vNT.minor = osVer.dwMinorVersion;
            vNT.build = osVer.dwBuildNumber;
        }
        else
        {
            SL_LOG_ERROR("RtlGetVersion failed %s", std::system_category().message(GetLastError()).c_str());
        }
    }
    else if(!vKernel)
    {
        // Return false only if kernel version also failed
        SL_LOG_ERROR("Failed to retrieve the RtlGetVersion() function from ntdll.");
        return false;
    }

    // Pick a higher version, rtlGetVersion reports version selected on the exe compatibility mode not the actual OS version
    if (vKernel > vNT)
    {
        SL_LOG_INFO("Application running in compatibility mode - version %s", vNT.toStr().c_str());
        caps->osVersionMajor = vKernel.major;
        caps->osVersionMinor = vKernel.minor;
        caps->osVersionBuild = vKernel.build;
    }
    else
    {
        caps->osVersionMajor = vNT.major;
        caps->osVersionMinor = vNT.minor;
        caps->osVersionBuild = vNT.build;
    }

    auto NtQueryTimerResolution = reinterpret_cast<PFunNtQueryTimerResolution>(GetProcAddress(handle, "NtQueryTimerResolution"));
    auto NtSetTimerResolution = reinterpret_cast<PFunNtSetTimerResolution>(GetProcAddress(handle, "NtSetTimerResolution"));
    if (NtSetTimerResolution && NtQueryTimerResolution)
    {
        // SL wants a minimum timer resolution of 0.5ms. The application may have already set a finer-grained value, this is fine, and SL should not modify the timer resolution.
        const ULONG slPreferredTimerRes = 5000UL;

        ULONG minRes = ULONG_MAX;
        ULONG maxRes = slPreferredTimerRes;
        ULONG setRes = slPreferredTimerRes;
        ULONG currentRes = ULONG_MAX;

        if (!NT_SUCCESS(NtQueryTimerResolution(&minRes, &maxRes, &currentRes)))
        {
            SL_LOG_WARN("Failed to query high resolution timer capabilities, assuming it supports at least %u [100 ns units].", slPreferredTimerRes);
        }

        if (currentRes > slPreferredTimerRes)
        {
            if (maxRes > slPreferredTimerRes)
            {
                SL_LOG_INFO("Preferred high resolution timer resolution (%u) is unsupported, trying %u [100 ns units] instead.", slPreferredTimerRes, maxRes);
                setRes = maxRes;
            }

            if (NT_SUCCESS(NtSetTimerResolution(setRes, TRUE, &currentRes)))
            {
                SL_LOG_INFO("Changed high resolution timer resolution to %u [100 ns units].", currentRes);
            }
            else
            {
                SL_LOG_WARN("Failed to change high resolution timer resolution to %u [100 ns units].", setRes);
            }
        }
    }
    else
    {
        SL_LOG_WARN("Failed to retrieve the NtQueryTimerResolution() and NtSetTimerResolution() functions from ntdll.");
    }
    return res;
}

sl::Result findAdapter(const sl::AdapterInfo& adapterInfo, uint32_t adapterMask)
{
    auto& ctx = (*common::getContext());

    LUID id{};
    uint32_t deviceId{};
    if (adapterInfo.vkPhysicalDevice)
    {
        chi::Vulkan::getLUIDFromDevice(adapterInfo.vkPhysicalDevice, deviceId, &id);
    }
    else if (!adapterInfo.deviceLUID || sizeof(LUID) > adapterInfo.deviceLUIDSizeInBytes)
    {
        return sl::Result::eErrorInvalidParameter;
    }
    else
    {
        memcpy_s(&id, sizeof(LUID), adapterInfo.deviceLUID, sizeof(LUID));
    }
    for (uint32_t i = 0; i < common::kMaxNumSupportedGPUs; i++)
    {
        if (ctx.caps->adapters[i].deviceId == deviceId || (ctx.caps->adapters[i].id.HighPart == id.HighPart && ctx.caps->adapters[i].id.LowPart == id.LowPart))
        {
            if (ctx.caps->adapters[i].bit & adapterMask) return sl::Result::eOk;
        }
    }
    return sl::Result::eErrorAdapterNotSupported;
}

//! Figure out if we are supported on the current hardware or not
//! 
void updateEmbeddedJSON(json& config)
{
    // Provide shared interfaces so other plugins can use them
    api::getContext()->parameters->set(param::common::kKeyboardAPI, extra::keyboard::getInterface());
    api::getContext()->parameters->set(param::common::kPFunUpdateCommonEmbeddedJSONConfig, updateCommonEmbeddedJSONConfig);
    api::getContext()->parameters->set(param::common::kPFunGetStringFromModule, getStringFromModule);
    api::getContext()->parameters->set(param::common::kPFunNGXGetFeatureRequirements, ngx::getNGXFeatureRequirements);
    auto& ctx = (*common::getContext());

    // Now we need to check OS and GPU capabilities
    // Note that this always succeeds but HW info in caps might not be complete (NVAPI not running on non-NVDA hardware etc.) 
    getSystemCaps(ctx.caps);
    // Let's get the OS info and update our timer resolution (both use ntdll.dll so combined for convenience)
    getOSVersionAndUpdateTimerResolution(ctx.caps);

    // Allow other plugins to query system caps (this is static pointer in our context)
    api::getContext()->parameters->set(param::common::kSystemCaps, (void*)ctx.caps);
    api::getContext()->parameters->set(param::common::kPFunFindAdapter, findAdapter);

    // Min HW architecture 0 since we can run on any adapter
    common::PluginInfo info{};
    info.SHA = GIT_LAST_COMMIT_SHORT;
    updateCommonEmbeddedJSONConfig(&config, info);

    if (ctx.caps->osVersionMajor < 10)
    {
        SL_LOG_WARN("Detected Windows OS version %u.%u.%u - Win10 or higher is required to use SL - all features will be disabled", ctx.caps->osVersionMajor, ctx.caps->osVersionMinor, ctx.caps->osVersionBuild);
    }
    else
    {
        SL_LOG_INFO("Detected Windows OS version %u.%u.%u", ctx.caps->osVersionMajor, ctx.caps->osVersionMinor, ctx.caps->osVersionBuild);
    }
}

//! The only exported function - gateway to all functionality
SL_EXPORT void* slGetPluginFunction(const char* functionName)
{
    //! Core API
    SL_EXPORT_FUNCTION(slOnPluginLoad);
    SL_EXPORT_FUNCTION(slOnPluginShutdown);
    SL_EXPORT_FUNCTION(slOnPluginStartup);
    SL_EXPORT_FUNCTION(slSetTag);
    SL_EXPORT_FUNCTION(slSetTagForFrame);
    SL_EXPORT_FUNCTION(slSetConstants);
    SL_EXPORT_FUNCTION(slEvaluateFeature)

    //! Hooks defined in the JSON config above

    //! D3D12
    SL_EXPORT_FUNCTION(slHookCreateSwapChain);
    SL_EXPORT_FUNCTION(slHookCreateSwapChainForHwnd);
    SL_EXPORT_FUNCTION(slHookCreateSwapChainForCoreWindow);
    SL_EXPORT_FUNCTION(slHookPresent);
    SL_EXPORT_FUNCTION(slHookPresent1);
    SL_EXPORT_FUNCTION(slHookAfterPresent);
    SL_EXPORT_FUNCTION(slHookResizeSwapChainPre);
    
    //! Vulkan
    SL_EXPORT_FUNCTION(slHookVkPresent);
    SL_EXPORT_FUNCTION(slHookVkAfterPresent);
    SL_EXPORT_FUNCTION(slHookVkCmdBindPipeline);
    SL_EXPORT_FUNCTION(slHookVkCmdBindDescriptorSets);
    SL_EXPORT_FUNCTION(slHookVkBeginCommandBuffer);

    return nullptr;
}

}

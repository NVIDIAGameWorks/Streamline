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

#include "include/sl.h"
#include "include/sl_consts.h"
#include "include/sl_helpers.h"
#include "source/core/sl.log/log.h"
#include "source/plugins/sl.common/resourceTaggingForFrame.h"

namespace sl
{
namespace common
{

// RAII wrapper for read-only access to ProtectedResourceTagContainer
class ScopedResourceTagContainerReadAccess
{
public:
    ScopedResourceTagContainerReadAccess(const ProtectedResourceTagContainer& container)
        : m_container(container)
        , m_lock(container.resourceTagContainerMutex)
    {}

    const ProtectedResourceTagContainer& m_container;

private:
    std::shared_lock<std::shared_timed_mutex> m_lock;
};

// RAII wrapper for write access to ProtectedResourceTagContainer
class ScopedResourceTagContainerWriteAccess
{
public:
    ScopedResourceTagContainerWriteAccess(ProtectedResourceTagContainer& container)
        : m_container(container)
        , m_lock(container.resourceTagContainerMutex)
    {}

    ProtectedResourceTagContainer& m_container;

private:
    std::unique_lock<std::shared_timed_mutex> m_lock;
};

std::unique_ptr<ScopedResourceTagContainerReadAccess>
ResourceTaggingForFrame::findFrameForReading(uint32_t frameIndex)
{
    const auto& frame = m_frames[frameIndex % m_frames.size()];
    // this grabs the mutex for the frame
    auto pFrame = std::make_unique<ScopedResourceTagContainerReadAccess>(frame);

    // do those tags have the correct frame index?
    if (frame.getFrameIndex() == frameIndex)
        return pFrame;
    return nullptr;
}

std::unique_ptr<ScopedResourceTagContainerWriteAccess>
ResourceTaggingForFrame::findFrameForWriting(uint32_t frameIndex)
{
    auto& frame = m_frames[frameIndex % m_frames.size()];

    // this grabs the mutex for the frame
    auto pFrame = std::make_unique<ScopedResourceTagContainerWriteAccess>(frame);

    return pFrame;
}

ResourceTaggingForFrame::ResourceTaggingForFrame(chi::ICompute* pCompute, chi::IResourcePool* pPool)
    : m_pCompute(pCompute), m_pPool(pPool)
{
    assert(pCompute && pPool);
    m_pCompute->getRenderAPI(m_platform);
}

// frame-aware resource tagging internal functionality for tagging API slSetTagForFrame
sl::Result common::ResourceTaggingForFrame::setTag(const sl::Resource* resource,
                                                   BufferType tag,
                                                   uint32_t id,
                                                   const Extent* ext,
                                                   ResourceLifecycle lifecycle,
                                                   CommandBuffer* cmdBuffer,
                                                   bool localTag,
                                                   const PrecisionInfo* pi,
                                                   const sl::FrameToken& frame)
{
    // here we're recycling the old tags
    recycleTags();

    uint64_t uid = ((uint64_t)tag << 32) | (uint64_t)id;
    uint32_t currFrameId = frame;

    auto frameAccess = findFrameForWriting(currFrameId);
    // if frame index doesn't match - this means it's an old frame containing old tags
    if (frameAccess->m_container.getFrameIndex() != currFrameId)
    {
        this->recycleTagsForFrame(*frameAccess);
        frameAccess->m_container.setFrameIndex(currFrameId);
    }
    auto& frameTags = frameAccess->m_container.resourceTagContainer;
    auto& frameTag = frameTags[uid];

    // release the old tag
    m_pPool->recycle(frameTag.clone);
    m_pCompute->stopTrackingResource(currFrameId, uid, &frameTag.res);

    // make the tag empty
    frameTag = CommonResource();

    // bake the new tag
    if (resource && resource->native)
    {
        frameTag.res = *(sl::Resource*)resource;
        if (m_platform == RenderAPI::eD3D11)
        {
            // Force common state for d3d11 in case engine is providing something that won't work on compute queue
            frameTag.res.state = 0;
        }
#if defined(SL_PRODUCTION) || defined(SL_DEVELOP)
        // Check if state is provided but only if not running on D3D11
        if (m_platform != RenderAPI::eD3D11 && resource->state == UINT_MAX)
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
                        tag == kBufferTypeShadowDenoised || tag == kBufferTypeSpecularHitDenoised ||
                        tag == kBufferTypeDiffuseHitDenoised || tag == kBufferTypeBackbuffer;
        if (!writeTag && lifecycle != ResourceLifecycle::eValidUntilPresent)
        {
            //! Only make a copy if this tag is required by at least one loaded and supported plugin on the same
            //! viewport and with immutable life-cycle.
            //!
            //! If tag is required on present we have to make a copy always, if tag is required on evaluate
            //! we make a copy only if buffer is tagged as "valid only now" and this is not a local tag.
            bool makeCopy = false;
            {
                std::lock_guard lock(requiredTagMutex);
                auto requiredOnPresent =
                    requiredTags.find({id, tag, ResourceLifecycle::eValidUntilPresent}) != requiredTags.end();
                auto requiredOnEvaluate =
                    requiredTags.find({id, tag, ResourceLifecycle::eValidUntilEvaluate}) != requiredTags.end();
                makeCopy = requiredOnPresent ||
                                (requiredOnEvaluate && lifecycle == ResourceLifecycle::eOnlyValidNow && !localTag);
            }

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

                // Defaults to eCopyDestination state
                frameTag.clone =
                    m_pPool->allocate(actualResource,
                                       (SL_RESOURCE_NAME("clone.tagVolatile.") + sl::getBufferTypeAsStr(tag) + "." + std::to_string(id)).c_str());

                // Get tagged resource's state
                chi::ResourceState state{};
                m_pCompute->getResourceState(frameTag.res.state, state);
                // Now store clone's state for further use in SL
                m_pCompute->getNativeResourceState(chi::ResourceState::eCopyDestination, frameTag.res.state);
                extra::ScopedTasks revTransitions;
                chi::ResourceTransition transitions[] = {
                    {actualResource, chi::ResourceState::eCopySource, state},
                };
                CHI_CHECK_RR(m_pCompute->transitionResources(cmdBuffer,
                                                              transitions,
                                                              (uint32_t)countof(transitions),
                                                              &revTransitions));
                CHI_CHECK_RR(m_pCompute->copyResource(cmdBuffer, frameTag.clone, actualResource));

                // We've made a copy of the original resource and we're not doing AddRef() on the original. So set the
                // original to nullptr - that way nobody can access it (it may become invalid at some point).
                frameTag.res.native = nullptr;
            }
        }
    }

    if (ext)
    {
        frameTag.extent = *ext;
    }

    if (pi)
    {
        frameTag.pi = *pi;
    }

    if (frameTag.res.native && !localTag)
    {
        m_pCompute->startTrackingResource(currFrameId, uid, &frameTag.res);
    }

#if SL_TAG_LOG_ENABLE
    SL_LOG_VERBOSE("Resource tag set for resource %p, buffer type %s, viewport %d, frame %d",
                   sl::chi::Resource(cRes)->native,
                   sl::getBufferTypeAsStr(tag),
                   id,
                   currFrameId);
#endif

    return Result::eOk;
}

//! Thread safe get/set resource tag
//!
void common::ResourceTaggingForFrame::getTag(BufferType tagType,
                                             uint32_t frameId,
                                             uint32_t viewportId,
                                             CommonResource& res,
                                             const sl::BaseStructure** inputs,
                                             uint32_t numInputs,
                                             bool optional)
{
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
                    std::lock_guard<std::mutex> lock(requiredTagMutex);
                    requiredTags.insert({viewportId, tagType, ResourceLifecycle::eValidUntilEvaluate});

                    return;
                }
            }
        }
    }

    //! Now let's check the global ones
    uint64_t uid = ((uint64_t)tagType << 32) | (uint64_t)viewportId;
    {
        auto frameAccess = this->findFrameForReading(frameId);
        if (!frameAccess)
        {
            if (!optional)
            {
                SL_LOG_INFO("SL resource tags for frame %d not set yet!", frameId);
            }

            return;
        }
        const auto& frameTags = frameAccess->m_container.resourceTagContainer;
        if (auto searchTag = frameTags.find(uid); searchTag != frameTags.end())
        {
            res = searchTag->second;
#if SL_TAG_LOG_ENABLE
            SL_LOG_VERBOSE("Resource tag retrieved for resource %p, buffer type %s, viewport %d, frame %d",
                            sl::chi::Resource(res)->native,
                            sl::getBufferTypeAsStr(tagType),
                            viewportId,
                            frameId);
#endif
        }
        else if (!optional)
        {
            // TODO: check if the tag is one of the required tags declared for any of the enabled features in
            // respective common::PluginInfo
            //  and flag an error if so.
            SL_LOG_ERROR("Tag of buffer %s not set for frame %d, viewport %d",
                            getBufferTypeAsStr(tagType),
                            frameId,
                            viewportId);
        }
    }

    std::lock_guard<std::mutex> lock(requiredTagMutex);
    //! Keep track of what tags are requested for what viewport (unique insert)
    //!
    //! Note that the presence of a valid pointer to 'inputs' indicates that we are called
    //! during the evaluate feature call, otherwise tag was requested from a hook (present etc.)
    requiredTags.insert(
        {viewportId, tagType, inputs ? ResourceLifecycle::eValidUntilEvaluate : ResourceLifecycle::eValidUntilPresent});
}

common::ResourceTaggingForFrame::~ResourceTaggingForFrame()
{
    {
        std::lock_guard<std::mutex> lock(requiredTagMutex);
        requiredTags.clear();
    }

    for (uint32_t uFrame = 0; uFrame < m_frames.size(); ++uFrame)
    {
        auto pFrame = this->findFrameForWriting(uFrame);
        recycleTagsForFrame(*pFrame);
    }
}

void common::ResourceTaggingForFrame::recycleTags()
{
    // Try to acquire the recycling mutex - if we can't get it, another thread is already recycling
    std::unique_lock<std::mutex> lock(m_recyclingMutex, std::try_to_lock);
    if (!lock.owns_lock())
    {
        return; // Another thread is already recycling, we can skip
    }

    uint32_t curAppFrameIndex = UINT_MAX;
    api::getContext()->parameters->get(sl::param::latency::kMarkerPresentFrame, &curAppFrameIndex);
    // CPU optimization - execute all stuff inside the if() only once per frame
    if (m_prevSeenAppFrameIndex != curAppFrameIndex || curAppFrameIndex == UINT_MAX)
    {
        m_prevSeenAppFrameIndex = curAppFrameIndex;
        recycleTagsInternal(curAppFrameIndex);
    }
}

void common::ResourceTaggingForFrame::recycleTagsInternal(uint32_t currAppFrameId)
{
    // start with the current app frame and go backward. if we wrap around 0 - it's fine
    for (uint32_t uOldFrame = currAppFrameId - 2; ; --uOldFrame)
    {
        auto pFrame = this->findFrameForWriting(uOldFrame);
        // if that's frame index that we don't expect, this means we have either already
        // recycled this frame before, or we are now looking at a future frame
        if (pFrame->m_container.getFrameIndex() != uOldFrame)
        {
            break;
        }
        recycleTagsForFrame(*pFrame);
        // set invalid past frame index to indicate we've recycled this
        pFrame->m_container.setFrameIndex(uOldFrame - (uint32_t)m_frames.size());
    }
}

void common::ResourceTaggingForFrame::recycleTagsForFrame(
    ScopedResourceTagContainerWriteAccess &frameAccess)
{
    auto& tags = frameAccess.m_container.resourceTagContainer;

    for (auto it = tags.begin(); it != tags.end();++it)
    {
        m_pPool->recycle(it->second.clone);
        m_pCompute->stopTrackingResource(frameAccess.m_container.getFrameIndex(), it->first, &it->second.res);

    }
    tags.clear();
}

} // namespace common
} // namespace sl

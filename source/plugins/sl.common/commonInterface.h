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

#pragma once

#include <atomic>
#include <map>
#include <unordered_set>
#include <shared_mutex>
#include <vector>

#include "include/sl.h"
#include "include/sl_helpers.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.log/log.h"
#include "commonDRSInterface.h"

#define NVAPI_VALIDATE_RF(f) {auto r = f; if(r != NVAPI_OK) { SL_LOG_ERROR( "%s failed error %d", #f, r); return false;} };

#define CHECK_NGX(func)                                                                                  \
{                                                                                                        \
  NVSDK_NGX_Result status = (func);                                                                      \
  if(status == NVSDK_NGX_Result_FAIL_NotImplemented)  { SL_LOG_WARN("%s not implemented", #func);}       \
  else if(status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("%s failed 0x%x", #func, status);}          \
}

#define CHECK_NGX_RETURN_ON_ERROR(func)                                                                                 \
{                                                                                                                       \
  NVSDK_NGX_Result status = (func);                                                                                     \
  if(status == NVSDK_NGX_Result_FAIL_NotImplemented)  { SL_LOG_WARN("%s not implemented", #func); return false;}        \
  else if(status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR( "%s failed 0x%x", #func, status); return false;}          \
}

struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;
enum NVSDK_NGX_Feature;

namespace sl
{
    struct BufferTagInfo
    {
        uint32_t viewportId{};
        BufferType type{};
        ResourceLifecycle lifecycle{};

        inline bool operator==(const BufferTagInfo& rhs) const {
            return viewportId == rhs.viewportId && type == rhs.type && lifecycle == rhs.lifecycle;
        }
    };

    struct BufferTagInfoHash
    {
        size_t operator()(const BufferTagInfo& info) const
        {
            return (size_t(info.viewportId) << 48) | (size_t(info.type) << 32) | size_t(info.lifecycle);
        }
    };

namespace chi
{
using CommandList = void*;
class ICompute;
}

// Threading: proxyBuffers vector is only modified during swapchain create/resize/destroy.
// On D3D12, present is serialized by the runtime so no concurrent access occurs.
// On Vulkan, acquire and present can run on different threads (see dlss_gEntry.cpp),
// but swapchain recreation requires external synchronization by the app per Vulkan spec
// (see defines.h: "In VK, queue is an externally synchronized parameter").
// The SDK relies on this app-side guarantee — no mutex is added here for proxyBuffers.
// proxyBufferIndex is atomic as a forward-looking measure for Vulkan acquire/present threading.
struct SwapChainContext
{
    virtual ~SwapChainContext() = default;

    chi::Resource getCurrentProxyBuffer() const
    {
        uint32_t idx = proxyBufferIndex.load(std::memory_order_acquire);
        return (idx < proxyBuffers.size()) ? proxyBuffers[idx] : nullptr;
    }
    chi::Resource getProxyBuffer(uint32_t index) const
    {
        return (index < proxyBuffers.size()) ? proxyBuffers[index] : nullptr;
    }
    uint32_t getCurrentProxyBufferIndex() const { return proxyBufferIndex.load(std::memory_order_acquire); }
    uint32_t getProxyBufferCount() const { return static_cast<uint32_t>(proxyBuffers.size()); }
    bool hasProxyBuffers() const { return !proxyBuffers.empty(); }

    //! Platform-agnostic accessor: retrieves the SwapChainContext stored on the native swap chain
    //! via ICompute::getSwapChainPrivateData and returns its current proxy buffer.
    static chi::Resource getProxyBufferFromSwapChain(chi::ICompute* compute, void* nativeSwapChain)
    {
        if (!compute || !nativeSwapChain) return nullptr;

        void* rawCtx = nullptr;
        auto status = compute->getSwapChainPrivateData(nativeSwapChain, &rawCtx);
        if (status != chi::ComputeStatus::eOk || !rawCtx)
        {
            return nullptr;
        }
        auto* ctx = static_cast<SwapChainContext*>(rawCtx);
        if (!ctx->hasProxyBuffers())
        {
            return nullptr;
        }
        return ctx->getCurrentProxyBuffer();
    }

    void addProxyBuffer(chi::Resource buffer) { proxyBuffers.push_back(buffer); }

    void resizeProxyBuffers(chi::ICompute* compute, chi::Resource templateBuffer, uint32_t count, const char friendlyName[] = "sl.proxy-buffer")
    {
        if (!proxyBuffers.empty())
        {
            SL_LOG_WARN("resizeProxyBuffers called with %u existing proxy buffers — destroying them first", (uint32_t)proxyBuffers.size());
        }
        destroyAndClearProxyBuffers(compute);
        chi::ResourceDescription desc(templateBuffer->width, templateBuffer->height, templateBuffer->nativeFormat,
            chi::HeapType::eHeapTypeDefault, chi::ResourceState::ePresent,
            chi::ResourceFlags::eShaderResourceStorage | chi::ResourceFlags::eColorAttachment);
        for (uint32_t i = 0; i < count; ++i)
        {
            chi::Resource buffer{};
            auto status = compute->createTexture2D(desc, buffer, friendlyName);
            if (status != chi::ComputeStatus::eOk)
            {
                SL_LOG_ERROR("resizeProxyBuffers: createTexture2D failed at buffer %u/%u (status %d) — cleaning up", i, count, status);
                destroyAndClearProxyBuffers(compute);
                return;
            }
            proxyBuffers.push_back(buffer);
        }
    }

    void destroyAndClearProxyBuffers(chi::ICompute* compute)
    {
        for (auto& buffer : proxyBuffers)
        {
            compute->destroyResource(buffer);
        }
        proxyBuffers.clear();
    }

    void advanceProxyBufferIndex()
    {
        if (proxyBuffers.empty())
        {
            proxyBufferIndex.store(0, std::memory_order_release);
        }
        else
        {
            proxyBufferIndex.store(
                (proxyBufferIndex.load(std::memory_order_relaxed) + 1) % static_cast<uint32_t>(proxyBuffers.size()),
                std::memory_order_release);
        }
    }

private:
    std::vector<chi::Resource> proxyBuffers{};
    std::atomic<uint32_t> proxyBufferIndex{};
};

namespace common
{
    struct ResourceTaggingGeneral;
    struct ResourceTaggingForFrame;
}

struct CommonResource
{
    friend common::ResourceTaggingGeneral;
    friend common::ResourceTaggingForFrame;

    inline operator bool() { return clone || res.native != nullptr; }
    inline operator bool() const { return clone || res.native != nullptr; }
    
    inline operator const chi::Resource() const
    { 
        if (clone) return clone;
        return (const chi::Resource)&res;
    }
    inline operator chi::Resource()
    {
        if (clone) return clone;
        return (chi::Resource)&res;
    }
    inline void* getNative() const {
        if (clone) return clone.getNative();
        return res.native;
    }
    inline operator void*() const
    {
        return getNative();
    }
    inline operator void*()
    {
        return getNative();
    }
    inline CommonResource& operator=(chi::Resource rhs)
    {
        if (rhs) res = *rhs; else { res = {}; extent = {}; pi = {}; clone = {}; }
        return *this;
    }
    inline operator const Extent& () const { return extent; }
    inline operator const PrecisionInfo& () const { return pi; }
    inline bool isCloned() const { return clone; };
    inline uint32_t getState() const { return res.state; }
    inline const Extent& getExtent() const { return extent; }
    inline const PrecisionInfo& getPrecisionInfo() const { return pi; }
    
    uint64_t uFrameWhenTagged = ~0ull;

private:
    sl::Resource res{};
    Extent extent{};
    PrecisionInfo pi{};
    chi::HashedResource clone{};
};

using PFunGetTag = void(BufferType tag, uint32_t frameId, uint32_t id, CommonResource& res, const sl::BaseStructure** inputs, uint32_t numInputs, bool optional);

inline Result getTaggedResource(BufferType tagType, CommonResource& res, uint32_t frameId, uint32_t viewportId, bool optional = false, const sl::BaseStructure** inputs = nullptr, uint32_t numInputs = 0)
{
    res = {};

    static PFunGetTag* getTagThreadSafe = {};
    if (!getTagThreadSafe)
    {
        param::getPointerParam(api::getContext()->parameters, sl::param::global::kPFunGetTag, &getTagThreadSafe);
    }
    // Always returns an instance of common resource even if invalid (not provided by host, all values null)
    getTagThreadSafe(tagType, frameId, viewportId, res, inputs, numInputs, optional);
    if (!res && !optional)
    {
        SL_LOG_ERROR("Failed to find global tag '%s', please make sure to tag all required buffers", getBufferTypeAsStr(tagType));
        return Result::eErrorMissingInputParameter;
    }
    return Result::eOk;
}

struct CommonResource;
struct Constants;
using BufferType = uint32_t;

namespace common
{

// Limiting to 8 GPUs to handle unexpected cases, at LEAST
// (iGPU + dGPU) x 2 for remote desktop adapter
// or 2x dGPU x 2 for remote desktop adapter
constexpr uint32_t kMaxNumSupportedGPUs = 8;

struct Adapter
{
    LUID id{};
    chi::VendorId vendor{};
    uint32_t bit; // in the adapter bit-mask
    uint32_t architecture{};
    uint32_t implementation{};
    uint32_t revision{};
    uint32_t deviceId{};
    void* nativeInterface{};
};

using PFunFindAdapter = sl::Result(const sl::AdapterInfo& info, uint32_t adapterMask);

struct SystemCaps
{
    uint32_t gpuCount{};
    uint32_t osVersionMajor{};
    uint32_t osVersionMinor{};
    uint32_t osVersionBuild{};
    uint32_t driverVersionMajor{};
    uint32_t driverVersionMinor{};
    Adapter adapters[kMaxNumSupportedGPUs]{};
    uint32_t gpuLoad[kMaxNumSupportedGPUs]{}; // percentage
    bool hwsSupported{}; // OS wide setting, not per adapter
    bool laptopDevice{};
};

std::pair<sl::chi::ICompute*, sl::chi::ICompute*> createCompute(void* device, RenderAPI deviceType, bool dx11On12);
bool destroyCompute();

// Get info about the GPU, id can be null in which case we get info for GPU 0
using PFunGetGPUInfo = bool(SystemCaps& info);

// NGX context

struct PluginInfo;

using PFunNGXCreateFeature = bool(void* cmdList, NVSDK_NGX_Feature feature, NVSDK_NGX_Handle** handle, const char* id);
using PFunNGXEvaluateFeature = bool(void* cmdList, NVSDK_NGX_Handle* handle, const char* id);
using PFunNGXReleaseFeature = bool(NVSDK_NGX_Handle* handle, const char* id);
using PFunNGXBeforeReleaseFeature = void(NVSDK_NGX_Handle* handle);
using PFunNGXUpdateFeature = void(NVSDK_NGX_Feature feature);
using PFunNGXGetFeatureCaps = bool(NVSDK_NGX_Feature feature, PluginInfo& info);

constexpr uint32_t kMaxNumBeforeReleaseCallbacks = 32;

struct NGXContext
{
    NVSDK_NGX_Parameter* params{};
    PFunNGXCreateFeature* createFeature{};
    PFunNGXReleaseFeature* releaseFeature{};
    PFunNGXEvaluateFeature* evaluateFeature{};
    PFunNGXUpdateFeature* updateFeature{};
};

struct EventData
{
    uint32_t id = 0;
    uint32_t frame = 0;

    inline bool empty() const
    {
        return id == 0 && frame == 0;
    }
};

struct GetDataResult
{
    enum GetDataResultValue
    {
        eNotFound = 0,
        eFound = 1,
        eFoundExact = 2
    } value;
    
    GetDataResult(GetDataResultValue v = eNotFound) : value(v) {};
    inline operator bool() const { return value != eNotFound; }
};

inline bool operator!(GetDataResult r)
{
    return !((uint8_t)r);
}

struct OpticalFlowInfo
{
    bool nativeHWSupport = false;
    uint32_t queueFamily{};
    uint32_t queueIndex{};
};

struct PluginInfo
{
    PluginInfo() {};
    PluginInfo(const PluginInfo& rhs) = delete;
    Version minOS{};
    Version minDriver{};
    const char* SHA{};
    uint32_t minGPUArchitecture{}; 
    bool needsNGX{};
    bool needsDX11On12{};
    bool needsDRS{};
    std::vector<std::pair<BufferType, ResourceLifecycle>> requiredTags;
    std::vector<std::string> vkInstanceExtensions;
    std::vector<std::string> vkDeviceExtensions;
    uint32_t minVkAPIVersion{};
    OpticalFlowInfo opticalFlowInfo{};
};

// NOTE: Using void* instead of json* to avoid including large json header
using PFunUpdateCommonEmbeddedJSONConfig = void(void* config, const PluginInfo& info);
using PFunGetStringFromModule = bool(const char* moduleName, const char* stringName, std::string& value);
using PFunGetConstants = GetDataResult(const EventData&, Constants** consts);
using PFunRegisterFeatureViewport = void(Feature feature, uint32_t viewportId);
using PFunUnregisterFeatureViewport = void(Feature feature, uint32_t viewportId);

inline GetDataResult getConsts(const EventData& data, sl::Constants** consts)
{
    auto parameters = api::getContext()->parameters;
    common::PFunGetConstants* getConsts = {};
    param::getPointerParam(parameters, param::global::kPFunGetConsts, &getConsts);
    if (!getConsts)
    {
        SL_LOG_ERROR( "Cannot obtain common constants");
        return GetDataResult();
    }
    return getConsts(data, consts);
}

bool getFeatureViewports(Feature feature, std::vector<uint32_t>& viewportIds);

using PFunBeginEndEvent = sl::Result(chi::CommandList cmdList, const common::EventData& data, const sl::BaseStructure** inputs, uint32_t numInputs);
using PFunRegisterEvaluateCallbacks = void(Feature feature, PFunBeginEndEvent* beginEvent, PFunBeginEndEvent* endEvent);

CommandBuffer* getNativeCommandBuffer(CommandBuffer* cmdBuffer, bool* slProxy = false);
void registerEvaluateCallbacks(Feature feature, PFunBeginEndEvent* beginEvent, PFunBeginEndEvent* endEvent);
bool onLoad(const void* managerConfig, const void* extraConfig, chi::IResourcePool* pool);

struct EvaluateCallbacks
{
    PFunBeginEndEvent* beginEvaluate;
    PFunBeginEndEvent* endEvaluate;
};

template<typename T, typename... Args>
void packData(std::vector<uint8_t>& blob, const T* a)
{
    if (a)
    {
        auto offset = blob.size();
        blob.resize(offset + sizeof(T));
        auto p = blob.data() + offset;
        *((T*)p) = *a;
    }
}

template<typename T, typename... Args>
void packData(std::vector<uint8_t>& blob, const T* a, Args... args)
{
    packData(blob, a);
    packData(blob, args...);
}

template<typename T, typename... Args>
void unpackData(std::vector<uint8_t>& blob, size_t& offset, T** a)
{
    if (blob.size() > offset)
    {
        auto p = blob.data() + offset;
        *a = ((T*)p);
        (*a)->next = {};
        offset += sizeof(T);
    }
    else
    {
        a = nullptr;
    }
}

template<typename T, typename... Args>
void unpackData(std::vector<uint8_t>& blob, size_t& offset, T** a, Args... args)
{
    unpackData(blob, offset, a);
    unpackData(blob, offset, args...);
}

//! Unique frame data
//! 
//! By default we assume that no more than 3 unique data sets will
//! be prepared (queuing up no more than 3 frames in advance).
//! 
//! We also assume that, by default, data does NOT need to be set each frame
//! (we will fetch whatever was set last) but in some cases that is needed 
//! if data does change every frame.
//! 
template<uint32_t dataQueueSize = 3, bool mustSetEachFrame = false >
struct ViewportIdFrameData
{
    struct FrameData
    {
        FrameData() {};
        FrameData(const std::vector<uint8_t>& d, uint32_t f) : data(d), frame(f) {};
        FrameData(const FrameData& rhs) { operator=(rhs); }
        inline FrameData& operator=(const FrameData& rhs)
        {
            data = rhs.data;
            frame = rhs.frame;
            return *this;
        }

        std::vector<uint8_t> data{};
        uint32_t frame{};
    };

    struct IndexedFrameData
    {
        IndexedFrameData() {};
        IndexedFrameData(const IndexedFrameData& rhs) { operator=(rhs); }
        inline IndexedFrameData& operator=(const IndexedFrameData& rhs)
        {
            index = rhs.index;
            lastIndex = rhs.lastIndex;
            frames = rhs.frames;
            return *this;
        }

        uint32_t index = {};
        uint32_t lastIndex = UINT_MAX;
        std::vector<FrameData> frames = {};
    };

    ViewportIdFrameData(const char* name) : m_name(name) {};

    template<typename T, typename... Args>
    bool set(uint32_t frame, uint32_t id, const T* a)
    {
        std::vector<uint8_t> blob;
        packData(blob, a);
        return set(blob, frame, id);
    }

    template<typename T, typename... Args>
    bool set(uint32_t frame, uint32_t id, const T* a, Args... args)
    {
        std::vector<uint8_t> blob;
        packData(blob, a);
        packData(blob, args...);
        return set(blob, frame, id);
    }

    template<typename T, typename... Args>
    GetDataResult get(const common::EventData& ev, T** a)
    {
        std::vector<uint8_t>* blob{};
        auto res = get(ev, blob);
        if (res)
        {
            size_t offset = 0;
            unpackData(*blob, offset, a);
            if (*a == nullptr) return GetDataResult::eNotFound;
        }
        return res;
    }

    template<typename T, typename... Args>
    GetDataResult get(const common::EventData& ev, T** a, Args... args)
    {
        std::vector<uint8_t>* blob{};
        auto res = get(ev, blob);
        if (res)
        {
            size_t offset = 0;
            unpackData(*blob, offset, a);
            unpackData(*blob, offset, args...);
            if (*a == nullptr) return GetDataResult::eNotFound;
        }
        return res;
    }

private:

    bool set(const std::vector<uint8_t>& data, uint32_t frame, uint32_t id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& item = m_list[id];
        if (item.frames.empty())
        {
            item.frames.resize(dataQueueSize);
        }
        if (item.lastIndex != UINT_MAX && item.frames[item.lastIndex].frame == frame)
        {
            //! Settings constants more than once per frame for the same unique id
            //! 
            //! This is fine ONLY if constants are identical so check
            auto& lastData = item.frames[item.lastIndex].data;
            if (lastData.size() != data.size() || (memcmp(lastData.data(), data.data(), data.size()) != 0))
            {
                // Incoming and the existing data either have different size or different contents, this is not allowed within the same frame
                item.frames[item.lastIndex] = { data, frame };
                if (mustSetEachFrame)
                {
                    SL_LOG_ERROR( "Setting different '%s' constants multiple times within the same frame is NOT allowed!", m_name.c_str());
                    return false;
                }
                return true;
            }
            else
            {
                // Data at the last set index is identical, let it slide, nothing to do here.
                return true;
            }
        }
        item.frames[item.index] = { data, frame };
        item.lastIndex = item.index;
        item.index = (item.index + 1) % dataQueueSize;
        return true;
    }

    GetDataResult get(const common::EventData& ev, std::vector<uint8_t>*& outData)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto* item = &m_list[ev.id];
        if (item->frames.empty())
        {
            // Not set for this id so let's default to 0
            item = &m_list[0];
            if (item->frames.empty())
            {
                // Not set for 0, this is definitely not allowed
                return GetDataResult::eNotFound;
            }
        }
        for (uint32_t i = 0; i < dataQueueSize; i++)
        {
            uint32_t n = (item->lastIndex + i) % dataQueueSize;
            if (item->frames[n].frame == ev.frame)
            {
                outData = &item->frames[n].data;
                return GetDataResult::eFoundExact;
            }
        }
        outData = &item->frames[item->lastIndex].data;
        if (!ev.empty())
        {
            if (mustSetEachFrame)
            {
                // This can really spam the log due to changing frame index
                SL_LOG_ERROR_ONCE( "Unable to find '%s' constants for frame %u - id %u - using last set for frame %u - this needs to be fixed if occurring every frame", m_name.c_str(), ev.frame, ev.id, item->frames[item->lastIndex].frame);
            }
            else
            {
                SL_LOG_WARN_ONCE("Unable to find '%s' constants for frame %u - id %u - using last set for frame %u - this is OK since consts are flagged as not needed every frame", m_name.c_str(), ev.frame, ev.id, item->frames[item->lastIndex].frame);
            }
        }
        return GetDataResult::eFound;
    }

    std::string m_name = {};
    std::mutex m_mutex = {};
    std::map<uint32_t, IndexedFrameData> m_list = {};
};

struct ResourceTaggingBase
{
    // Legacy resource-tagging API (slSetTag) implementation replaces existing tag only when a new tag of same type arrives which is incorrect
    // as tag lifecycle validity is only until the end of Present for that frame and client is expected to set tags every frame.
    // Tags can be local or global and can even be set during slEvaluateFeature without using explicit API for that.
    // Further slEvaluateFeature API is frame-aware, even though legacy API didn't associate tags with frames whereas new API is supposed to.
    // Also, this API sets and retrieves tags in the same call and client may not even invoke explicit tagging API (slSetTag) or invoke
    // both these APIs in any order making it difficult to detect when to do frame-based tracking of resources.
    // Since consolidating the bug-ridden legacy resource tagging implementation with the new frame-aware one can add complexity and difficulty
    // in maintainabilty, isolating the legacy one from the new for now for simplicity and maintainability.
    virtual sl::Result setTag(const sl::Resource* resource,
        BufferType,
        uint32_t,
        const Extent*,
        ResourceLifecycle,
        CommandBuffer*,
        bool,
        const PrecisionInfo*,
        const sl::FrameToken&) = 0;
    virtual void getTag(BufferType,
        uint32_t,
        uint32_t,
        CommonResource&,
        const sl::BaseStructure**,
        uint32_t,
        bool) = 0;

    virtual ~ResourceTaggingBase() = default;

protected:
    std::unordered_set<BufferTagInfo, BufferTagInfoHash> requiredTags{};
};

struct ResourceTaggingGeneral : public ResourceTaggingBase
{
    virtual sl::Result setTag(const sl::Resource* resource,
        BufferType tag,
        uint32_t id,
        const Extent* ext,
        ResourceLifecycle lifecycle,
        CommandBuffer* cmdBuffer,
        bool localTag,
        const PrecisionInfo* pi,
        const sl::FrameToken& frame) override final;
    virtual void getTag(BufferType tagType,
        uint32_t frameId,
        uint32_t viewportId,
        CommonResource& res,
        const sl::BaseStructure** inputs,
        uint32_t numInputs,
        bool optional) override final;

    ~ResourceTaggingGeneral() override final;

private:
    std::map<uint64_t, CommonResource> idToResourceMap;
    std::mutex resourceTagMutex{};
};

}
}

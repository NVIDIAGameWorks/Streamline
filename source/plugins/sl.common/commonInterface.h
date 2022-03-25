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

#include <map>

#include "include/sl.h"

#define NVAPI_VALIDATE_RF(f) {auto r = f; if(r != NVAPI_OK) { SL_LOG_ERROR("%s failed error %d", #f, r); return false;} };

#define CHECK_NGX(func)                                                                     \
{                                                                                           \
  NVSDK_NGX_Result status = (func);                                                         \
  if(status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("%s failed %u", #func, status);}    \
}

#define CHECK_NGX_RETURN_ON_ERROR(func)                                                                 \
{                                                                                                       \
  NVSDK_NGX_Result status = (func);                                                                     \
  if(status != NVSDK_NGX_Result_Success) { SL_LOG_ERROR("%s failed %u", #func, status); return false;}  \
}

struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;
enum NVSDK_NGX_Feature;

namespace sl
{

namespace chi
{
using CommandList = void*;
}

struct CommonResource
{
    sl::Resource res = {};
    Extent extent = {};
};

using PFunGetTag = CommonResource * (BufferType tag, uint32_t id);

inline bool getTaggedResource(BufferType tag, void*& res, uint32_t id = 0, Extent* ext = nullptr)
{
    res = nullptr;
    static PFunGetTag* getTag = {};
    if (!getTag)
    {
        param::getPointerParam(api::getContext()->parameters, sl::param::global::kPFunGetTag, &getTag);
    }
    CommonResource* cr = getTag(tag, id);
    res = cr->res.view ? &cr->res : cr->res.native;
    if (ext)
    {
        *ext = cr->extent;
    }
    return cr->res.native != nullptr;
}

struct CommonResource;
struct Constants;
enum BufferType;

namespace common
{
struct GPUArch
{
    uint32_t gpuCount;
    uint32_t driverVersionMajor;
    uint32_t driverVersionMinor;
    // We cover up to 2 GPUs (iGPU + dGPU on laptops)
    uint32_t architecture[2];
    uint32_t implementation[2];
    uint32_t revision[2];
};

bool createCompute(void* device, uint32_t deviceType);
bool destroyCompute();

// Get info about the GPU, id can be null in which case we get info for GPU 0
using PFunGetGPUInfo = bool(GPUArch& info, LUID* id);

// NGX context
using PFunNGXCreateFeature = bool(void* cmdList, NVSDK_NGX_Feature feature, NVSDK_NGX_Handle** handle);
using PFunNGXEvaluateFeature = bool(void* cmdList, NVSDK_NGX_Handle* handle);
using PFunNGXReleaseFeature = bool(NVSDK_NGX_Handle* handle);

struct NGXContext
{
    NVSDK_NGX_Parameter* params;
    PFunNGXCreateFeature* createFeature;
    PFunNGXReleaseFeature* releaseFeature;
    PFunNGXEvaluateFeature* evaluateFeature;
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

using PFunGetConstants = bool(const EventData&, Constants& consts);

inline bool getConsts(const EventData& data, sl::Constants& consts)
{
    auto parameters = api::getContext()->parameters;
    common::PFunGetConstants* getConsts = {};
    param::getPointerParam(parameters, param::global::kPFunGetConsts, &getConsts);
    if (!getConsts || !getConsts(data, consts))
    {
        SL_LOG_ERROR("Cannot obtain common constants");
        return false;
    }
    return true;
}

using PFunBeginEvent = void(chi::CommandList cmdList, const common::EventData& data);
using PFunEndEvent = void(chi::CommandList cmdList);
using PFunRegisterEvaluateCallbacks = void(Feature feature, PFunBeginEvent* beginEvent, PFunEndEvent* endEvent);

bool evaluateFeature(void* pCmdList, Feature feature, uint32_t frameIndex, uint32_t id);
void registerEvaluateCallbacks(Feature feature, PFunBeginEvent* beginEvent, PFunEndEvent* endEvent);

template<typename T, uint32_t kDataQueueSize = 24, bool mustSetEachFrame = true>
struct ViewportIdFrameData
{
    struct FrameData
    {
        T data;
        uint32_t frame;
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
        uint32_t lastIndex = {};
        std::vector<FrameData> frames = {};
    };

    ViewportIdFrameData(const char* name) : m_name(name) {};

    void set(const T& data, uint32_t frame, uint32_t id)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& item = m_list[id];
        if (item.frames.empty())
        {
            item.frames.resize(kDataQueueSize);
        }
        item.frames[item.index] = { data, frame };
        item.lastIndex = item.index;
        item.index = (item.index + 1) % kDataQueueSize;
    }

    bool get(const common::EventData& ev, T& outData)
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
                return false;
            }
        }
        for (uint32_t i = 0; i < kDataQueueSize; i++)
        {
            uint32_t n = (item->lastIndex + i) % kDataQueueSize;
            if (item->frames[n].frame == ev.frame)
            {
                outData = item->frames[n].data;
                return true;
            }
        }
        outData = item->frames[item->lastIndex].data;
        if (!ev.empty())
        {
            if (mustSetEachFrame)
            {
                SL_LOG_WARN("Unable to find constants for frame %u - id %u - using last set for frame %u", ev.frame, ev.id, item->frames[item->lastIndex].frame);
            }
            else
            {
                SL_LOG_WARN_ONCE("Unable to find constants for frame %u - id %u - using last set for frame %u - this is OK since consts are flagged as not needed every frame", ev.frame, ev.id, item->frames[item->lastIndex].frame);
            }
        }
        return true;
    }

    std::map<uint32_t, IndexedFrameData> m_list = {};
    std::string m_name = {};
    std::mutex m_mutex = {};
};

}
}

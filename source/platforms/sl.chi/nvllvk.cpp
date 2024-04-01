/*
* Copyright (c) 2023 NVIDIA CORPORATION. All rights reserved
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

#ifdef SL_WITH_NVLLVK

#include "source/core/sl.log/log.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "nvllvk.h"

#include "external/reflex-sdk-vk/inc/NvLowLatencyVk.h"

#define LL_CHECK(f) {auto _r = f;if(_r != NvLL_VK_Status::NVLL_VK_OK){SL_LOG_ERROR( "%s failed - error %u",#f,_r); return ComputeStatus::eError;}}

namespace sl
{
namespace chi
{

class NvLowLatencyVk : public IReflexVk
{
private:
    VkDevice m_device{};
    VkLayerDispatchTable m_ddt{};

    VkSemaphore m_lowLatencySemaphore{};
    uint64_t reflexSemaphoreValue = 0;

    HMODULE m_hmodReflex{};
public:
    ComputeStatus init(VkDevice device, param::IParameters* params)
    {
        m_device = device;

        // Path where our modules are located
        wchar_t* pluginPath{};
        param::getPointerParam(params, param::global::kPluginPath, &pluginPath);
        if (!pluginPath)
        {
            SL_LOG_ERROR( "Cannot find path to plugins");
            return ComputeStatus::eError;
        }
        std::wstring path(pluginPath);
        path += L"/NvLowLatencyVk.dll";
        // This call translates to signature check in production and regular load otherwise
        m_hmodReflex = security::loadLibrary(path.c_str());
        if (!m_hmodReflex)
        {
            SL_LOG_ERROR( "Failed to load %S", path.c_str());
            return ComputeStatus::eError;
        }

        // Low latency API
        auto llRes = NvLL_VK_Initialize();
        if (llRes)
        {
            SL_LOG_WARN("Low latency API for VK failed to initialize %d", llRes);
        }
        else
        {
            HANDLE semaphore;
            llRes = NvLL_VK_InitLowLatencyDevice(m_device, &semaphore);
            if (llRes)
            {
                SL_LOG_WARN("Low latency API for VK failed to initialize device %d", llRes);
            }
            else
            {
                m_lowLatencySemaphore = (VkSemaphore)semaphore;
            }
        }
        return ComputeStatus::eOk;
    }

    virtual ComputeStatus shutdown() override
    {
        NvLL_VK_DestroyLowLatencyDevice(m_device);
        NvLL_VK_Unload();

        if (m_hmodReflex)
        {
            FreeLibrary(m_hmodReflex);
            m_hmodReflex = {};
        }
        return ComputeStatus::eOk;
    }

    virtual void initDispatchTable(VkLayerDispatchTable table) override
    {
        m_ddt = table;
    }

    virtual ComputeStatus setSleepMode(const ReflexOptions& consts) override
    {
        NVLL_VK_SET_SLEEP_MODE_PARAMS params{ 
            consts.mode != ReflexMode::eOff,
            consts.mode == ReflexMode::eLowLatencyWithBoost,
            consts.frameLimitUs 
        };
        LL_CHECK(NvLL_VK_SetSleepMode(m_device, &params));
        return ComputeStatus::eOk;
    }

    virtual ComputeStatus getSleepStatus(ReflexState& settings) override
    {
        NVLL_VK_GET_SLEEP_STATUS_PARAMS params{};
        LL_CHECK(NvLL_VK_GetSleepStatus(m_device, &params));
        return ComputeStatus::eOk;
    }
    
    virtual ComputeStatus getReport(ReflexState& settings) override
    {
        NVLL_VK_LATENCY_RESULT_PARAMS params{};
        LL_CHECK(NvLL_VK_GetLatency(m_device, &params));
        for (auto i = 0; i < 64; i++)
        {
            settings.frameReport[i].frameID = params.frameReport[i].frameID;
            settings.frameReport[i].inputSampleTime = params.frameReport[i].inputSampleTime;
            settings.frameReport[i].simStartTime = params.frameReport[i].simStartTime;
            settings.frameReport[i].simEndTime = params.frameReport[i].simEndTime;
            settings.frameReport[i].renderSubmitStartTime = params.frameReport[i].renderSubmitStartTime;
            settings.frameReport[i].renderSubmitEndTime = params.frameReport[i].renderSubmitEndTime;
            settings.frameReport[i].presentStartTime = params.frameReport[i].presentStartTime;
            settings.frameReport[i].presentEndTime = params.frameReport[i].presentEndTime;
            settings.frameReport[i].driverStartTime = params.frameReport[i].driverStartTime;
            settings.frameReport[i].driverEndTime = params.frameReport[i].driverEndTime;
            settings.frameReport[i].osRenderQueueStartTime = params.frameReport[i].osRenderQueueStartTime;
            settings.frameReport[i].osRenderQueueEndTime = params.frameReport[i].osRenderQueueEndTime;
            settings.frameReport[i].gpuRenderStartTime = params.frameReport[i].gpuRenderStartTime;
            settings.frameReport[i].gpuRenderEndTime = params.frameReport[i].gpuRenderEndTime;
            settings.frameReport[i].gpuActiveRenderTimeUs = (uint32_t)(params.frameReport[i].gpuRenderEndTime - params.frameReport[i].gpuRenderStartTime);
            settings.frameReport[i].gpuFrameTimeUs = i == 0 ? 0 : (uint32_t)(params.frameReport[i].gpuRenderEndTime - params.frameReport[i - 1].gpuRenderEndTime);
        }
        return ComputeStatus::eOk;
    }

    virtual ComputeStatus sleep() override
    {
        reflexSemaphoreValue++;
        LL_CHECK(NvLL_VK_Sleep(m_device, reflexSemaphoreValue));
        VkSemaphoreWaitInfo waitInfo;
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.pNext = NULL;
        waitInfo.flags = 0;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores = &m_lowLatencySemaphore;
        waitInfo.pValues = &reflexSemaphoreValue;
        m_ddt.WaitSemaphores(m_device, &waitInfo, kMaxSemaphoreWaitUs);
        return ComputeStatus::eOk;
    }

    virtual ComputeStatus setMarker(PCLMarker marker, uint64_t frameId)
    {
        NVLL_VK_LATENCY_MARKER_PARAMS params{ frameId, (NVLL_VK_LATENCY_MARKER_TYPE)marker };
        LL_CHECK(NvLL_VK_SetLatencyMarker(m_device, &params));
        return ComputeStatus::eOk;
    }

    virtual ComputeStatus notifyOutOfBandCommandQueue(CommandQueue queue, OutOfBandCommandQueueType type) override
    {
        LL_CHECK(NvLL_VK_NotifyOutOfBandQueue(m_device, (VkQueue)((CommandQueueVk*)queue)->native, (NVLL_VK_OUT_OF_BAND_QUEUE_TYPE)type));
        return ComputeStatus::eOk;
    }

    virtual ComputeStatus setAsyncFrameMarker(CommandQueue queue, PCLMarker marker, uint64_t frameId) override
    {
        NVLL_VK_LATENCY_MARKER_PARAMS params{ frameId, (NVLL_VK_LATENCY_MARKER_TYPE)marker };
        LL_CHECK(NvLL_VK_SetLatencyMarker(m_device, &params));
        return ComputeStatus::eOk;
    }
};

IReflexVk* CreateNvLowLatencyVk(VkDevice device, param::IParameters* params)
{
    auto ptr = new NvLowLatencyVk();
    ComputeStatus res = ptr->init(device, params);
    if (res != ComputeStatus::eOk)
    {
        SL_LOG_INFO("Failed to init NvLowLatencyVk: %d", res);
        delete ptr;
        ptr = nullptr;
    }
    return ptr;
}

} // namespace chi
} // namespace sl

#endif

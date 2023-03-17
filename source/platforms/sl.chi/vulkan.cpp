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

#include "source/core/sl.log/log.h"
#include "source/platforms/sl.chi/vulkan.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "shaders/vulkan_clear_image_view_spirv.h"
#include "external/reflex-sdk-vk/inc/NvLowLatencyVk.h"
#include "external/vulkan/1.3.204.1/include/vulkan/vulkan_win32.h"

// Errors are negative so don't check for VK_SUCCESS only since there are other 'non fatal' values > 0, we show them as warnings
#define VK_CHECK(f) {auto _r = f;if(_r < 0){SL_LOG_ERROR("%s failed - error %d",#f,_r); return ComputeStatus::eError;} else if(_r != 0) {SL_LOG_WARN("%s - warning %d",#f,_r);}}
#define VK_CHECK_RV(f) {auto _r = f;if(_r < 0){SL_LOG_ERROR("%s failed - error %d",#f,_r); return;} else if(_r != 0) {SL_LOG_WARN("%s - warning %d",#f,_r);}}
#define VK_CHECK_RF(f) {auto _r = f;if(_r < 0){SL_LOG_ERROR("%s failed - error %d",#f,_r); return false;} else if(_r != 0) {SL_LOG_WARN("%s - warning %d",#f,_r);}}
#define VK_CHECK_RN(f) {auto _r = f;if(_r < 0){SL_LOG_ERROR("%s failed - error %d",#f,_r); return nullptr;} else if(_r != 0) {SL_LOG_WARN("%s - warning %d",#f,_r);}}
#define VK_CHECK_RI(f) {auto _r = f;if(_r < 0){SL_LOG_ERROR("%s failed - error %d",#f,_r); return _r;} else if(_r != 0) {SL_LOG_WARN("%s - warning %d",#f,_r);}}
#define VK_CHECK_RE(res, f) res = f;if(res < 0){SL_LOG_ERROR("%s failed - error %d",#f,res); return res;} else if(res != 0) {SL_LOG_WARN("%s - warning %d",#f,res);}
#define VK_CHECK_RWS(f) {auto _r = f;if(_r < 0){SL_LOG_ERROR("%s failed - error %d",#f,_r); return WaitStatus::eError;} else if(_r == VK_TIMEOUT) {SL_LOG_WARN("%s - timed out", #f); return WaitStatus::eTimeout;}}

#define LL_CHECK(f) {auto _r = f;if(_r != NvLL_VK_Status::NVLL_VK_OK){SL_LOG_ERROR( "%s failed - error %u",#f,_r); return ComputeStatus::eError;}}

namespace sl
{
namespace chi
{

Vulkan s_vulkan;
ICompute *getVulkan()
{
    return &s_vulkan;
}

constexpr uint64_t kMaxSemaphoreWaitUs = 500000000; // 500ms max wait on any semaphore;

class CommandListContextVK : public ICommandListContext
{
    struct WaitInfo
    {
        VkSemaphore fence;
        uint64_t value;
    };
    std::vector<WaitInfo> m_waitingQueue;

    VkLayerDispatchTable m_ddt;
    interposer::VkTable* m_vk;

    ICompute* m_compute = {};
    VkQueue m_cmdQueue;
    VkSemaphore m_presentSemaphore{};
    VkSemaphore m_acquireSemaphore{};
    std::vector<VkCommandBuffer> m_cmdBuffer;
    std::vector<VkCommandPool> m_allocator;
    std::vector<VkSemaphore> m_fence;
    std::vector<uint64_t> m_fenceValue = {};
    bool m_cmdListIsRecording = false;
    uint32_t m_emptyIndex = 0; // used for WAR see below
    uint32_t m_index = 0;
    uint32_t m_lastIndex = UINT_MAX;
    uint32_t m_clCount = 0;
    uint32_t m_bufferCount = 0;
    uint32_t m_bufferToPresent = 0;
    std::wstring m_name;
    VkDevice m_device = {};
    std::mutex m_mtxQueueList;

    // Keep validation layer happy
    const VkPipelineStageFlags waitDstStageMask[4] = { VK_PIPELINE_STAGE_ALL_COMMANDS_BIT , VK_PIPELINE_STAGE_ALL_COMMANDS_BIT , VK_PIPELINE_STAGE_ALL_COMMANDS_BIT , VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };

public:

    void init(ICompute* c, interposer::VkTable* vkMap, const char* debugName, VkDevice dev, CommandQueueVk* queue, uint32_t count)
    {
        m_compute = c;
        m_device = dev;
        m_vk = vkMap;
        m_ddt = m_vk->dispatchDeviceMap[dev];
        m_name = extra::utf8ToUtf16(debugName);
        m_cmdQueue = (VkQueue)queue->native;
        m_bufferCount = count;
        // Allocate double, see below why
        m_clCount = m_bufferCount * 2;
        m_allocator.resize(m_clCount);
        m_fence.resize(m_clCount);
        m_fenceValue.resize(m_clCount);
        m_cmdBuffer.resize(m_clCount);
    
        VkSemaphoreCreateInfo createInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = {};
        createInfo.flags = 0;
        VK_CHECK_RV(m_ddt.CreateSemaphore(dev, &createInfo, NULL, &m_presentSemaphore));
        VK_CHECK_RV(m_ddt.CreateSemaphore(dev, &createInfo, NULL, &m_acquireSemaphore));

        sl::Resource r;
        r.native = m_presentSemaphore;
        r.type = (ResourceType)ResourceType::eFence;
        m_compute->setDebugName(&r, "present_semaphore");
        r.native = m_acquireSemaphore;
        r.type = (ResourceType)ResourceType::eFence;
        m_compute->setDebugName(&r, "acquire_semaphore");

        SL_LOG_INFO("Creating command context %s - cmd buffers %u - dummy cmd buffers %u", debugName, m_bufferCount, m_clCount - m_bufferCount);

        // First N used for regular work submission, second N empty buffers for driver WAR when waiting with no workload
        for (uint32_t i = 0; i < m_clCount; i++)
        {
            {
                VkSemaphoreTypeCreateInfo timelineCreateInfo;
                timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
                timelineCreateInfo.pNext = NULL;
                timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
                timelineCreateInfo.initialValue = 0;

                VkSemaphoreCreateInfo createInfo;
                createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                createInfo.pNext = &timelineCreateInfo;
                createInfo.flags = 0;

                VK_CHECK_RV(m_ddt.CreateSemaphore(dev, &createInfo, NULL, &m_fence[i]));

                m_fenceValue[i] = 0;

                sl::Resource r;
                r.native = m_fence[i];
                r.type = (ResourceType)ResourceType::eFence;
                m_compute->setDebugName(&r, (std::string(debugName) + "_semaphore").c_str());
            }
            {
                const VkCommandPoolCreateInfo createInfo = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue->family };
                VK_CHECK_RV(m_ddt.CreateCommandPool(m_device, &createInfo, nullptr, &m_allocator[i]));
                sl::Resource r;
                r.native = m_allocator[i];
                r.type = (ResourceType)ResourceType::eCommandPool;
                m_compute->setDebugName(&r, (std::string(debugName) + "_command_pool").c_str());
            }
            {
                const VkCommandBufferAllocateInfo allocInfo =
                {
                    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr,
                    m_allocator[i], VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1
                };
                VK_CHECK_RV(m_ddt.AllocateCommandBuffers(m_device, &allocInfo, &m_cmdBuffer[i]));
                sl::Resource r;
                r.native = m_cmdBuffer[i];
                r.type = (ResourceType)ResourceType::eCommandBuffer;
                m_compute->setDebugName(&r, (std::string(debugName) + "_command_buffer").c_str());
            }
        }
        
    }

    void shutdown()
    {
        m_ddt.DestroySemaphore(m_device, m_presentSemaphore, nullptr);
        m_ddt.DestroySemaphore(m_device, m_acquireSemaphore, nullptr);
        for (uint32_t i = 0; i < 2 * m_bufferCount; i++)
        {
            m_ddt.FreeCommandBuffers(m_device, m_allocator[i], 1, &m_cmdBuffer[i]);
            m_ddt.DestroyCommandPool(m_device, m_allocator[i], nullptr);
            m_ddt.DestroySemaphore(m_device, m_fence[i], nullptr);
        }
        m_cmdBuffer.clear();
        m_allocator.clear();
        m_fence.clear();
    }

    RenderAPI getType() { return RenderAPI::eVulkan; }

    CommandList getCmdList()
    {
        return m_cmdBuffer[m_index];
    }

    CommandQueue getCmdQueue()
    {
        return m_cmdQueue;
    }

    CommandAllocator getCmdAllocator()
    {
        return m_allocator[m_index];
    }

    Handle getFenceEvent()
    {
        return nullptr;
    }

    Fence getFence(uint32_t index)
    {
        return m_fence[index];
    }

    bool beginCommandList()
    {
        if (m_cmdListIsRecording)
        {
            return true;
        }

        auto idx = m_index;
        auto syncValue = m_fenceValue[m_index];
        
        uint64_t completedValue;
        VK_CHECK_RF(m_ddt.GetSemaphoreCounterValue(m_device, m_fence[idx], &completedValue));
        if (completedValue < syncValue)
        {
            VkSemaphoreWaitInfo waitInfo;
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            waitInfo.pNext = NULL;
            waitInfo.flags = 0;
            waitInfo.semaphoreCount = 1;
            waitInfo.pSemaphores = &m_fence[idx];
            waitInfo.pValues = &syncValue;
            VK_CHECK_RF(m_ddt.WaitSemaphores(m_device, &waitInfo, kMaxSemaphoreWaitUs));
        }

        // One time usage since we wait for the last workload to finish
        const VkCommandBufferBeginInfo info =
        {
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
        };
        m_cmdListIsRecording = VK_SUCCESS == m_ddt.BeginCommandBuffer(m_cmdBuffer[idx], &info);

        return m_cmdListIsRecording;
    }

    bool executeCommandList(const GPUSyncInfo* info)
    {
        if (!m_cmdListIsRecording)
        {
            return false;
        }

        // Helps with crash dumps if we lose device below by allowing correct execution of the begin/end command buffer logic
        m_cmdListIsRecording = false;

        VK_CHECK_RF(m_ddt.EndCommandBuffer(m_cmdBuffer[m_index]));

        auto idx = m_index;
        uint64_t syncValue = m_fenceValue[m_index] + 1;
        m_fenceValue[m_index] = syncValue;
        m_lastIndex = m_index;
        m_index = (m_index + 1) % m_bufferCount;

        std::vector<chi::Fence> waitSemaphores;
        std::vector<chi::Fence> signalSemaphores = { m_fence[idx] };
        std::vector<uint64_t> waitValues;
        std::vector<uint64_t> signalValues = { syncValue };
        if (info)
        {
            waitSemaphores.insert(waitSemaphores.end(), info->waitSemaphores.begin(), info->waitSemaphores.end());
            signalSemaphores.insert(signalSemaphores.end(), info->signalSemaphores.begin(), info->signalSemaphores.end());
            signalValues.insert(signalValues.end(), info->signalValues.begin(), info->signalValues.end());
            waitValues.insert(waitValues.end(), info->waitValues.begin(), info->waitValues.end());
            if (info->signalPresentSemaphore)
            {
                signalSemaphores.insert(signalSemaphores.end(), m_presentSemaphore);
                signalValues.insert(signalValues.end(), kBinarySemaphoreValue); // Must provide value although this is binary semaphore
            }
        }

        VkTimelineSemaphoreSubmitInfo timelineInfo;
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = NULL;
        timelineInfo.waitSemaphoreValueCount = (uint32_t)waitValues.size();
        timelineInfo.pWaitSemaphoreValues = waitValues.data();
        timelineInfo.signalSemaphoreValueCount = (uint32_t)signalValues.size();
        timelineInfo.pSignalSemaphoreValues = signalValues.data();

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
        submitInfo.pWaitSemaphores = (VkSemaphore*)waitSemaphores.data();
        submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
        submitInfo.pSignalSemaphores = (VkSemaphore*)signalSemaphores.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_cmdBuffer[idx];
        submitInfo.pWaitDstStageMask = waitDstStageMask;
        VK_CHECK_RF(m_ddt.QueueSubmit(m_cmdQueue, 1, &submitInfo, info ? (VkFence)info->fence : nullptr));

        //SL_LOG_INFO("Submitting on %S index %u value %llu", name.c_str(), index, syncValue);

        return true;
    }

    WaitStatus flushAll()
    {
        // Wait for the last signaled value to complete on all semaphores
        VkSemaphoreWaitInfo waitInfo;
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.pNext = NULL;
        waitInfo.flags = 0;
        waitInfo.semaphoreCount = m_clCount;
        waitInfo.pSemaphores = &m_fence[0];
        waitInfo.pValues = &m_fenceValue[0];
        VK_CHECK_RWS(m_ddt.WaitSemaphores(m_device, &waitInfo, kMaxSemaphoreWaitUs));
       
        return WaitStatus::eNoTimeout;
    }

    uint32_t getBufferCount()
    {
        return m_bufferCount;
    }

    uint32_t getCurrentCommandListIndex()
    {
        return m_index;
    }

    bool isCommandListRecording()
    {
        return m_cmdListIsRecording;
    }

    uint64_t getSyncValueAtIndex(uint32_t idx)
    {
        return m_fenceValue[idx];
    }
    
    SyncPoint getNextSyncPoint()
    {
        return { m_fence[m_index], m_fenceValue[m_index] + 1 };
    }

    bool signalAllWaitingOnQueues()
    {
        std::lock_guard<std::mutex> lock(m_mtxQueueList);
        for (auto& other : m_waitingQueue)
        {
            // We are waiting on GPU for these queues, signal them to get out of the deadlock
            uint64_t completedValue{};
            VK_CHECK_RF(m_ddt.GetSemaphoreCounterValue(m_device, other.fence, &completedValue));
            
            // Desperate times desperate measures, make sure to signal new value
            auto syncValue = other.value;
            while (completedValue >= syncValue)
            {
                syncValue++;
            }
            
            VkSemaphoreSignalInfo info{};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
            info.semaphore = other.fence;
            info.value = other.value;
            VK_CHECK_RF(m_ddt.SignalSemaphore(m_device, &info));
            //SL_LOG_INFO("Signaled %S index %u value %llu", other.ctx->name.c_str(), other.clIndex, other.syncValue);
        }
        m_waitingQueue.clear();
        return true;
    }

    WaitStatus waitForCommandListToFinish(uint32_t i)
    {
        if (!didCommandListFinish(i))
        {
            VkSemaphoreWaitInfo waitInfo;
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            waitInfo.pNext = NULL;
            waitInfo.flags = 0;
            waitInfo.semaphoreCount = 1;
            waitInfo.pSemaphores = &m_fence[i];
            waitInfo.pValues = &m_fenceValue[i];
            VK_CHECK_RWS(m_ddt.WaitSemaphores(m_device, &waitInfo, kMaxSemaphoreWaitUs));
        }
        //SL_LOG_INFO("Flushing on %S index %u value %llu", name.c_str(), i, fenceValue[i]);
        return WaitStatus::eNoTimeout;
    }

    bool didCommandListFinish(uint32_t index)
    {
        uint64_t completedValue;
        VK_CHECK_RF(m_ddt.GetSemaphoreCounterValue(m_device, m_fence[index], &completedValue));
        return completedValue >= m_fenceValue[m_index];
    }

    WaitStatus waitCPUFence(Fence fence, uint64_t syncValue)
    {
        auto semaphore = (VkSemaphore)fence;
        uint64_t completedValue;
        VK_CHECK_RWS(m_ddt.GetSemaphoreCounterValue(m_device, semaphore, &completedValue));
        if (completedValue < syncValue)
        {
            VkSemaphoreWaitInfo waitInfo;
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            waitInfo.pNext = NULL;
            waitInfo.flags = 0;
            waitInfo.semaphoreCount = 1;
            waitInfo.pSemaphores = &semaphore;
            waitInfo.pValues = &syncValue;
            VK_CHECK_RWS(m_ddt.WaitSemaphores(m_device, &waitInfo, kMaxSemaphoreWaitUs));
        }
        return WaitStatus::eNoTimeout;
    }

    void syncGPU(const GPUSyncInfo* info)
    {
        // IMPORTANT: When using Vulkan we cannot submit null command buffer and expect it to 
        // wait on semaphore, dummy command buffer is required!
        // 
        // Hack due to bug in the driver 3869204, open close empty cmd buffer but keep doing
        // N-buffering to avoid reusing same empty cmd buffer for multiple wait requests.

        std::vector<chi::Fence> waitSemaphores;
        std::vector<uint64_t> waitValues;
        std::vector<chi::Fence> signalSemaphores;
        std::vector<uint64_t> signalValues;

        if (info->useEmptyCmdBuffer)
        {
            // Note that we are using upper N cmd buffers as empty hence + bufferCount
            m_emptyIndex = ((m_emptyIndex + 1) % m_bufferCount + m_bufferCount);
            uint64_t completedValue;
            VK_CHECK_RV(m_ddt.GetSemaphoreCounterValue(m_device, m_fence[m_emptyIndex], &completedValue));
            if (completedValue < m_fenceValue[m_emptyIndex])
            {
                VkSemaphoreWaitInfo waitInfo;
                waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
                waitInfo.pNext = NULL;
                waitInfo.flags = 0;
                waitInfo.semaphoreCount = 1;
                waitInfo.pSemaphores = &m_fence[m_emptyIndex];
                waitInfo.pValues = &m_fenceValue[m_emptyIndex];
                VK_CHECK_RV(m_ddt.WaitSemaphores(m_device, &waitInfo, kMaxSemaphoreWaitUs));
            }

            auto signalFence = m_fence[m_emptyIndex];
            auto signalFenceValue = ++m_fenceValue[m_emptyIndex];

            const VkCommandBufferBeginInfo cmdBufferInfo =
            {
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr,
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, nullptr
            };
            VK_CHECK_RV(m_ddt.BeginCommandBuffer(m_cmdBuffer[m_emptyIndex], &cmdBufferInfo));
            VK_CHECK_RV(m_ddt.EndCommandBuffer(m_cmdBuffer[m_emptyIndex]));

            // Our "empty" signal
            signalSemaphores.push_back(signalFence);
            signalValues.push_back(signalFenceValue);
        }

        // External semaphores (if any)
        if (info)
        {
            waitSemaphores.insert(waitSemaphores.end(), info->waitSemaphores.begin(), info->waitSemaphores.end());
            waitValues.insert(waitValues.end(), info->waitValues.begin(), info->waitValues.end());
        }
        if(info)
        {
            signalSemaphores.insert(signalSemaphores.end(), info->signalSemaphores.begin(), info->signalSemaphores.end());
            signalValues.insert(signalValues.end(), info->signalValues.begin(), info->signalValues.end());
        }

        VkTimelineSemaphoreSubmitInfo timelineInfo{};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = NULL;
        timelineInfo.waitSemaphoreValueCount = (uint32_t)waitValues.size();
        timelineInfo.pWaitSemaphoreValues = waitValues.data();
        timelineInfo.signalSemaphoreValueCount = (uint32_t)signalValues.size();
        timelineInfo.pSignalSemaphoreValues = signalValues.data();

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
        submitInfo.pWaitSemaphores = (VkSemaphore*)waitSemaphores.data();
        submitInfo.signalSemaphoreCount = (uint32_t)signalSemaphores.size();
        submitInfo.pSignalSemaphores = (VkSemaphore*)signalSemaphores.data();
        if (info->useEmptyCmdBuffer)
        {
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_cmdBuffer[m_emptyIndex];
        }
        submitInfo.pWaitDstStageMask = waitDstStageMask;
        VK_CHECK_RV(m_ddt.QueueSubmit(m_cmdQueue, 1, &submitInfo, nullptr));
    }

    void signalGPUFenceAt(uint32_t index)
    {
        signalGPUFence(m_fence[index], ++m_fenceValue[m_index]);
    }

    void signalGPUFence(Fence fence, uint64_t syncValue)
    {
        GPUSyncInfo info;
        info.signalSemaphores = { (VkSemaphore)fence };
        info.signalValues = { syncValue };
        syncGPU(&info);
    }

    void waitGPUFence(Fence fence, uint64_t syncValue)
    {
        GPUSyncInfo info;
        info.waitSemaphores = { (VkSemaphore)fence };
        info.waitValues = { syncValue };
        syncGPU(&info);
    }

    void waitOnGPUForTheOtherQueue(const ICommandListContext* other, uint32_t clIndex, uint64_t syncValue)
    {
        auto tmp = (const CommandListContextVK*)other;
        if (!tmp || tmp->m_cmdQueue == m_cmdQueue) return;

        VkSemaphore waitFence = tmp->m_fence[clIndex];
        uint64_t waitFenceValue = syncValue;

        GPUSyncInfo info;
        info.waitSemaphores = { waitFence };
        info.waitValues = { waitFenceValue };
        syncGPU(&info);

        // Store sync data
        std::lock_guard<std::mutex> lock(m_mtxQueueList);
        bool found = false;
        for (auto& other : m_waitingQueue)
        {
            if (other.fence == waitFence)
            {
                found = true;
                other.fence = waitFence;
                other.value = waitFenceValue;
                break;
            }
        }
        if (!found)
        {
            m_waitingQueue.push_back({ waitFence, waitFenceValue });
        }

    }

    WaitStatus waitForCommandList(FlushType ft)
    {
        // Flush command list, to avoid it still referencing resources that may be destroyed after this call
        if (m_cmdListIsRecording)
        {
            executeCommandList(nullptr);
        }
        
        if (ft == eCurrent)
        {
            VkSemaphoreWaitInfo waitInfo;
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            waitInfo.pNext = NULL;
            waitInfo.flags = 0;
            waitInfo.semaphoreCount = 1;
            waitInfo.pSemaphores = &m_fence[m_lastIndex];
            waitInfo.pValues = &m_fenceValue[m_lastIndex];
            VK_CHECK_RWS(m_ddt.WaitSemaphores(m_device, &waitInfo, kMaxSemaphoreWaitUs));
            //SL_LOG_INFO("Flush current %S index %u value %llu", name.c_str(), index, syncValue);
        }
        else if (ft == eDefault)
        {
            // Default, wait for previous frame at this index (N frames behind to finish)
            auto syncValue = m_fenceValue[m_lastIndex] - 1;
            VkSemaphoreWaitInfo waitInfo;
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            waitInfo.pNext = NULL;
            waitInfo.flags = 0;
            waitInfo.semaphoreCount = 1;
            waitInfo.pSemaphores = &m_fence[m_lastIndex];
            waitInfo.pValues = &syncValue;
            VK_CHECK_RWS(m_ddt.WaitSemaphores(m_device, &waitInfo, kMaxSemaphoreWaitUs));
        }

        return WaitStatus::eNoTimeout;
    }

    int acquireNextBufferIndex(SwapChain chain, uint32_t& bufferIndex, Fence* waitSemaphore)
    {
        SwapChainVk* sc = (SwapChainVk*)chain;
        const uint64_t timeout = 10 * 1000;
        bufferIndex = UINT32_MAX;
        // With VK it is important to always return the "error" code
        int res{};
        VK_CHECK_RE(res, m_ddt.AcquireNextImageKHR(m_device, (VkSwapchainKHR)sc->native, timeout, m_acquireSemaphore, nullptr, &bufferIndex));
        m_bufferToPresent = bufferIndex;
        if (waitSemaphore)
        {
            *waitSemaphore = m_acquireSemaphore;
        }
        return res;
    }

    int present(SwapChain chain, uint32_t sync, uint32_t flags, void* params)
    {
        SwapChainVk* sc = (SwapChainVk*)chain;
        const VkPresentInfoKHR info = {
        VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        nullptr,
        // Cannot wait here on present semaphore (however acquires next image has to wait before doing a copy to back buffer)
        1,
        &m_presentSemaphore, 
        1,
        &(VkSwapchainKHR)sc->native,
        &m_bufferToPresent, nullptr
        };
        // With VK it is important to always return the "error" code
        int res{};
        VK_CHECK_RE(res, m_ddt.QueuePresentKHR(m_cmdQueue, &info));
        return res;
    }
};


inline VkImageLayout toVkImageLayout(ResourceState state)
{
    switch (state)
    {
        case ResourceState::eUndefined: return VK_IMAGE_LAYOUT_UNDEFINED;
        case ResourceState::eGeneral: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eVertexBuffer: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eIndexBuffer: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eConstantBuffer: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eArgumentBuffer: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eTextureRead: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ResourceState::eStorageRead: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eStorageWrite: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eStorageRW: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eColorAttachmentRead: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ResourceState::eColorAttachmentWrite: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ResourceState::eDepthStencilAttachmentWrite: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eDepthStencilAttachmentRead: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ResourceState::eCopySource: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ResourceState::eCopyDestination: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case ResourceState::eAccelStructRead: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eAccelStructWrite: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eResolveSource: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::eResolveDestination: return VK_IMAGE_LAYOUT_GENERAL;
        case ResourceState::ePresent: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    };
    return VK_IMAGE_LAYOUT_GENERAL;
};

VkAccessFlags toVkAccessFlags(ResourceState state)
{
    VkAccessFlags flags = 0;

    switch (state)
    {
    case ResourceState::eVertexBuffer: return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    case ResourceState::eIndexBuffer: return VK_ACCESS_INDEX_READ_BIT;
    case ResourceState::eConstantBuffer: return VK_ACCESS_UNIFORM_READ_BIT;
    case ResourceState::eArgumentBuffer: return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    case ResourceState::eTextureRead: return VK_ACCESS_SHADER_READ_BIT;
    case ResourceState::eStorageRead: return VK_ACCESS_SHADER_READ_BIT;
    case ResourceState::eStorageWrite: return VK_ACCESS_SHADER_WRITE_BIT;
    case ResourceState::eStorageRW: return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case ResourceState::eColorAttachmentRead: return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    case ResourceState::eColorAttachmentWrite: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case ResourceState::eDepthStencilAttachmentWrite: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case ResourceState::eDepthStencilAttachmentRead: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    case ResourceState::eCopySource: return VK_ACCESS_TRANSFER_READ_BIT;
    case ResourceState::eCopyDestination: return VK_ACCESS_TRANSFER_WRITE_BIT;
    case ResourceState::eAccelStructRead: return VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    case ResourceState::eAccelStructWrite: return VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    case ResourceState::eResolveSource: return VK_ACCESS_TRANSFER_READ_BIT;
    case ResourceState::eResolveDestination: return VK_ACCESS_TRANSFER_WRITE_BIT;
    };
    return flags;
}

ComputeStatus Vulkan::getResourceState(uint32_t states, ResourceState& resourceStates)
{
    resourceStates = ResourceState::eUndefined;
    
    if(states & VK_IMAGE_LAYOUT_GENERAL) resourceStates = ResourceState::eGeneral;
    if(states & VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) resourceStates |= ResourceState::eTextureRead;
    if(states & VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) resourceStates |= ResourceState::eColorAttachmentWrite;
    if(states & VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL) resourceStates |= ResourceState::eDepthStencilAttachmentRead;
    if(states & VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) resourceStates |= ResourceState::eCopySource;
    if(states & VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) resourceStates |= ResourceState::eCopyDestination;
    if(states & VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) resourceStates |= ResourceState::ePresent;
    
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::getNativeResourceState(ResourceState states, uint32_t& resourceStates)
{
    resourceStates = VK_IMAGE_LAYOUT_UNDEFINED;

    if (states & ResourceState::eGeneral) resourceStates = VK_IMAGE_LAYOUT_GENERAL;
    if (states & ResourceState::eTextureRead) resourceStates |= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (states & ResourceState::eColorAttachmentRead) resourceStates |= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (states & ResourceState::eColorAttachmentWrite) resourceStates |= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (states & ResourceState::eDepthStencilAttachmentRead) resourceStates |= VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    if (states & ResourceState::eCopySource) resourceStates |= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if (states & ResourceState::eCopyDestination) resourceStates |= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    if (states & ResourceState::ePresent) resourceStates |= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    return ComputeStatus::eOk;
}

VkImageUsageFlags toVkImageUsageFlags(ResourceFlags usageFlags)
{
    VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (usageFlags & ResourceFlags::eShaderResource)
    {
        flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (usageFlags & ResourceFlags::eShaderResourceStorage)
    {
        flags |= VK_IMAGE_USAGE_STORAGE_BIT;
    }
    if (usageFlags & ResourceFlags::eColorAttachment)
    {
        flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }
    if (usageFlags & ResourceFlags::eDepthStencilAttachment)
    {
        flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    return flags;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {

    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {

    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        //std::string s(pCallbackData->pMessage);
        //std::replace(s.begin(), s.end(), '%', ' ');
        //SL_LOG_WARN(s.c_str());
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        
        //assert(0x1608dec0 != pCallbackData->messageIdNumber);

        // 0x3936bc0c - barrier validation - cannot issue barrier within renderpass
        // 0xd3c27d87 - barrier validation - cannot issue clear image within renderpass
        // 0x1608dec0 - donut error cmdDraw
        std::vector<uint32_t> s_disable = { 0x3936bc0c, 0xd3c27d87, 0x1608dec0, 0xb50452b0, 0x1e8b83b0, 0xe825f293, 0x3cf4c632, 0x15559cd5 };
        if (std::find(s_disable.begin(), s_disable.end(), pCallbackData->messageIdNumber) == s_disable.end())
        {
            SL_LOG_ERROR( pCallbackData->pMessage);
        }
    }

    // The return value of this callback controls whether the Vulkan call that caused the validation message will be aborted or not
    // We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message to abort
    // If you instead want to have calls abort, pass in VK_TRUE and the function will return VK_ERROR_VALIDATION_FAILED_EXT 
    return VK_FALSE;
}

ComputeStatus Vulkan::init(Device device, param::IParameters* params)
{
    void ** deviceArray = (void**)device;

    m_instance = (VkInstance)deviceArray[0];
    m_device = (VkDevice)deviceArray[1];
    m_physicalDevice = (VkPhysicalDevice)deviceArray[2];

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
    
    // For callbacks we just need VkDevice
    Generic::init(m_device, params);

    interposer::VkTable* vk{};
    if (!param::getPointerParam(m_parameters, sl::param::global::kVulkanTable, &vk))
    {
        return ComputeStatus::eNoImplementation;
    }

    m_vk = new interposer::VkTable;
    m_vk->getInstanceProcAddr = vk->getInstanceProcAddr;
    m_vk->getDeviceProcAddr = vk->getDeviceProcAddr;
    m_vk->computeQueueFamily = vk->computeQueueFamily;
    m_vk->computeQueueIndex = vk->computeQueueIndex;
    m_vk->graphicsQueueFamily = vk->graphicsQueueFamily;
    m_vk->graphicsQueueIndex = vk->graphicsQueueIndex;
    m_vk->mapVulkanInstanceAPI(m_instance);
    m_vk->mapVulkanDeviceAPI(m_device);
    m_ddt = m_vk->dispatchDeviceMap[m_device];
    m_idt = m_vk->dispatchInstanceMap[m_instance];

    if(m_idt.CreateDebugUtilsMessengerEXT)
    {
        // The report flags determine what type of messages for the layers will be displayed
        // For validating (debugging) an application the error and warning bits should suffice
        VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
            
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
        debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessengerCallback;
        m_idt.CreateDebugUtilsMessengerEXT(m_instance, &debugUtilsMessengerCI, nullptr, &m_debugUtilsMessenger);
    }

    VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.mipLodBias = 0.0;
    samplerCreateInfo.maxAnisotropy = 1;
    samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
    samplerCreateInfo.minLod = 0.0; // must be 0 when unnormalized
    samplerCreateInfo.maxLod = 0.0;
    samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    VkResult result;
    {
        result = m_ddt.CreateSampler(m_device, &samplerCreateInfo, 0, &m_sampler[eSamplerLinearClamp]);
        assert(result == VK_SUCCESS);
    }
    {
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        result = m_ddt.CreateSampler(m_device, &samplerCreateInfo, 0, &m_sampler[eSamplerLinearMirror]);
        assert(result == VK_SUCCESS);
    }
    {
        samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
        samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
        result = m_ddt.CreateSampler(m_device, &samplerCreateInfo, 0, &m_sampler[eSamplerPointMirror]);
        assert(result == VK_SUCCESS);
    }
    {
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        result = m_ddt.CreateSampler(m_device, &samplerCreateInfo, 0, &m_sampler[eSamplerPointClamp]);
        assert(result == VK_SUCCESS);
    }
    m_idt.GetPhysicalDeviceMemoryProperties(m_physicalDevice, &m_vkPhysicalDeviceMemoryProperties);

    // Create the descriptor pool, layout, and set for image view clears
    VkDescriptorSetLayoutBinding bindings[2] = { };
    bindings[0].binding = 0;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

    VkDescriptorSetLayoutCreateInfo dslInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    dslInfo.bindingCount = 1;
    dslInfo.pBindings = bindings;
    dslInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;

    result = m_ddt.CreateDescriptorSetLayout(m_device, &dslInfo, 0, &m_imageViewClear.descriptorSetLayout);
    if (result != VK_SUCCESS) {
        return ComputeStatus::eError;
    }

    VkPushConstantRange range;
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset = 0;
    range.size = (4+4) * 4;

    VkPipelineLayoutCreateInfo plInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &m_imageViewClear.descriptorSetLayout;
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &range;

    result = m_ddt.CreatePipelineLayout(m_device, &plInfo, 0, &m_imageViewClear.pipelineLayout);
    if (result != VK_SUCCESS) {
        return ComputeStatus::eError;
    }

    // Create the compute pipeline for image view clears
    VkShaderModule csm;

    VkShaderModuleCreateInfo shaderInfo = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    shaderInfo.codeSize = vulkan_clear_image_view_comp_spv_len;
    shaderInfo.pCode = (const uint32_t *)&vulkan_clear_image_view_comp_spv[0];

    result = m_ddt.CreateShaderModule(m_device, &shaderInfo, nullptr, &csm);
    if (result != VK_SUCCESS) {
        return ComputeStatus::eError;
    }

    VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipelineInfo.layout = m_imageViewClear.pipelineLayout;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = csm;
    pipelineInfo.stage.pName = "main";
    result = m_ddt.CreateComputePipelines(m_device, nullptr, 1, &pipelineInfo, 0, &m_imageViewClear.doClear);
    if (result != VK_SUCCESS) {
        return ComputeStatus::eError;
    }

    m_ddt.DestroyShaderModule(m_device, csm, nullptr);

    // all Vulkan drivers are expected to support ZBC clear without padding
    m_bFastUAVClearSupported = true;

    genericPostInit();

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::shutdown()
{
    NvLL_VK_DestroyLowLatencyDevice(m_device);
    NvLL_VK_Unload();

    if (m_hmodReflex)
    {
        FreeLibrary(m_hmodReflex);
        m_hmodReflex = {};
    }

    if (m_idt.DestroyDebugUtilsMessengerEXT && m_debugUtilsMessenger)
    {
        m_idt.DestroyDebugUtilsMessengerEXT(m_instance, m_debugUtilsMessenger, nullptr);
    }

    // cleanup samplers
    for (uint32_t u = 0; u < countof(m_sampler); ++u)
    {
        if (m_sampler[u])
        {
            m_ddt.DestroySampler(m_device, m_sampler[u], nullptr);
            m_sampler[u] = nullptr;
        }
    }

    // Clean up image view clear
    m_ddt.DestroyDescriptorSetLayout(m_device, m_imageViewClear.descriptorSetLayout, nullptr);
    m_ddt.DestroyPipelineLayout(m_device, m_imageViewClear.pipelineLayout, nullptr);
    m_ddt.DestroyPipeline(m_device, m_imageViewClear.doClear, nullptr);

    Generic::shutdown();

    for (auto Cubin = m_kernels.begin(); Cubin != m_kernels.end(); Cubin++)
    {
        KernelDataVK *cubinVk = (KernelDataVK *)(*Cubin).second;
        if (cubinVk->pipeline)
        {
            m_ddt.DestroyPipeline(m_device, cubinVk->pipeline, nullptr);
            m_ddt.DestroyPipelineLayout(m_device, cubinVk->pipelineLayout, nullptr);
            m_ddt.DestroyDescriptorSetLayout(m_device, cubinVk->descriptorSetLayout, nullptr);
            m_ddt.DestroyShaderModule(m_device, cubinVk->shaderModule, nullptr);
        }
        else
        {
            //m_ddt.DestroyCuFunctionNVX(m_device, cubinVk->Function, nullptr);
            //m_ddt.DestroyCuModuleNVX(m_device, cubinVk->Shader, nullptr);
        }
        delete (*Cubin).second;
    }
    m_kernels.clear();

    delete m_vk;
    m_vk = {};

    return Generic::shutdown();
}

ComputeStatus Vulkan::getRenderAPI(RenderAPI &OutType)
{
    OutType = RenderAPI::eVulkan;
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::getVendorId(VendorId& id)
{
    VkPhysicalDeviceIDProperties physicalDeviceIDProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES };
    VkPhysicalDeviceProperties2 physicalDeviceProperties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &physicalDeviceIDProperties };
    m_idt.GetPhysicalDeviceProperties2(m_physicalDevice, &physicalDeviceProperties2);

    id = (VendorId)physicalDeviceProperties2.properties.vendorID;

    return ComputeStatus::eError;
}

ComputeStatus Vulkan::restorePipeline(CommandList cmdList)
{
    if (!cmdList) return ComputeStatus::eOk;

    VulkanThreadContext* thread = (VulkanThreadContext*)m_getThreadContext();

    if (thread->PipelineBindPoint != VK_PIPELINE_BIND_POINT_MAX_ENUM)
    {
        m_ddt.CmdBindPipeline((VkCommandBuffer)cmdList, thread->PipelineBindPoint, thread->Pipeline);
    }
    if (thread->PipelineBindPointDesc != VK_PIPELINE_BIND_POINT_MAX_ENUM)
    {
        m_ddt.CmdBindDescriptorSets((VkCommandBuffer)cmdList, thread->PipelineBindPointDesc, thread->Layout, thread->FirstSet, thread->DescriptorCount, thread->DescriptorSets, thread->DynamicOffsetCount, thread->DynamicOffsets);
    }
    
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::createKernel(void *blob, unsigned int blobSize, const char* fileName, const char *entryPoint, Kernel &kernel)
{
    if (!blob || !fileName || !entryPoint)
    {
        return ComputeStatus::eInvalidArgument;
    }

    size_t hash = 0;
    const char* p = fileName;
    while (*p)
    {
        hash_combine(hash, *p++);
    }
    p = entryPoint;
    while (*p)
    {
        hash_combine(hash, *p++);
    }
    auto i = blobSize;
    while (i--)
    {
        hash_combine(hash, ((char*)blob)[i]);
    }

    ComputeStatus Res = ComputeStatus::eOk;
    KernelDataVK *data = {};
    bool missing = false;
    {
        std::scoped_lock lock(m_mutexKernel);
        auto it = m_kernels.find(hash);
        missing = it == m_kernels.end();
        if (missing)
        {
            data = new KernelDataVK{};
            data->hash = hash;
            m_kernels[hash] = data;
        }
    }
    if (missing)
    {
        data->name = fileName;
        data->entryPoint = entryPoint;
        constexpr uint32_t kSPIRVMagicNumber = 0x07230203;
        uint32_t header = *(uint32_t*)blob;
        if (header == kSPIRVMagicNumber)
        {
            data->kernelBlob.resize(blobSize);
            memcpy(data->kernelBlob.data(), blob, blobSize);
            SL_LOG_VERBOSE("Creating SPIR-V kernel %s:%s hash %llu", fileName, entryPoint, hash);
                        
            VkShaderModuleCreateInfo moduleCreateInfo{};
            moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            moduleCreateInfo.codeSize = blobSize;
            moduleCreateInfo.pCode = (uint32_t*)blob;
            VK_CHECK(m_ddt.CreateShaderModule(m_device, &moduleCreateInfo, NULL, &data->shaderModule));
        }
        else
        {
            SL_LOG_ERROR( "Unsupported kernel blob");
            assert(false);
        }
    }
    kernel = hash;
    return Res;
}

ComputeStatus Vulkan::destroyKernel(Kernel& kernel)
{
    if (!kernel) return ComputeStatus::eOk;
    std::scoped_lock lock(m_mutexKernel);
    auto cubin = m_kernels.find(kernel);
    if (cubin == m_kernels.end())
    {
        return ComputeStatus::eInvalidCall;
    }

    KernelDataVK *cubinVk = (KernelDataVK *)(*cubin).second;
    if (cubinVk->shaderModule)
    {
        m_ddt.DestroyShaderModule(m_device, cubinVk->shaderModule, nullptr);
    }
    
    delete (*cubin).second;
    m_kernels.erase(cubin);
    kernel = {};
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::createCommandListContext(CommandQueue queue, uint32_t count, ICommandListContext*& ctx, const char friendlyName[])
{ 
    auto tmp = new CommandListContextVK();
    tmp->init(this, m_vk, friendlyName, m_device, (CommandQueueVk*)queue, count);
    ctx = tmp;
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::destroyCommandListContext(ICommandListContext* ctx)
{ 
    if (ctx)
    {
        auto tmp = (CommandListContextVK*)ctx;
        tmp->shutdown();
        delete tmp;
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::createCommandQueue(CommandQueueType type, CommandQueue& queue, const char friendlyName[], uint32_t index)
{
    queue = {};
    if (type == CommandQueueType::eCompute)
    {
        VkQueue tmp = {};
        m_ddt.GetDeviceQueue(m_device, m_vk->computeQueueFamily, m_vk->computeQueueIndex + index, &tmp);
        if (!tmp)
        {
            return ComputeStatus::eError;
        }
        queue = new chi::CommandQueueVk{ tmp, type, m_vk->computeQueueFamily, m_vk->computeQueueIndex + index };
    }
    else if (type == CommandQueueType::eGraphics)
    {
        VkQueue tmp = {};
        m_ddt.GetDeviceQueue(m_device, m_vk->graphicsQueueFamily, m_vk->graphicsQueueIndex + index, &tmp);
        if (!tmp)
        {
            return ComputeStatus::eError;
        }
        queue = new chi::CommandQueueVk{ tmp, type, m_vk->graphicsQueueFamily, m_vk->graphicsQueueIndex + index };
    }
    else
    {
        return ComputeStatus::eNoImplementation;
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::destroyCommandQueue(CommandQueue& queue)
{ 
    auto tmp = (chi::CommandQueueVk*)queue;
    delete tmp;
    return ComputeStatus::eOk; 
}

ComputeStatus Vulkan::createFence(FenceFlags flags, uint64_t initialValue, Fence& outFence, const char friendlyName[])
{
    VkSemaphoreTypeCreateInfo timelineCreateInfo;
    timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineCreateInfo.pNext = NULL;
    timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineCreateInfo.initialValue = 0;

    VkSemaphoreCreateInfo createInfo;
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = &timelineCreateInfo;
    createInfo.flags = 0;

    VkSemaphore fence;
    VK_CHECK(m_ddt.CreateSemaphore(m_device, &createInfo, NULL, &fence));

    outFence = new SemaphoreVk(fence);

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::destroyFence(Fence fence)
{
    auto semaphore = (SemaphoreVk*)fence;
    if (semaphore)
    {
        m_ddt.DestroySemaphore(m_device, (VkSemaphore)semaphore->native, NULL);
        delete semaphore;
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::bindSharedState(CommandList InCmdList, unsigned int node)
{
    m_cmdBuffer = (VkCommandBuffer)InCmdList;
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::bindKernel(const Kernel InKernel)
{
    auto& thread = m_dispatchContext.getContext();
    
    {
        std::scoped_lock lock(m_mutexKernel);
        auto it = m_kernels.find(InKernel);
        if (it == m_kernels.end())
        {
            return ComputeStatus::eInvalidCall;
        }
        thread.kernel = (KernelDataVK*)(*it).second;
    }
    
    auto it = thread.psoToSignature.find(thread.kernel->hash);
    if (it == thread.psoToSignature.end())
    {
        thread.signature = new ResourceBindingDesc;
        thread.psoToSignature[thread.kernel->hash] = thread.signature;
    }
    else
    {
        thread.signature = (*it).second;
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::bindConsts(uint32_t base, uint32_t reg, void *data, size_t dataSize, uint32_t instances)
{
    auto& thread = m_dispatchContext.getContext();
    if (!thread.kernel) return ComputeStatus::eInvalidArgument;

    if (instances < 3)
    {
        SL_LOG_WARN("Detected too low instance count for circular constant buffer - please use num_viewports * 3 formula");
    }

    // VK alignment requirement is 0x40
    auto alignedDataSize = extra::align((uint32_t)dataSize, 64U);

    // Implementation aligned with d3d12, we allocate CB_SIZE * instances buffer and access at different offset on each bind
    if (thread.signature->descriptors.find(base) != thread.signature->descriptors.end())
    {
        auto& slot = thread.signature->descriptors[base];
        assert(slot.type == DescriptorType::eConstantBuffer);
        slot.instance = (slot.instance + 1) % instances;
        uint32_t offset = slot.instance * alignedDataSize;
        memcpy((uint8_t*)slot.mapped + offset, data, dataSize);
        thread.signature->offsets[slot.offsetIndex] = offset;
    }
    else
    {
        BindingSlot slot = {};
        slot.instance = 0;
        slot.type = DescriptorType::eConstantBuffer;
        slot.registerIndex = base;
        ResourceDescription cbDesc = ResourceDescription{alignedDataSize * instances,1,chi::NativeFormatUnknown,chi::eHeapTypeUpload, chi::ResourceState::eConstantBuffer};
        Resource cb;
        CHI_CHECK(createBuffer(cbDesc, cb, "const buffer"));
        slot.handles.push_back(((sl::Resource*)cb)->native);
        slot.mapped = {};
        auto info = (sl::Resource*)cb;
        VK_CHECK(m_ddt.MapMemory(m_device, (VkDeviceMemory)info->memory, 0, cbDesc.width, 0, &slot.mapped));
        slot.dataRange = (uint32_t)dataSize;
        slot.offsetIndex = (uint32_t)thread.signature->offsets.size();
        uint32_t offset = slot.instance * alignedDataSize;
        memcpy((uint8_t*)slot.mapped + offset, data, dataSize);
        thread.signature->descriptors[base] = slot;
        thread.signature->offsets.push_back(offset);
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::bindSampler(uint32_t base, uint32_t reg, Sampler sampler)
{
    auto& thread = m_dispatchContext.getContext();
    if (!thread.kernel) return ComputeStatus::eInvalidArgument;

    if (thread.signature->descriptors.find(base) != thread.signature->descriptors.end())
    {
        auto& slot = thread.signature->descriptors[base];
        assert(slot.type == DescriptorType::eSampler);
        slot.dirty |= slot.handles.back() != m_sampler[(uint32_t)sampler];
        slot.handles.back() = m_sampler[(uint32_t)sampler];
    }
    else
    {
        BindingSlot slot = {};
        slot.type = DescriptorType::eSampler;
        slot.registerIndex = base;
        slot.handles.push_back(m_sampler[(uint32_t)sampler]);
        thread.signature->descriptors[base] = slot;
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::bindTexture(uint32_t base, uint32_t reg, Resource InResource, uint32_t mipOffset, uint32_t mipLevels)
{
    auto& thread = m_dispatchContext.getContext();
    if (!thread.kernel) return ComputeStatus::eInvalidArgument;

    auto resource = (sl::Resource*)InResource;

    if (thread.signature->descriptors.find(base) != thread.signature->descriptors.end())
    {
        auto& slot = thread.signature->descriptors[base];
        assert(slot.type == DescriptorType::eTexture);
        auto value = resource ? resource->view : nullptr;
        slot.dirty |= slot.handles.back() != value;
        slot.handles.back() = resource ? resource->view : nullptr;
    }
    else
    {
        BindingSlot slot = {};
        slot.type = DescriptorType::eTexture;
        slot.registerIndex = base;
        slot.handles.push_back(resource ? resource->view : nullptr);
        thread.signature->descriptors[base] = slot;
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::bindRWTexture(uint32_t base, uint32_t reg, Resource InResource, uint32_t mipOffset)
{
    auto& thread = m_dispatchContext.getContext();
    if (!thread.kernel) return ComputeStatus::eInvalidArgument;

    auto resource = (sl::Resource*)InResource;
    if (thread.signature->descriptors.find(base) != thread.signature->descriptors.end())
    {
        auto& slot = thread.signature->descriptors[base];
        assert(slot.type == DescriptorType::eStorageTexture);
        auto value = resource ? resource->view : nullptr;
        slot.dirty |= slot.handles.back() != value;
        slot.handles.back() = value;
    }
    else
    {
        BindingSlot slot = {};
        slot.type = DescriptorType::eStorageTexture;
        slot.registerIndex = base;
        slot.handles.push_back(resource ? resource->view : nullptr);
        thread.signature->descriptors[base] = slot;
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::bindRawBuffer(uint32_t base, uint32_t reg, Resource InResource)
{
    auto& thread = m_dispatchContext.getContext();
    if (!thread.kernel) return ComputeStatus::eInvalidArgument;

    auto resource = (sl::Resource*)InResource;

    if (thread.signature->descriptors.find(base) != thread.signature->descriptors.end())
    {
        auto& slot = thread.signature->descriptors[base];
        assert(slot.type == DescriptorType::eStorageBuffer);
        slot.dirty |= slot.handles.back() != resource->native;
        slot.handles.back() = resource->native;
    }
    else
    {
        BindingSlot slot = {};
        slot.type = DescriptorType::eStorageBuffer;
        slot.registerIndex = base;
        slot.handles.push_back(resource->native);
        thread.signature->descriptors[base] = slot;
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::processDescriptors(DispatchData& thread)
{
    if (thread.signatureToDesc.find(thread.signature) == thread.signatureToDesc.end())
    {
        std::vector<VkDescriptorSetLayoutBinding> bindings = { };
        std::vector<VkDescriptorPoolSize> poolSizes = { };
        for (auto it : thread.signature->descriptors)
        {
            auto& slot = it.second;
            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = slot.registerIndex;
            binding.descriptorCount = (uint32_t)slot.handles.size();
            binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            VkDescriptorPoolSize ps = {};
            ps.descriptorCount = (uint32_t)slot.handles.size();
            if (slot.type == DescriptorType::eStorageBuffer)
            {
                ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            }
            else if (slot.type == DescriptorType::eStorageTexture)
            {
                ps.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            }
            else if (slot.type == DescriptorType::eTexture)
            {
                ps.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            }
            else if (slot.type == DescriptorType::eSampler)
            {
                ps.type = VK_DESCRIPTOR_TYPE_SAMPLER;
                binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            }
            else if (slot.type == DescriptorType::eConstantBuffer)
            {
                ps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
            }
            bindings.push_back(binding);
            poolSizes.push_back(ps);
        }

        // This is not per thread and can be reused
        if (!thread.kernel->pipelineLayout)
        {
            assert(!thread.kernel->descriptorSetLayout);

            VkDescriptorSetLayoutCreateInfo dslInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            dslInfo.bindingCount = (uint32_t)bindings.size();
            dslInfo.pBindings = bindings.data();
            //dslInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
            VK_CHECK(m_ddt.CreateDescriptorSetLayout(m_device, &dslInfo, 0, &thread.kernel->descriptorSetLayout));

            VkPipelineLayoutCreateInfo plInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
            plInfo.setLayoutCount = 1;
            plInfo.pSetLayouts = &thread.kernel->descriptorSetLayout;
            plInfo.pushConstantRangeCount = 0;
            plInfo.pPushConstantRanges = {};
            VK_CHECK(m_ddt.CreatePipelineLayout(m_device, &plInfo, 0, &thread.kernel->pipelineLayout));
        }

        VkDescriptorPoolCreateInfo descriptorPoolInfo =
        {
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            nullptr,
            (VkDescriptorPoolCreateFlags)0,
            (uint32_t)thread.kernel->numDescriptors,
            (uint32_t)(poolSizes.size()),
            poolSizes.data()
        };
        PoolDescCombo& combo = thread.signatureToDesc[thread.signature];
        VK_CHECK(m_ddt.CreateDescriptorPool(m_device, &descriptorPoolInfo, nullptr, &combo.pool));
        combo.desc.resize(thread.kernel->numDescriptors);
        for (uint32_t i = 0; i < thread.kernel->numDescriptors; i++)
        {
            VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO , nullptr, combo.pool, 1, &thread.kernel->descriptorSetLayout };
            VK_CHECK(m_ddt.AllocateDescriptorSets(m_device, &allocInfo, &combo.desc[i]));
        }
    }

    auto writeBufferDescriptorSet = [](VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorBufferInfo* bufferInfo, uint32_t descriptorCount = 1)->VkWriteDescriptorSet
    {
        VkWriteDescriptorSet descSet{};
        descSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descSet.dstSet = dstSet;
        descSet.descriptorType = type;
        descSet.dstBinding = binding;
        descSet.pBufferInfo = bufferInfo;
        descSet.descriptorCount = descriptorCount;
        return descSet;
    };

    auto writeImageDescriptorSet = [](VkDescriptorSet dstSet, VkDescriptorType type, uint32_t binding, VkDescriptorImageInfo* imageInfo, uint32_t descriptorCount = 1)->VkWriteDescriptorSet
    {
        VkWriteDescriptorSet descSet{};
        descSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descSet.dstSet = dstSet;
        descSet.descriptorType = type;
        descSet.dstBinding = binding;
        descSet.pImageInfo = imageInfo;
        descSet.descriptorCount = descriptorCount;
        return descSet;
    };

    auto& combo = thread.signatureToDesc[thread.signature];
    {
        std::vector<VkWriteDescriptorSet> writeDescriptorSets = {};
        std::vector<VkDescriptorBufferInfo> buffers[16];
        std::vector<VkDescriptorImageInfo> images[16];
        bool needsUpdate = false;
        for (auto& it : thread.signature->descriptors)
        {
            auto& slot = it.second;
            assert(slot.registerIndex < 16);
            needsUpdate |= slot.dirty;
        }
        if (needsUpdate)
        {
            thread.kernel->descriptorIndex = (thread.kernel->descriptorIndex + 1) % thread.kernel->numDescriptors;
            auto index = thread.kernel->descriptorIndex;
            for (auto& it : thread.signature->descriptors)
            {
                auto& slot = it.second;
                if (slot.type == DescriptorType::eStorageBuffer)
                {
                    for (auto& h : slot.handles)
                    {
                        VkDescriptorBufferInfo info = {};
                        auto buffer = (VkBuffer)h;
                        if (buffer)
                        {
                            info =
                            {
                                buffer,                           // VkBuffer        buffer;
                                0,                                // VkDeviceSize    offset;
                                VK_WHOLE_SIZE                     // VkDeviceSize    range;
                            };
                        }
                        buffers[slot.registerIndex].push_back(info);
                    }
                    writeDescriptorSets.push_back(writeBufferDescriptorSet(combo.desc[index], VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, slot.registerIndex, buffers[slot.registerIndex].data(), (uint32_t)slot.handles.size()));
                }
                else if (slot.type == DescriptorType::eStorageTexture)
                {
                    for (auto& h : slot.handles)
                    {
                        VkDescriptorImageInfo info = { nullptr, (VkImageView)h, VK_IMAGE_LAYOUT_GENERAL };
                        images[slot.registerIndex].push_back(info);
                    }
                    writeDescriptorSets.push_back(writeImageDescriptorSet(combo.desc[index], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, slot.registerIndex, images[slot.registerIndex].data(), (uint32_t)slot.handles.size()));
                }
                else if (slot.type == DescriptorType::eTexture)
                {
                    for (auto& h : slot.handles)
                    {
                        VkDescriptorImageInfo info = { nullptr, (VkImageView)h, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
                        images[slot.registerIndex].push_back(info);
                    }
                    writeDescriptorSets.push_back(writeImageDescriptorSet(combo.desc[index], VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, slot.registerIndex, images[slot.registerIndex].data(), (uint32_t)slot.handles.size()));
                }
                else if (slot.type == DescriptorType::eSampler)
                {
                    for (auto& h : slot.handles)
                    {
                        VkDescriptorImageInfo info = { (VkSampler)h,nullptr,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
                        images[slot.registerIndex].push_back(info);
                    }
                    writeDescriptorSets.push_back(writeImageDescriptorSet(combo.desc[index], VK_DESCRIPTOR_TYPE_SAMPLER, slot.registerIndex, images[slot.registerIndex].data(), (uint32_t)slot.handles.size()));
                }
                else if (slot.type == DescriptorType::eConstantBuffer)
                {
                    auto buffer = (VkBuffer)slot.handles.front();
                    VkDescriptorBufferInfo info = {};
                    if (buffer)
                    {
                        info =
                        {
                            buffer,                           // VkBuffer        buffer;
                            0,                                // VkDeviceSize    offset;
                            slot.dataRange                    // VkDeviceSize    range;
                        };
                    }
                    buffers[slot.registerIndex].push_back(info);
                    writeDescriptorSets.push_back(writeBufferDescriptorSet(combo.desc[index], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, slot.registerIndex, buffers[slot.registerIndex].data(), (uint32_t)slot.handles.size()));
                }
                slot.dirty = false;
            }
            auto sz = static_cast<uint32_t>(writeDescriptorSets.size());
            m_ddt.UpdateDescriptorSets(m_device, sz, writeDescriptorSets.data(), 0, NULL);
        }
    }

    if (!thread.kernel->pipeline)
    {
        VkComputePipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        pipelineInfo.layout = thread.kernel->pipelineLayout;
        pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipelineInfo.stage.module = thread.kernel->shaderModule;
        pipelineInfo.stage.pName = "main";
        VK_CHECK(m_ddt.CreateComputePipelines(m_device, nullptr, 1, &pipelineInfo, 0, &thread.kernel->pipeline));
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::dispatch(unsigned int blockX, unsigned int blockY, unsigned int blockZ)
{
    auto& thread = m_dispatchContext.getContext();
    if (!thread.kernel) return ComputeStatus::eInvalidArgument;

    if (thread.kernel->shaderModule)
    {
        processDescriptors(thread);

        auto& combo = thread.signatureToDesc[thread.signature];

        m_ddt.CmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, thread.kernel->pipeline);
        m_ddt.CmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, thread.kernel->pipelineLayout, 0, 1, &combo.desc[thread.kernel->descriptorIndex], (uint32_t)thread.signature->offsets.size(), thread.signature->offsets.data());
        m_ddt.CmdDispatch(m_cmdBuffer, blockX, blockY, blockZ);
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::createTexture2DResourceSharedImpl(ResourceDescription &resDesc, Resource &outResource, bool UseNativeFormat, ResourceState initialState)
{
    VkImageView imageView{};
    VkImage image{};
    VkDeviceMemory deviceMemory{};

    if (resDesc.format == Format::eFormatINVALID)
    {
        getFormat(resDesc.nativeFormat, resDesc.format);
    }

    if (isFormatSupported(resDesc.format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
    {
        resDesc.flags |= ResourceFlags::eShaderResourceStorage;
    }
    else
    {
        resDesc.flags &= ~ResourceFlags::eShaderResourceStorage;
        resDesc.state &= ~ResourceState::eStorageRW;
    }
    if (isFormatSupported(resDesc.format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
    {
        resDesc.flags |= ResourceFlags::eColorAttachment;
    }
    else
    {
        resDesc.flags &= ~ResourceFlags::eColorAttachment;
        resDesc.state &= ~ResourceState::eColorAttachmentRW;
    }
    if (isFormatSupported(resDesc.format, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        resDesc.flags |= ResourceFlags::eDepthStencilAttachment;
    }
    else
    {
        resDesc.flags &= ~ResourceFlags::eDepthStencilAttachment;
        resDesc.state &= ~(ResourceState::eDepthStencilAttachmentRead | ResourceState::eDepthStencilAttachmentWrite);
    }
    if (isFormatSupported(resDesc.format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
    {
        resDesc.flags |= ResourceFlags::eShaderResource;
    }
    else
    {
        resDesc.flags &= ~ResourceFlags::eShaderResource;        
    }

    VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imageInfo.flags = 0;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    if (UseNativeFormat)
    {
        assert(resDesc.nativeFormat != NativeFormatUnknown);
        imageInfo.format = (VkFormat)resDesc.nativeFormat;
    }
    else
    {
        assert(resDesc.format != eFormatINVALID);
        NativeFormat native;
        getNativeFormat(resDesc.format, native);
        imageInfo.format = (VkFormat)native; 
    }
    imageInfo.extent.width = resDesc.width;
    imageInfo.extent.height = resDesc.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = resDesc.mips;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; 
    //getNativeResourceState(initialState, *(uint32_t*)&imageInfo.initialLayout);
    
    VkMemoryPropertyFlags memProps = 0;

    switch (resDesc.heapType) {
    case eHeapTypeDefault:
        memProps |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        imageInfo.usage = toVkImageUsageFlags(resDesc.flags) 
                        // Internal textures created with eHeapTypeDefault heap type
                        // can be used as a target of a CopyResource() call which in turn calls into vkCmdCopyImage().
                        // The vulkan spec specifies that such a texture needs to have the VK_IMAGE_USAGE_TRANSFER_DST_BIT flag set on creation.
                        // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkCmdCopyImage-dstImage-00131
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    case eHeapTypeUpload:
        memProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        break;
    case eHeapTypeReadback:
        memProps |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                    VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        break;
    }

    if (m_allocateCallback)
    {
        // Host is handling resource allocation
        ResourceAllocationDesc desc = { ResourceType::eTex2d, &imageInfo, memProps, nullptr };
        auto res = m_allocateCallback(&desc, m_device);
        image = (VkImage)res.native;
        deviceMemory = (VkDeviceMemory)res.memory;
        imageView = (VkImageView)res.view;
    }
    else
    {
        VkResult result = m_ddt.CreateImage(m_device, &imageInfo, 0, &image);
        if (result != VK_SUCCESS) {
            return ComputeStatus::eError;
        }

        VkMemoryRequirements memReqs = { };
        m_ddt.GetImageMemoryRequirements(m_device, image, &memReqs);

        // Find an available memory type that satifies the requested properties.
        uint32_t memoryTypeIndex;
        for (memoryTypeIndex = 0; memoryTypeIndex < m_vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++memoryTypeIndex) {
            if (!(memReqs.memoryTypeBits & (1 << memoryTypeIndex))) {
                continue;
            }
            if ((m_vkPhysicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memProps) == memProps) {
                break;
            }
        }
        if (memoryTypeIndex >= m_vkPhysicalDeviceMemoryProperties.memoryTypeCount) {
            m_ddt.DestroyImage(m_device, image, nullptr);
            return ComputeStatus::eError;
        }

        VkMemoryAllocateInfo memInfo = {
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,          // sType
            NULL,                                            // pNext
            memReqs.size,                                    // allocationSize
            memoryTypeIndex,                                 // memoryTypeIndex
        };

        // Ideally, we'd do suballocation, but we expect relatively few platform-managed
        // buffer objects, so just take the simple route for now    
        result = m_ddt.AllocateMemory(m_device, &memInfo, 0, &deviceMemory);

        if (result != VK_SUCCESS) {
            m_ddt.DestroyImage(m_device, image, nullptr);
            return ComputeStatus::eError;
        }

        result = m_ddt.BindImageMemory(m_device, image, deviceMemory, 0);
        if (result != VK_SUCCESS) {
            m_ddt.FreeMemory(m_device, deviceMemory, nullptr);
            m_ddt.DestroyImage(m_device, image, nullptr);
            return ComputeStatus::eError;
        }

        VkImageViewCreateInfo texViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        texViewCreateInfo.image = image;
        texViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        texViewCreateInfo.format = imageInfo.format;
        texViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        texViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        texViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        texViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        texViewCreateInfo.subresourceRange.aspectMask = resDesc.flags & ResourceFlags::eDepthStencilAttachment ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        texViewCreateInfo.subresourceRange.baseMipLevel = 0;
        texViewCreateInfo.subresourceRange.levelCount = 1;
        texViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        texViewCreateInfo.subresourceRange.layerCount = 1;

        result = m_ddt.CreateImageView(m_device, &texViewCreateInfo, 0, &imageView);
        if (result != VK_SUCCESS) {
            m_ddt.FreeMemory(m_device, deviceMemory, nullptr);
            m_ddt.DestroyImage(m_device, image, nullptr);
            return ComputeStatus::eError;
        }
    }
    
    // This pointer is deleted when DestroyResource is called on the object.
    outResource = new sl::Resource{ ResourceType::eTex2d, image, deviceMemory, imageView, VK_IMAGE_LAYOUT_UNDEFINED};
    outResource->nativeFormat = imageInfo.format;
    outResource->state = VK_IMAGE_LAYOUT_UNDEFINED;
    outResource->width = imageInfo.extent.width;
    outResource->height = imageInfo.extent.height;
    outResource->arrayLayers = imageInfo.extent.depth;
    outResource->mipLevels = imageInfo.mipLevels;
    outResource->flags = imageInfo.flags;
    outResource->usage = imageInfo.usage;

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::createBufferResourceImpl(ResourceDescription &resDesc, Resource &outResource, ResourceState initialState)
{
    VkBuffer buffer{};
    VkDeviceMemory deviceMemory{};

    VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = resDesc.width;
    assert(resDesc.height == 1);
    bufferInfo.flags = 0;

    VkMemoryPropertyFlags memProps = 0;

    switch (resDesc.heapType) {
    case eHeapTypeDefault:
        memProps        |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                         // Internal buffers created with eHeapTypeDefault heap type
                         // can be used as a target of a CopyHostToDeviceBuffer() call which in turn calls into vkCmdCopyBuffer().
                         // The vulkan spec specifies that such a buffer needs to have the VK_BUFFER_USAGE_TRANSFER_DST_BIT flag set on creation.
                         // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkCmdCopyBuffer-dstBuffer-00120
                         | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                         // Internal buffers created with eHeapTypeDefault heap type
                         // can be added to a shader input/output via SetInputBuffer()/SetOutputBuffer() call which in turn calls
                         // into vkGetBufferDeviceAddress/vkGetBufferDeviceAddressKHR/vkGetBufferDeviceAddressEXT().
                         // The vulkan spec specifies that such a buffer needs to have the VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT flag set on creation.
                         // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-VkBufferDeviceAddressInfo-buffer-02601
                         | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        break;
    case eHeapTypeUpload:
        memProps        |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                         // Internal buffers created with eHeapTypeUpload heap type
                         // can be used as a source of a CopyHostToDeviceBuffer() call which in turn calls into vkCmdCopyBuffer().
                         // The vulkan spec specifies that such a buffer needs to have the VK_BUFFER_USAGE_TRANSFER_SRC_BIT flag set on creation.
                         // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkCmdCopyBuffer-srcBuffer-00118
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if (resDesc.flags & ResourceFlags::eConstantBuffer)
        {
            bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }
        break;
    case eHeapTypeReadback:
        memProps        |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT  |
                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                           VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
                         // Internal buffers created with eHeapTypeReadback heap type
                         // can be used as a target of a CopyBufferToReadbackBuffer() call which in turn calls into vkCmdCopyBuffer().
                         // The vulkan spec specifies that such a buffer needs to have the VK_BUFFER_USAGE_TRANSFER_DST_BIT flag set on creation.
                         // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#VUID-vkCmdCopyBuffer-dstBuffer-00120
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        break;
    }

    if (m_allocateCallback)
    {
        // Host is handling resource allocation
        ResourceAllocationDesc desc = { ResourceType::eBuffer, &bufferInfo, memProps, nullptr };
        auto res = m_allocateCallback(&desc, m_device);
        sl::Resource* outResourceVK = new sl::Resource{ desc.type, res.native, res.memory, res.view, res.state };
        outResource = (Resource)outResourceVK;
        outResource->width = (uint32_t)bufferInfo.size;
        outResource->height = 1;
        outResource->mipLevels = 1;
        outResource->arrayLayers = 1;
        outResource->nativeFormat = VK_FORMAT_UNDEFINED;
        return ComputeStatus::eOk;
    }
    
    VkResult result = m_ddt.CreateBuffer(m_device, &bufferInfo, 0, &buffer);

    if (result != VK_SUCCESS) {
        return ComputeStatus::eError;
    }

    VkMemoryRequirements memReqs = { };
    m_ddt.GetBufferMemoryRequirements(m_device, buffer, &memReqs);

    // Find an available memory type that satifies the requested properties.
    uint32_t memoryTypeIndex;
    for (memoryTypeIndex = 0; memoryTypeIndex < m_vkPhysicalDeviceMemoryProperties.memoryTypeCount; ++memoryTypeIndex) {
        if (!(memReqs.memoryTypeBits & (1 << memoryTypeIndex))) {
            continue;
        }
        if ((m_vkPhysicalDeviceMemoryProperties.memoryTypes[memoryTypeIndex].propertyFlags & memProps) == memProps) {
            break;
        }
    }
    if (memoryTypeIndex >= m_vkPhysicalDeviceMemoryProperties.memoryTypeCount) {
        m_ddt.DestroyBuffer(m_device, buffer, nullptr);
        return ComputeStatus::eError;
    }

    VkMemoryAllocateInfo memInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,          // sType
        NULL,                                            // pNext
        memReqs.size,                                    // allocationSize
        memoryTypeIndex,                                 // memoryTypeIndex
    };

    // If the VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress feature is enabled and buffer was created 
    // with the VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT bit set, memory must have been allocated with the VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT bit set
    VkMemoryAllocateFlagsInfo memFlags = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,    // sType
        NULL,                                            // pNext
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,           // allocationSize
        0                                                // memoryTypeIndex
    };
    if (bufferInfo.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) 
    {
        memInfo.pNext = &memFlags;
    }

    // Ideally, we'd do suballocation, but we expect relatively few platform-managed
    // buffer objects, so just take the simple route for now
    

    result = m_ddt.AllocateMemory(m_device, &memInfo, 0, &deviceMemory);

    if (result != VK_SUCCESS) {
        m_ddt.DestroyBuffer(m_device, buffer, nullptr);
        return ComputeStatus::eError;
    }

    result = m_ddt.BindBufferMemory(m_device, buffer, deviceMemory, 0);
    if (result != VK_SUCCESS) {
        m_ddt.FreeMemory(m_device, deviceMemory, nullptr);
        m_ddt.DestroyBuffer(m_device, buffer, nullptr);
        return ComputeStatus::eError;
    }
        
    VkBufferView view = {};
    //VkBufferViewCreateInfo bufferViewCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO };
    //bufferViewCreateInfo.pNext = nullptr;
    //bufferViewCreateInfo.flags = (VkBufferViewCreateFlags)0;
    //bufferViewCreateInfo.buffer = buffer;
    //bufferViewCreateInfo.format = VK_FORMAT_UNDEFINED;
    //bufferViewCreateInfo.offset = 0,
    //bufferViewCreateInfo.range = bufferInfo.size; // different from DX12, this is size in bytes not number of elements (buffer size / format size)
    //m_ddt.CreateBufferView(m_device, &bufferViewCreateInfo, nullptr, &view);

    // The lifetime of this resource is handled by us. It is deleted when DestroyResource is called on the object.
    outResource = new sl::Resource{ ResourceType::eBuffer, buffer, deviceMemory, view, 0 };
    outResource->width = (uint32_t)bufferInfo.size;
    outResource->height = 1;
    outResource->mipLevels = 1;
    outResource->arrayLayers = 1;
    outResource->nativeFormat = VK_FORMAT_UNDEFINED;

    // No state tracking for buffers

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::copyHostToDeviceBuffer(CommandList InCmdList, uint64_t InSize, const void *InData, Resource InUploadResource, Resource InTargetResource, unsigned long long InUploadOffset, unsigned long long InDstOffset)
{
    sl::Resource* dstResource = (sl::Resource*)InTargetResource;
    if (dstResource->type != ResourceType::eBuffer) return ComputeStatus::eInvalidArgument;
    VkBuffer dst = (VkBuffer)dstResource->native;

    sl::Resource* scratchResource = (sl::Resource*)InUploadResource;
    if (dstResource->type != ResourceType::eBuffer) return ComputeStatus::eInvalidArgument;
    VkBuffer scratch = (VkBuffer)scratchResource->native;

    uint8_t *StagingPtr = nullptr;

    VkDeviceMemory mem = (VkDeviceMemory)scratchResource->memory;
    
    VkResult result = m_ddt.MapMemory(m_device, mem, 0, InSize, 0, (void**)&StagingPtr);
    if (result != VK_SUCCESS) {
        return ComputeStatus::eError;
    }

    StagingPtr += InUploadOffset;

    memcpy(StagingPtr, InData, InSize);

    m_ddt.UnmapMemory(m_device, mem);

    VkCommandBuffer commandBuffer = (VkCommandBuffer)InCmdList;

    VkBufferCopy copyRegion = { InUploadOffset, InDstOffset, InSize };
    m_ddt.CmdCopyBuffer(commandBuffer, scratch, dst, 1, &copyRegion);

    {
        VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, 0, 0, 0);
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::copyHostToDeviceTexture(CommandList InCmdList, uint64_t InSize, uint64_t RowPitch, const void* InData, Resource InTargetResource, Resource& InUploadResource)
{
    auto commandBuffer = (VkCommandBuffer)InCmdList;

    auto dstResource = (sl::Resource*)InTargetResource;
    auto scratchResource = (sl::Resource*)InUploadResource;

    if (!dstResource || !scratchResource)  return ComputeStatus::eInvalidPointer;
    if (dstResource->type != ResourceType::eTex2d) return ComputeStatus::eInvalidArgument;
    if (scratchResource->type != ResourceType::eBuffer) return ComputeStatus::eInvalidArgument;

    auto dst = (VkImage)dstResource->native;
    auto scratch = (VkBuffer)scratchResource->native;
    auto mem = (VkDeviceMemory)scratchResource->memory;

    // Copy to staging buffer
    void *stagingPtr = nullptr;
    auto result = m_ddt.MapMemory(m_device, mem, 0, InSize, 0, &stagingPtr);
    if (result != VK_SUCCESS)
    {
        return ComputeStatus::eError;
    }
    memcpy(stagingPtr, InData, InSize);
    m_ddt.UnmapMemory(m_device, mem);

    {
        const VkImageMemoryBarrier transferBarrier{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT, // dstAccessMask
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            dst,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transferBarrier);
    }

    ResourceDescription desc;
    getResourceDescription(dstResource, desc);

    // Copy from staging to texture
    VkBufferImageCopy buffImageCopyRegions{};
    buffImageCopyRegions.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    buffImageCopyRegions.imageSubresource.layerCount = 1;
    buffImageCopyRegions.imageExtent = { desc.width, desc.height, 1 };
    m_ddt.CmdCopyBufferToImage(commandBuffer, scratch, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffImageCopyRegions);

    {
        const VkImageMemoryBarrier useBarrier{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            nullptr,
            VK_ACCESS_TRANSFER_WRITE_BIT, // srcAccessMask
            VK_ACCESS_SHADER_READ_BIT, // dstAccessMask
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // oldLayout
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, // newLayout
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            dst,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &useBarrier);
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::insertGPUBarrier(CommandList InCmdList, Resource InResource, BarrierType InBarrierType)
{
    VkCommandBuffer commandBuffer = (VkCommandBuffer)InCmdList;

    if (InBarrierType == BarrierType::eBarrierTypeUAV)
    {
        if (!InResource) return ComputeStatus::eInvalidArgument;

        sl::Resource* inResourceVK = (sl::Resource*)InResource;
        if(inResourceVK->type == ResourceType::eBuffer)
        {
            VkBufferMemoryBarrier memoryBarrier = {
                VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                NULL,
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                (VkBuffer)inResourceVK->native,
                0,
                VK_WHOLE_SIZE
            };

            m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 1, &memoryBarrier, 0, 0);
        }
        else
        {
            VkImageMemoryBarrier memoryBarrier = {
                VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                NULL,
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_QUEUE_FAMILY_IGNORED,
                VK_QUEUE_FAMILY_IGNORED,
                (VkImage)inResourceVK->native,
                { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}
            };

            m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, 0, 0, 0, 1, &memoryBarrier);
        }
    }
    else
    {
        assert(false);
        return ComputeStatus::eNotSupported;
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::getResourceState(Resource resource, ResourceState& state)
{
    state = ResourceState::eUnknown;
    return resource ? getResourceState(resource->state, state) : ComputeStatus::eOk;
}

ComputeStatus Vulkan::transitionResourceImpl(CommandList cmdList, const ResourceTransition *transitions, uint32_t count)
{
    std::vector<VkImageMemoryBarrier> images;
    std::vector<VkBufferMemoryBarrier> buffers;

    for (uint32_t i = 0; i < count; i++)
    {
        // If same state nothing to do
        if (transitions[i].from == transitions[i].to) continue;

        auto info = (sl::Resource*)transitions[i].resource;
        if (info->type == ResourceType::eBuffer)
        {
            VkBufferMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.srcAccessMask = toVkAccessFlags(transitions[i].from);
            barrier.dstAccessMask = toVkAccessFlags(transitions[i].to);
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.buffer = (VkBuffer)info->native;
            barrier.offset = 0;
            barrier.size = VK_WHOLE_SIZE;
            buffers.push_back(barrier);
        }
        else
        {
            bool depth = (transitions[i].from & ResourceState::eDepthStencilAttachmentRead) != 0 || (transitions[i].to & ResourceState::eDepthStencilAttachmentRead) != 0;
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.pNext = nullptr;
            barrier.oldLayout = toVkImageLayout(transitions[i].from);
            barrier.newLayout = toVkImageLayout(transitions[i].to);
            barrier.srcAccessMask = toVkAccessFlags(transitions[i].from);
            barrier.dstAccessMask = toVkAccessFlags(transitions[i].to);
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = (VkImage)info->native;
            barrier.subresourceRange.aspectMask = depth ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
            images.push_back(barrier);
        }
    }
    if (!images.empty() || !buffers.empty())
    {
        m_ddt.CmdPipelineBarrier((VkCommandBuffer)cmdList, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, (uint32_t)buffers.size(), buffers.data(), (uint32_t)images.size(), images.data());
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::getResourceDescription(Resource resource, ResourceDescription &resDesc)
{
    if (!resource || !resource->native)
    {
        return ComputeStatus::eInvalidArgument;
    }

    auto info = (sl::Resource*)resource;

    resDesc = {};

    if (resource->type != ResourceType::eTex2d && resource->type != ResourceType::eBuffer)
    {
        return ComputeStatus::eInvalidArgument;
    }

    resDesc.width = resource->width;
    resDesc.height = resource->height;
    resDesc.nativeFormat = resource->nativeFormat;
    resDesc.mips = resource->mipLevels;
    resDesc.depth = resource->arrayLayers;

    getResourceState(resource->state, resDesc.state);
    getFormat(resDesc.nativeFormat, resDesc.format);

    if (resource->type != ResourceType::eBuffer)
    {
        if (isFormatSupported(resDesc.format, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT))
        {
            resDesc.flags |= ResourceFlags::eShaderResourceStorage;
        }
        else
        {
            resDesc.flags &= ~ResourceFlags::eShaderResourceStorage;
            resDesc.state &= ~ResourceState::eStorageRW;
        }
        if (isFormatSupported(resDesc.format, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
        {
            resDesc.flags |= ResourceFlags::eColorAttachment;
        }
        else
        {
            resDesc.flags &= ~ResourceFlags::eColorAttachment;
            resDesc.state &= ~ResourceState::eColorAttachmentRW;
        }
        if (isFormatSupported(resDesc.format, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT))
        {
            resDesc.flags |= ResourceFlags::eDepthStencilAttachment;
        }
        else
        {
            resDesc.flags &= ~ResourceFlags::eDepthStencilAttachment;
            resDesc.state &= ~(ResourceState::eDepthStencilAttachmentRead | ResourceState::eDepthStencilAttachmentWrite);
        }
        if (isFormatSupported(resDesc.format, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT))
        {
            resDesc.flags |= ResourceFlags::eShaderResource;
        }
        else
        {
            resDesc.flags &= ~ResourceFlags::eShaderResource;
        }
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::getStaticVKMethods()
{
    // We are not linking vulkan-1.lib anywhere in the SL code
    if (!s_module)
    {
        s_module = ::LoadLibraryA("vulkan-1.dll");
        if (s_module)
        {
            vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(GetProcAddress(s_module, "vkCreateInstance"));
            vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(GetProcAddress(s_module, "vkDestroyInstance"));
            vkGetPhysicalDeviceProperties2 = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(GetProcAddress(s_module, "vkGetPhysicalDeviceProperties2"));
            vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(GetProcAddress(s_module, "vkEnumeratePhysicalDevices"));
        }
    }
    if (!vkCreateInstance || !vkDestroyInstance || !vkGetPhysicalDeviceProperties2 || !vkEnumeratePhysicalDevices)
    {
        SL_LOG_ERROR("Failed to obtain VK API");
        return ComputeStatus::eError;
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::createInstanceAndFindPhysicalDevice(uint32_t id, chi::Instance& instance, chi::PhysicalDevice& device)
{
    auto res = getStaticVKMethods();
    if (res == ComputeStatus::eOk)
    {
        res = ComputeStatus::eError;

        std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

        VkInstance inst{};
        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.enabledExtensionCount = 2;
        info.ppEnabledExtensionNames = instanceExtensions.data();
        VK_CHECK(vkCreateInstance(&info, nullptr, &inst));

        instance = inst;

        uint32_t adapterCount = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(inst, &adapterCount, nullptr));
        std::vector<VkPhysicalDevice> physicalDevices(adapterCount);
        VK_CHECK(vkEnumeratePhysicalDevices(inst, &adapterCount, physicalDevices.data()));

        for (uint32_t i = 0; i < adapterCount; i++)
        {
            auto physicalDevice = physicalDevices[i];

            VkPhysicalDeviceIDProperties physicalDeviceIDProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES, nullptr };
            VkPhysicalDeviceProperties2 physicalDeviceProperties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &physicalDeviceIDProperties };

            vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties2);
            if (physicalDeviceProperties2.properties.deviceID == id)
            {
                device = physicalDevice;
                res = ComputeStatus::eOk;
                break;
            }
        }
    }
    return res;
}

ComputeStatus Vulkan::destroyInstance(chi::Instance& instance)
{
    auto res = getStaticVKMethods();
    if (res == ComputeStatus::eOk)
    {
        vkDestroyInstance((VkInstance)instance, nullptr);
    }
    return res;
}

ComputeStatus Vulkan::getLUIDFromDevice(chi::PhysicalDevice device, uint32_t& deviceId, LUID* outId)
{
    auto res = getStaticVKMethods();
    if (res == ComputeStatus::eOk)
    {
        VkPhysicalDeviceIDProperties physicalDeviceIDProperties = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES, nullptr };
        VkPhysicalDeviceProperties2 physicalDeviceProperties2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &physicalDeviceIDProperties };

        *outId = {};
        auto physicalDevice = (VkPhysicalDevice)device;
        vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties2);
        if (physicalDeviceIDProperties.deviceLUIDValid)
        {
            memcpy(outId, physicalDeviceIDProperties.deviceLUID, sizeof(LUID));
        }
        deviceId = physicalDeviceProperties2.properties.deviceID;
    }
    return res;
}

ComputeStatus Vulkan::mapResource(CommandList cmdList, Resource resource, void*& data, uint32_t subResource, uint64_t offset, uint64_t totalBytes)
{
    auto src = (sl::Resource*)resource;
    if (!src) return ComputeStatus::eInvalidPointer;
    
    void* mapped{};
    m_ddt.MapMemory(m_device, (VkDeviceMemory)src->memory, offset, totalBytes, 0, &mapped);
    data = mapped;
    return mapped != nullptr ? ComputeStatus::eOk : ComputeStatus::eError;
}

ComputeStatus Vulkan::unmapResource(CommandList cmdList, Resource resource, uint32_t subResource)
{
    auto src = (sl::Resource*)resource;
    if (!src) return ComputeStatus::eInvalidPointer;

    m_ddt.UnmapMemory(m_device, (VkDeviceMemory)src->memory);
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::copyResource(CommandList InCmdList, Resource InDstResource, Resource InSrcResource)
{
    auto src = (sl::Resource*)InSrcResource;
    auto dst = (sl::Resource*)InDstResource;
    if (src->type != dst->type)
    {
        SL_LOG_ERROR( "Mismatched resources in copy");
        return ComputeStatus::eError;
    }

    ResourceDescription desc;
    getResourceDescription(src, desc);

    if (src->type == ResourceType::eBuffer)
    {
        VkBufferCopy copyRegion = { 0, 0, desc.width };
        m_ddt.CmdCopyBuffer((VkCommandBuffer)InCmdList,(VkBuffer)src->native,(VkBuffer)dst->native,1,&copyRegion);
    }
    else
    {
        VkImageCopy copyRegion = 
        { 
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            {0,0,0},
            { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
            {0, 0, 0},
            {desc.width, desc.height, 1}
        };
        bool depth = desc.flags & ResourceFlags::eDepthStencilAttachment;
        if (depth)
        {
            copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        }
        m_ddt.CmdCopyImage((VkCommandBuffer)InCmdList, (VkImage)src->native, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, (VkImage)dst->native, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    }

    return ComputeStatus::eOk;
}

bool Vulkan::isFormatSupported(Format format, VkFormatFeatureFlagBits flag)
{
    uint32_t native;
    getNativeFormat(format, native);
    if (native == VK_FORMAT_UNDEFINED)
    {
        assert(false);
        SL_LOG_ERROR( "Cannot have undefined format");
    }
    VkFormatProperties props{};
    m_idt.GetPhysicalDeviceFormatProperties(m_physicalDevice, (VkFormat)native, &props);
    return (props.optimalTilingFeatures & flag) != 0;
}

ComputeStatus Vulkan::cloneResource(Resource InResource, Resource &OutResource, const char friendlyName[], ResourceState initialState, unsigned int InCreationMask, unsigned int InVisibilityMask)
{
    auto src = (sl::Resource*)InResource;
    ResourceDescription desc;
    CHI_CHECK(getResourceDescription(src, desc));

    desc.state = initialState;

    if (src->type == ResourceType::eBuffer)
    {
        createBuffer(desc, OutResource, friendlyName);
    }
    else
    {
        createTexture2D(desc, OutResource, friendlyName);
    }
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::clearView(CommandList InCmdList, Resource InResource, const float4 Color, const RECT* pRects, unsigned int NumRects, CLEAR_TYPE &outType)
{
    outType = CLEAR_UNDEFINED;
    
    VkCommandBuffer commandBuffer = (VkCommandBuffer)InCmdList;

    // Update the push descriptor for the image view
    VkDescriptorImageInfo imageInfo = { 0 };
    VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

    if (!InResource) return ComputeStatus::eInvalidArgument;

    sl::Resource* vkResource = (sl::Resource*)InResource;
    if (vkResource->type == ResourceType::eBuffer) return ComputeStatus::eInvalidArgument;

    if (NumRects == 0)
    {
        VkClearColorValue clearColor;
        clearColor.float32[0] = Color.x;
        clearColor.float32[1] = Color.y;
        clearColor.float32[2] = Color.z;
        clearColor.float32[3] = Color.w;
        VkImageSubresourceRange subresourceRange;
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.baseMipLevel = 0;
        subresourceRange.levelCount = 1;
        subresourceRange.baseArrayLayer = 0;
        subresourceRange.layerCount = 1;
        m_ddt.CmdClearColorImage(commandBuffer, (VkImage)vkResource->native, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &subresourceRange);
        outType = CLEAR_ZBC_WITHOUT_PADDING;
    }
    else
    {
        imageInfo.imageView = (VkImageView)vkResource->native;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo = &imageInfo;
        write.pNext = NULL;

        // XXX Toss in a very heavy barrier for now
        {
            VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_WRITE_BIT };
            m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &memoryBarrier, 0, 0, 0, 0);
        }

        m_ddt.CmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_imageViewClear.doClear);

        m_ddt.CmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_imageViewClear.pipelineLayout, 0, 1, &write);

        // Update the push constant for the color
        m_ddt.CmdPushConstants(commandBuffer, m_imageViewClear.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 16, 4 * 4, &Color);

        // For each rectangle, update the offset and dispatch with the size of the rectangle
        for (unsigned int r = 0; r < NumRects; r++) {
            uint32_t offsetSize[4] = { uint32_t(pRects[r].left), uint32_t(pRects[r].top),
                                       uint32_t(pRects[r].right - pRects[r].left),
                                       uint32_t(pRects[r].bottom - pRects[r].top) };

            m_ddt.CmdPushConstants(commandBuffer, m_imageViewClear.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 4 * 4, &offsetSize);

            m_ddt.CmdDispatch(commandBuffer, (offsetSize[2] + 15) / 16, (offsetSize[3] + 15) / 16, 1);
        }

        // XXX Toss in a very heavy barrier for now
        {
            VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER, NULL,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_INPUT_ATTACHMENT_READ_BIT |
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT |
                VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT };
            m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, 0, 0, 0);
        }
        outType = CLEAR_NON_ZBC;
    }

    return ComputeStatus::eOk;
}

int Vulkan::destroyResourceDeferredImpl(const Resource resource)
{
    // Note: From SL 2.0 there is no special VK Resource structure, it is all unified with d3d
    
    // Try to find a buffer to free first
    if (resource->type == ResourceType::eFence)
    {
        m_ddt.DestroySemaphore(m_device, (VkSemaphore)resource->native, nullptr);
    }
    else if(resource->type == ResourceType::eBuffer)
    {
        m_ddt.DestroyBuffer(m_device, (VkBuffer)resource->native, nullptr);
    }
    else
    {
        m_ddt.DestroyImageView(m_device, (VkImageView)resource->view, nullptr);
        if (resource->memory)
        {
            // If there is no memory then we did not create this image
            m_ddt.DestroyImage(m_device, (VkImage)resource->native, nullptr);
        }
    }
    if (resource->memory)
    {
        m_ddt.FreeMemory(m_device, (VkDeviceMemory)resource->memory, nullptr);
    }

    return 0;
}

std::wstring Vulkan::getDebugName(Resource res)
{
    return L"Unknown";
}

ComputeStatus Vulkan::setDebugName(Resource InOutResource, const char InFriendlyName[])
{
#if !(defined SL_PRODUCTION || defined SL_REL_EXT_DEV)
    sl::Resource* vkResource = (sl::Resource*) InOutResource;

    // The VK_EXT_debug_utils may not have been enabled so don't try to set names by default
    if (m_ddt.SetDebugUtilsObjectNameEXT == nullptr)
    {
        return ComputeStatus::eError;
    }

    if (vkResource->type == ResourceType::eBuffer)
    {
        const VkDebugUtilsObjectNameInfoEXT ObjectNameInfo =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            nullptr,
            VK_OBJECT_TYPE_BUFFER,
            (uint64_t)vkResource->native,
            InFriendlyName,
        };

        m_ddt.SetDebugUtilsObjectNameEXT(m_device, &ObjectNameInfo);
    }
    else if (vkResource->type == ResourceType::eCommandQueue)
    {
        VkDebugUtilsObjectNameInfoEXT ObjectNameInfo =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            nullptr,
            VK_OBJECT_TYPE_QUEUE,
            (uint64_t)vkResource->native,
            InFriendlyName,
        };
        m_ddt.SetDebugUtilsObjectNameEXT(m_device, &ObjectNameInfo);
    }
    else if (vkResource->type == ResourceType::eCommandBuffer)
    {
        VkDebugUtilsObjectNameInfoEXT ObjectNameInfo =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            nullptr,
            VK_OBJECT_TYPE_COMMAND_BUFFER,
            (uint64_t)vkResource->native,
            InFriendlyName,
        };
        m_ddt.SetDebugUtilsObjectNameEXT(m_device, &ObjectNameInfo);
    }
    else if (vkResource->type == ResourceType::eCommandPool)
    {
        VkDebugUtilsObjectNameInfoEXT ObjectNameInfo =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            nullptr,
            VK_OBJECT_TYPE_COMMAND_POOL,
            (uint64_t)vkResource->native,
            InFriendlyName,
        };
        m_ddt.SetDebugUtilsObjectNameEXT(m_device, &ObjectNameInfo);
    }
    else if (vkResource->type == ResourceType::eFence)
    {
        VkDebugUtilsObjectNameInfoEXT ObjectNameInfo =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            nullptr,
            VK_OBJECT_TYPE_SEMAPHORE,
            (uint64_t)vkResource->native,
            InFriendlyName,
        };
        m_ddt.SetDebugUtilsObjectNameEXT(m_device, &ObjectNameInfo);
    }
    else if (vkResource->type == ResourceType::eSwapchain)
    {
        VkDebugUtilsObjectNameInfoEXT ObjectNameInfo =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            nullptr,
            VK_OBJECT_TYPE_SWAPCHAIN_KHR,
            (uint64_t)vkResource->native,
            InFriendlyName,
        };
        m_ddt.SetDebugUtilsObjectNameEXT(m_device, &ObjectNameInfo);
    }
    else if (vkResource->type == ResourceType::eTex2d)
    {
        VkDebugUtilsObjectNameInfoEXT ObjectNameInfo =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            nullptr,
            VK_OBJECT_TYPE_IMAGE_VIEW,
            (uint64_t)vkResource->view,
            InFriendlyName,
        };
        m_ddt.SetDebugUtilsObjectNameEXT(m_device, &ObjectNameInfo);
        
        ObjectNameInfo =
        {
            VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            nullptr,
            VK_OBJECT_TYPE_IMAGE,
            (uint64_t)vkResource->native,
            InFriendlyName,
        };
        m_ddt.SetDebugUtilsObjectNameEXT(m_device, &ObjectNameInfo);
    }
#endif
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::copyBufferToReadbackBuffer(CommandList InCmdList, Resource InResource, Resource OutResource, unsigned int InBytesToCopy)
{
    VkCommandBuffer commandBuffer = (VkCommandBuffer)InCmdList;

    // Throw in a memory barrier here, because the VK cubin resource transition implementations are just dummies that don't do anything,
    // due to the nature of the VK API (the interface NGX exposes doesn't give enough information for doing resource transitions in general)
    // and so we just expect all input resources to be in VK_IMAGE_LAYOUT_GENERAL. But this forces us to surround our copy
    // calls with a memory barrier ourselves.
    {
        VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        memoryBarrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &memoryBarrier, 0, 0, 0, 0);
    }

    sl::Resource* vkInResource = (sl::Resource*)InResource;
    sl::Resource* vkOutResource = (sl::Resource*)OutResource;

    assert(vkInResource->type == ResourceType::eBuffer);
    assert(vkOutResource->type == ResourceType::eBuffer);

    VkBufferCopy region = {};
    region.srcOffset = 0;
    region.dstOffset = 0;
    region.size = InBytesToCopy;

    m_ddt.CmdCopyBuffer(commandBuffer, (VkBuffer)vkInResource->native, (VkBuffer)vkOutResource->native, 1, &region);

    {
        VkMemoryBarrier memoryBarrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        memoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        memoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        m_ddt.CmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &memoryBarrier, 0, 0, 0, 0);
    }

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::beginPerfSection(CommandList cmdList, const char *key, unsigned int node, bool reset)
{
#if SL_ENABLE_TIMING
    std::scoped_lock lock(m_mutexProfiler);
    auto Section = m_SectionPerfMap[node].find(key);
    if (Section == m_SectionPerfMap[node].end())
    {
        PerfData Data = {};
        m_SectionPerfMap[node][key] = Data;
        Section = m_SectionPerfMap[node].find(key);
    }

    PerfData &Data = (*Section).second;
    if (reset)
    {
        for(int i = 0; i < SL_READBACK_QUEUE_SIZE; i++)
            Data.Reset[i] = true;
    }

    VkCommandBuffer commandBuffer = (VkCommandBuffer)cmdList;

    if (!Data.QueryPool[Data.QueryIdx])
    {
        VkQueryPoolCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        createInfo.pNext = nullptr;
        createInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        createInfo.queryCount = 2;

        VkResult res = m_ddt.CreateQueryPool(m_device, &createInfo, nullptr, &Data.QueryPool[Data.QueryIdx]);
        if (res != VK_SUCCESS)
        {
            SL_LOG_ERROR( "Failed to create query pool");
            return ComputeStatus::eError;
        }
        m_ddt.CmdResetQueryPool(commandBuffer, Data.QueryPool[Data.QueryIdx], 0, 2);
    }
    else
    {
        uint64_t dxNanoSecondTS[2];
        m_ddt.GetQueryPoolResults(m_device, Data.QueryPool[Data.QueryIdx], 1, 1, sizeof(uint64_t), &dxNanoSecondTS[1], 0, VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
        m_ddt.GetQueryPoolResults(m_device, Data.QueryPool[Data.QueryIdx], 0, 1, sizeof(uint64_t), &dxNanoSecondTS[0], 0, VK_QUERY_RESULT_WAIT_BIT | VK_QUERY_RESULT_64_BIT);
        {
            float Delta = (dxNanoSecondTS[1] - dxNanoSecondTS[0]) / 1e06f;
            if (!Data.Reset[Data.QueryIdx])
            {
                Data.AccumulatedTimeMS += Delta;
                Data.NumExecutedQueries++;
            }
            else
            {
                Data.Reset[Data.QueryIdx] = false;
                Data.AccumulatedTimeMS = 0;
                Data.NumExecutedQueries = 0;
            }
        }
        m_ddt.CmdResetQueryPool(commandBuffer, Data.QueryPool[Data.QueryIdx], 0, 2);

    }

    m_ddt.CmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, Data.QueryPool[Data.QueryIdx], 0);
#endif
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::endPerfSection(CommandList cmdList, const char *key, float &avgTimeMS, unsigned int node)
{
#if SL_ENABLE_TIMING
    std::scoped_lock lock(m_mutexProfiler);
    auto Section = m_SectionPerfMap[node].find(key);
    if (Section == m_SectionPerfMap[node].end())
    {
        return ComputeStatus::eError;
    }
    VkCommandBuffer commandBuffer = (VkCommandBuffer)cmdList;

    PerfData &Data = (*Section).second;
    m_ddt.CmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, Data.QueryPool[Data.QueryIdx], 1);
    Data.QueryIdx = (Data.QueryIdx + 1) % SL_READBACK_QUEUE_SIZE;

    avgTimeMS = Data.NumExecutedQueries ? Data.AccumulatedTimeMS / Data.NumExecutedQueries : 0;
    //OutAvgTimeMS = Data.Times.empty() ? 0.0f : (float)Data.Times[Data.Times.size() / 2];
#else
    avgTimeMS = 0;
#endif
    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::getSwapChainBuffer(SwapChain swapchain, uint32_t index, Resource& buffer)
{
    auto* sc = (SwapChainVk*)swapchain;
    // Get the swapchain vk images
    uint32_t swapchainImageCount = 0;
    std::vector<VkImage> swapchainImages;
    VK_CHECK(m_ddt.GetSwapchainImagesKHR(m_device, (VkSwapchainKHR)sc->native, &swapchainImageCount, nullptr));
    swapchainImages.resize(swapchainImageCount);
    VK_CHECK(m_ddt.GetSwapchainImagesKHR(m_device, (VkSwapchainKHR)sc->native, &swapchainImageCount, swapchainImages.data()));

    VkImageViewCreateInfo texViewCreateInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    texViewCreateInfo.image = swapchainImages[index];
    texViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    texViewCreateInfo.format = sc->info.imageFormat;
    texViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    texViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    texViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    texViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    texViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    texViewCreateInfo.subresourceRange.baseMipLevel = 0;
    texViewCreateInfo.subresourceRange.levelCount = 1;
    texViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    texViewCreateInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VK_CHECK(m_ddt.CreateImageView(m_device, &texViewCreateInfo, 0, &imageView));
    
    // This pointer is deleted when DestroyResource is called on the object.
    buffer = new sl::Resource{ ResourceType::eTex2d, swapchainImages[index], nullptr, imageView };
    buffer->nativeFormat = sc->info.imageFormat;
    buffer->width = sc->info.imageExtent.width;
    buffer->height = sc->info.imageExtent.height;
    buffer->mipLevels = 1;
    buffer->arrayLayers = 1;

    // We free these buffers but never allocate them so account for the VRAM
    manageVRAM(buffer, VRAMOperation::eAlloc);

    return ComputeStatus::eOk;
}

ComputeStatus Vulkan::getNativeFormat(Format format, NativeFormat& native)
{
    native = VK_FORMAT_UNDEFINED;
    switch (format)
    {
        case eFormatRGB10A2UN:  native = VK_FORMAT_A2B10G10R10_UNORM_PACK32; break;
        case eFormatRGBA8UN:    native = VK_FORMAT_R8G8B8A8_UNORM; break;
        case eFormatBGRA8UN:    native = VK_FORMAT_B8G8R8A8_UNORM; break;
        case eFormatR8UN:       native = VK_FORMAT_R8_UNORM; break;
        case eFormatRGBA32F:    native = VK_FORMAT_R32G32B32A32_SFLOAT; break;
        case eFormatRGB32F:     native = VK_FORMAT_R32G32B32_SFLOAT; break;
        case eFormatRGBA16F:    native = VK_FORMAT_R16G16B16A16_SFLOAT; break;
        case eFormatRGB16F:     native = VK_FORMAT_R16G16B16_SFLOAT; break;
        case eFormatRGB11F:     native = VK_FORMAT_B10G11R11_UFLOAT_PACK32; break;
        case eFormatRG16F:      native = VK_FORMAT_R16G16_SFLOAT; break;
        case eFormatRG16UI:     native = VK_FORMAT_R16G16_UINT; break;
        case eFormatRG16SI:     native = VK_FORMAT_R16G16_SINT; break;
        case eFormatR16F:       native = VK_FORMAT_R16_SFLOAT; break;
        case eFormatR8UI:       native = VK_FORMAT_R8_UINT; break;
        case eFormatR16UI:      native = VK_FORMAT_R16_UINT; break;
        case eFormatRG16UN:     native = VK_FORMAT_R16G16_UNORM; break;
        case eFormatR32UI:      native = VK_FORMAT_R32_UINT; break;
        case eFormatRG32UI:     native = VK_FORMAT_R32G32_UINT; break;
        case eFormatRG32F:     native = VK_FORMAT_R32G32_SFLOAT; break;
        case eFormatSRGBA8UN:   native = VK_FORMAT_R8G8B8A8_SRGB; break;
        case eFormatSBGRA8UN:   native = VK_FORMAT_B8G8R8A8_SRGB; break;
        case eFormatD24S8:      native = VK_FORMAT_D24_UNORM_S8_UINT; break;
        case eFormatD32S32:      native = VK_FORMAT_D32_SFLOAT; break;
        case eFormatR32F:      native = VK_FORMAT_R32_SFLOAT; break;
        case eFormatE5M3: assert(false);
    }
    
    return ComputeStatus::eOk;
};

 ComputeStatus Vulkan::getFormat(NativeFormat nativeFmt, Format& format)
{
    VkFormat fmt = static_cast<VkFormat>(nativeFmt);
    format = eFormatINVALID;

    switch (fmt)
    {
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:    format = eFormatRGB10A2UN; break;
        case VK_FORMAT_R8G8B8A8_SRGB:               format = eFormatSRGBA8UN; break;
        case VK_FORMAT_B8G8R8A8_SRGB:               format = eFormatSBGRA8UN; break;
        case VK_FORMAT_B8G8R8A8_UNORM:              format = eFormatBGRA8UN; break;
        case VK_FORMAT_R8G8B8A8_UNORM:              format = eFormatRGBA8UN; break;
        case VK_FORMAT_R32G32B32A32_SFLOAT:         format = eFormatRGBA32F; break;
        case VK_FORMAT_R32G32B32_SFLOAT:            format = eFormatRGB32F; break;
        case VK_FORMAT_R16G16B16A16_SFLOAT:         format = eFormatRGBA16F; break;
        case VK_FORMAT_R16G16B16_SFLOAT:            format = eFormatRGB16F; break;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:     format = eFormatRGB11F; break;
        case VK_FORMAT_R16G16_SFLOAT:               format = eFormatRG16F; break;
        case VK_FORMAT_R16_SFLOAT:                  format = eFormatR16F; break;
        case VK_FORMAT_R8_UINT:                     format = eFormatR8UI; break;
        case VK_FORMAT_R16_UINT:                    format = eFormatR16UI; break;
        case VK_FORMAT_R16G16_UNORM:                format = eFormatRG16UN; break;
        case VK_FORMAT_R32_UINT:                    format = eFormatR32UI; break;
        case VK_FORMAT_R32_SFLOAT:                  format = eFormatR32F; break;
        case VK_FORMAT_R32G32_UINT:                 format = eFormatRG32UI; break;
        case VK_FORMAT_R32G32_SFLOAT:               format = eFormatRG32F; break;
        case VK_FORMAT_D24_UNORM_S8_UINT:           format = eFormatD24S8; break;
        case VK_FORMAT_D32_SFLOAT:                  format = eFormatD32S32; break;
        default:                                    format = eFormatINVALID;
    }

    return ComputeStatus::eOk;
};

 ComputeStatus Vulkan::setSleepMode(const ReflexOptions& consts)
 {
     NVLL_VK_SET_SLEEP_MODE_PARAMS params{ 
         consts.mode != ReflexMode::eOff,
         consts.mode == ReflexMode::eLowLatencyWithBoost,
         consts.frameLimitUs 
     };
     LL_CHECK(NvLL_VK_SetSleepMode(m_device, &params));
     return ComputeStatus::eOk;
 }

 ComputeStatus Vulkan::getSleepStatus(ReflexState& settings)
 {
     NVLL_VK_GET_SLEEP_STATUS_PARAMS params{};
     LL_CHECK(NvLL_VK_GetSleepStatus(m_device, &params));
     return ComputeStatus::eOk;
 }

 ComputeStatus Vulkan::getLatencyReport(ReflexState& settings)
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

 ComputeStatus Vulkan::sleep()
 {
#ifndef SL_PRODUCTION
     bool vkValidationOn = false;
     m_parameters->get(sl::param::interposer::kVKValidationActive, &vkValidationOn);
     if(vkValidationOn) return ComputeStatus::eOk;
#endif

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

 ComputeStatus Vulkan::setReflexMarker(ReflexMarker marker, uint64_t frameId)
 {
     NVLL_VK_LATENCY_MARKER_PARAMS params{ frameId, (NVLL_VK_LATENCY_MARKER_TYPE)marker };
     LL_CHECK(NvLL_VK_SetLatencyMarker(m_device, &params));
     return ComputeStatus::eOk;
 }

 ComputeStatus Vulkan::notifyOutOfBandCommandQueue(CommandQueue queue, OutOfBandCommandQueueType type)
 {
     LL_CHECK(NvLL_VK_NotifyOutOfBandQueue(m_device, (VkQueue)((CommandQueueVk*)queue)->native, (NVLL_VK_OUT_OF_BAND_QUEUE_TYPE)type));
     return ComputeStatus::eOk;
 }
 ComputeStatus Vulkan::setAsyncFrameMarker(CommandQueue queue, ReflexMarker marker, uint64_t frameId)
 {
     NVLL_VK_LATENCY_MARKER_PARAMS params{ frameId, (NVLL_VK_LATENCY_MARKER_TYPE)marker };
     LL_CHECK(NvLL_VK_SetLatencyMarker(m_device, &params));
     return ComputeStatus::eOk;
 }
}
}

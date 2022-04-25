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

#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.plugin-manager/pluginManager.h"
#include "source/core/sl.interposer/vulkan/layer.h"

namespace sl
{
namespace interposer
{


static VkTable vk = {};

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkInstance* pInstance)
{
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    if (pCreateInfo->pApplicationInfo)
    {
        appInfo = *pCreateInfo->pApplicationInfo;
    }

    if (appInfo.apiVersion < VK_API_VERSION_1_2)
    {
        appInfo.apiVersion = VK_API_VERSION_1_2;
    }

    VkInstanceCreateInfo createInfo = *pCreateInfo;
    createInfo.pApplicationInfo = &appInfo;

#ifndef SL_PRODUCTION
    // Enable debug message tracking
    std::vector<const char*> extensions;
    for (uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
    {
        extensions.push_back(createInfo.ppEnabledExtensionNames[i]);
    }
    if (std::find(extensions.begin(), extensions.end(), std::string(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) == extensions.end())
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    if (std::find(extensions.begin(), extensions.end(), std::string(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME)) == extensions.end())
    {
        extensions.push_back(VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME);
    }
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();
#endif

    VkLayerInstanceCreateInfo* layerCreateInfo = (VkLayerInstanceCreateInfo*)pCreateInfo->pNext;

    // Step through the chain of pNext until we get to the link info
    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO))
    {
        layerCreateInfo = (VkLayerInstanceCreateInfo*)layerCreateInfo->pNext;
    }

    if (!layerCreateInfo)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vk.getInstanceProcAddr = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    // Move to the next layer
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateInstance createInstanceFunc = (PFN_vkCreateInstance)vk.getInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");

    auto res = createInstanceFunc(&createInfo, pAllocator, pInstance);
    if (res != VK_SUCCESS)
    {
        SL_LOG_ERROR("vkCreateInstance failed");
        return res;
    }

    vk.instance = *pInstance;

    vk.mapVulkanInstanceAPI(vk.instance);

    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDevice* pDevice)
{
    VkLayerDeviceCreateInfo* layerCreateInfo = (VkLayerDeviceCreateInfo*)pCreateInfo->pNext;

    // Step through the chain of pNext until we get to the link info
    while (layerCreateInfo && (layerCreateInfo->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO || layerCreateInfo->function != VK_LAYER_LINK_INFO))
    {
        layerCreateInfo = (VkLayerDeviceCreateInfo*)layerCreateInfo->pNext;
    }

    if (layerCreateInfo == NULL)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vk.getInstanceProcAddr = layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    vk.getDeviceProcAddr = layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    // Move to the next layer
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    PFN_vkCreateDevice createDeviceFunc = (PFN_vkCreateDevice)vk.getInstanceProcAddr(VK_NULL_HANDLE, "vkCreateDevice");

    // Queue family properties, used for setting up requested queues upon device creation
    uint32_t queueFamilyCount;
    vk.dispatchInstanceMap[vk.instance].GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties;
    queueFamilyProperties.resize(queueFamilyCount);
    vk.dispatchInstanceMap[vk.instance].GetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

    vk.graphicsQueueFamily = 0;
    vk.computeQueueFamily = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
    {
        if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            SL_LOG_VERBOSE("Found Vulkan graphics queue family at index %u", i);
            vk.graphicsQueueFamily = i;
        }
        else if ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            SL_LOG_VERBOSE("Found Vulkan compute queue family at index %u", i);
            vk.computeQueueFamily = i;
        }
    }

    auto createInfo = *pCreateInfo;

    // Enable extra extensions SL requires
    std::vector<const char*> extensions;
    for (uint32_t i = 0; i < createInfo.enabledExtensionCount; i++)
    {
        extensions.push_back(createInfo.ppEnabledExtensionNames[i]);
    }
    if (std::find(extensions.begin(), extensions.end(), std::string(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME)) == extensions.end())
    {
        extensions.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
    }
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkPhysicalDeviceVulkan12Features enable12Features{};
    enable12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    enable12Features.timelineSemaphore = true;
    enable12Features.pNext = (void*)createInfo.pNext;
    createInfo.pNext = &enable12Features;

    // We have to add an extra graphics and compute queues for SL workloads
    vk.computeQueueIndex = 0;
    vk.graphicsQueueIndex = 0;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t i = 0; i < createInfo.queueCreateInfoCount; i++)
    {
        queueCreateInfos.push_back(createInfo.pQueueCreateInfos[i]);
        if (createInfo.pQueueCreateInfos[i].queueFamilyIndex == vk.computeQueueFamily)
        {
            vk.computeQueueIndex++;
            queueCreateInfos.back().queueCount++;
        }
        if (createInfo.pQueueCreateInfos[i].queueFamilyIndex == vk.graphicsQueueFamily)
        {
            vk.graphicsQueueIndex++;
            queueCreateInfos.back().queueCount++;
        }
    }

    const float defaultQueuePriority = 0.0f;
    VkDeviceQueueCreateInfo queueInfo{};

    if (vk.computeQueueIndex == 0)
    {
        // We have to add compute queue explicitly since host has none
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = vk.computeQueueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &defaultQueuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();

    auto res = createDeviceFunc(physicalDevice, &createInfo, pAllocator, pDevice);

    if (res != VK_SUCCESS)
    {
        SL_LOG_ERROR("vkCreateDevice failed");
        return res;
    }

    vk.device = *pDevice;
    vk.mapVulkanDeviceAPI(*pDevice);

    sl::plugin_manager::getInterface()->setVulkanDevice(physicalDevice, *pDevice, vk.instance);

    sl::param::getInterface()->set(sl::param::global::kVulkanTable, &vk);

    return res;
}

VkResult VKAPI_CALL vkCreateImage(VkDevice Device, const VkImageCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkImage* Image)
{
    VkResult Result = vk.dispatchDeviceMap[vk.device].CreateImage(Device, CreateInfo, Allocator, Image);

    using vkCreateImage_t = VkResult(VkDevice Device, const VkImageCreateInfo* CreateInfo, const VkAllocationCallbacks* Allocator, VkImage* Image);
    const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooksWithoutLazyInit(FunctionHookID::eVulkan_CreateImage);
    for (auto hook : hooks) ((vkCreateImage_t*)hook)(Device, CreateInfo, Allocator, Image);

    return Result;
}

VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer CommandBuffer, const VkCommandBufferBeginInfo* BeginInfo)
{
    auto res = vk.dispatchDeviceMap[vk.device].BeginCommandBuffer(CommandBuffer, BeginInfo);

    using vkBeginCommandBuffer_t = void(VkCommandBuffer CommandBuffer, const VkCommandBufferBeginInfo* BeginInfo);
    const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooksWithoutLazyInit(FunctionHookID::eVulkan_BeginCommandBuffer);
    for (auto hook : hooks) ((vkBeginCommandBuffer_t*)hook)(CommandBuffer, BeginInfo);

    return res;
}

void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipeline Pipeline)
{
    vk.dispatchDeviceMap[vk.device].CmdBindPipeline(CommandBuffer, PipelineBindPoint, Pipeline);

    using vkCmdBindPipeline_t = void(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipeline Pipeline);
    const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooksWithoutLazyInit(FunctionHookID::eVulkan_CmdBindPipeline);
    for (auto hook : hooks) ((vkCmdBindPipeline_t*)hook)(CommandBuffer, PipelineBindPoint, Pipeline);
}

void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipelineLayout Layout, uint32_t FirstSet, uint32_t DescriptorSetCount, const VkDescriptorSet* DescriptorSets, uint32_t DynamicOffsetCount, const uint32_t* DynamicOffsets)
{
    vk.dispatchDeviceMap[vk.device].CmdBindDescriptorSets(CommandBuffer, PipelineBindPoint, Layout, FirstSet, DescriptorSetCount, DescriptorSets, DynamicOffsetCount, DynamicOffsets);

    using vkCmdBindDescriptorSets_t = void(VkCommandBuffer CommandBuffer, VkPipelineBindPoint PipelineBindPoint, VkPipelineLayout Layout, uint32_t FirstSet, uint32_t DescriptorSetCount, const VkDescriptorSet* DescriptorSets, uint32_t DynamicOffsetCount, const uint32_t* DynamicOffsets);
    const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooksWithoutLazyInit(FunctionHookID::eVulkan_CmdBindDescriptorSets);
    for (auto hook : hooks) ((vkCmdBindDescriptorSets_t*)hook)(CommandBuffer, PipelineBindPoint, Layout, FirstSet, DescriptorSetCount, DescriptorSets, DynamicOffsetCount, DynamicOffsets);
}

void VKAPI_CALL vkCmdWaitEvents(VkCommandBuffer CommandBuffer, uint32_t EventCount, const VkEvent* Events,
    VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask,
    uint32_t MemoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers,
    uint32_t BufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t ImageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
    vk.dispatchDeviceMap[vk.device].CmdWaitEvents(CommandBuffer, EventCount, Events, SrcStageMask, DstStageMask, MemoryBarrierCount, pMemoryBarriers,
        BufferMemoryBarrierCount, pBufferMemoryBarriers, ImageMemoryBarrierCount, pImageMemoryBarriers);
}

void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer CommandBuffer, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, VkDependencyFlags DependencyFlags,
    uint32_t MemoryBarrierCount, const VkMemoryBarrier* MemoryBarriers,
    uint32_t BufferMemoryBarrierCount, const VkBufferMemoryBarrier* BufferMemoryBarriers,
    uint32_t ImageMemoryBarrierCount, const VkImageMemoryBarrier* ImageMemoryBarriers)
{
    vk.dispatchDeviceMap[vk.device].CmdPipelineBarrier(CommandBuffer, SrcStageMask, DstStageMask, DependencyFlags, MemoryBarrierCount, MemoryBarriers, BufferMemoryBarrierCount, BufferMemoryBarriers, ImageMemoryBarrierCount, ImageMemoryBarriers);

    using vkCmdPipelineBarrier_t = void(VkCommandBuffer CommandBuffer, VkPipelineStageFlags SrcStageMask, VkPipelineStageFlags DstStageMask, VkDependencyFlags DependencyFlags,
        uint32_t MemoryBarrierCount, const VkMemoryBarrier* MemoryBarriers,
        uint32_t BufferMemoryBarrierCount, const VkBufferMemoryBarrier* BufferMemoryBarriers,
        uint32_t ImageMemoryBarrierCount, const VkImageMemoryBarrier* ImageMemoryBarriers);
    const auto& hooks = sl::plugin_manager::getInterface()->getAfterHooksWithoutLazyInit(FunctionHookID::eVulkan_CmdPipelineBarrier);
    for (auto hook : hooks) ((vkCmdPipelineBarrier_t*)hook)(CommandBuffer, SrcStageMask, DstStageMask, DependencyFlags, MemoryBarrierCount, MemoryBarriers, BufferMemoryBarrierCount, BufferMemoryBarriers, ImageMemoryBarrierCount, ImageMemoryBarriers);
}

VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice Device, const VkSwapchainCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSwapchainKHR* Swapchain)
{
    using vkCreateSwapchainKHR_t = VkResult(VkDevice Device, const VkSwapchainCreateInfoKHR* CreateInfo, const VkAllocationCallbacks* Allocator, VkSwapchainKHR* Swapchain, bool& Skip);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooks(FunctionHookID::eVulkan_CreateSwapchainKHR);
    bool skip = false;
    for (auto hook : hooks) ((vkCreateSwapchainKHR_t*)hook)(Device, CreateInfo, Allocator, Swapchain, skip);

    VkResult result = VK_SUCCESS;
    if (!skip)
    {
        result = vk.dispatchDeviceMap[vk.device].CreateSwapchainKHR(Device, CreateInfo, Allocator, Swapchain);
    }
    return result;
}

VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice Device, VkSwapchainKHR Swapchain, uint32_t* SwapchainImageCount, VkImage* SwapchainImages)
{
    using vkGetSwapchainImagesKHR_t = VkResult(VkDevice Device, VkSwapchainKHR Swapchain, uint32_t* SwapchainImageCount, VkImage* SwapchainImages, bool& Skip);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooksWithoutLazyInit(FunctionHookID::eVulkan_GetSwapchainImagesKHR);
    bool skip = false;
    for (auto hook : hooks) ((vkGetSwapchainImagesKHR_t*)hook)(Device, Swapchain, SwapchainImageCount, SwapchainImages, skip);

    VkResult result = VK_SUCCESS;
    if (!skip)
    {
        result = vk.dispatchDeviceMap[vk.device].GetSwapchainImagesKHR(Device, Swapchain, SwapchainImageCount, SwapchainImages);
    }
    return result;
}

VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice Device, VkSwapchainKHR Swapchain, uint64_t Timeout, VkSemaphore Semaphore, VkFence Fence, uint32_t* ImageIndex)
{
    using vkAcquireNextImageKHR_t = VkResult(VkDevice Device, VkSwapchainKHR Swapchain, uint64_t Timeout, VkSemaphore Semaphore, VkFence Fence, uint32_t* ImageIndex, bool& Skip);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooksWithoutLazyInit(FunctionHookID::eVulkan_AcquireNextImageKHR);
    bool skip = false;
    for (auto hook : hooks) ((vkAcquireNextImageKHR_t*)hook)(Device, Swapchain, Timeout, Semaphore, Fence, ImageIndex, skip);

    VkResult result = VK_SUCCESS;
    if (!skip)
    {
        result = vk.dispatchDeviceMap[vk.device].AcquireNextImageKHR(Device, Swapchain, Timeout, Semaphore, Fence, ImageIndex);
    }
    return result;
}

VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue Queue, const VkPresentInfoKHR* PresentInfo)
{
    using vkQueuePresentKHR_t = VkResult(VkQueue Queue, const VkPresentInfoKHR* PresentInfo, bool& Skip);
    const auto& hooks = sl::plugin_manager::getInterface()->getBeforeHooksWithoutLazyInit(FunctionHookID::eVulkan_Present);
    bool skip = false;
    for (auto hook : hooks) ((vkQueuePresentKHR_t*)hook)(Queue, PresentInfo, skip);

    VkResult Result = {};
    if (!skip)
    {
        Result = vk.dispatchDeviceMap[vk.device].QueuePresentKHR(Queue, PresentInfo);
    }
    return Result;
}

}
}

extern "C"
{

using namespace sl::interposer;
 
#define SL_INTERCEPT(F)                            \
if (strcmp(pName, #F) == 0)                        \
{                                                  \
  return (PFN_vkVoidFunction)sl::interposer::F;    \
}

PFN_vkVoidFunction VKAPI_CALL slLayerGetDeviceProcAddr(VkDevice device, const char* pName)
{
    // Redirect only the hooks we need
    SL_INTERCEPT(vkCreateInstance);
    SL_INTERCEPT(vkCreateDevice);
    SL_INTERCEPT(vkQueuePresentKHR);
    SL_INTERCEPT(vkCreateImage);
    SL_INTERCEPT(vkCmdPipelineBarrier);
    SL_INTERCEPT(vkCmdBindPipeline);
    SL_INTERCEPT(vkCmdBindDescriptorSets);
    SL_INTERCEPT(vkCreateSwapchainKHR);
    SL_INTERCEPT(vkGetSwapchainImagesKHR);
    SL_INTERCEPT(vkAcquireNextImageKHR);
    SL_INTERCEPT(vkBeginCommandBuffer);

    std::lock_guard<std::mutex> lock(vk.mutex);
    return vk.dispatchDeviceMap[device].GetDeviceProcAddr(device, pName);
}

PFN_vkVoidFunction VKAPI_CALL slLayerGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    // Redirect only the hooks we need
    SL_INTERCEPT(vkCreateInstance);
    SL_INTERCEPT(vkCreateDevice);
    SL_INTERCEPT(vkQueuePresentKHR);
    SL_INTERCEPT(vkCreateImage);
    SL_INTERCEPT(vkCmdPipelineBarrier);
    SL_INTERCEPT(vkCmdBindPipeline);
    SL_INTERCEPT(vkCmdBindDescriptorSets);
    SL_INTERCEPT(vkCreateSwapchainKHR);
    SL_INTERCEPT(vkGetSwapchainImagesKHR);
    SL_INTERCEPT(vkAcquireNextImageKHR);
    SL_INTERCEPT(vkBeginCommandBuffer);

    std::lock_guard<std::mutex> lock(vk.mutex);
    return vk.dispatchInstanceMap[instance].GetInstanceProcAddr(instance, pName);
}

} // extern "C"
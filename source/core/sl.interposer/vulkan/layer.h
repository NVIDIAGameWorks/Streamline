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

#include <mutex>

#include "external/vulkan/include/vulkan/vulkan.h"
#include "external/vulkan/include/vulkan/vk_layer.h"
#include "external/vulkan/include/vulkan/vk_layer_dispatch_table.h"

//#include "vulkannv.h"

namespace sl
{

namespace interposer
{

struct VkTable
{
    VkDevice device;
    VkInstance instance;

    PFN_vkGetDeviceProcAddr getDeviceProcAddr;
    PFN_vkGetInstanceProcAddr getInstanceProcAddr;

    uint32_t computeQueueIndex = 0;
    uint32_t computeQueueFamily = 0;
    uint32_t graphicsQueueIndex = 0;
    uint32_t graphicsQueueFamily = 0;
    uint32_t opticalFlowQueueIndex = 0;
    uint32_t opticalFlowQueueFamily = 0;

    bool nativeOpticalFlowHWSupport = false;

    std::mutex mutex;
    std::map<void*, VkLayerInstanceDispatchTable> dispatchInstanceMap;
    std::map<void*, VkLayerDispatchTable> dispatchDeviceMap;
    std::map<VkPhysicalDevice, VkInstance> instanceDeviceMap;

    void mapVulkanInstanceAPI(VkInstance instance);
    void mapVulkanDeviceAPI(VkDevice device);

    PFN_vkCreateCuModuleNVX vkCreateCuModuleNVX;
    PFN_vkCreateCuFunctionNVX vkCreateCuFunctionNVX;
    PFN_vkDestroyCuModuleNVX vkDestroyCuModuleNVX;
    PFN_vkDestroyCuFunctionNVX vkDestroyCuFunctionNVX;
    PFN_vkCmdCuLaunchKernelNVX vkCmdCuLaunchKernelNVX;
    PFN_vkGetImageViewAddressNVX vkGetImageViewAddressNVX;
    PFN_vkGetImageViewHandleNVX vkGetImageViewHandleNVX;
};

#define SL_GIPR(F) dt.##F = (PFN_vk##F)getInstanceProcAddr(instance, "vk" #F)
#define SL_GDPR(F) dt.##F = (PFN_vk##F)getDeviceProcAddr(device, "vk" #F)

inline void VkTable::mapVulkanInstanceAPI(VkInstance instance)
{
    VkLayerInstanceDispatchTable dt{};

#if defined(VK_VERSION_1_0)
    dt.GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)getInstanceProcAddr(instance, "vkGetInstanceProcAddr");
    dt.CreateDevice = (PFN_vkCreateDevice)getInstanceProcAddr(instance, "vkCreateDevice");
    dt.CreateInstance = (PFN_vkCreateInstance)getInstanceProcAddr(instance, "vkCreateInstance");
    dt.DestroyInstance = (PFN_vkDestroyInstance)getInstanceProcAddr(instance, "vkDestroyInstance");
    dt.EnumerateDeviceExtensionProperties = (PFN_vkEnumerateDeviceExtensionProperties)getInstanceProcAddr(instance, "vkEnumerateDeviceExtensionProperties");
    dt.EnumerateDeviceLayerProperties = (PFN_vkEnumerateDeviceLayerProperties)getInstanceProcAddr(instance, "vkEnumerateDeviceLayerProperties");
    dt.EnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)getInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
    dt.GetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures");
    dt.GetPhysicalDeviceFormatProperties = (PFN_vkGetPhysicalDeviceFormatProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties");
    dt.GetPhysicalDeviceImageFormatProperties = (PFN_vkGetPhysicalDeviceImageFormatProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties");
    dt.GetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties");
    dt.GetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
    dt.GetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    dt.GetPhysicalDeviceSparseImageFormatProperties = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSparseImageFormatProperties");
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
    dt.EnumeratePhysicalDeviceGroups = (PFN_vkEnumeratePhysicalDeviceGroups)getInstanceProcAddr(instance, "vkEnumeratePhysicalDeviceGroups");
    dt.GetPhysicalDeviceExternalBufferProperties = (PFN_vkGetPhysicalDeviceExternalBufferProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalBufferProperties");
    dt.GetPhysicalDeviceExternalFenceProperties = (PFN_vkGetPhysicalDeviceExternalFenceProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalFenceProperties");
    dt.GetPhysicalDeviceExternalSemaphoreProperties = (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)getInstanceProcAddr(instance, "vkGetPhysicalDeviceExternalSemaphoreProperties");
    dt.GetPhysicalDeviceFeatures2 = (PFN_vkGetPhysicalDeviceFeatures2)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2");
    dt.GetPhysicalDeviceFormatProperties2 = (PFN_vkGetPhysicalDeviceFormatProperties2)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2");
    dt.GetPhysicalDeviceImageFormatProperties2 = (PFN_vkGetPhysicalDeviceImageFormatProperties2)getInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties2");
    dt.GetPhysicalDeviceMemoryProperties2 = (PFN_vkGetPhysicalDeviceMemoryProperties2)getInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties2");
    dt.GetPhysicalDeviceProperties2 = (PFN_vkGetPhysicalDeviceProperties2)getInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2");
    dt.GetPhysicalDeviceQueueFamilyProperties2 = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)getInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties2");
    dt.GetPhysicalDeviceSparseImageFormatProperties2 = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSparseImageFormatProperties2");
#endif /* defined(VK_VERSION_1_1) */

#if defined(VK_EXT_debug_report)
    auto test = (PFN_vkCreateDebugReportCallbackEXT)getInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    dt.CreateDebugReportCallbackEXT = test;// (PFN_vkCreateDebugReportCallbackEXT)getInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    dt.DebugReportMessageEXT = (PFN_vkDebugReportMessageEXT)getInstanceProcAddr(instance, "vkDebugReportMessageEXT");
    dt.DestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)getInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
#endif /* defined(VK_EXT_debug_report) */
#if defined(VK_EXT_debug_utils)
    dt.CreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)getInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    dt.DestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)getInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    dt.SubmitDebugUtilsMessageEXT = (PFN_vkSubmitDebugUtilsMessageEXT)getInstanceProcAddr(instance, "vkSubmitDebugUtilsMessageEXT");
#endif /* defined(VK_EXT_debug_utils) */

#if defined(VK_KHR_get_physical_device_properties2)
    dt.GetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR");
    dt.GetPhysicalDeviceFormatProperties2KHR = (PFN_vkGetPhysicalDeviceFormatProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2KHR");
    dt.GetPhysicalDeviceImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceImageFormatProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceImageFormatProperties2KHR");
    dt.GetPhysicalDeviceMemoryProperties2KHR = (PFN_vkGetPhysicalDeviceMemoryProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties2KHR");
    dt.GetPhysicalDeviceProperties2KHR = (PFN_vkGetPhysicalDeviceProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
    dt.GetPhysicalDeviceQueueFamilyProperties2KHR = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties2KHR");
    dt.GetPhysicalDeviceSparseImageFormatProperties2KHR = (PFN_vkGetPhysicalDeviceSparseImageFormatProperties2KHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSparseImageFormatProperties2KHR");
#endif /* defined(VK_KHR_get_physical_device_properties2) */

#if defined(VK_KHR_surface)
    dt.DestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)getInstanceProcAddr(instance, "vkDestroySurfaceKHR");
    dt.GetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    dt.GetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
    dt.GetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    dt.GetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
#endif /* defined(VK_KHR_surface) */

#if defined(VK_KHR_win32_surface)
    dt.CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)getInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");
    dt.GetPhysicalDeviceWin32PresentationSupportKHR = (PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR)getInstanceProcAddr(instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR");
#endif /* defined(VK_KHR_win32_surface) */

    std::lock_guard<std::mutex> lock(mutex);
    dispatchInstanceMap[instance] = dt;
}

inline void VkTable::mapVulkanDeviceAPI(VkDevice device)
{
    VkLayerDispatchTable dt{};

    // Optional NVDA extensions
    vkCreateCuModuleNVX = (PFN_vkCreateCuModuleNVX)getDeviceProcAddr(device, "vkCreateCuModuleNVX");
    vkCreateCuFunctionNVX = (PFN_vkCreateCuFunctionNVX)getDeviceProcAddr(device, "vkCreateCuFunctionNVX");
    vkDestroyCuModuleNVX = (PFN_vkDestroyCuModuleNVX)getDeviceProcAddr(device, "vkDestroyCuModuleNVX");
    vkDestroyCuFunctionNVX = (PFN_vkDestroyCuFunctionNVX)getDeviceProcAddr(device, "vkDestroyCuFunctionNVX");
    vkCmdCuLaunchKernelNVX = (PFN_vkCmdCuLaunchKernelNVX)getDeviceProcAddr(device, "vkCmdCuLaunchKernelNVX");
    vkGetImageViewHandleNVX = (PFN_vkGetImageViewHandleNVX)getDeviceProcAddr(device, "vkGetImageViewHandleNVX");
    vkGetImageViewAddressNVX = (PFN_vkGetImageViewAddressNVX)getDeviceProcAddr(device, "vkGetImageViewAddressNVX");

#if defined(VK_VERSION_1_0)
    dt.GetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)getDeviceProcAddr(device, "vkGetDeviceProcAddr");
    dt.AllocateCommandBuffers = (PFN_vkAllocateCommandBuffers)getDeviceProcAddr(device, "vkAllocateCommandBuffers");
    dt.AllocateDescriptorSets = (PFN_vkAllocateDescriptorSets)getDeviceProcAddr(device, "vkAllocateDescriptorSets");
    dt.AllocateMemory = (PFN_vkAllocateMemory)getDeviceProcAddr(device, "vkAllocateMemory");
    dt.BeginCommandBuffer = (PFN_vkBeginCommandBuffer)getDeviceProcAddr(device, "vkBeginCommandBuffer");
    dt.BindBufferMemory = (PFN_vkBindBufferMemory)getDeviceProcAddr(device, "vkBindBufferMemory");
    dt.BindImageMemory = (PFN_vkBindImageMemory)getDeviceProcAddr(device, "vkBindImageMemory");
    dt.CmdBeginQuery = (PFN_vkCmdBeginQuery)getDeviceProcAddr(device, "vkCmdBeginQuery");
    dt.CmdBeginRenderPass = (PFN_vkCmdBeginRenderPass)getDeviceProcAddr(device, "vkCmdBeginRenderPass");
    dt.CmdBindDescriptorSets = (PFN_vkCmdBindDescriptorSets)getDeviceProcAddr(device, "vkCmdBindDescriptorSets");
    dt.CmdBindIndexBuffer = (PFN_vkCmdBindIndexBuffer)getDeviceProcAddr(device, "vkCmdBindIndexBuffer");
    dt.CmdBindPipeline = (PFN_vkCmdBindPipeline)getDeviceProcAddr(device, "vkCmdBindPipeline");
    dt.CmdBindVertexBuffers = (PFN_vkCmdBindVertexBuffers)getDeviceProcAddr(device, "vkCmdBindVertexBuffers");
    dt.CmdBlitImage = (PFN_vkCmdBlitImage)getDeviceProcAddr(device, "vkCmdBlitImage");
    dt.CmdClearAttachments = (PFN_vkCmdClearAttachments)getDeviceProcAddr(device, "vkCmdClearAttachments");
    dt.CmdClearColorImage = (PFN_vkCmdClearColorImage)getDeviceProcAddr(device, "vkCmdClearColorImage");
    dt.CmdClearDepthStencilImage = (PFN_vkCmdClearDepthStencilImage)getDeviceProcAddr(device, "vkCmdClearDepthStencilImage");
    dt.CmdCopyBuffer = (PFN_vkCmdCopyBuffer)getDeviceProcAddr(device, "vkCmdCopyBuffer");
    dt.CmdCopyBufferToImage = (PFN_vkCmdCopyBufferToImage)getDeviceProcAddr(device, "vkCmdCopyBufferToImage");
    dt.CmdCopyImage = (PFN_vkCmdCopyImage)getDeviceProcAddr(device, "vkCmdCopyImage");
    dt.CmdCopyImageToBuffer = (PFN_vkCmdCopyImageToBuffer)getDeviceProcAddr(device, "vkCmdCopyImageToBuffer");
    dt.CmdCopyQueryPoolResults = (PFN_vkCmdCopyQueryPoolResults)getDeviceProcAddr(device, "vkCmdCopyQueryPoolResults");
    dt.CmdDispatch = (PFN_vkCmdDispatch)getDeviceProcAddr(device, "vkCmdDispatch");
    dt.CmdDispatchIndirect = (PFN_vkCmdDispatchIndirect)getDeviceProcAddr(device, "vkCmdDispatchIndirect");
    dt.CmdDraw = (PFN_vkCmdDraw)getDeviceProcAddr(device, "vkCmdDraw");
    dt.CmdDrawIndexed = (PFN_vkCmdDrawIndexed)getDeviceProcAddr(device, "vkCmdDrawIndexed");
    dt.CmdDrawIndexedIndirect = (PFN_vkCmdDrawIndexedIndirect)getDeviceProcAddr(device, "vkCmdDrawIndexedIndirect");
    dt.CmdDrawIndirect = (PFN_vkCmdDrawIndirect)getDeviceProcAddr(device, "vkCmdDrawIndirect");
    dt.CmdEndQuery = (PFN_vkCmdEndQuery)getDeviceProcAddr(device, "vkCmdEndQuery");
    dt.CmdEndRenderPass = (PFN_vkCmdEndRenderPass)getDeviceProcAddr(device, "vkCmdEndRenderPass");
    dt.CmdExecuteCommands = (PFN_vkCmdExecuteCommands)getDeviceProcAddr(device, "vkCmdExecuteCommands");
    dt.CmdFillBuffer = (PFN_vkCmdFillBuffer)getDeviceProcAddr(device, "vkCmdFillBuffer");
    dt.CmdNextSubpass = (PFN_vkCmdNextSubpass)getDeviceProcAddr(device, "vkCmdNextSubpass");
    dt.CmdPipelineBarrier = (PFN_vkCmdPipelineBarrier)getDeviceProcAddr(device, "vkCmdPipelineBarrier");
    dt.CmdPushConstants = (PFN_vkCmdPushConstants)getDeviceProcAddr(device, "vkCmdPushConstants");
    dt.CmdResetEvent = (PFN_vkCmdResetEvent)getDeviceProcAddr(device, "vkCmdResetEvent");
    dt.CmdResetQueryPool = (PFN_vkCmdResetQueryPool)getDeviceProcAddr(device, "vkCmdResetQueryPool");
    dt.CmdResolveImage = (PFN_vkCmdResolveImage)getDeviceProcAddr(device, "vkCmdResolveImage");
    dt.CmdSetBlendConstants = (PFN_vkCmdSetBlendConstants)getDeviceProcAddr(device, "vkCmdSetBlendConstants");
    dt.CmdSetDepthBias = (PFN_vkCmdSetDepthBias)getDeviceProcAddr(device, "vkCmdSetDepthBias");
    dt.CmdSetDepthBounds = (PFN_vkCmdSetDepthBounds)getDeviceProcAddr(device, "vkCmdSetDepthBounds");
    dt.CmdSetEvent = (PFN_vkCmdSetEvent)getDeviceProcAddr(device, "vkCmdSetEvent");
    dt.CmdSetLineWidth = (PFN_vkCmdSetLineWidth)getDeviceProcAddr(device, "vkCmdSetLineWidth");
    dt.CmdSetScissor = (PFN_vkCmdSetScissor)getDeviceProcAddr(device, "vkCmdSetScissor");
    dt.CmdSetStencilCompareMask = (PFN_vkCmdSetStencilCompareMask)getDeviceProcAddr(device, "vkCmdSetStencilCompareMask");
    dt.CmdSetStencilReference = (PFN_vkCmdSetStencilReference)getDeviceProcAddr(device, "vkCmdSetStencilReference");
    dt.CmdSetStencilWriteMask = (PFN_vkCmdSetStencilWriteMask)getDeviceProcAddr(device, "vkCmdSetStencilWriteMask");
    dt.CmdSetViewport = (PFN_vkCmdSetViewport)getDeviceProcAddr(device, "vkCmdSetViewport");
    dt.CmdUpdateBuffer = (PFN_vkCmdUpdateBuffer)getDeviceProcAddr(device, "vkCmdUpdateBuffer");
    dt.CmdWaitEvents = (PFN_vkCmdWaitEvents)getDeviceProcAddr(device, "vkCmdWaitEvents");
    dt.CmdWriteTimestamp = (PFN_vkCmdWriteTimestamp)getDeviceProcAddr(device, "vkCmdWriteTimestamp");
    dt.CreateBuffer = (PFN_vkCreateBuffer)getDeviceProcAddr(device, "vkCreateBuffer");
    dt.CreateBufferView = (PFN_vkCreateBufferView)getDeviceProcAddr(device, "vkCreateBufferView");
    dt.CreateCommandPool = (PFN_vkCreateCommandPool)getDeviceProcAddr(device, "vkCreateCommandPool");
    dt.CreateComputePipelines = (PFN_vkCreateComputePipelines)getDeviceProcAddr(device, "vkCreateComputePipelines");
    dt.CreateDescriptorPool = (PFN_vkCreateDescriptorPool)getDeviceProcAddr(device, "vkCreateDescriptorPool");
    dt.CreateDescriptorSetLayout = (PFN_vkCreateDescriptorSetLayout)getDeviceProcAddr(device, "vkCreateDescriptorSetLayout");
    dt.CreateEvent = (PFN_vkCreateEvent)getDeviceProcAddr(device, "vkCreateEvent");
    dt.CreateFence = (PFN_vkCreateFence)getDeviceProcAddr(device, "vkCreateFence");
    dt.CreateFramebuffer = (PFN_vkCreateFramebuffer)getDeviceProcAddr(device, "vkCreateFramebuffer");
    dt.CreateGraphicsPipelines = (PFN_vkCreateGraphicsPipelines)getDeviceProcAddr(device, "vkCreateGraphicsPipelines");
    dt.CreateImage = (PFN_vkCreateImage)getDeviceProcAddr(device, "vkCreateImage");
    dt.CreateImageView = (PFN_vkCreateImageView)getDeviceProcAddr(device, "vkCreateImageView");
    dt.CreatePipelineCache = (PFN_vkCreatePipelineCache)getDeviceProcAddr(device, "vkCreatePipelineCache");
    dt.CreatePipelineLayout = (PFN_vkCreatePipelineLayout)getDeviceProcAddr(device, "vkCreatePipelineLayout");
    dt.CreateQueryPool = (PFN_vkCreateQueryPool)getDeviceProcAddr(device, "vkCreateQueryPool");
    dt.CreateRenderPass = (PFN_vkCreateRenderPass)getDeviceProcAddr(device, "vkCreateRenderPass");
    dt.CreateSampler = (PFN_vkCreateSampler)getDeviceProcAddr(device, "vkCreateSampler");
    dt.CreateSemaphore = (PFN_vkCreateSemaphore)getDeviceProcAddr(device, "vkCreateSemaphore");
    dt.CreateShaderModule = (PFN_vkCreateShaderModule)getDeviceProcAddr(device, "vkCreateShaderModule");
    dt.DestroyBuffer = (PFN_vkDestroyBuffer)getDeviceProcAddr(device, "vkDestroyBuffer");
    dt.DestroyBufferView = (PFN_vkDestroyBufferView)getDeviceProcAddr(device, "vkDestroyBufferView");
    dt.DestroyCommandPool = (PFN_vkDestroyCommandPool)getDeviceProcAddr(device, "vkDestroyCommandPool");
    dt.DestroyDescriptorPool = (PFN_vkDestroyDescriptorPool)getDeviceProcAddr(device, "vkDestroyDescriptorPool");
    dt.DestroyDescriptorSetLayout = (PFN_vkDestroyDescriptorSetLayout)getDeviceProcAddr(device, "vkDestroyDescriptorSetLayout");
    dt.DestroyDevice = (PFN_vkDestroyDevice)getDeviceProcAddr(device, "vkDestroyDevice");
    dt.DestroyEvent = (PFN_vkDestroyEvent)getDeviceProcAddr(device, "vkDestroyEvent");
    dt.DestroyFence = (PFN_vkDestroyFence)getDeviceProcAddr(device, "vkDestroyFence");
    dt.DestroyFramebuffer = (PFN_vkDestroyFramebuffer)getDeviceProcAddr(device, "vkDestroyFramebuffer");
    dt.DestroyImage = (PFN_vkDestroyImage)getDeviceProcAddr(device, "vkDestroyImage");
    dt.DestroyImageView = (PFN_vkDestroyImageView)getDeviceProcAddr(device, "vkDestroyImageView");
    dt.DestroyPipeline = (PFN_vkDestroyPipeline)getDeviceProcAddr(device, "vkDestroyPipeline");
    dt.DestroyPipelineCache = (PFN_vkDestroyPipelineCache)getDeviceProcAddr(device, "vkDestroyPipelineCache");
    dt.DestroyPipelineLayout = (PFN_vkDestroyPipelineLayout)getDeviceProcAddr(device, "vkDestroyPipelineLayout");
    dt.DestroyQueryPool = (PFN_vkDestroyQueryPool)getDeviceProcAddr(device, "vkDestroyQueryPool");
    dt.DestroyRenderPass = (PFN_vkDestroyRenderPass)getDeviceProcAddr(device, "vkDestroyRenderPass");
    dt.DestroySampler = (PFN_vkDestroySampler)getDeviceProcAddr(device, "vkDestroySampler");
    dt.DestroySemaphore = (PFN_vkDestroySemaphore)getDeviceProcAddr(device, "vkDestroySemaphore");
    dt.DestroyShaderModule = (PFN_vkDestroyShaderModule)getDeviceProcAddr(device, "vkDestroyShaderModule");
    dt.DeviceWaitIdle = (PFN_vkDeviceWaitIdle)getDeviceProcAddr(device, "vkDeviceWaitIdle");
    dt.EndCommandBuffer = (PFN_vkEndCommandBuffer)getDeviceProcAddr(device, "vkEndCommandBuffer");
    dt.FlushMappedMemoryRanges = (PFN_vkFlushMappedMemoryRanges)getDeviceProcAddr(device, "vkFlushMappedMemoryRanges");
    dt.FreeCommandBuffers = (PFN_vkFreeCommandBuffers)getDeviceProcAddr(device, "vkFreeCommandBuffers");
    dt.FreeDescriptorSets = (PFN_vkFreeDescriptorSets)getDeviceProcAddr(device, "vkFreeDescriptorSets");
    dt.FreeMemory = (PFN_vkFreeMemory)getDeviceProcAddr(device, "vkFreeMemory");
    dt.GetBufferMemoryRequirements = (PFN_vkGetBufferMemoryRequirements)getDeviceProcAddr(device, "vkGetBufferMemoryRequirements");
    dt.GetDeviceMemoryCommitment = (PFN_vkGetDeviceMemoryCommitment)getDeviceProcAddr(device, "vkGetDeviceMemoryCommitment");
    dt.GetDeviceQueue = (PFN_vkGetDeviceQueue)getDeviceProcAddr(device, "vkGetDeviceQueue");
    dt.GetEventStatus = (PFN_vkGetEventStatus)getDeviceProcAddr(device, "vkGetEventStatus");
    dt.GetFenceStatus = (PFN_vkGetFenceStatus)getDeviceProcAddr(device, "vkGetFenceStatus");
    dt.GetImageMemoryRequirements = (PFN_vkGetImageMemoryRequirements)getDeviceProcAddr(device, "vkGetImageMemoryRequirements");
    dt.GetImageSparseMemoryRequirements = (PFN_vkGetImageSparseMemoryRequirements)getDeviceProcAddr(device, "vkGetImageSparseMemoryRequirements");
    dt.GetImageSubresourceLayout = (PFN_vkGetImageSubresourceLayout)getDeviceProcAddr(device, "vkGetImageSubresourceLayout");
    dt.GetPipelineCacheData = (PFN_vkGetPipelineCacheData)getDeviceProcAddr(device, "vkGetPipelineCacheData");
    dt.GetQueryPoolResults = (PFN_vkGetQueryPoolResults)getDeviceProcAddr(device, "vkGetQueryPoolResults");
    dt.GetRenderAreaGranularity = (PFN_vkGetRenderAreaGranularity)getDeviceProcAddr(device, "vkGetRenderAreaGranularity");
    dt.InvalidateMappedMemoryRanges = (PFN_vkInvalidateMappedMemoryRanges)getDeviceProcAddr(device, "vkInvalidateMappedMemoryRanges");
    dt.MapMemory = (PFN_vkMapMemory)getDeviceProcAddr(device, "vkMapMemory");
    dt.MergePipelineCaches = (PFN_vkMergePipelineCaches)getDeviceProcAddr(device, "vkMergePipelineCaches");
    dt.QueueBindSparse = (PFN_vkQueueBindSparse)getDeviceProcAddr(device, "vkQueueBindSparse");
    dt.QueueSubmit = (PFN_vkQueueSubmit)getDeviceProcAddr(device, "vkQueueSubmit");
    dt.QueueWaitIdle = (PFN_vkQueueWaitIdle)getDeviceProcAddr(device, "vkQueueWaitIdle");
    dt.ResetCommandBuffer = (PFN_vkResetCommandBuffer)getDeviceProcAddr(device, "vkResetCommandBuffer");
    dt.ResetCommandPool = (PFN_vkResetCommandPool)getDeviceProcAddr(device, "vkResetCommandPool");
    dt.ResetDescriptorPool = (PFN_vkResetDescriptorPool)getDeviceProcAddr(device, "vkResetDescriptorPool");
    dt.ResetEvent = (PFN_vkResetEvent)getDeviceProcAddr(device, "vkResetEvent");
    dt.ResetFences = (PFN_vkResetFences)getDeviceProcAddr(device, "vkResetFences");
    dt.SetEvent = (PFN_vkSetEvent)getDeviceProcAddr(device, "vkSetEvent");
    dt.UnmapMemory = (PFN_vkUnmapMemory)getDeviceProcAddr(device, "vkUnmapMemory");
    dt.UpdateDescriptorSets = (PFN_vkUpdateDescriptorSets)getDeviceProcAddr(device, "vkUpdateDescriptorSets");
    dt.WaitForFences = (PFN_vkWaitForFences)getDeviceProcAddr(device, "vkWaitForFences");
#endif /* defined(VK_VERSION_1_0) */
#if defined(VK_VERSION_1_1)
    dt.BindBufferMemory2 = (PFN_vkBindBufferMemory2)getDeviceProcAddr(device, "vkBindBufferMemory2");
    dt.BindImageMemory2 = (PFN_vkBindImageMemory2)getDeviceProcAddr(device, "vkBindImageMemory2");
    dt.CmdDispatchBase = (PFN_vkCmdDispatchBase)getDeviceProcAddr(device, "vkCmdDispatchBase");
    dt.CmdSetDeviceMask = (PFN_vkCmdSetDeviceMask)getDeviceProcAddr(device, "vkCmdSetDeviceMask");
    dt.CreateDescriptorUpdateTemplate = (PFN_vkCreateDescriptorUpdateTemplate)getDeviceProcAddr(device, "vkCreateDescriptorUpdateTemplate");
    dt.CreateSamplerYcbcrConversion = (PFN_vkCreateSamplerYcbcrConversion)getDeviceProcAddr(device, "vkCreateSamplerYcbcrConversion");
    dt.DestroyDescriptorUpdateTemplate = (PFN_vkDestroyDescriptorUpdateTemplate)getDeviceProcAddr(device, "vkDestroyDescriptorUpdateTemplate");
    dt.DestroySamplerYcbcrConversion = (PFN_vkDestroySamplerYcbcrConversion)getDeviceProcAddr(device, "vkDestroySamplerYcbcrConversion");
    dt.GetBufferMemoryRequirements2 = (PFN_vkGetBufferMemoryRequirements2)getDeviceProcAddr(device, "vkGetBufferMemoryRequirements2");
    dt.GetDescriptorSetLayoutSupport = (PFN_vkGetDescriptorSetLayoutSupport)getDeviceProcAddr(device, "vkGetDescriptorSetLayoutSupport");
    dt.GetDeviceGroupPeerMemoryFeatures = (PFN_vkGetDeviceGroupPeerMemoryFeatures)getDeviceProcAddr(device, "vkGetDeviceGroupPeerMemoryFeatures");
    dt.GetDeviceQueue2 = (PFN_vkGetDeviceQueue2)getDeviceProcAddr(device, "vkGetDeviceQueue2");
    dt.GetImageMemoryRequirements2 = (PFN_vkGetImageMemoryRequirements2)getDeviceProcAddr(device, "vkGetImageMemoryRequirements2");
    dt.GetImageSparseMemoryRequirements2 = (PFN_vkGetImageSparseMemoryRequirements2)getDeviceProcAddr(device, "vkGetImageSparseMemoryRequirements2");
    dt.TrimCommandPool = (PFN_vkTrimCommandPool)getDeviceProcAddr(device, "vkTrimCommandPool");
    dt.UpdateDescriptorSetWithTemplate = (PFN_vkUpdateDescriptorSetWithTemplate)getDeviceProcAddr(device, "vkUpdateDescriptorSetWithTemplate");
#endif /* defined(VK_VERSION_1_1) */
#if defined(VK_VERSION_1_2)
    dt.CmdBeginRenderPass2 = (PFN_vkCmdBeginRenderPass2)getDeviceProcAddr(device, "vkCmdBeginRenderPass2");
    dt.CmdDrawIndexedIndirectCount = (PFN_vkCmdDrawIndexedIndirectCount)getDeviceProcAddr(device, "vkCmdDrawIndexedIndirectCount");
    dt.CmdDrawIndirectCount = (PFN_vkCmdDrawIndirectCount)getDeviceProcAddr(device, "vkCmdDrawIndirectCount");
    dt.CmdEndRenderPass2 = (PFN_vkCmdEndRenderPass2)getDeviceProcAddr(device, "vkCmdEndRenderPass2");
    dt.CmdNextSubpass2 = (PFN_vkCmdNextSubpass2)getDeviceProcAddr(device, "vkCmdNextSubpass2");
    dt.CreateRenderPass2 = (PFN_vkCreateRenderPass2)getDeviceProcAddr(device, "vkCreateRenderPass2");
    dt.GetBufferDeviceAddress = (PFN_vkGetBufferDeviceAddress)getDeviceProcAddr(device, "vkGetBufferDeviceAddress");
    dt.GetBufferOpaqueCaptureAddress = (PFN_vkGetBufferOpaqueCaptureAddress)getDeviceProcAddr(device, "vkGetBufferOpaqueCaptureAddress");
    dt.GetDeviceMemoryOpaqueCaptureAddress = (PFN_vkGetDeviceMemoryOpaqueCaptureAddress)getDeviceProcAddr(device, "vkGetDeviceMemoryOpaqueCaptureAddress");
    dt.GetSemaphoreCounterValue = (PFN_vkGetSemaphoreCounterValue)getDeviceProcAddr(device, "vkGetSemaphoreCounterValue");
    dt.ResetQueryPool = (PFN_vkResetQueryPool)getDeviceProcAddr(device, "vkResetQueryPool");
    dt.SignalSemaphore = (PFN_vkSignalSemaphore)getDeviceProcAddr(device, "vkSignalSemaphore");
    dt.WaitSemaphores = (PFN_vkWaitSemaphores)getDeviceProcAddr(device, "vkWaitSemaphores");
#endif /* defined(VK_VERSION_1_2) */

#if defined(VK_EXT_debug_marker)
    dt.CmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)getDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT");
    dt.CmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)getDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT");
    dt.CmdDebugMarkerInsertEXT = (PFN_vkCmdDebugMarkerInsertEXT)getDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT");
    dt.DebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)getDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT");
    dt.DebugMarkerSetObjectTagEXT = (PFN_vkDebugMarkerSetObjectTagEXT)getDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT");
#endif /* defined(VK_EXT_debug_marker) */

#if defined(VK_KHR_swapchain)
    dt.AcquireNextImageKHR = (PFN_vkAcquireNextImageKHR)getDeviceProcAddr(device, "vkAcquireNextImageKHR");
    dt.CreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)getDeviceProcAddr(device, "vkCreateSwapchainKHR");
    dt.DestroySwapchainKHR = (PFN_vkDestroySwapchainKHR)getDeviceProcAddr(device, "vkDestroySwapchainKHR");
    dt.GetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR)getDeviceProcAddr(device, "vkGetSwapchainImagesKHR");
    dt.QueuePresentKHR = (PFN_vkQueuePresentKHR)getDeviceProcAddr(device, "vkQueuePresentKHR");
#endif /* defined(VK_KHR_swapchain) */

#if defined(VK_KHR_get_memory_requirements2)
    dt.GetBufferMemoryRequirements2KHR = (PFN_vkGetBufferMemoryRequirements2KHR)getDeviceProcAddr(device, "vkGetBufferMemoryRequirements2KHR");
    dt.GetImageMemoryRequirements2KHR = (PFN_vkGetImageMemoryRequirements2KHR)getDeviceProcAddr(device, "vkGetImageMemoryRequirements2KHR");
    dt.GetImageSparseMemoryRequirements2KHR = (PFN_vkGetImageSparseMemoryRequirements2KHR)getDeviceProcAddr(device, "vkGetImageSparseMemoryRequirements2KHR");
#endif /* defined(VK_KHR_get_memory_requirements2) */

#if defined(VK_KHR_push_descriptor)
    dt.CmdPushDescriptorSetKHR = (PFN_vkCmdPushDescriptorSetKHR)getDeviceProcAddr(device, "vkCmdPushDescriptorSetKHR");
#endif /* defined(VK_KHR_push_descriptor) */

#if defined(VK_EXT_debug_utils)
    SL_GDPR(SetDebugUtilsObjectNameEXT);
    SL_GDPR(SetDebugUtilsObjectTagEXT);
    SL_GDPR(QueueBeginDebugUtilsLabelEXT);
    SL_GDPR(QueueEndDebugUtilsLabelEXT);
    SL_GDPR(QueueInsertDebugUtilsLabelEXT);
    SL_GDPR(CmdBeginDebugUtilsLabelEXT);
    SL_GDPR(CmdEndDebugUtilsLabelEXT);
    SL_GDPR(CmdInsertDebugUtilsLabelEXT);
#endif /* defined(VK_EXT_debug_utils) */

#if defined(VK_VERSION_1_3)
    SL_GDPR(CreatePrivateDataSlot);
    SL_GDPR(DestroyPrivateDataSlot);
    SL_GDPR(SetPrivateData);
    SL_GDPR(GetPrivateData);
    SL_GDPR(CmdSetEvent2);
    SL_GDPR(CmdResetEvent2);
    SL_GDPR(CmdWaitEvents2);
    SL_GDPR(CmdPipelineBarrier2);
    SL_GDPR(CmdWriteTimestamp2);
    SL_GDPR(QueueSubmit2);
    SL_GDPR(CmdCopyBuffer2);
    SL_GDPR(CmdCopyImage2);
    SL_GDPR(CmdCopyBufferToImage2);
    SL_GDPR(CmdCopyImageToBuffer2);
    SL_GDPR(CmdBlitImage2);
    SL_GDPR(CmdResolveImage2);
    SL_GDPR(CmdBeginRendering);
    SL_GDPR(CmdEndRendering);
    SL_GDPR(CmdSetCullMode);
    SL_GDPR(CmdSetFrontFace);
    SL_GDPR(CmdSetPrimitiveTopology);
    SL_GDPR(CmdSetViewportWithCount);
    SL_GDPR(CmdSetScissorWithCount);
    SL_GDPR(CmdBindVertexBuffers2);
    SL_GDPR(CmdSetDepthTestEnable);
    SL_GDPR(CmdSetDepthWriteEnable);
    SL_GDPR(CmdSetDepthCompareOp);
    SL_GDPR(CmdSetDepthBoundsTestEnable);
    SL_GDPR(CmdSetStencilTestEnable);
    SL_GDPR(CmdSetStencilOp);
    SL_GDPR(CmdSetRasterizerDiscardEnable);
    SL_GDPR(CmdSetDepthBiasEnable);
    SL_GDPR(CmdSetPrimitiveRestartEnable);
    SL_GDPR(GetDeviceBufferMemoryRequirements);
    SL_GDPR(GetDeviceImageMemoryRequirements);
    SL_GDPR(GetDeviceImageSparseMemoryRequirements);
#endif

    std::lock_guard<std::mutex> lock(mutex);
    dispatchDeviceMap[device] = dt;
}

}
}

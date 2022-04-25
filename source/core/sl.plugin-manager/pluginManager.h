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

#include <vector>
#include <map>

#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

struct ID3D11Device;
struct ID3D12Device;

enum class FunctionHookID : uint32_t
{
    eIDXGIFactory_CreateSwapChain,
    eIDXGIFactory2_CreateSwapChainForHwnd,
    eIDXGIFactory2_CreateSwapChainForCoreWindow,
    eIDXGISwapChain_Present,
    eIDXGISwapChain_GetBuffer,
    eIDXGISwapChain_ResizeBuffers,
    eIDXGISwapChain_GetCurrentBackBufferIndex,
    eIDXGISwapChain_SetFullscreenState,
    eID3D12Device_CreateCommittedResource,
    eID3D12Device_CreatePlacedResource,
    eID3D12Device_CreateReservedResource,
    eID3D12GraphicsCommandList_ResourceBarrier,
    eVulkan_BeginCommandBuffer,
    eVulkan_CmdBindPipeline,
    eVulkan_Present,
    eVulkan_CmdPipelineBarrier,
    eVulkan_CmdBindDescriptorSets,
    eVulkan_CreateSwapchainKHR,
    eVulkan_GetSwapchainImagesKHR,
    eVulkan_AcquireNextImageKHR,
    eVulkan_CreateImage,
    eMaxNum
};

namespace sl
{

struct Preferences;

namespace param
{
    struct Parameters;
}
namespace interposer
{
using VirtualAddress = void*;
}

enum Feature : uint32_t;

namespace plugin_manager
{

using HookList = std::vector<interposer::VirtualAddress>;
struct FeatureParameters
{
    std::string supportedAdapters;
    std::string setConstants;
    std::string getSettings;
};

struct IPluginManager
{
    virtual bool loadPlugins() = 0;
    virtual void unloadPlugins() = 0;
    virtual bool initializePlugins() = 0;

    virtual const HookList& getBeforeHooks(FunctionHookID functionHookID) = 0;
    virtual const HookList& getAfterHooks(FunctionHookID functionHookID) = 0;
    virtual const HookList& getBeforeHooksWithoutLazyInit(FunctionHookID functionHookID) = 0;
    virtual const HookList& getAfterHooksWithoutLazyInit(FunctionHookID functionHookID) = 0;

    virtual bool setFeatureEnabled(Feature feature, bool value) = 0;
    virtual void setPreferences(const Preferences& pref) = 0;
    virtual void setApplicationId(int appId) = 0;
    virtual void setD3D12Device(ID3D12Device* device) = 0;
    virtual void setD3D11Device(ID3D11Device* device) = 0;
    virtual void setVulkanDevice(VkPhysicalDevice physicalDevice, VkDevice device, VkInstance instance) = 0;
    
    virtual bool isSupportedOnThisMachine() const = 0;
    virtual bool isRunningD3D12() const = 0;
    virtual bool isRunningD3D11() const = 0;
    virtual bool isRunningVulkan() const = 0;
    virtual bool isInitialized() const = 0;
    virtual bool arePluginsLoaded() const = 0;
    virtual const FeatureParameters* getFeatureParameters(Feature feature) const = 0;
};

IPluginManager* getInterface();
void destroyInterface();

}
}
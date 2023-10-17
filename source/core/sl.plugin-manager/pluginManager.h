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

#include "include/sl_hooks.h"
#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

struct ID3D12Device;
struct ID3D11Device;

namespace sl
{

struct Preferences;

using CommandBuffer = void;

namespace param
{
    struct Parameters;
}
namespace interposer
{
using VirtualAddress = void*;
}

using Feature = uint32_t;

namespace plugin_manager
{

using HookPair = std::pair<interposer::VirtualAddress, sl::Feature>;
using HookList = std::vector<HookPair>;

using PFun_slSetDataInternal = Result(const sl::BaseStructure* inputs, sl::CommandBuffer* cmdBuffer);
using PFun_slGetDataInternal = Result(const sl::BaseStructure* inputs, sl::BaseStructure* outputs, sl::CommandBuffer* cmdBuffer);
using PFun_slIsSupported = Result(const sl::AdapterInfo& adapterInfo);

struct FeatureContext
{
    bool initialized = false;
    bool enabled = true;
    uint32_t supportedAdapters = 0;
    api::PFuncGetPluginFunction* getFunction{};
    PFun_slSetDataInternal* setData{};
    PFun_slGetDataInternal* getData{};
    PFun_slAllocateResources* allocResources{};
    PFun_slFreeResources* freeResources{};
    PFun_slEvaluateFeature* evaluate{};
    PFun_slSetTag* setTag{};
    PFun_slSetConstants* setConstants{};
    PFun_slIsSupported* isSupported{};
};

struct IPluginManager
{
    virtual Result loadPlugins() = 0;
    virtual void unloadPlugins() = 0;
    virtual Result initializePlugins() = 0;

    virtual const HookList& getBeforeHooks(FunctionHookID functionHookID) = 0;
    virtual const HookList& getAfterHooks(FunctionHookID functionHookID) = 0;
    virtual const HookList& getBeforeHooksWithoutLazyInit(FunctionHookID functionHookID) = 0;
    virtual const HookList& getAfterHooksWithoutLazyInit(FunctionHookID functionHookID) = 0;

    virtual Result setHostSDKVersion(uint64_t sdkVersion) = 0;
    virtual Result setFeatureEnabled(Feature feature, bool value) = 0;
    virtual void setPreferences(const Preferences& pref) = 0;
    virtual const Preferences& getPreferences() const = 0;
    virtual void setApplicationId(int appId) = 0;
    virtual void setD3D12Device(ID3D12Device* device) = 0;
    virtual void setD3D11Device(ID3D11Device* device) = 0;
    virtual void setVulkanDevice(VkPhysicalDevice physicalDevice, VkDevice device, VkInstance instance) = 0;

    virtual ID3D12Device* getD3D12Device() const = 0;
    virtual ID3D11Device* getD3D11Device() const = 0;
    virtual VkDevice getVulkanDevice() const = 0;

    virtual bool isProxyNeeded(const char* className) = 0;
    virtual bool isInitialized() const = 0;
    virtual bool arePluginsLoaded() const = 0;

    virtual const Version& getHostSDKVersion() = 0;

    virtual const FeatureContext* getFeatureContext(Feature feature) = 0;

    virtual bool getExternalFeatureConfig(Feature feature, const char** configAsText) = 0;
    virtual bool getLoadedFeatureConfigs(std::vector<json>& configList) const = 0;
    virtual bool getLoadedFeatures(std::vector<Feature>& featureList) const = 0;
};

IPluginManager* getInterface();
void destroyInterface();

}
}

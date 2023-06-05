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

#ifdef SL_WINDOWS
#include <d3d11.h>
#include "source/core/sl.interposer/d3d12/d3d12.h"
#include "source/core/sl.interposer/d3d12/d3d12Device.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandQueue.h"
#include "source/core/sl.interposer/d3d12/d3d12CommandList.h"
#include "source/core/sl.interposer/dxgi/dxgiFactory.h"
#include "source/core/sl.interposer/dxgi/dxgiSwapchain.h"
#include "external/vulkan/include/vulkan/vulkan.h"
#include <versionhelpers.h>
#else
#include <unistd.h>
#endif
#include <chrono>

#include "include/sl.h"
#include "include/sl_dlss_g.h"
#include "include/sl_hooks.h"
#include "internal.h"
#include "source/core/sl.exception/exception.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.interposer/hook.h"
#include "source/core/sl.plugin-manager/pluginManager.h"
#include "include/sl_helpers.h"
#include "include/sl_helpers_vk.h"

using namespace sl;

//! API

inline sl::Result slValidateState()
{
    if (!plugin_manager::getInterface()->arePluginsLoaded())
    {
        SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");
        return sl::Result::eErrorNotInitialized;
    }
    return sl::Result::eOk;
}


inline sl::Result slValidateFeatureContext(sl::Feature f, const sl::plugin_manager::FeatureContext*& ctx)
{
    ctx = plugin_manager::getInterface()->getFeatureContext(f);
    if (!ctx)
    {
        SL_LOG_ERROR("'%s' is missing.", getFeatureAsStr(f));
        return Result::eErrorFeatureMissing;
    }
    if (!ctx->supportedAdapters)
    {
        SL_LOG_ERROR("'%s' is not supported.", getFeatureAsStr(f));
        return Result::eErrorFeatureNotSupported;
    }
    return Result::eOk;
}

struct FrameHandleImplementation : public FrameToken
{
    FrameHandleImplementation() {};

    virtual operator uint32_t() const override final { return counter.load(); };

    std::atomic<uint32_t> counter{};
};

//! Normally host would work with no more than 2 frames at the same time but sl.reflex sometimes 
//! needs to send markers for previous and next frame so the total number of inflight frames can be higher
constexpr uint32_t kMaxNumFrameHandles = 6;

struct APIContext
{
    std::mutex mtxFrameHandle{};
    uint32_t frameCounter = 0;
    uint32_t frameHandleIndex = 0;
    FrameHandleImplementation frameHandles[kMaxNumFrameHandles];

    std::map<Feature, std::pair<size_t, BufferType*>> requiredTags;
    std::map<Feature, std::pair<size_t, char**>> vkInstanceExtensions;
    std::map<Feature, std::pair<size_t, char**>> vkDeviceExtensions;
    std::map<Feature, std::pair<size_t, char**>> vkFeatures12;
    std::map<Feature, std::pair<size_t, char**>> vkFeatures13;
    std::map<Feature, std::pair<size_t, char**>> vkFeaturesOpticalflowNV;
};

APIContext s_ctx;

sl::Result slInit(const Preferences &pref, uint64_t sdkVersion)
{
    //! IMPORTANT:
    //! 
    //! As explained in sl_struct.h any new elements must be placed at the end
    //! of each structure and version must be increased or new elements can be
    //! added in a new structure which is then chained. This assert ensures
    //! that new element(s) are NOT added in the middle of a structure.
    static_assert(offsetof(sl::Preferences, renderAPI) == 136, "new elements can only be added at the end of each structure");

    auto init = [&pref, &sdkVersion]()->sl::Result
    {
        // Setup logging first
        auto log = log::getInterface();
        log->enableConsole(pref.showConsole);
        log->setLogLevel(pref.logLevel);
        log->setLogPath(pref.pathToLogsAndData);
        log->setLogName(L"sl.log");
        log->setLogCallback((void*)pref.logMessageCallback);

        if (sl::interposer::getInterface()->isEnabled())
        {
            // NOTE: Defaults to true but host can override this since 
            // some games might not work correctly with a proxy DXGI.
            bool useDXGIProxy = pref.flags & PreferenceFlags::eUseDXGIFactoryProxy;
            sl::interposer::getInterface()->setUseDXGIProxy(useDXGIProxy);

#ifndef SL_PRODUCTION
            // Allow overrides via 'sl.interposer.json'
            if (!sl::interposer::getInterface()->getConfigPath().empty())
            {
                auto config = sl::interposer::getInterface()->getConfig();
                log->enableConsole(config.showConsole);
                if (!config.logPath.empty())
                {
                    log->setLogPath(extra::toWStr(config.logPath).c_str());
                }
                auto level = std::clamp(config.logLevel, 0U, 2U);
                log->setLogLevel((LogLevel)level);
                log->setLogMessageDelay(config.logMessageDelayMs);
                SL_LOG_HINT("Overriding interposer settings with values from %S\\sl.interposer.json", sl::interposer::getInterface()->getConfigPath().c_str());
                if (config.waitForDebugger)
                {
                    SL_LOG_INFO("Waiting for debugger to attach ...");
#ifdef SL_WINDOWS
                    while (!IsDebuggerPresent())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
#endif
                }
            }
#endif

            // Check to see if RenderDoc is present and notify the user
#ifdef SL_WINDOWS
            HMODULE renderDocMod = GetModuleHandleA("renderdoc.dll");
            if (renderDocMod)
            {
                SL_LOG_WARN("RenderDoc has been detected.  As RenderDoc disables NVAPI, any plugins which require NVAPI will be disabled.");
            }
#endif
#ifdef SL_LINUX
            void* renderDocMod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD);
            if (renderDocMod)
            {
                SL_LOG_WARN("RenderDoc has been detected.  As RenderDoc disables NVAPI, any plugins which require NVAPI will be disabled.");
            }
#endif

            auto manager = plugin_manager::getInterface();
            if (manager->isInitialized())
            {
                SL_LOG_ERROR( "slInit must be called before any DXGI/D3D12/D3D11/Vulkan APIs are invoked");
                return Result::eErrorInitNotCalled;
            }
            
            if (SL_FAILED(result, manager->setHostSDKVersion(sdkVersion)))
            {
                return result;
            }

            manager->setPreferences(pref);

            param::getInterface()->set(param::global::kPFunAllocateResource, pref.allocateCallback);
            param::getInterface()->set(param::global::kPFunReleaseResource, pref.releaseCallback);
            param::getInterface()->set(param::global::kLogInterface, log::getInterface());

            // Enumerate plugins and check if they are supported or not
            return manager->loadPlugins();
        }

        return Result::eOk;
    };

    SL_EXCEPTION_HANDLE_START
    return init();
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler)
}

Result slShutdown()
{
    SL_EXCEPTION_HANDLE_START;
    
    auto freeDynamicStrings = [](std::map<Feature, std::pair<size_t, char**>>& list)->void
    {
        for (auto& e : list)
        {
            auto i = e.second.first;
            while (i--)
            {
                delete[] e.second.second[i];
            }
            delete[] e.second.second;
        }
        list.clear();
    };

    freeDynamicStrings(s_ctx.vkDeviceExtensions);
    freeDynamicStrings(s_ctx.vkInstanceExtensions);
    freeDynamicStrings(s_ctx.vkFeatures12);
    freeDynamicStrings(s_ctx.vkFeatures13);

    auto manager = plugin_manager::getInterface();
    if (!manager->arePluginsLoaded())
    {
        SL_LOG_ERROR_ONCE("SL not initialized");
        return Result::eErrorNotInitialized;
    }
    manager->unloadPlugins();

    plugin_manager::destroyInterface();
    param::destroyInterface();
    log::destroyInterface();
    interposer::destroyInterface();

    return Result::eOk;

    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slIsFeatureLoaded(sl::Feature feature, bool& enabled)
{
    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    const sl::plugin_manager::FeatureContext* ctx;
    SL_CHECK(slValidateFeatureContext(feature, ctx));
    enabled = ctx->enabled;
    return Result::eOk;
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slSetFeatureLoaded(sl::Feature feature, bool enabled)
{
    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    return plugin_manager::getInterface()->setFeatureEnabled(feature, enabled);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slSetTag(const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer)
{
    //! IMPORTANT:
    //! 
    //! As explained in sl_struct.h any new elements must be placed at the end
    //! of each structure and version must be increased or new elements can be
    //! added in a new structure which is then chained. This assert ensures
    //! that new element(s) are NOT added in the middle of a structure.
    static_assert(offsetof(sl::ResourceTag, extent) == 48, "new elements can only be added at the end of each structure");
    static_assert(offsetof(sl::Resource, reserved) == 104, "new elements can only be added at the end of each structure");

    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    const sl::plugin_manager::FeatureContext* ctx;
    SL_CHECK(slValidateFeatureContext(kFeatureCommon, ctx));
    if (!tags || numTags == 0) return Result::eErrorInvalidParameter;
    return ctx->setTag(viewport, tags, numTags, cmdBuffer);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slSetConstants(const Constants& values, const FrameToken& frame, const ViewportHandle& viewport)
{
    //! IMPORTANT:
    //! 
    //! As explained in sl_struct.h any new elements must be placed at the end
    //! of each structure and version must be increased or new elements can be
    //! added in a new structure which is then chained. This assert ensures
    //! that new element(s) are NOT added in the middle of a structure.
    static_assert(offsetof(sl::Constants, motionVectorsJittered) == 450, "new elements can only be added at the end of each structure");

    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    const sl::plugin_manager::FeatureContext* ctx;
    SL_CHECK(slValidateFeatureContext(kFeatureCommon, ctx));
    return ctx->setConstants(values, frame, viewport);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slAllocateResources(sl::CommandBuffer* cmdBuffer, sl::Feature feature, const sl::ViewportHandle& viewport)
{
    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    const sl::plugin_manager::FeatureContext* ctx;
    SL_CHECK(slValidateFeatureContext(feature, ctx));
    if (!ctx->allocResources)
    {
        SL_LOG_WARN_ONCE("Unable to obtain callback 'allocateResource', plugin does not support explicit resource allocation.");
        return Result::eErrorMissingOrInvalidAPI;
    }
    return ctx->allocResources(cmdBuffer, feature, viewport);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slFreeResources(sl::Feature feature, const sl::ViewportHandle& viewport)
{
    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    const sl::plugin_manager::FeatureContext* ctx;
    SL_CHECK(slValidateFeatureContext(feature, ctx));
    if (!ctx->freeResources)
    {
        SL_LOG_WARN_ONCE("Unable to obtain callback 'freeResources', plugin does not support explicit resource deallocation.");
        return Result::eErrorMissingOrInvalidAPI;
    }
    return ctx->freeResources(feature, viewport);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slEvaluateFeature(sl::Feature feature, const sl::FrameToken& frame, const sl::BaseStructure** inputs, uint32_t numInputs, sl::CommandBuffer* cmdBuffer)
{
    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    const sl::plugin_manager::FeatureContext* ctx;
    SL_CHECK(slValidateFeatureContext(sl::kFeatureCommon, ctx));
    return ctx->evaluate(feature, frame, inputs, numInputs, cmdBuffer);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slSetVulkanInfo(const sl::VulkanInfo& info)
{
    //! IMPORTANT:
    //! 
    //! As explained in sl_struct.h any new elements must be placed at the end
    //! of each structure and version must be increased or new elements can be
    //! added in a new structure which is then chained. This assert ensures
    //! that new element(s) are NOT added in the middle of a structure.
    static_assert(offsetof(sl::VulkanInfo, useNativeOpticalFlowMode) == 80, "new elements can only be added at the end of each structure");

    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    extern sl::Result processVulkanInterface(const sl::VulkanInfo*);
    if (SL_FAILED(result, processVulkanInterface(&info)))
    {
        return result;
    }
    auto pluginManager = sl::plugin_manager::getInterface();
    pluginManager->setVulkanDevice(info.physicalDevice, info.device, info.instance);
    // We have the device info so we can initialize our plugins
    return pluginManager->initializePlugins();
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler)
}

Result slSetD3DDevice(void* baseInterface)
{
    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());

    auto unknown = static_cast<IUnknown*>(baseInterface);

    auto pluginManager = sl::plugin_manager::getInterface();
    
    {
        interposer::D3D12Device* d3d12Device{};
        if (SUCCEEDED(unknown->QueryInterface(&d3d12Device)))
        {
            d3d12Device->Release();
            pluginManager->setD3D12Device(d3d12Device->m_base);
            return pluginManager->initializePlugins();
        }
    }

    {
        ID3D12Device* d3d12Device{};
        if (SUCCEEDED(unknown->QueryInterface(&d3d12Device)))
        {
            d3d12Device->Release();
            pluginManager->setD3D12Device(d3d12Device);
            return pluginManager->initializePlugins();
        }
    }
    
    {
        ID3D11Device* d3d11Device{};
        if (SUCCEEDED(unknown->QueryInterface(&d3d11Device)))
        {
            d3d11Device->Release();
            pluginManager->setD3D11Device(d3d11Device);
            return pluginManager->initializePlugins();
        }
    }

    SL_LOG_ERROR( "Unknown interface provided - expecting ID3D12Device or ID3D11Device");

    return Result::eErrorUnsupportedInterface;
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler)
}

Result slGetNativeInterface(void* proxyInterface, void** baseInterface)
{
    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());

    if (!proxyInterface || !baseInterface)
    {
        SL_LOG_ERROR("Missing inputs parameters");
        return Result::eErrorInvalidParameter;
    }

#ifndef SL_PRODUCTION
    // For research and debugging purposes we can force proxies in non-production builds
    if (sl::interposer::getInterface()->getConfig().forceProxies)
    {
        *baseInterface = proxyInterface;
        return Result::eOk;
    }
#endif

    // This must be a IUnknown interface
    auto unknown = static_cast<IUnknown*>(proxyInterface);
    
    sl::interposer::D3D12Device* d3d12Proxy{};
    if (SUCCEEDED(unknown->QueryInterface(&d3d12Proxy)))
    {
        d3d12Proxy->Release();
        *baseInterface = d3d12Proxy->m_base;
        d3d12Proxy->m_base->AddRef();
        return Result::eOk;
    }
    sl::interposer::DXGIFactory* factoryProxy{};
    if (SUCCEEDED(unknown->QueryInterface(&factoryProxy)))
    {
        factoryProxy->Release();
        *baseInterface = factoryProxy->m_base;
        factoryProxy->m_base->AddRef();
        return Result::eOk;
    }
    sl::interposer::DXGISwapChain* swapChainProxy{};
    if (SUCCEEDED(unknown->QueryInterface(&swapChainProxy)))
    {
        swapChainProxy->Release();
        *baseInterface = swapChainProxy->m_base;
        swapChainProxy->m_base->AddRef();
        return Result::eOk;
    }
    sl::interposer::D3D12CommandQueue* queue{};
    if (SUCCEEDED(unknown->QueryInterface(&queue)))
    {
        queue->Release();
        *baseInterface = queue->m_base;
        queue->m_base->AddRef();
        return Result::eOk;
    }
    sl::interposer::D3D12GraphicsCommandList* list{};
    if (SUCCEEDED(unknown->QueryInterface(&list)))
    {
        list->Release();
        *baseInterface = list->m_base;
        list->m_base->AddRef();
        return Result::eOk;
    }

    //! Host has passed in something that is not an SL proxy
    //! 
    //! This can happen when SL is not linked directly so it
    //! is required to call slUpgradeInterface manually.
    *baseInterface = proxyInterface;
    // Making sure to add reference
    unknown->AddRef();
    return Result::eOk;

    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler)
}

Result slUpgradeInterface(void** baseInterface)
{
    auto upgradeInterface = [baseInterface]()-> Result
    {
        SL_CHECK(slValidateState());

#if SL_WINDOWS
        if (!baseInterface || !*baseInterface)
        {
            SL_LOG_ERROR( "Missing input interface");
            return Result::eErrorMissingInputParameter;
        }

        if (!sl::interposer::getInterface()->isEnabled())
        {
            // When interposer is disabled we do not provide any proxies, host just uses base interface as is
            return Result::eOk;
        }

        bool proxiesEnabledByDefault = (plugin_manager::getInterface()->getPreferences().flags & PreferenceFlags::eUseManualHooking) == 0;

        // This must be IUnknown interface
        auto unknown = static_cast<IUnknown*>(*baseInterface);
        // First check if this is SL proxy
        StreamlineRetrieveBaseInterface* base{};
        if (SUCCEEDED(unknown->QueryInterface(&base)))
        {
            base->Release();
            if (proxiesEnabledByDefault)
            {
                // Already upgraded to SL interface, nothing to do
                SL_LOG_VERBOSE("Base interface 0x%llx already upgraded to use SL proxy", unknown);
                return Result::eOk;
            }
            else
            {
                SL_LOG_ERROR( "Base interface 0x%llx already upgraded to use SL proxy but 'PreferenceFlag::eUseManualHooking' flag is specified in sl::Preferences, check if you are still linking `sl.interposer.lib`", unknown);
                return Result::eErrorInvalidIntegration;
            }
        }

        ID3D12Device* d3d12Device{};
        if (SUCCEEDED(unknown->QueryInterface(&d3d12Device)))
        {
            d3d12Device->Release();
            ID3D12Device** outInterface = (ID3D12Device**)baseInterface;
            SL_LOG_INFO("Upgrading ID3D12Device to use SL proxy ...");
            auto proxy = new sl::interposer::D3D12Device(*outInterface);
            proxy->checkAndUpgradeInterface(__uuidof(ID3D12Device10));
            *outInterface = proxy;
            return Result::eOk;
        }

        ID3D11Device* d3d11Device{};
        if (SUCCEEDED(unknown->QueryInterface(&d3d11Device)))
        {
            d3d11Device->Release();
            SL_LOG_INFO("ID3D11Device does NOT have SL proxy - using base interface");
            plugin_manager::getInterface()->setD3D11Device(d3d11Device);
            return Result::eOk;
        }

        IDXGIFactory* factory{};
        if (SUCCEEDED(unknown->QueryInterface(&factory)))
        {
            factory->Release();
            SL_LOG_INFO("Upgrading IDXGIFactory to use SL proxy ...");
            IDXGIFactory** outInterface = (IDXGIFactory**)baseInterface;
            auto proxy = new sl::interposer::DXGIFactory(*outInterface);
            proxy->checkAndUpgradeInterface(__uuidof(IDXGIFactory7));
            *outInterface = proxy;
            return Result::eOk;
        }

        IDXGISwapChain* swapChain{};
        if (SUCCEEDED(unknown->QueryInterface(&swapChain)))
        {
            swapChain->Release();
            ID3D11Device* d3d11Device{};
            ID3D12Device* d3d12Device{};
            swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&d3d11Device);
            swapChain->GetDevice(__uuidof(ID3D12Device), (void**)&d3d12Device);
            SL_LOG_INFO("Upgrading IDXGISwapChain to use SL proxy ...");
            IDXGISwapChain** outInterface = (IDXGISwapChain**)baseInterface;
            if (d3d12Device)
            {
                d3d12Device->Release();
                auto proxy = new sl::interposer::DXGISwapChain(d3d12Device, *outInterface);
                *outInterface = proxy;
                proxy->checkAndUpgradeInterface(__uuidof(IDXGISwapChain4));
            }
            else if(d3d11Device)
            {
                d3d11Device->Release();
                auto proxy = new sl::interposer::DXGISwapChain(d3d11Device, *outInterface);
                *outInterface = proxy;
                proxy->checkAndUpgradeInterface(__uuidof(IDXGISwapChain4));
            }
            else
            {
                SL_LOG_ERROR("Unable to retrieve D3D device from IDXGISwapChain");
                return Result::eErrorInvalidParameter;
            }
            return Result::eOk;
        }

        SL_LOG_ERROR( "Unable to upgrade unsupported interface");
#else
        SL_LOG_ERROR("This method is not supported on Linux");
#endif

        return Result::eErrorUnsupportedInterface;
    };

    SL_EXCEPTION_HANDLE_START
    return upgradeInterface();
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler)
}

Result slIsFeatureSupported(sl::Feature feature, const sl::AdapterInfo& adapterInfo)
{
    //! IMPORTANT:
    //! 
    //! As explained in sl_struct.h any new elements must be placed at the end
    //! of each structure and version must be increased or new elements can be
    //! added in a new structure which is then chained. This assert ensures
    //! that new element(s) are NOT added in the middle of a structure.
    static_assert(offsetof(sl::AdapterInfo, vkPhysicalDevice) == 48, "new elements can only be added at the end of each structure");

    auto isSupported = [](sl::Feature feature, const sl::AdapterInfo& adapterInfo)->Result
    {
        //! NOTE: 
        //! 
        //! Removed all logging to avoid confusion when feature is not loaded purposely.
        //! Also since we return a specific error code no real need for extra logging.
        //! 
        SL_CHECK(slValidateState());

        const char* jsonConfig{};
        if (!plugin_manager::getInterface()->getExternalFeatureConfig(feature, &jsonConfig))
        {
            return Result::eErrorFeatureMissing;
        }

        // Any JSON parser can be used here
        std::istringstream stream(jsonConfig);
        nlohmann::json cfg;
        stream >> cfg;

        bool osSupported = cfg["os"]["supported"];
        bool driverSupported = cfg["driver"]["supported"];

        if (cfg.contains("hws"))
        {
            bool required = cfg["hws"]["required"];
            bool detected = cfg["hws"]["detected"];
            if (required && !detected)
            {
                SL_LOG_ERROR("Feature '%s' requires GPU hardware scheduling to be enabled in the OS", getFeatureAsStr(feature));
                return Result::eErrorOSDisabledHWS;
            }
        }

        // Handle errors at the end so we can fill the structure with all the information needed
        if (!osSupported)
        {
            return Result::eErrorOSOutOfDate;
        }

        if (!driverSupported)
        {
            return Result::eErrorDriverOutOfDate;
        }

        FeatureRequirements featureReqs;
        slGetFeatureRequirements(feature, featureReqs);

        auto renderAPI = plugin_manager::getInterface()->getPreferences().renderAPI;
        switch (renderAPI)
        {
            case RenderAPI::eD3D11:
                if (!(featureReqs.flags & FeatureRequirementFlags::eD3D11Supported))
                {
                    SL_LOG_WARN("D3D11 not supported for this plugin");
                    return Result::eErrorMissingOrInvalidAPI;
                }
                break;
            case RenderAPI::eD3D12:
                if (!(featureReqs.flags & FeatureRequirementFlags::eD3D12Supported))
                {
                    SL_LOG_WARN("D3D12 not supported for this plugin");
                    return Result::eErrorMissingOrInvalidAPI;
                }
                break;
            case RenderAPI::eVulkan:
                if (!(featureReqs.flags & FeatureRequirementFlags::eVulkanSupported))
                {
                    SL_LOG_WARN("Vulkan not supported for this plugin");
                    return Result::eErrorMissingOrInvalidAPI;
                }
                break;
            default:
                SL_LOG_ERROR("Unexpected renderAPI value passed to slInit!");
                return Result::eErrorInvalidParameter;
        }

        // Check if the feature is supported on any available adapters
        bool featureSupported = cfg["feature"]["supported"];
        if (!featureSupported)
        {
            return Result::eErrorNoSupportedAdapterFound;
        }

        auto ctx = plugin_manager::getInterface()->getFeatureContext(feature);
        if (!ctx)
        {
            return Result::eErrorFeatureMissing;
        }

        // Not having 'isSupported' function indicates that plugin is supported on all adapters by design.
        // 
        // Also if adapter info is not provided there is nothing further to check
        if (!ctx->isSupported || !adapterInfo.deviceLUID) return Result::eOk;

        return ctx->isSupported(adapterInfo);
    };

    SL_EXCEPTION_HANDLE_START;
    return isSupported(feature, adapterInfo);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slGetFeatureVersion(sl::Feature feature, sl::FeatureVersion& version)
{
    //! IMPORTANT:
    //! 
    //! As explained in sl_struct.h any new elements must be placed at the end
    //! of each structure and version must be increased or new elements can be
    //! added in a new structure which is then chained. This assert ensures
    //! that new element(s) are NOT added in the middle of a structure.
    static_assert(offsetof(sl::FeatureVersion, versionNGX) == 44, "new elements can only be added at the end of each structure");

    auto getVersion = [](sl::Feature feature, sl::FeatureVersion& version)->Result
    {
        SL_CHECK(slValidateState());
        const char* jsonConfig{};
        if (!plugin_manager::getInterface()->getExternalFeatureConfig(feature, &jsonConfig))
        {
            SL_LOG_ERROR("Feature '%s' was not loaded", getFeatureAsStr(feature));
            return Result::eErrorFeatureMissing;
        }

        // Any JSON parser can be used here
        std::istringstream stream(jsonConfig);
        nlohmann::json cfg;
        stream >> cfg;

        if (cfg.contains("version"))
        {
            std::string v = cfg["version"]["sl"];
            sscanf_s(v.c_str(), "%u.%u.%u", &version.versionSL.major, &version.versionSL.minor, &version.versionSL.build);
            v = cfg["version"]["ngx"];
            sscanf_s(v.c_str(), "%u.%u.%u", &version.versionNGX.major, &version.versionNGX.minor, &version.versionNGX.build);
        }
        else
        {
            // This is OK, some features just default to SL SDK version and don't use NGX
            version.versionNGX = {};
            version.versionSL.major = SL_VERSION_MAJOR;
            version.versionSL.minor = SL_VERSION_MINOR;
            version.versionSL.build = SL_VERSION_PATCH;
        }
        return Result::eOk;
    };

    SL_EXCEPTION_HANDLE_START;
    return getVersion(feature, version);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler);
}

Result slGetFeatureRequirements(sl::Feature feature, sl::FeatureRequirements& requirements)
{
    //! IMPORTANT:
    //! 
    //! As explained in sl_struct.h any new elements must be placed at the end
    //! of each structure and version must be increased or new elements can be
    //! added in a new structure which is then chained. This assert ensures
    //! that new element(s) are NOT added in the middle of a structure.
    static_assert(offsetof(sl::FeatureRequirements, vkNumOpticalFlowQueuesRequired) == 176, "new elements can only be added at the end of each structure");

    SL_EXCEPTION_HANDLE_START;

    auto getFeatureRequirements = [](sl::Feature feature, sl::FeatureRequirements& requirements)->Result
    {
        SL_CHECK(slValidateState());

        requirements = {};

        const char* jsonConfig{};
        if (!plugin_manager::getInterface()->getExternalFeatureConfig(feature, &jsonConfig))
        {
            SL_LOG_ERROR( "Feature '%s' was not loaded", getFeatureAsStr(feature));
            return Result::eErrorFeatureMissing;
        }

        auto ctx = plugin_manager::getInterface()->getFeatureContext(feature);

        // Any JSON parser can be used here
        std::istringstream stream(jsonConfig);
        nlohmann::json cfg;
        stream >> cfg;

        if (cfg.contains("vsync"))
        {
            if(bool value = !cfg["vsync"]["supported"]) requirements.flags |= FeatureRequirementFlags::eVSyncOffRequired;
        }
        if (cfg.contains("hws"))
        {
            if (bool value = cfg["hws"]["required"]) requirements.flags |= FeatureRequirementFlags::eHardwareSchedulingRequired;
        }
        std::vector<std::string> supportedRHIs = cfg["feature"]["rhi"];
        if (std::find(supportedRHIs.begin(), supportedRHIs.end(), "d3d11") != supportedRHIs.end())  requirements.flags |= FeatureRequirementFlags::eD3D11Supported;
        if (std::find(supportedRHIs.begin(), supportedRHIs.end(), "d3d12") != supportedRHIs.end())  requirements.flags |= FeatureRequirementFlags::eD3D12Supported;
        if (std::find(supportedRHIs.begin(), supportedRHIs.end(), "vk") != supportedRHIs.end())  requirements.flags |= FeatureRequirementFlags::eVulkanSupported;

        std::string version = cfg["os"]["detected"];
        sscanf_s(version.c_str(), "%u.%u.%u", &requirements.osVersionDetected.major, &requirements.osVersionDetected.minor, &requirements.osVersionDetected.build);
        version = cfg["os"]["required"];
        sscanf_s(version.c_str(), "%u.%u.%u", &requirements.osVersionRequired.major, &requirements.osVersionRequired.minor, &requirements.osVersionRequired.build);

        version = cfg["driver"]["detected"];
        sscanf_s(version.c_str(), "%u.%u.%u", &requirements.driverVersionDetected.major, &requirements.driverVersionDetected.minor, &requirements.driverVersionDetected.build);
        version = cfg["driver"]["required"];
        sscanf_s(version.c_str(), "%u.%u.%u", &requirements.driverVersionRequired.major, &requirements.driverVersionRequired.minor, &requirements.driverVersionRequired.build);

        if (cfg.contains("/feature/viewport/maxCount"_json_pointer))
        {
            requirements.maxNumViewports = cfg["feature"]["viewport"]["maxCount"];
        }

        if (cfg.contains("/feature/cpu/maxThreadCount"_json_pointer))
        {
            requirements.maxNumCPUThreads = cfg["feature"]["cpu"]["maxThreadCount"];
        }

        if (cfg.contains("/feature/tags"_json_pointer))
        {
            auto it = s_ctx.requiredTags.find(feature);
            if (it == s_ctx.requiredTags.end())
            {
                std::vector<uint32_t> tags = cfg["feature"]["tags"];
                auto list = new BufferType[tags.size()];
                for (size_t i = 0; i < tags.size(); i++)
                {
                    list[i] = (BufferType)tags[i];
                }
                s_ctx.requiredTags[feature] = { tags.size(), list };
            }
            auto& data = s_ctx.requiredTags[feature];
            requirements.requiredTags = data.second;
            requirements.numRequiredTags = (uint32_t)data.first;
        }

        // VK bits
        if (cfg.contains("/vk/device/extensions"_json_pointer))
        {
            auto it = s_ctx.vkDeviceExtensions.find(feature);
            if (it == s_ctx.vkDeviceExtensions.end())
            {
                std::vector<std::string> deviceExtensions = cfg["vk"]["device"]["extensions"];
                auto list = new char* [deviceExtensions.size()];
                uint32_t i = 0;
                for (auto& e : deviceExtensions)
                {
                    list[i] = new char[e.size() + 1];
                    strcpy_s(list[i], e.size() + 1, e.data());
                    i++;
                }
                s_ctx.vkDeviceExtensions[feature] = { deviceExtensions.size(), list };
            }
            auto& data = s_ctx.vkDeviceExtensions[feature];
            requirements.vkNumDeviceExtensions = (uint32_t)data.first;
            requirements.vkDeviceExtensions = (const char**)data.second;
        }
        if (cfg.contains("/vk/instance/extensions"_json_pointer))
        {
            auto it = s_ctx.vkInstanceExtensions.find(feature);
            if (it == s_ctx.vkInstanceExtensions.end())
            {
                std::vector<std::string> instanceExtensions = cfg["vk"]["instance"]["extensions"];
                auto list = new char* [instanceExtensions.size()];
                uint32_t i = 0;
                for (auto& e : instanceExtensions)
                {
                    list[i] = new char[e.size() + 1];
                    strcpy_s(list[i], e.size() + 1, e.data());
                    i++;
                }
                s_ctx.vkInstanceExtensions[feature] = { instanceExtensions.size(), list };
            }
            auto& data = s_ctx.vkInstanceExtensions[feature];
            requirements.vkNumInstanceExtensions = (uint32_t)data.first;
            requirements.vkInstanceExtensions = (const char**)data.second;
        }
        if (cfg.contains("/vk/device/1.2_features"_json_pointer))
        {
            auto it = s_ctx.vkFeatures12.find(feature);
            if (it == s_ctx.vkFeatures12.end())
            {
                std::vector<std::string> features = cfg["vk"]["device"]["1.2_features"];
                auto list = new char* [features.size()];
                uint32_t i = 0;
                for (auto& e : features)
                {
                    list[i] = new char[e.size() + 1];
                    strcpy_s(list[i], e.size() + 1, e.data());
                    i++;
                }
                s_ctx.vkFeatures12[feature] = { features.size(), list };
            }
            auto& data = s_ctx.vkFeatures12[feature];
            requirements.vkNumFeatures12 = (uint32_t)data.first;
            requirements.vkFeatures12 = (const char**)data.second;
        }
        if (cfg.contains("/vk/device/1.3_features"_json_pointer))
        {
            auto it = s_ctx.vkFeatures13.find(feature);
            if (it == s_ctx.vkFeatures13.end())
            {
                std::vector<std::string> features = cfg["vk"]["device"]["1.3_features"];
                auto list = new char*[features.size()];
                uint32_t i = 0;
                for (auto& e : features)
                {
                    list[i] = new char[e.size() + 1];
                    strcpy_s(list[i], e.size() + 1, e.data());
                    i++;
                }
                s_ctx.vkFeatures13[feature] = { features.size(), list };
            }
            auto& data = s_ctx.vkFeatures13[feature];
            requirements.vkNumFeatures13 = (uint32_t)data.first;
            requirements.vkFeatures13 = (const char**)data.second;
        }
        // Additional queues?
        if (cfg.contains("/external/vk/device/queues/graphics/count"_json_pointer))
        {
            requirements.vkNumGraphicsQueuesRequired = cfg["vk"]["device"]["queues"]["graphics"]["count"];
        }
        if (cfg.contains("/external/vk/device/queues/compute/count"_json_pointer))
        {
            requirements.vkNumComputeQueuesRequired = cfg["vk"]["device"]["queues"]["compute"]["count"];
        }
        if (cfg.contains("/external/vk/device/queues/opticalflow/count"_json_pointer))
        {
            if (requirements.structVersion >= kStructVersion2)
            {
                requirements.vkNumOpticalFlowQueuesRequired = cfg["vk"]["device"]["queues"]["opticalflow"]["count"];
            }
        }

        return Result::eOk;
    };

    return getFeatureRequirements(feature, requirements);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler)
}

Result slGetFeatureFunction(sl::Feature feature, const char* functionName, void*& function)
{
    SL_EXCEPTION_HANDLE_START;
    SL_CHECK(slValidateState());
    const sl::plugin_manager::FeatureContext* ctx;
    SL_CHECK(slValidateFeatureContext(feature, ctx));
    // When running legacy integrations via sl.interposer redirect we cannot enforce this logic
    if (sl::plugin_manager::getInterface()->getHostSDKVersion() != Version(1, 5, 0))
    {
        if (!ctx->initialized)
        {
            SL_LOG_ERROR("'%s' has not been initialized yet. Did you forget to create device, swap-chain and or call slSetD3DDevice/slSetVulkanInfo?", getFeatureAsStr(feature));
            return Result::eErrorNotInitialized;
        }
    }
    if (!functionName) return Result::eErrorInvalidParameter;
    function = ctx->getFunction(functionName);
    return function ? Result::eOk : Result::eErrorMissingOrInvalidAPI;
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler)
}

Result slGetNewFrameToken(FrameToken*& handle, const uint32_t* frameIndex)
{
    auto getFrame = [](FrameToken*& handle, const uint32_t* frameIndex)->Result
    {
        SL_CHECK(slValidateState());

        std::scoped_lock lock(s_ctx.mtxFrameHandle);

        //! Two scenarios:
        //! 
        //! - If frame index is not provided we advance our internal counter and return next token
        //! - If frame index is provided then reuse the previous one if index is the same
        //! 
        //! Host can request multiple frame tokens with an identical frame index within the same frame, this is totally valid.
        if (!frameIndex || (*frameIndex != s_ctx.frameHandles[s_ctx.frameHandleIndex].counter.load()))
        {
            s_ctx.frameHandleIndex = (s_ctx.frameHandleIndex + 1) % kMaxNumFrameHandles;
            s_ctx.frameHandles[s_ctx.frameHandleIndex].counter.store(frameIndex ? *frameIndex : ++s_ctx.frameCounter);
        }

        handle = &s_ctx.frameHandles[s_ctx.frameHandleIndex];
        return Result::eOk;
    };
    SL_EXCEPTION_HANDLE_START;
    return getFrame(handle, frameIndex);
    SL_EXCEPTION_HANDLE_END_RETURN(Result::eErrorExceptionHandler)
}

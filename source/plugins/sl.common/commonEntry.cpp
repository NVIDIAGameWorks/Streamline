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

#include <dxgi1_6.h>
#include <future>

#include "include/sl.h"
#include "source/core/sl.api/internal.h"
#include "include/sl_consts.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.param/parameters.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "source/core/sl.interposer/vulkan/layer.h"
#include "source/plugins/sl.common/versions.h"
#include "source/platforms/sl.chi/d3d12.h"

#ifdef SL_WINDOWS
#define NV_WINDOWS
// Needed for SHGetKnownFolderPath
#include <ShlObj.h>
#pragma comment(lib,"shlwapi.lib")
#endif

#include "external/ngx/Include/nvsdk_ngx.h"
#include "external/ngx/Include/nvsdk_ngx_helpers.h"
#include "external/ngx/Include/nvsdk_ngx_helpers_vk.h"
#include "external/ngx/Include/nvsdk_ngx_defs.h"
#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

namespace sl
{

// Implemented in the common interface
extern HRESULT slHookCreateCommittedResource(const D3D12_HEAP_PROPERTIES* pHeapProperties, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC* pResourceDesc, D3D12_RESOURCE_STATES InitialResourceState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riidResource, void** ppvResource);
extern HRESULT slHookCreatePlacedResource(ID3D12Heap* pHeap, UINT64 HeapOffset, const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource);
extern HRESULT slHookCreateReservedResource(const D3D12_RESOURCE_DESC* pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE* pOptimizedClearValue, REFIID riid, void** ppvResource);
extern void slHookResourceBarrier(ID3D12GraphicsCommandList* pCmdList, UINT NumBarriers, const D3D12_RESOURCE_BARRIER* pBarriers);
extern HRESULT slHookPresent(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, bool& Skip);
extern HRESULT slHookPresent1(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, DXGI_PRESENT_PARAMETERS* params, bool& Skip);
extern HRESULT slHookResizeSwapChainPre(IDXGISwapChain* swapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags);

extern bool getGPUInfo(common::SystemCaps*& info);

//! Our common context
//! 
//! Here we keep tagged resources, NGX context
//! and other common stuff that comes along
//! and can be shared with other plugins.
//! 
struct CommonEntryContext
{
    bool needNGX = false;
    common::NGXContext ngxContext = {};

    chi::PlatformType platform = chi::ePlatformTypeD3D12;

    std::mutex resourceTagMutex = {};
    std::map<uint64_t, CommonResource> idToResourceMap;
    common::ViewportIdFrameData<3> constants = { "common" };
};
static CommonEntryContext ctx = {};

//! Thread safe get/set resource tag
//! 
CommonResource* getCommonTag(BufferType tag, uint32_t id)
{
    uint64_t uid = ((uint64_t)tag << 32) | (uint64_t)id;
    std::lock_guard<std::mutex> lock(ctx.resourceTagMutex);
    return &ctx.idToResourceMap[uid];
}

bool setCommonTag(const sl::Resource* resource, BufferType tag, uint32_t id, const Extent* ext)
{
    uint64_t uid = ((uint64_t)tag << 32) | (uint64_t)id;
    CommonResource cr = {};
    if (resource)
    {
        cr.res = *(sl::Resource*)resource;
    }
    if (ext)
    {
        cr.extent = *ext;
    }
    std::lock_guard<std::mutex> lock(ctx.resourceTagMutex);
    ctx.idToResourceMap[uid] = cr;
    return true;
}

//! Make sure host has provided common constants and
//! has not left something as an invalid value
void validateCommonConstants(const Constants& consts)
{
#define SL_VALIDATE_FLOAT4x4(v) if(v[0].x == INVALID_FLOAT) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}
    SL_VALIDATE_FLOAT4x4(consts.cameraViewToClip);
    SL_VALIDATE_FLOAT4x4(consts.clipToCameraView);
    SL_VALIDATE_FLOAT4x4(consts.clipToPrevClip);
    SL_VALIDATE_FLOAT4x4(consts.prevClipToClip);

#define SL_VALIDATE_FLOAT2(v) if(v.x == INVALID_FLOAT || v.y == INVALID_FLOAT) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}
    SL_VALIDATE_FLOAT2(consts.jitterOffset);
    SL_VALIDATE_FLOAT2(consts.mvecScale);
    SL_VALIDATE_FLOAT2(consts.cameraPinholeOffset);

#define SL_VALIDATE_FLOAT3(v) if(v.x == INVALID_FLOAT || v.y == INVALID_FLOAT || v.z == INVALID_FLOAT) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}

    SL_VALIDATE_FLOAT3(consts.cameraPos);
    SL_VALIDATE_FLOAT3(consts.cameraUp);
    SL_VALIDATE_FLOAT3(consts.cameraRight);
    SL_VALIDATE_FLOAT3(consts.cameraFwd);

#define SL_VALIDATE_FLOAT(v) if(v == INVALID_FLOAT) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}

    SL_VALIDATE_FLOAT(consts.cameraNear);
    SL_VALIDATE_FLOAT(consts.cameraFar);
    SL_VALIDATE_FLOAT(consts.cameraFOV);
    SL_VALIDATE_FLOAT(consts.cameraAspectRatio);
    SL_VALIDATE_FLOAT(consts.motionVectorsInvalidValue);

#define SL_VALIDATE_BOOL(v) if(v == Boolean::eInvalid) {SL_LOG_WARN("Value %s should not be left as invalid", #v);}

    SL_VALIDATE_BOOL(consts.depthInverted);
    SL_VALIDATE_BOOL(consts.cameraMotionIncluded);
    SL_VALIDATE_BOOL(consts.motionVectors3D);
    SL_VALIDATE_BOOL(consts.reset);
    SL_VALIDATE_BOOL(consts.notRenderingGameFrames);
    SL_VALIDATE_BOOL(consts.orthographicProjection);
    SL_VALIDATE_BOOL(consts.motionVectorsDilated);
    SL_VALIDATE_BOOL(consts.motionVectorsJittered);
}

//! Thread safe get/set common constants
bool setCommonConstants(const Constants& consts, uint32_t frame, uint32_t id)
{
    SL_RUN_ONCE
    {
        validateCommonConstants(consts);
    }
    // Common constants are per frame, per special id (viewport, instance etc)
    ctx.constants.set(frame, id, &consts);
    return true;
}

bool getCommonConstants(const common::EventData& ev, Constants** consts)
{
    return ctx.constants.get(ev, consts);
}

namespace ngx
{

//! NGX management
//! 
//! Common spot for all NGX functionality, create/eval/release feature
//! 
//! Shared with all other plugins as NGXContext
//! 
bool createNGXFeature(void* cmdList, NVSDK_NGX_Feature feature, NVSDK_NGX_Handle** handle)
{
    if (ctx.platform == chi::ePlatformTypeD3D11)
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_D3D11_CreateFeature((ID3D11DeviceContext*)cmdList, feature, ctx.ngxContext.params, handle));
    }
    else if (ctx.platform == chi::ePlatformTypeD3D12)
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_D3D12_CreateFeature((ID3D12GraphicsCommandList*)cmdList, feature, ctx.ngxContext.params, handle));
    }
    else
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_VULKAN_CreateFeature((VkCommandBuffer)cmdList, feature, ctx.ngxContext.params, handle));
    }
    return true;
}

bool evaluateNGXFeature(void* cmdList, NVSDK_NGX_Handle* handle)
{
    if (ctx.platform == chi::ePlatformTypeD3D11)
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_D3D11_EvaluateFeature((ID3D11DeviceContext*)cmdList, handle, ctx.ngxContext.params, nullptr));
    }
    else if (ctx.platform == chi::ePlatformTypeD3D12)
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_D3D12_EvaluateFeature((ID3D12GraphicsCommandList*)cmdList, handle, ctx.ngxContext.params, nullptr));
    }
    else
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_VULKAN_EvaluateFeature((VkCommandBuffer)cmdList, handle, ctx.ngxContext.params, nullptr));
    }
    return true;
}

bool releaseNGXFeature(NVSDK_NGX_Handle* handle)
{
    if (ctx.platform == chi::ePlatformTypeD3D11)
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_D3D11_ReleaseFeature(handle));
    }
    else if (ctx.platform == chi::ePlatformTypeD3D12)
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_D3D12_ReleaseFeature(handle));
    }
    else
    {
        CHECK_NGX_RETURN_ON_ERROR(NVSDK_NGX_VULKAN_ReleaseFeature(handle));
    }
    return true;
}

//! Managing allocations coming from NGX
void allocateNGXResourceCallback(D3D12_RESOURCE_DESC* desc, int state, CD3DX12_HEAP_PROPERTIES* heap, ID3D12Resource** resource)
{
    chi::ICompute* compute = {};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kComputeAPI, &compute);
    chi::Resource res = {};
    chi::ResourceDescription resDesc = {};
    resDesc.width = (uint32_t)desc->Width;
    resDesc.height = desc->Height;
    resDesc.mips = desc->MipLevels;
    compute->getResourceState(state, resDesc.state);
    resDesc.nativeFormat = desc->Format;
    resDesc.format = chi::eFormatINVALID;
    resDesc.heapType = (chi::HeapType)heap->Type;

    //! Redirecting to host app if allocate callback is specified in sl::Preferences
    if (desc->Dimension == D3D12_RESOURCE_DIMENSION::D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        compute->createBuffer(resDesc, res);
    }
    else
    {
        compute->createTexture2D(resDesc, res);
    }

    *resource = (ID3D12Resource*)res;
}

//! Managing deallocations coming from NGX
void releaseNGXResourceCallback(IUnknown* resource)
{
    if (resource)
    {
        chi::ICompute* compute = {};
        param::getPointerParam(api::getContext()->parameters, sl::param::common::kComputeAPI, &compute);
        auto res = (chi::Resource)resource;
        //! Redirecting to host app if deallocate callback is specified in sl::Preferences
        compute->destroyResource(res);
    }
}

void ngxLog(const char* message, NVSDK_NGX_Logging_Level loggingLevel, NVSDK_NGX_Feature sourceComponent)
{
    switch (loggingLevel)
    {
        case NVSDK_NGX_LOGGING_LEVEL_ON: SL_LOG_INFO(message); break;
        case NVSDK_NGX_LOGGING_LEVEL_VERBOSE: SL_LOG_VERBOSE(message); break;
    }
};

} // namespace ngx

//! Main entry point - starting our plugin
//! 
bool slOnPluginStartup(const char* jsonConfig, void* device, param::IParameters* parameters)
{
    SL_PLUGIN_COMMON_STARTUP();
    
    //! We handle all common functionality - common constants and tagging
    parameters->set(sl::param::global::kPFunSetConsts, setCommonConstants);
    parameters->set(param::global::kPFunGetConsts, getCommonConstants);
    parameters->set(param::common::kPFunEvaluateFeature, common::evaluateFeature);
    parameters->set(param::common::kPFunRegisterEvaluateCallbacks, common::registerEvaluateCallbacks);
    parameters->set(param::global::kPFunSetTag, setCommonTag);
    parameters->set(param::global::kPFunGetTag, getCommonTag);

    //! Plugin manager gives us the device type and the application id
    json& config = *(json*)api::getContext()->loaderConfig;
    uint32_t deviceType = chi::ePlatformTypeD3D12;
    int appId = 0;
    config.at("appId").get_to(appId);
    config.at("deviceType").get_to(deviceType);

    //! Some optional tweaks, NGX logging included in SL logging 
    uint32_t logLevelNGX = log::getInterface()->getLogLevel();
    //! Extra config is always `sl.plugin_name.json` so in our case `sl.common.json`
    json& extraConfig = *(json*)api::getContext()->extConfig;
    if (extraConfig.contains("logLevelNGX"))
    {
        extraConfig.at("logLevelNGX").get_to(logLevelNGX);
        SL_LOG_HINT("Overriding NGX logging level to %u'", logLevelNGX);
    }
    //! Optional hot-key bindings
    if (extraConfig.contains("keys"))
    {
        auto keys = extraConfig.at("keys");
        for (auto& key : keys)
        {
            extra::keyboard::VirtKey vk;
            std::string id;
            key.at("alt").get_to(vk.m_bAlt);
            key.at("ctrl").get_to(vk.m_bControl);
            key.at("shift").get_to(vk.m_bShift);
            key.at("key").get_to(vk.m_mainKey);
            key.at("id").get_to(id);
            extra::keyboard::getInterface()->registerKey(id.c_str(), vk);
            SL_LOG_HINT("Overriding key combo for '%s'", id.c_str());
        }
    }

    // Now let's create our compute interface
    ctx.platform = (chi::PlatformType)deviceType;
    common::createCompute(device, ctx.platform);

   
    // Check if any of the plugins requested NGX
    ctx.needNGX = false;
    parameters->get(param::global::kNeedNGX, &ctx.needNGX);
    if (ctx.needNGX)
    {
        // NGX initialization
        SL_LOG_INFO("At least one plugin requires NGX, trying to initialize ...");

        // Reset our flag until we see if NGX can be initialized correctly
        ctx.needNGX = false;

        // We also need to provide path for logging
        PWSTR documentsDataPath = NULL;
#ifdef SL_WINDOWS
        if (FAILED(SHGetKnownFolderPath(FOLDERID_PublicDocuments, 0, NULL, &documentsDataPath)))
        {
            SL_LOG_ERROR("Failed to obtain path to documents");
        }
#endif
        // We need to provide path to the NGX modules
        wchar_t* slPluginPathUtf16 = {};
        param::getPointerParam(parameters, param::global::kPluginPath, &slPluginPathUtf16);
        // Always check first where our plugins are then the other paths
        std::vector<std::wstring> ngxPathsTmp = {slPluginPathUtf16 };
        std::vector<wchar_t*> ngxPaths = { slPluginPathUtf16 };
        auto& paths = config.at("paths");
        for (auto& p : paths)
        {
            std::string s;
            p.get_to(s);
            auto ws = extra::utf8ToUtf16(s.c_str());
            if (std::find(ngxPathsTmp.begin(), ngxPathsTmp.end(), ws) == ngxPathsTmp.end())
            {
                ngxPathsTmp.push_back(ws);
                ngxPaths.push_back((wchar_t*)ngxPathsTmp.back().c_str());
            }
        }

        NVSDK_NGX_FeatureCommonInfo info = {};
        info.PathListInfo.Length = (uint32_t)ngxPaths.size();
        info.PathListInfo.Path = ngxPaths.data();
        {
            // We can control NXG logging as well
            
            info.LoggingInfo.LoggingCallback = ngx::ngxLog;
            info.LoggingInfo.DisableOtherLoggingSinks = true;
            switch (logLevelNGX)
            {
                case LogLevel::eLogLevelOff:
                    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_OFF;
                    break;
                case LogLevel::eLogLevelDefault:
                    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_ON;
                    break;
                case LogLevel::eLogLevelVerbose:
                    info.LoggingInfo.MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE;
                    break;
            }
        }

        NVSDK_NGX_Result ngxStatus{};
        if (deviceType == chi::ePlatformTypeD3D11)
        {
            ngxStatus = NVSDK_NGX_D3D11_Init(appId, documentsDataPath, (ID3D11Device*)device, &info, NVSDK_NGX_Version_API);
            ngxStatus = NVSDK_NGX_D3D11_GetCapabilityParameters(&ctx.ngxContext.params);
        }
        else if (deviceType == chi::ePlatformTypeD3D12)
        {
            ngxStatus = NVSDK_NGX_D3D12_Init(appId, documentsDataPath, (ID3D12Device*)device, &info, NVSDK_NGX_Version_API);
            ngxStatus = NVSDK_NGX_D3D12_GetCapabilityParameters(&ctx.ngxContext.params);
        }
        else
        {
            VkDevices* slVkDevices = (VkDevices*)device;

            sl::interposer::VkTable* vk = {};
            if (!param::getPointerParam(parameters, sl::param::global::kVulkanTable, &vk))
            {
                SL_LOG_ERROR("Unable to obtain Vulkan table from the Streamline layer");
                return false;
            }

            assert(vk->dispatchDeviceMap.find(slVkDevices->device) != vk->dispatchDeviceMap.end());
            assert(vk->dispatchInstanceMap.find(slVkDevices->instance) != vk->dispatchInstanceMap.end());

            ngxStatus = NVSDK_NGX_VULKAN_Init(appId, documentsDataPath, slVkDevices->instance, slVkDevices->physical, slVkDevices->device, /*vk->getInstanceProcAddr, vk->getDeviceProcAddr,*/ &info, NVSDK_NGX_Version_API);
            ngxStatus = NVSDK_NGX_VULKAN_GetCapabilityParameters(&ctx.ngxContext.params);
        }

        if (ngxStatus == NVSDK_NGX_Result_Success)
        {
        SL_LOG_HINT("NGX loaded - app id %u - logging to %S", appId, documentsDataPath);
        
        ctx.needNGX = true;

        // Register callbacks so we can manage memory for NGX
        ctx.ngxContext.params->Set(NVSDK_NGX_Parameter_ResourceAllocCallback, ngx::allocateNGXResourceCallback);
        ctx.ngxContext.params->Set(NVSDK_NGX_Parameter_ResourceReleaseCallback, ngx::releaseNGXResourceCallback);

        // Provide NGX context to other plugins
        ctx.ngxContext.createFeature = ngx::createNGXFeature;
        ctx.ngxContext.releaseFeature = ngx::releaseNGXFeature;
        ctx.ngxContext.evaluateFeature = ngx::evaluateNGXFeature;
        parameters->set(param::global::kNGXContext, &ctx.ngxContext);
        }
        else
        {
            SL_LOG_WARN("Failed to initialize NGX, any SL feature requiring NGX will be unloaded and disabled");
        }
    }

    return true;
}

//! Main exit point - shutting down our plugin
//! 
void slOnPluginShutdown()
{
    if (ctx.needNGX)
    {
        SL_LOG_INFO("Shutting down NGX");
        if (ctx.platform == chi::ePlatformTypeD3D11)
        {
            NVSDK_NGX_D3D11_Shutdown1(nullptr);
        }
        else if (ctx.platform == chi::ePlatformTypeD3D12)
        {
            NVSDK_NGX_D3D12_Shutdown1(nullptr);
        }
        else
        {
            NVSDK_NGX_VULKAN_Shutdown1(nullptr);
        }
        ctx.needNGX = false;
    }

    // Common shutdown, if we loaded an OTA
    // it will shutdown it down automatically
    plugin::onShutdown(api::getContext());

    common::destroyCompute();
}

//! These are the hooks we need to track resources
//! 
static const char* JSON = R"json(
{
    "id" : -1,
    "priority" : 0,
    "namespace" : "common",
    "hooks" :
    [
        {
            "class": "ID3D12Device",
            "target" : "CreateCommittedResource",
            "replacement" : "slHookCreateCommittedResource",
            "base" : "after"
        },
        {
            "class": "ID3D12Device",
            "target" : "CreatePlacedResource",
            "replacement" : "slHookCreatePlacedResource",
            "base" : "after"
        },
        {
            "class": "ID3D12Device",
            "target" : "CreateReservedResource",
            "replacement" : "slHookCreateReservedResource",
            "base" : "after"
        },
        {
            "class": "ID3D12GraphicsCommandList",
            "target" : "ResourceBarrier",
            "replacement" : "slHookResourceBarrier",
            "base" : "after"
        },

        {
            "class": "IDXGISwapChain",
            "target" : "ResizeBuffers",
            "replacement" : "slHookResizeSwapChainPre",
            "base" : "before"
        },
        {
            "class": "IDXGISwapChain",
            "target" : "Present",
            "replacement" : "slHookPresent",
            "base" : "before"
        },
        {
            "class": "IDXGISwapChain",
            "target" : "Present1",
            "replacement" : "slHookPresent1",
            "base" : "before"
        }
    ]
}
)json";

using PFunRtlGetVersion = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

bool getOSVersion(common::SystemCaps* caps)
{
    bool res = false;
    RTL_OSVERSIONINFOW osVer = {};
    auto mod = LoadLibraryExW(L"ntdll.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (mod) 
    {
        auto rtlGetVersion = (PFunRtlGetVersion)GetProcAddress(mod, "RtlGetVersion");
        if (rtlGetVersion)
        {
            osVer.dwOSVersionInfoSize = sizeof(osVer);
            if (res = !rtlGetVersion(&osVer))
            {
                caps->osVersionMajor = osVer.dwMajorVersion;
                caps->osVersionMinor = osVer.dwMinorVersion;
                caps->osVersionBuild = osVer.dwBuildNumber;
            }
        }
    }
    FreeLibrary(mod);
    return res;
}

//! Figure out if we are supported on the current hardware or not
//! 
uint32_t getSupportedAdapterMask()
{
    // Provide shared interface for keyboard
    api::getContext()->parameters->set(param::common::kKeyboardAPI, extra::keyboard::getInterface());

    // Now we need to check OS and GPU capabilities
    common::SystemCaps* caps{};
    if (getGPUInfo(caps))
    {
        // SL does not work on Win7, only Win10+
        auto osVersion = getOSVersion(caps);
        if (caps->osVersionMajor < 10)
        {
            SL_LOG_ERROR("Win10 or higher is required to use SL - all features will be disabled");
            return 0;
        }
        SL_LOG_INFO("Detected Windows OS version %u.%u.%u", caps->osVersionMajor, caps->osVersionMinor, caps->osVersionBuild);
        // Allow other plugins to query system caps
        api::getContext()->parameters->set(sl::param::common::kSystemCaps, (void*)caps);
    }

    // Always supported across all adapters assuming all the above checks passed
    return ~0;
}

//! Define our plugin
SL_PLUGIN_DEFINE("sl.common", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON, getSupportedAdapterMask())


//! The only exported function - gateway to all functionality
SL_EXPORT void* slGetPluginFunction(const char* functionName)
{
    //! Forward declarations
    const char* slGetPluginJSONConfig();
    void slSetParameters(sl::param::IParameters * p);

    //! Redirect to OTA if any
    SL_EXPORT_OTA;

    //! Core API
    SL_EXPORT_FUNCTION(slSetParameters);
    SL_EXPORT_FUNCTION(slOnPluginShutdown);
    SL_EXPORT_FUNCTION(slOnPluginStartup);
    SL_EXPORT_FUNCTION(slGetPluginJSONConfig);

    //! Hooks defined in the JSON config above

    //! D3D12
    SL_EXPORT_FUNCTION(slHookPresent);
    SL_EXPORT_FUNCTION(slHookPresent1);
    SL_EXPORT_FUNCTION(slHookResizeSwapChainPre);
    SL_EXPORT_FUNCTION(slHookResourceBarrier);
    SL_EXPORT_FUNCTION(slHookCreateCommittedResource);
    SL_EXPORT_FUNCTION(slHookCreatePlacedResource);
    SL_EXPORT_FUNCTION(slHookCreateReservedResource);

    return nullptr;
}

}
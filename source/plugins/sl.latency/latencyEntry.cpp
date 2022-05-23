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
#include "include/sl_consts.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.param/parameters.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.template/versions.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "source/plugins/sl.latency/latencystats.h"
#include "_artifacts/gitVersion.h"
#include "external/nvapi/nvapi.h"
#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

//! GPU agnostic stats definition
NVSTATS_DEFINE();

namespace sl
{

//! Our common context
//! 
//! Here we can keep whatever global state we need
//! 
struct LatencyContext
{
    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};

    // Compute API
    chi::PlatformType platform = chi::ePlatformTypeD3D12;
    chi::ICompute* compute{};

    //! Latest constants
    LatencyConstants constants{};

    //! Specifies if low-latency mode is available or not
    bool lowLatencyAvailable = false;
    //! Specifies if latency report is available or not
    bool latencyReportAvailable = false;


    //! Stats initialized or not
    bool initialized = false;

    //! Debug text stats
    std::string stats;
};
static LatencyContext s_ctx = {};

//! Set constants for our plugin (if any, this is optional and should be thread safe)
bool slSetConstants(const void* data, uint32_t frameIndex, uint32_t id)
{
    auto consts = (const LatencyConstants*)data;
    if (!consts)
    {
        return false;
    }
    if (!s_ctx.lowLatencyAvailable)
    {
        // At the moment low latency is only possible on NVDA hw
        if (consts->mode == LatencyMode::eLatencyModeLowLatency || consts->mode == LatencyMode::eLatencyModeLowLatencyWithBoost)
        {
            SL_LOG_WARN_ONCE("Low-latency modes are only supported on NVIDIA hardware through Reflex, collecting latency stats only");
        }
    }

    if (!s_ctx.initialized)
    {
        if (consts->virtualKey && (consts->virtualKey != VK_F13 && consts->virtualKey != VK_F14 && consts->virtualKey != VK_F15))
        {
            SL_LOG_ERROR("Latency virtual key can only be assigned to VK_F13, VK_F14 or VK_F15");
            return false;
        }
        //! GPU agnostic latency stats initialization
        s_ctx.initialized = true;
        NVSTATS_INIT(consts->virtualKey, 0);
    }
    
    g_ReflexStatsVirtualKey = consts->virtualKey;

    if (consts->mode != s_ctx.constants.mode || consts->useMarkersToOptimize != s_ctx.constants.useMarkersToOptimize || consts->frameLimitUs != s_ctx.constants.frameLimitUs)
    {
        s_ctx.constants = *consts;
        if (s_ctx.lowLatencyAvailable)
        {
            CHI_VALIDATE(s_ctx.compute->setSleepMode(*consts));
        }

        auto v = api::getContext()->pluginVersion;
        std::string mode[] = { "off", "on", "on with boost" };
        s_ctx.stats = extra::format("sl.latency {} - mode {} - using markers {} - fps cap {}us - {}", v.toStr(), mode[consts->mode], consts->useMarkersToOptimize, consts->frameLimitUs, GIT_LAST_COMMIT);
        api::getContext()->parameters->set(sl::param::latency::kStats, (void*)s_ctx.stats.c_str());
    }
    else
    {
        SL_LOG_WARN_ONCE("Latency constants did not change, there is no need to call slSetFeatureConstants unless settings changed.");
    }
    return true;
}

//! Get settings for our plugin (optional and depending on if we need to provide any settings back to the host)
bool slGetSettings(const void* cdata, void* sdata)
{
    //auto consts = (const LatencyConstants*)cdata;
    auto settings = (LatencySettings*)sdata;
    if (!settings)
    {
        return false;
    }
    // Based on hw and driver we assume that low latency should be available
    if (s_ctx.lowLatencyAvailable)
    {
        // NVAPI call can still fail so adjust flags
        s_ctx.lowLatencyAvailable = s_ctx.compute->getSleepStatus(*settings) == chi::ComputeStatus::eComputeStatusOk;
        s_ctx.latencyReportAvailable = s_ctx.compute->getLatencyReport(*settings) == chi::ComputeStatus::eComputeStatusOk;
    }
    settings->lowLatencyAvailable = s_ctx.lowLatencyAvailable;
    settings->latencyReportAvailable = s_ctx.latencyReportAvailable;
    // Allow host to check Windows messages for the special low latency message
    settings->statsWindowMessage = g_ReflexStatsWindowMessage;
    return true;
}

//! Begin evaluation for our plugin (if we use evalFeature mechanism to inject functionality in to the command buffer)
//! 
void latencyBeginEvaluation(chi::CommandList pCmdList, const common::EventData& evd)
{
    // Special 'marker' for low latency mode
    if (evd.id == eLatencyMarkerSleep)
    {
        if (s_ctx.lowLatencyAvailable)
        {
            s_ctx.lowLatencyAvailable = s_ctx.compute->sleep() == chi::ComputeStatus::eComputeStatusOk;
        }
    }
    else
    {
        if (s_ctx.lowLatencyAvailable && evd.id != NVSTATS_PC_LATENCY_PING)
        {
            CHI_VALIDATE(s_ctx.compute->setLatencyMarker((LatencyMarker)evd.id, evd.frame));
        }
        // Marking the end of the frame which is useful for other plugins
        if (evd.id == eLatencyMarkerPresentStart || evd.id == eLatencyMarkerPresentEnd)
        {
            api::getContext()->parameters->set(sl::param::latency::kMarkerFrame, evd.frame);
        }
        NVSTATS_MARKER((NVSTATS_LATENCY_MARKER_TYPE)evd.id, evd.frame);
    }

    // Mark the last frame we were active
    uint32_t frame = 0;
    CHI_VALIDATE(s_ctx.compute->getFinishedFrameIndex(frame));
    api::getContext()->parameters->set(sl::param::latency::kCurrentFrame, frame + 1);
}

//! End evaluation for our plugin (if we use evalFeature mechanism to inject functionality in to the command buffer)
//! 
void latencyEndEvaluation(chi::CommandList cmdList)
{
    // Nothing to do here really
}

//! Allows other plugins to set GPU agnostic stats
void setLatencyStatsMarker(LatencyMarker marker, uint32_t frameId)
{
    NVSTATS_MARKER(marker, frameId);
}

//! Main entry point - starting our plugin
//! 
//! IMPORTANT: Plugins are started based on their priority.
//! sl.common always starts first since it has priority 0
//!
bool slOnPluginStartup(const char* jsonConfig, void* device, param::IParameters* parameters)
{
    //! Common startup and setup
    //!     
    SL_PLUGIN_COMMON_STARTUP();

    //! Register our evaluate callbacks
    //!
    //! Note that sl.common handles evaluateFeature calls from the host
    //! and distributes eval calls to the right plugin based on the feature id
    //! 
    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &s_ctx.registerEvaluateCallbacks))
    {
        SL_LOG_ERROR("Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        return false;
    }
    s_ctx.registerEvaluateCallbacks(eFeatureLatency, latencyBeginEvaluation, latencyEndEvaluation);

    //! Allow other plugins to set latency stats
    parameters->set(param::latency::kPFunSetLatencyStatsMarker, setLatencyStatsMarker);

    //! Plugin manager gives us the device type and the application id
    //! 
    json& config = *(json*)api::getContext()->loaderConfig;
    uint32_t deviceType{};
    int appId{};
    config.at("appId").get_to(appId);
    config.at("deviceType").get_to(deviceType);

    //! Now let's obtain compute interface if we need to dispatch some compute work
    //! 
    s_ctx.platform = (chi::PlatformType)deviceType;
    if (!param::getPointerParam(parameters, sl::param::common::kComputeAPI, &s_ctx.compute))
    {
        SL_LOG_ERROR("Cannot obtain compute interface - check that sl.common was initialized correctly");
        return false;
    }

    return true;
}

//! Main exit point - shutting down our plugin
//! 
//! IMPORTANT: Plugins are shutdown in the inverse order based to their priority.
//! sl.common always shuts down LAST since it has priority 0
//!
void slOnPluginShutdown()
{
    // If we used 'evaluateFeature' mechanism reset the callbacks here
    s_ctx.registerEvaluateCallbacks(eFeatureLatency, nullptr, nullptr);

    //! GPU agnostic latency stats shutdown
    NVSTATS_SHUTDOWN();

    // Common shutdown
    plugin::onShutdown(api::getContext());
}

//! These are the hooks we need to do whatever our plugin is trying to do
//! 
//! See pluginManager.h for the full list of currently supported hooks
//! 
//! Hooks are registered and executed by their priority. If it is important 
//! for your plugin to run before/after some other plugin please check the 
//! priorities listed by the plugin manager in the log during the startup.
//!
//! IMPORTANT: Please note that priority '0' is reserved for the sl.common plugin.
//!
static const char* JSON = R"json(
{
    "id" : 3,
    "priority" : 1,
    "namespace" : "latency",
    "hooks" :
    [
    ]
}
)json";

//! Figure out if we are supported on the current hardware or not
//! 
uint32_t getSupportedAdapterMask()
{
    // Latency can be GPU agnostic to some degree
    uint32_t adapterMask = ~0;

    // Defaults everything to false
    s_ctx.lowLatencyAvailable = false;
    s_ctx.latencyReportAvailable = false;

    // Figure out if we should use NVAPI or not
    common::GPUArch* info{};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kGPUInfo, &info);
    // NVDA driver has to be 455+ otherwise Reflex won't work
    if (info && info->driverVersionMajor > 455)
    {
        // We start with Pascal+ then later check again if GetSleepStatus returns error or not
        for (uint32_t i = 0; i < info->gpuCount; i++)
        {
            s_ctx.lowLatencyAvailable |= info->architecture[i] >= NV_GPU_ARCHITECTURE_ID::NV_GPU_ARCHITECTURE_GP100;
        }
    }
    return adapterMask;
}

//! Define our plugin, make sure to update version numbers in versions.h
SL_PLUGIN_DEFINE("sl.latency", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON, getSupportedAdapterMask())

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
    SL_EXPORT_FUNCTION(slSetConstants);
    SL_EXPORT_FUNCTION(slGetSettings);
    
    return nullptr;
}

}
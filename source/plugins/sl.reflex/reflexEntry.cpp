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
#include <map>
#include <assert.h>

#include "include/sl.h"
#include "include/sl_consts.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.param/parameters.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.template/versions.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "source/plugins/sl.reflex/pclstats.h"
#include "source/plugins/sl.imgui/imgui.h"
#include "_artifacts/json/reflex_json.h"
#include "_artifacts/gitVersion.h"
#include "external/nvapi/nvapi.h"
#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

//! GPU agnostic stats definition
PCLSTATS_DEFINE();

namespace sl
{

namespace reflex
{

struct UIStats
{
    std::mutex mtx;
    std::string mode;
    std::string markers;
    std::string fpsCap;
    std::string presentFrame;
    std::string sleeping;
};

//! Our common context
//! 
//! Here we can keep whatever global state we need
//! 
struct LatencyContext
{
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(LatencyContext);
    void onCreateContext() {};
    void onDestroyContext() {};

    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};

    // Compute API
    RenderAPI platform = RenderAPI::eD3D12;
    chi::ICompute* compute{};

    UIStats uiStats{};

    // Engine type (Unity, UE etc)
    EngineType engine{};

    //! Latest constants
    ReflexOptions constants{};

    //! Can be overridden via sl.reflex.json config
    uint32_t frameLimitUs = UINT_MAX;
    bool useMarkersToOptimizeOverride = false;
    bool useMarkersToOptimizeOverrideValue = false;

    //! Specifies if low-latency mode is available or not
    bool lowLatencyAvailable = false;
    //! Specifies if latency report is available or not
    bool latencyReportAvailable = false;
    //! Specifies ownership of flash indicator toggle (true = driver, false = application)
    bool flashIndicatorDriverControlled = false;

    extra::AverageValueMeter sleepMeter{};

    //! Stats initialized or not
    std::atomic<bool> initialized = false;
    std::atomic<bool> enabled = false;

    //! Debug text stats
    std::string stats;
};
}

//! Embedded JSON, containing information about the plugin and the hooks it requires.
static std::string JSON = std::string(reflex_json, &reflex_json[reflex_json_len]);

void updateEmbeddedJSON(json& config);

//! Define our plugin, make sure to update version numbers in versions.h
SL_PLUGIN_DEFINE("sl.reflex", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON.c_str(), updateEmbeddedJSON, reflex, LatencyContext)

//! Figure out if we are supported on the current hardware or not
//! 
void updateEmbeddedJSON(json& config)
{
    // Latency can be GPU agnostic to some degree
    uint32_t adapterMask = ~0;

    auto& ctx = (*reflex::getContext());

    // Defaults everything to false
    ctx.lowLatencyAvailable = false;
    ctx.latencyReportAvailable = false;
    ctx.flashIndicatorDriverControlled = false;

    // Check if plugin is supported or not on this platform and set the flag accordingly
    common::SystemCaps* caps = {};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kSystemCaps, &caps);
    common::PFunUpdateCommonEmbeddedJSONConfig* updateCommonEmbeddedJSONConfig{};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kPFunUpdateCommonEmbeddedJSONConfig, &updateCommonEmbeddedJSONConfig);
    if (caps && updateCommonEmbeddedJSONConfig)
    {
        // All defaults since sl.reflex can run on any adapter
        common::PluginInfo info{};
        info.SHA = GIT_LAST_COMMIT_SHORT;
        updateCommonEmbeddedJSONConfig(&config, info);
    }

    // Figure out if we should use NVAPI or not
    // 
    // NVDA driver has to be 455+ otherwise Reflex low-latency won't work
    sl::Version minDriver(455, 0, 0);
    if (caps && caps->driverVersionMajor > 455)
    {
        // We start with Pascal+ then later check again if GetSleepStatus returns error or not
        for (uint32_t i = 0; i < caps->gpuCount; i++)
        {
            ctx.lowLatencyAvailable |= caps->adapters[i].architecture >= NV_GPU_ARCHITECTURE_ID::NV_GPU_ARCHITECTURE_GP100;
            // Starting since 511.23 flash indicator should be controlled by GFE instead of application
            ctx.flashIndicatorDriverControlled |= (caps->driverVersionMajor * 100 + caps->driverVersionMinor) >= 51123;
        }
    }

    config["external"]["reflex"]["lowLatencyAvailable"] = ctx.lowLatencyAvailable;
    config["external"]["reflex"]["flashIndicatorDriverControlled"] = ctx.flashIndicatorDriverControlled;
}

//! Update stats shown on screen
void updateStats(uint32_t presentFrameIndex)
{
#ifndef SL_PRODUCTION
    auto& ctx = (*reflex::getContext());
    std::string mode[] = { "Off", "On", "On with boost" };

    std::scoped_lock lock(ctx.uiStats.mtx);
    ctx.uiStats.mode = "Mode: " + mode[ctx.constants.mode];
    ctx.uiStats.markers = extra::format("Optimize with markers: {}", (ctx.constants.useMarkersToOptimize ? "Yes" : "No"));
    ctx.uiStats.fpsCap = extra::format("FPS cap: {}us", ctx.constants.frameLimitUs);
    if(presentFrameIndex) ctx.uiStats.presentFrame = extra::format("Present marker frame: {}", presentFrameIndex);
    ctx.uiStats.sleeping = extra::format("Sleeping: {}ms", ctx.sleepMeter.getMean());
#endif
}

//! Set constants for our plugin (if any, this is optional and should be thread safe)
Result slSetData(const BaseStructure* inputs, CommandBuffer* cmdBuffer)
{
    auto& ctx = (*reflex::getContext());

    if (!ctx.compute)
    {
        return Result::eErrorInvalidIntegration;
    }

    auto marker = findStruct<ReflexHelper>(inputs);
    auto consts = findStruct<ReflexOptions>(inputs);
    auto frame = findStruct<FrameToken>(inputs);
    
    if (marker && frame)
    {
        common::EventData evd = { (uint32_t)*marker, *frame };

        // Special 'marker' for low latency mode
        if (*marker == (ReflexMarker)kReflexMarkerSleep)
        {
            if (ctx.lowLatencyAvailable)
            {
#ifdef SL_PRODUCTION
                ctx.lowLatencyAvailable = ctx.compute->sleep() == chi::ComputeStatus::eOk;
#else
                ctx.sleepMeter.begin();
                ctx.lowLatencyAvailable = ctx.compute->sleep() == chi::ComputeStatus::eOk;
                ctx.sleepMeter.end();
#endif
            }
        }
        else
        {
            // According to Cody we want markers set even when Reflex is off
            if (ctx.lowLatencyAvailable && evd.id != PCLSTATS_PC_LATENCY_PING)
            {
                CHI_VALIDATE(ctx.compute->setReflexMarker((ReflexMarker)evd.id, evd.frame));
            }

            // Special case for Unity, it is hard to provide present markers so using render markers
            if (evd.id == ReflexMarker::ePresentStart || (ctx.engine == EngineType::eUnity && evd.id == ReflexMarker::eRenderSubmitStart))
            {
                api::getContext()->parameters->set(sl::param::latency::kMarkerFrame, evd.frame);
                updateStats(evd.frame);

                // Mark the last frame we were active
                //
                // NOTE: We do this on present marker only to prevent
                // scenarios where simulation marker for new frame comes in
                // and advances the frame index
                if (ctx.enabled.load())
                {
                    uint32_t frame = 0;
                    CHI_VALIDATE(ctx.compute->getFinishedFrameIndex(frame));
                    api::getContext()->parameters->set(sl::param::latency::kCurrentFrame, frame + 1);
                }
            }

            PCLSTATS_MARKER((PCLSTATS_LATENCY_MARKER_TYPE)evd.id, evd.frame);
        }
    }
    else
    {
        if (!consts)
        {
            return Result::eErrorMissingInputParameter;
        }
        if (!ctx.lowLatencyAvailable)
        {
            // At the moment low latency is only possible on NVDA hw
            if (consts->mode == ReflexMode::eLowLatency || consts->mode == ReflexMode::eLowLatencyWithBoost)
            {
                SL_LOG_WARN_ONCE("Low-latency modes are only supported on NVIDIA hardware through Reflex, collecting latency stats only");
            }
        }


        bool expected = false;
        if (ctx.initialized.compare_exchange_weak(expected, true))
        {
            if (consts->virtualKey && (consts->virtualKey != VK_F13 && consts->virtualKey != VK_F14 && consts->virtualKey != VK_F15))
            {
                SL_LOG_ERROR("Latency virtual key can only be assigned to VK_F13, VK_F14 or VK_F15");
                return Result::eErrorInvalidParameter;
            }
            //! GPU agnostic latency stats initialization

            uint32_t idThread = 0;
            idThread = consts->idThread;

            PCLSTATS_INIT(0);
        }

        {
            uint32_t idThread = 0;
            idThread = consts->idThread;

            PCLSTATS_SET_ID_THREAD(idThread);
        }

        PCLSTATS_SET_VIRTUAL_KEY(consts->virtualKey);

        {
            ctx.constants = *consts;
            ctx.enabled.store(consts->mode != ReflexMode::eOff);
#ifndef SL_PRODUCTION
            // Override from config (if any)
            if (ctx.frameLimitUs != UINT_MAX)
            {
                ctx.constants.frameLimitUs = ctx.frameLimitUs;
            }
            if (ctx.useMarkersToOptimizeOverride)
            {
                ctx.constants.useMarkersToOptimize = ctx.useMarkersToOptimizeOverrideValue;
            }
#endif
            if (ctx.lowLatencyAvailable)
            {
                CHI_VALIDATE(ctx.compute->setSleepMode(ctx.constants));
            }
            updateStats(0);
        }
    }
    
    return Result::eOk;
}

Result slGetData(const BaseStructure* inputs, BaseStructure* outputs, CommandBuffer* cmdBuffer)
{
    auto& ctx = (*reflex::getContext());
    
    auto settings = findStruct<ReflexState>(outputs);
    if (!settings)
    {
        return Result::eErrorMissingInputParameter;
    }
    // Based on hw and driver we assume that low latency should be available
    if (ctx.compute && ctx.lowLatencyAvailable)
    {
        // NVAPI call can still fail so adjust flags
        ctx.lowLatencyAvailable = ctx.compute->getSleepStatus(*settings) == chi::ComputeStatus::eOk;
        ctx.latencyReportAvailable = ctx.compute->getLatencyReport(*settings) == chi::ComputeStatus::eOk;
    }
    settings->lowLatencyAvailable = ctx.lowLatencyAvailable;
    settings->latencyReportAvailable = ctx.latencyReportAvailable;
    settings->flashIndicatorDriverControlled = ctx.flashIndicatorDriverControlled;
    // Allow host to check Windows messages for the special low latency message
    settings->statsWindowMessage = g_PCLStatsWindowMessage;
    return Result::eOk;
}

//! Allows other plugins to set GPU agnostic stats
void setLatencyStatsMarker(ReflexMarker marker, uint32_t frameId)
{
    PCLSTATS_MARKER(marker, frameId);
}

//! Main entry point - starting our plugin
//! 
//! IMPORTANT: Plugins are started based on their priority.
//! sl.common always starts first since it has priority 0
//!
bool slOnPluginStartup(const char* jsonConfig, void* device)
{
    //! Common startup and setup
    //!     
    SL_PLUGIN_COMMON_STARTUP();

    auto& ctx = (*reflex::getContext());

    auto parameters = api::getContext()->parameters;

    //! Register our evaluate callbacks
    //!
    //! Note that sl.common handles evaluate calls from the host
    //! and distributes eval calls to the right plugin based on the feature id
    //! 
    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &ctx.registerEvaluateCallbacks))
    {
        SL_LOG_ERROR( "Cannot obtain `registerEvaluateCallbacks` interface - check that sl.common was initialized correctly");
        return false;
    }

    //! Allow other plugins to set latency stats
    parameters->set(param::latency::kPFunSetLatencyStatsMarker, setLatencyStatsMarker);

    //! Plugin manager gives us the device type and the application id
    //! 
    json& config = *(json*)api::getContext()->loaderConfig;
    uint32_t deviceType{};
    int appId{};
    config.at("appId").get_to(appId);
    config.at("deviceType").get_to(deviceType);
    if (config.contains("ngx"))
    {
        config.at("ngx").at("engineType").get_to(ctx.engine);
        if (ctx.engine == EngineType::eUnity)
        {
            SL_LOG_INFO("Detected Unity engine - using render submit markers instead of present to detect current frame");
        }
    }
    //! Now let's obtain compute interface if we need to dispatch some compute work
    //! 
    ctx.platform = (RenderAPI)deviceType;
    if (!param::getPointerParam(parameters, sl::param::common::kComputeAPI, &ctx.compute))
    {
        SL_LOG_ERROR( "Cannot obtain compute interface - check that sl.common was initialized correctly");
        return false;
    }

    json& extraConfig = *(json*)api::getContext()->extConfig;
    if (extraConfig.contains("frameLimitUs"))
    {
        extraConfig.at("frameLimitUs").get_to(ctx.frameLimitUs);
        SL_LOG_HINT("Read 'frameLimitUs' %u from JSON config", ctx.frameLimitUs);
    }
    if (extraConfig.contains("useMarkersToOptimize"))
    {
        extraConfig.at("useMarkersToOptimize").get_to(ctx.useMarkersToOptimizeOverrideValue);
        ctx.useMarkersToOptimizeOverride = true;
        SL_LOG_HINT("Read 'useMarkersToOptimize' %u from JSON config", ctx.useMarkersToOptimizeOverrideValue);
    }

    updateStats(0);

#ifndef SL_PRODUCTION
    // Check for UI and register our callback
    imgui::ImGUI* ui{};
    param::getPointerParam(parameters, param::imgui::kInterface, &ui);
    if (ui)
    {
        // Runs async from the present thread where UI is rendered just before frame is presented
        auto renderUI = [&ctx](imgui::ImGUI* ui, bool finalFrame)->void
        {
            auto v = api::getContext()->pluginVersion;
            if (ui->collapsingHeader(extra::format("sl.reflex v{}", (v.toStr() + "." + GIT_LAST_COMMIT_SHORT)).c_str(), imgui::kTreeNodeFlagDefaultOpen))
            {
                std::scoped_lock lock(ctx.uiStats.mtx);
                ui->text(ctx.uiStats.mode.c_str());
                ui->text(ctx.uiStats.markers.c_str());
                ui->text(ctx.uiStats.fpsCap.c_str());
                ui->text(ctx.uiStats.presentFrame.c_str());
                ui->text(ctx.uiStats.sleeping.c_str());
                if (finalFrame) ctx.stats = {};
            }
        };
        ui->registerRenderCallbacks(renderUI, nullptr);
    }
#endif

    return true;
}

//! Main exit point - shutting down our plugin
//! 
//! IMPORTANT: Plugins are shutdown in the inverse order based to their priority.
//! sl.common always shuts down LAST since it has priority 0
//!
void slOnPluginShutdown()
{
    auto& ctx = (*reflex::getContext());

    // If we used 'evaluate' mechanism reset the callbacks here
    ctx.registerEvaluateCallbacks(kFeatureReflex, nullptr, nullptr);

    //! GPU agnostic latency stats shutdown
    PCLSTATS_SHUTDOWN();

    // Common shutdown
    plugin::onShutdown(api::getContext());
}

//! Exports from sl_reflex.h
//! 
sl::Result slReflexGetState(sl::ReflexState& state)
{
    return slGetData(nullptr, &state, nullptr);
}

sl::Result slReflexSetMarker(sl::ReflexMarker marker, const sl::FrameToken& frame)
{
    sl::ReflexHelper inputs(marker);
    inputs.next = (BaseStructure*)&frame;
    return slSetData(&inputs, nullptr);
}

sl::Result slReflexSleep(const sl::FrameToken& frame)
{
    sl::ReflexHelper inputs((sl::ReflexMarker)0x1000);
    inputs.next = (BaseStructure*)&frame;
    return slSetData(&inputs,nullptr);
}

sl::Result slReflexSetOptions(const sl::ReflexOptions& options)
{
    return slSetData(&options, nullptr);
}

//! The only exported function - gateway to all functionality
SL_EXPORT void* slGetPluginFunction(const char* functionName)
{
    //! Forward declarations
    bool slOnPluginLoad(sl::param::IParameters * params, const char* loaderJSON, const char** pluginJSON);

    //! Redirect to OTA if any
    SL_EXPORT_OTA;

    //! Core API
    SL_EXPORT_FUNCTION(slOnPluginLoad);
    SL_EXPORT_FUNCTION(slOnPluginShutdown);
    SL_EXPORT_FUNCTION(slOnPluginStartup);
    SL_EXPORT_FUNCTION(slSetData);
    SL_EXPORT_FUNCTION(slGetData);
    
    SL_EXPORT_FUNCTION(slReflexGetState);
    SL_EXPORT_FUNCTION(slReflexSetMarker);
    SL_EXPORT_FUNCTION(slReflexSleep);
    SL_EXPORT_FUNCTION(slReflexSetOptions);

    return nullptr;
}

}

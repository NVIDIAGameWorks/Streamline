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

#include "include/sl.h"
#include "include/sl_consts.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.param/parameters.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.template/versions.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "source/plugins/sl.pcl/pcl.h"
#include "_artifacts/gitVersion.h"
#include "_artifacts/json/pcl_json.h"
#include "external/json/include/nlohmann/json.hpp"
#include "pclImpl.h"
using json = nlohmann::json;

//! GPU agnostic stats definition
PCLSTATS_DEFINE();

namespace sl
{

namespace pcl
{

static_assert(to_underlying(PCLHotKey::eVK_F13) == VK_F13);
static_assert(to_underlying(PCLHotKey::eVK_F14) == VK_F14);
static_assert(to_underlying(PCLHotKey::eVK_F15) == VK_F15);

static_assert(to_underlying(PCLMarker::eSimulationStart) == PCLSTATS_SIMULATION_START);
static_assert(to_underlying(PCLMarker::eSimulationEnd) == PCLSTATS_SIMULATION_END);
static_assert(to_underlying(PCLMarker::eRenderSubmitStart) == PCLSTATS_RENDERSUBMIT_START);
static_assert(to_underlying(PCLMarker::eRenderSubmitEnd) == PCLSTATS_RENDERSUBMIT_END);
static_assert(to_underlying(PCLMarker::ePresentStart) == PCLSTATS_PRESENT_START);
static_assert(to_underlying(PCLMarker::ePresentEnd) == PCLSTATS_PRESENT_END);
//static_assert(to_underlying(PCLMarker::eInputSample) == PCLSTATS_INPUT_SAMPLE); // Deprecated
static_assert(to_underlying(PCLMarker::eTriggerFlash) == PCLSTATS_TRIGGER_FLASH);
static_assert(to_underlying(PCLMarker::ePCLatencyPing) == PCLSTATS_PC_LATENCY_PING);
static_assert(to_underlying(PCLMarker::eOutOfBandRenderSubmitStart) == PCLSTATS_OUT_OF_BAND_RENDERSUBMIT_START);
static_assert(to_underlying(PCLMarker::eOutOfBandRenderSubmitEnd) == PCLSTATS_OUT_OF_BAND_RENDERSUBMIT_END);
static_assert(to_underlying(PCLMarker::eOutOfBandPresentStart) == PCLSTATS_OUT_OF_BAND_PRESENT_START);
static_assert(to_underlying(PCLMarker::eOutOfBandPresentEnd) == PCLSTATS_OUT_OF_BAND_PRESENT_END);
static_assert(to_underlying(PCLMarker::eControllerInputSample) == PCLSTATS_CONTROLLER_INPUT_SAMPLE);
static_assert(to_underlying(PCLMarker::eDeltaTCalculation) == PCLSTATS_DELTA_T_CALCULATION);
static_assert(to_underlying(PCLMarker::eLateWarpPresentStart) == PCLSTATS_LATE_WARP_PRESENT_START);
static_assert(to_underlying(PCLMarker::eLateWarpPresentEnd) == PCLSTATS_LATE_WARP_PRESENT_END);

//! Our common context
//! 
//! Here we can keep whatever global state we need
//! 
struct LatencyContext
{
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(LatencyContext);
    void onCreateContext() {};
    void onDestroyContext() {};


    //! Latest constants
    PCLOptions constants{};
    PFun_slPCLSetMarker* slReflexSetMarker{};
};
}

//! Embedded JSON, containing information about the plugin and the hooks it requires.
static std::string JSON = std::string(pcl_json, &pcl_json[pcl_json_len]);

void updateEmbeddedJSON(json& config);

//! Define our plugin, make sure to update version numbers in versions.h
SL_PLUGIN_DEFINE("sl.pcl", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON.c_str(), updateEmbeddedJSON, pcl, LatencyContext)

//! Figure out if we are supported on the current hardware or not
//! 
void updateEmbeddedJSON(json& config)
{
    auto& ctx = (*pcl::getContext());

    // Check if plugin is supported or not on this platform and set the flag accordingly
    common::SystemCaps* caps = {};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kSystemCaps, &caps);
    common::PFunUpdateCommonEmbeddedJSONConfig* updateCommonEmbeddedJSONConfig{};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kPFunUpdateCommonEmbeddedJSONConfig, &updateCommonEmbeddedJSONConfig);
    if (caps && updateCommonEmbeddedJSONConfig)
    {
        // All defaults since sl.pcl can run on any adapter
        common::PluginInfo info{};
        info.SHA = GIT_LAST_COMMIT_SHORT;
        updateCommonEmbeddedJSONConfig(&config, info);
    }
}

//! Set constants for our plugin (if any, this is optional and should be thread safe)
Result slSetData(const BaseStructure* inputs, CommandBuffer* cmdBuffer)
{
    auto& ctx = (*pcl::getContext());
    return sl::pcl::implSetData(inputs, ctx.constants);
}

Result slGetData(const BaseStructure* inputs, BaseStructure* outputs, CommandBuffer* cmdBuffer)
{
    return sl::pcl::implGetData(outputs);
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

    auto parameters = api::getContext()->parameters;

    sl::pcl::implOnPluginStartup(parameters, &slGetData, &slSetData);

    return true;
}

//! Main exit point - shutting down our plugin
//! 
//! IMPORTANT: Plugins are shutdown in the inverse order based to their priority.
//! sl.common always shuts down LAST since it has priority 0
//!
void slOnPluginShutdown()
{
    auto parameters = api::getContext()->parameters;
    sl::pcl::implOnPluginShutdown(parameters);

    // Common shutdown
    plugin::onShutdown(api::getContext());
}


//! Exports from sl_pcl.h
//! 
sl::Result slPCLGetState(sl::PCLState& state)
{
    return slGetData(nullptr, &state, nullptr);
}

sl::Result slPCLSetMarker(sl::PCLMarker marker, const sl::FrameToken& frame)
{
    auto& ctx = (*pcl::getContext());
    return sl::pcl::implSetMarker(&ctx.slReflexSetMarker, ctx.constants, marker, frame);
}

sl::Result slPCLSetOptions(const sl::PCLOptions& options)
{
    return slSetData(&options, nullptr);
}


//! The only exported function - gateway to all functionality
SL_EXPORT void* slGetPluginFunction(const char* functionName)
{
    //! Core API
    SL_EXPORT_FUNCTION(slOnPluginLoad);
    SL_EXPORT_FUNCTION(slOnPluginShutdown);
    SL_EXPORT_FUNCTION(slOnPluginStartup);
    SL_EXPORT_FUNCTION(slSetData);
    SL_EXPORT_FUNCTION(slGetData);
    
    SL_EXPORT_FUNCTION(slPCLGetState);
    SL_EXPORT_FUNCTION(slPCLSetMarker);
    SL_EXPORT_FUNCTION(slPCLSetOptions);

    return nullptr;
}

}

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
#include "external/nvapi/nvapi.h"
#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

//! IMPORTANT: This is our include with our constants and settings (if any)
//!
#include "include/sl_template.h"

namespace sl
{

//! Our common context
//! 
//! Here we can keep whatever global state we need
//! 
struct TemplateContext
{
    common::PFunRegisterEvaluateCallbacks* registerEvaluateCallbacks{};

    // For example, we can use this template to store incoming constants
    // 
    common::ViewportIdFrameData<> constants = { "template" };

    // Common constants for the frame/viewport we are currently evaluating
    // 
    // See 'templateBeginEvaluation' below for more details
    sl::Constants* commonConsts;

    // Feature constants (if any)
    //
    // Note that we can chain as many feature
    // constants as we want using the void* ext link.
    sl::TemplateConstants* templateConsts;

    // Some compute kernel we want to use
    chi::Kernel myDenoisingKernel{};

    // Our tagged inputs
    chi::Resource mvec{};
    chi::Resource depth{};
    chi::Resource input{};
    chi::Resource output{};
    sl::Extent mvecExt{};
    sl::Extent depthExt{};

    // Resource states provided by the host (optional)
    uint32_t mvecState{}, depthState{}, outputState{}, inputState{};

    // Compute API
    chi::PlatformType platform = chi::ePlatformTypeD3D12;
    chi::ICompute* compute{};
};
static TemplateContext s_ctx = {};

//! Set constants for our plugin (if any, this is optional and should be thread safe)
bool slSetConstants(const void* data, uint32_t frameIndex, uint32_t id)
{
    // For example, we can set out constants like this
    // 
    auto consts = (const TemplateConstants*)data;
    s_ctx.constants.set(frameIndex, id, consts);
    if (consts->mode == TemplateMode::eOff)
    {
        // User disabled our feature
        auto lambda = [/*capture references and data you need*/](void)->void
        {
            // Cleanup logic goes here
        };
        // Schedule delayed destroy (few frames later)
        CHI_VALIDATE(s_ctx.compute->destroy(lambda));
    }
    else
    {
        // User enabled our feature, nothing to do here
        // but rather in 'templateBeginEvaluation' when
        // we have access to the command buffer.
    }
    return true;
}

//! Begin evaluation for our plugin (if we use evalFeature mechanism to inject functionality in to the command buffer)
//! 
void templateBeginEvaluation(chi::CommandList pCmdList, const common::EventData& evd)
{
    //! Here we can go and fetch our constants based on the 'event data' - frame index, unique id etc.
    //! 

    // Get common constants if we need them
    //
    // Note that we are passing frame index, unique id provided with the 'evaluateFeature' call
    if (!common::getConsts(evd, &s_ctx.commonConsts))
    {
        SL_LOG_ERROR("Cannot obtain common constants");
        return;
    }

    // Get our constants (if any)
    //
    // Note that we are passing frame index, unique id provided with the 'evaluateFeature' call
    if (!s_ctx.constants.get(evd, &s_ctx.templateConsts))
    {
        SL_LOG_ERROR("Cannot obtain constants for sl.template plugin");
    }

    // Get tagged resources (if you need any)
    // 
    // For example, here we fetch depth and mvec with their extents
    // 
    getTaggedResource(eBufferTypeDepth, s_ctx.depth, evd.id, &s_ctx.depthExt, &s_ctx.depthState);
    getTaggedResource(eBufferTypeMVec, s_ctx.mvec, evd.id, &s_ctx.mvecExt, &s_ctx.mvecState);
    // Now we fetch shadow in/out, assuming our plugin does some sort of denoising
    getTaggedResource(eBufferTypeShadowNoisy, s_ctx.input, evd.id, nullptr, &s_ctx.inputState);
    getTaggedResource(eBufferTypeShadowDenoised, s_ctx.output, evd.id, nullptr, &s_ctx.outputState);

    // If tagged resources are mandatory check if they are provided or not
    if (!s_ctx.depth || !s_ctx.mvec || !s_ctx.input || !s_ctx.output)
    {
        SL_LOG_ERROR("Missing mandatory tags for sl.template plugin");
        return;
    }

    // If you need the extents check if they are valid
    if (!s_ctx.depthExt || !s_ctx.mvecExt)
    {
        SL_LOG_ERROR("Missing mandatory extents on tagged resources for sl.template plugin");
        return;
    }

    // Initialize your feature if it was never initialized before or if user toggled it back on by setting consts.mode = TemplateMode::eOn
    //
    // Use compute API to allocated any temporary buffers/textures you need here.
    //
    // You can also check if extents changed, resolution changed (can be passed as a plugin/feature constant for example)
}

//! End evaluation for our plugin (if we use evalFeature mechanism to inject functionality in to the command buffer)
//! 
void templateEndEvaluation(chi::CommandList cmdList)
{
    // For example, dispatch compute shader work

    chi::ResourceState mvecState{}, depthState{}, outputState{}, inputState{};

    // Check if state was give to us or not
    if (s_ctx.mvecState)
    {
        // Convert native to SL state
        CHI_VALIDATE(s_ctx.compute->getResourceState(s_ctx.mvecState, mvecState));
    }
    else
    {
        // Use internal resource tracking
        CHI_VALIDATE(s_ctx.compute->getResourceState(s_ctx.mvec, mvecState));
    }
    CHI_VALIDATE(s_ctx.compute->getResourceState(s_ctx.depth, depthState));
    CHI_VALIDATE(s_ctx.compute->getResourceState(s_ctx.input, inputState));
    CHI_VALIDATE(s_ctx.compute->getResourceState(s_ctx.output, outputState));

    // Scoped transition, it will return the resources back to their original states upon leaving this scope
    // 
    // This is optional but convenient so we don't have to call transition resources twice
    extra::ScopedTasks revTransitions;
    chi::ResourceTransition transitions[] =
    {
        {s_ctx.mvec, chi::ResourceState::eTextureRead, mvecState},
        {s_ctx.depth, chi::ResourceState::eTextureRead, depthState},
        {s_ctx.input, chi::ResourceState::eTextureRead, inputState},
        {s_ctx.output, chi::ResourceState::eStorageRW, outputState}
    };
    CHI_VALIDATE(s_ctx.compute->transitionResources(cmdList, transitions, (uint32_t)countof(transitions), &revTransitions));

    // Assuming 1080p dispatch
    uint32_t renderWidth = 1920;
    uint32_t renderHeight = 1080;
    uint32_t grid[] = { (renderWidth + 16 - 1) / 16, (renderHeight + 16 - 1) / 16, 1 };

    // Now setup our constants
    struct MyParamStruct
    {
        // Some dummy parameters for demonstration
        sl::float4x4 dummy0;
        sl::float4 dummy1;
        sl::float2 dummy2;
        uint32_t dummy3;
    };
    MyParamStruct cb{};

    // NOTE: SL compute interface uses implicit dispatch for simplicity.
    // 
    // Root signatures, constant updates, pipeline states etc. are all
    // managed automatically for convenience.


    // First we bind our descriptor heaps and other shared state
    CHI_VALIDATE(s_ctx.compute->bindSharedState(cmdList));
    // Now our kernel
    CHI_VALIDATE(s_ctx.compute->bindKernel(s_ctx.myDenoisingKernel));
    // Now our inputs, binding slot first, register second
    // This has to match your shader exactly
    CHI_VALIDATE(s_ctx.compute->bindSampler(0, 0, chi::eSamplerLinearClamp));
    CHI_VALIDATE(s_ctx.compute->bindTexture(1, 0, s_ctx.mvec));
    CHI_VALIDATE(s_ctx.compute->bindTexture(2, 1, s_ctx.depth));
    CHI_VALIDATE(s_ctx.compute->bindTexture(3, 2, s_ctx.input));
    CHI_VALIDATE(s_ctx.compute->bindRWTexture(4, 0, s_ctx.output));
    CHI_VALIDATE(s_ctx.compute->bindConsts(5, 0, &cb, sizeof(MyParamStruct), 3)); // 3 instances per frame, change as needed (num times we dispatch this kernel with different consants per frame)
    CHI_VALIDATE(s_ctx.compute->dispatch(grid[0], grid[1], grid[2]));

    // NOTE: sl.common will restore the pipeline to its original state 
    // 
    // When we return to the host from 'evaluateFeature' it will be like SL never changed anything
}

//! Get settings for our plugin (optional and depending on if we need to provide any settings back to the host)
bool slGetSettings(const void* cdata, void* sdata)
{
    // For example, we can set out constants like this
    // 
    // Note that TemplateConstants should be defined in sl_constants.h and provided by the host
    // 
    auto consts = (const TemplateConstants*)cdata;
    auto settings = (TemplateSettings*)sdata;
    return true;
}

//! Explicit allocation of resources
bool slAllocateResources(sl::CommandBuffer* cmdBuffer, sl::Feature feature, uint32_t id)
{
    return true;
}

//! Explicit de-allocation of resources
bool slFreeResources(sl::Feature feature, uint32_t id)
{
    return true;
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
    //! IMPORTANT: Add new enum in sl.h and match that id in JSON config for this plugin (see below)
    s_ctx.registerEvaluateCallbacks(/* Change to correct enum */ (Feature)eFeatureTemplate, templateBeginEvaluation, templateEndEvaluation);

    //! Plugin manager gives us the device type and the application id
    //! 
    json& config = *(json*)api::getContext()->loaderConfig;
    uint32_t deviceType{};
    int appId{};
    config.at("appId").get_to(appId);
    config.at("deviceType").get_to(deviceType);

    //! Extra config is always `sl.plugin_name.json` so in our case `sl.template.json`
    //! 
    //! Populated automatically by the SL_PLUGIN_COMMON_STARTUP macro
    //! 
    json& extraConfig = *(json*)api::getContext()->extConfig;
    if (extraConfig.contains("myKey"))
    {
        //! Extract your configuration data and do something with it
    }

    //! Now let's obtain compute interface if we need to dispatch some compute work
    //! 
    s_ctx.platform = (chi::PlatformType)deviceType;
    if (!param::getPointerParam(parameters, sl::param::common::kComputeAPI, &s_ctx.compute))
    {
        SL_LOG_ERROR("Cannot obtain compute interface - check that sl.common was initialized correctly");
        return false;
    }

    //! We can also register some hot-keys to toggle functionality etc.
    //! 
    //extra::keyboard::getInterface()->registerKey("my_key", extra::keyboard::VirtKey(VK_OEM_6, true, true));

    //! Now we create our kernel using the pre-compiled binary blobs (included from somewhere)
    if (s_ctx.platform == chi::ePlatformTypeVK)
    {
        // SPIR-V binary blob
        // 
        //CHI_CHECK_RF(s_ctx.compute->createKernel((void*)myDenoisingKernel_spv, myDenoisingKernel_spv_len, "myDenoisingKernel.cs", "main", s_ctx.myDenoisingKernel));
    }
    else
    {
        // DXBC binary blob
        // 
        //CHI_CHECK_RF(s_ctx.compute->createKernel((void*)myDenoisingKernel_cs, myDenoisingKernel_cs_len, "myDenoisingKernel.cs", "main", s_ctx.myDenoisingKernel));
    }
    return true;
}

//! Main exit point - shutting down our plugin
//! 
//! IMPORTANT: Plugins are shutdown in the inverse order based to their priority.
//! sl.common always shutsdown LAST since it has priority 0
//!
void slOnPluginShutdown()
{
    // Here we need to release/destroy any resource we created
    CHI_VALIDATE(s_ctx.compute->destroyKernel(s_ctx.myDenoisingKernel));

    // If we used 'evaluateFeature' mechanism reset the callbacks here
    //
    //! IMPORTANT: Add new enum in sl.h and match that id in JSON config for this plugin (see below)
    s_ctx.registerEvaluateCallbacks(/* Change to correct enum and also update the JSON config below */ (Feature)eFeatureTemplate, nullptr, nullptr);

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
//! IMPORTANT: Please note that id must be provided and it has to match the Feature enum we assign for this plugin
//!
static const char* JSON = R"json(
{
    "comment_id" : "id must match the sl::Feature enum in sl.h or sl_template.h, for example sl.dlss has id 0 hence eFeatureDLSS = 0",
    "id" : 65535,
    "comment_priority" : "plugins are executed in the order of their priority so keep that in mind",
    "priority" : 1,
    
    "comment_namespace" : "rename this to the namespace used for parameters used by your plugin",
    "namespace" : "template",
    
    "comment_optional_dependencies" : "the following lists can be used to specify dependencies, incompatibilities with other plugin(s)",
    "required_plugins" : ["sl.some_plugin_we_depend_on"],
    "exclusive_hooks" : ["IDXGISwapChain_SomeFunctionNobodyElseCanUse"],
    "incompatible_plugins" : ["sl.some_plugin_we_are_not_compatible_with"],
    
    "hooks" :
    [
        {
            "class": "IDXGISwapChain",
            "target" : "Present",
            "replacement" : "slHookPresent",
            "base" : "before"
        }
    ]
}
)json";

//! Example hook to handle SwapChain::Present calls
//! 
HRESULT slHookPresent(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, bool& Skip)
{
    // NOP present hook, we tell host NOT to skip the base implementation and return OK
    // 
    // This is just an example, if your plugin just needs to do something in `evaluateFeature`
    // then no hooks are necessary.
    //
    Skip = false;
    return S_OK;
}

//! Figure out if we are supported on the current hardware or not
//! 
uint32_t getSupportedAdapterMask()
{
    // Here we need to return a bitmask indicating on which adapter are we supported

    // If always supported across all adapters simply set all bits to 1
    uint32_t adapterMask = 0;

    // System capabilities info provided by sl.common
    common::SystemCaps* info = {};
    if (!param::getPointerParam(api::getContext()->parameters, sl::param::common::kSystemCaps, &info))
    {
        // Failed, do your own HW checks
    }
    else
    {
        for (uint32_t i = 0; i < info->gpuCount; i++)
        {
            // For example, we want to check for Turing+ to enable our feature
            auto turingOrBetter = info->architecture[i] >= NV_GPU_ARCHITECTURE_ID::NV_GPU_ARCHITECTURE_TU100;
            if (turingOrBetter)
            {
                adapterMask |= 1 << i;
            }
        }
    }
    return adapterMask;
}

//! Define our plugin, make sure to update version numbers in versions.h
SL_PLUGIN_DEFINE("sl.template", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON, getSupportedAdapterMask())

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
    SL_EXPORT_FUNCTION(slAllocateResources);
    SL_EXPORT_FUNCTION(slFreeResources);

    //! Hooks defined in the JSON config above

    //! D3D12
    SL_EXPORT_FUNCTION(slHookPresent);

    return nullptr;
}

}
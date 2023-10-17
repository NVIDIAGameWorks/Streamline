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
#include "_artifacts/json/template_json.h"
#include "_artifacts/gitVersion.h"

using json = nlohmann::json;

//! IMPORTANT: This is our include with our constants and settings (if any)
//!
#include "include/sl_template.h"

namespace sl
{

namespace tmpl
{
//! Our common context
//! 
//! Here we can keep whatever global state we need
//! 
struct TemplateContext
{
    SL_PLUGIN_CONTEXT_CREATE_DESTROY(TemplateContext);

    // Called when plugin is loaded, do any custom constructor initialization here
    void onCreateContext() {};

    // Called when plugin is unloaded, destroy any objects on heap here
    void onDestroyContext() {};

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
    CommonResource mvec{};
    CommonResource depth{};
    CommonResource input{};
    CommonResource output{};

    // Compute API
    RenderAPI platform = RenderAPI::eD3D12;
    chi::ICompute* compute{};
};
}

void updateEmbeddedJSON(json& config);

//! Embedded JSON, containing information about the plugin and the hooks it requires.
//! See template.json
static std::string JSON = std::string(template_json, &template_json[template_json_len]);

//! Define our plugin, make sure to update version numbers in versions.h
SL_PLUGIN_DEFINE("sl.template", Version(VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH), Version(0, 0, 1), JSON.c_str(), updateEmbeddedJSON, tmpl, TemplateContext)

//! Set constants for our plugin (if any, this is optional and should be thread safe)
Result slSetConstants(const void* data, uint32_t frameIndex, uint32_t id)
{
    auto& ctx = (*tmpl::getContext());

    // For example, we can set out constants like this
    // 
    auto consts = (const TemplateConstants*)data;
    ctx.constants.set(frameIndex, id, consts);
    if (consts->mode == TemplateMode::eOff)
    {
        // User disabled our feature
        auto lambda = [/*capture references and data you need*/](void)->void
        {
            // Cleanup logic goes here
        };
        // Schedule delayed destroy (few frames later)
        CHI_VALIDATE(ctx.compute->destroy(lambda));
    }
    else
    {
        // User enabled our feature, nothing to do here
        // but rather in 'templateBeginEvaluation' when
        // we have access to the command buffer.
    }
    return Result::eOk;
}

//! Begin evaluation for our plugin (if we use evalFeature mechanism to inject functionality in to the command buffer)
//! 
sl::Result templateBeginEvaluation(chi::CommandList pCmdList, const common::EventData& evd, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    auto& ctx = (*tmpl::getContext());

    //! Here we can go and fetch our constants based on the 'event data' - frame index, unique id etc.
    //! 

    // Get common constants if we need them
    //
    // Note that we are passing frame index, unique id provided with the 'evaluate' call
    if (!common::getConsts(evd, &ctx.commonConsts))
    {
        // Log error
        return sl::Result::eErrorMissingConstants;
    }

    // Get our constants (if any)
    //
    // Note that we are passing frame index, unique id provided with the 'evaluate' call
    if (!ctx.constants.get(evd, &ctx.templateConsts))
    {
        // Log error
    }

    // Get tagged resources (if you need any)
    // 
    // For example, here we fetch depth and mvec with their extents
    // 
    getTaggedResource(kBufferTypeDepth, ctx.depth, evd.id, false, inputs, numInputs);
    getTaggedResource(kBufferTypeMotionVectors, ctx.mvec, evd.id, false, inputs, numInputs);
    // Now we fetch shadow in/out, assuming our plugin does some sort of denoising
    getTaggedResource(kBufferTypeShadowNoisy, ctx.input, evd.id, false, inputs, numInputs);
    getTaggedResource(kBufferTypeShadowDenoised, ctx.output, evd.id, false, inputs, numInputs);

    // If tagged resources are mandatory check if they are provided or not
    if (!ctx.depth || !ctx.mvec || !ctx.input || !ctx.output)
    {
        // Log error
        return sl::Result::eErrorMissingInputParameter;
    }

    // If you need the extents check if they are valid
    if (!ctx.depth.getExtent() || !ctx.mvec.getExtent())
    {
        // Log error
        return sl::Result::eErrorMissingInputParameter;
    }

    // Initialize your feature if it was never initialized before or if user toggled it back on by setting consts.mode = TemplateMode::eOn
    //
    // Use compute API to allocated any temporary buffers/textures you need here.
    //
    // You can also check if extents changed, resolution changed (can be passed as a plugin/feature constant for example)
    return Result::eOk;
}

//! End evaluation for our plugin (if we use evalFeature mechanism to inject functionality in to the command buffer)
//! 
sl::Result templateEndEvaluation(chi::CommandList cmdList, const common::EventData& evd, const sl::BaseStructure** inputs, uint32_t numInputs)
{
    // For example, dispatch compute shader work

    auto& ctx = (*tmpl::getContext());

    chi::ResourceState mvecState{}, depthState{}, outputState{}, inputState{};

    // Convert native to SL state
    CHI_VALIDATE(ctx.compute->getResourceState(ctx.mvec.getState(), mvecState));
    
    CHI_VALIDATE(ctx.compute->getResourceState(ctx.depth.getState(), depthState));
    CHI_VALIDATE(ctx.compute->getResourceState(ctx.input.getState(), inputState));
    CHI_VALIDATE(ctx.compute->getResourceState(ctx.output.getState(), outputState));

    // Scoped transition, it will return the resources back to their original states upon leaving this scope
    // 
    // This is optional but convenient so we don't have to call transition resources twice
    extra::ScopedTasks revTransitions;
    chi::ResourceTransition transitions[] =
    {
        {ctx.mvec, chi::ResourceState::eTextureRead, mvecState},
        {ctx.depth, chi::ResourceState::eTextureRead, depthState},
        {ctx.input, chi::ResourceState::eTextureRead, inputState},
        {ctx.output, chi::ResourceState::eStorageRW, outputState}
    };
    CHI_VALIDATE(ctx.compute->transitionResources(cmdList, transitions, (uint32_t)countof(transitions), &revTransitions));

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
    CHI_VALIDATE(ctx.compute->bindSharedState(cmdList));
    // Now our kernel
    CHI_VALIDATE(ctx.compute->bindKernel(ctx.myDenoisingKernel));
    // Now our inputs, binding slot first, register second
    // This has to match your shader exactly
    CHI_VALIDATE(ctx.compute->bindSampler(0, 0, chi::eSamplerLinearClamp));
    CHI_VALIDATE(ctx.compute->bindTexture(1, 0, ctx.mvec));
    CHI_VALIDATE(ctx.compute->bindTexture(2, 1, ctx.depth));
    CHI_VALIDATE(ctx.compute->bindTexture(3, 2, ctx.input));
    CHI_VALIDATE(ctx.compute->bindRWTexture(4, 0, ctx.output));
    CHI_VALIDATE(ctx.compute->bindConsts(5, 0, &cb, sizeof(MyParamStruct), 3)); // 3 instances per frame, change as needed (num times we dispatch this kernel with different consants per frame)
    CHI_VALIDATE(ctx.compute->dispatch(grid[0], grid[1], grid[2]));

    // NOTE: sl.common will restore the pipeline to its original state 
    // 
    // When we return to the host from 'evaluate' it will be like SL never changed anything
    return Result::eOk;
}

//! Get settings for our plugin (optional and depending on if we need to provide any settings back to the host)
Result slGetSettings(const void* cdata, void* sdata)
{
    // For example, we can set out constants like this
    // 
    // Note that TemplateConstants should be defined in sl_constants.h and provided by the host
    // 
    auto consts = (const TemplateConstants*)cdata;
    auto settings = (TemplateSettings*)sdata;
    return Result::eOk;
}

//! Explicit allocation of resources
Result slAllocateResources(sl::CommandBuffer* cmdBuffer, sl::Feature feature, const sl::ViewportHandle& viewport)
{
    return Result::eOk;
}

//! Explicit de-allocation of resources
Result slFreeResources(sl::Feature feature, const sl::ViewportHandle& viewport)
{
    return Result::eOk;
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

    auto& ctx = (*tmpl::getContext());

    auto parameters = api::getContext()->parameters;

    //! Register our evaluate callbacks
    //!
    //! Note that sl.common handles evaluate calls from the host
    //! and distributes eval calls to the right plugin based on the feature id
    //! 
    if (!param::getPointerParam(parameters, param::common::kPFunRegisterEvaluateCallbacks, &ctx.registerEvaluateCallbacks))
    {
        // Log error
        return false;
    }
    //! IMPORTANT: Add new enum in sl.h and match that id in JSON config for this plugin (see below)
    ctx.registerEvaluateCallbacks(/* Change to correct id */ kFeatureTemplate, templateBeginEvaluation, templateEndEvaluation);

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
    ctx.platform = (RenderAPI)deviceType;
    if (!param::getPointerParam(parameters, sl::param::common::kComputeAPI, &ctx.compute))
    {
        // Log error
        return false;
    }

    //! We can also register some hot-keys to toggle functionality etc.
    //! 
    //extra::keyboard::getInterface()->registerKey("my_key", extra::keyboard::VirtKey(VK_OEM_6, true, true));

    //! Now we create our kernel using the pre-compiled binary blobs (included from somewhere)
    if (ctx.platform == RenderAPI::eVulkan)
    {
        // SPIR-V binary blob
        // 
        //CHI_CHECK_RF(ctx.compute->createKernel((void*)myDenoisingKernel_spv, myDenoisingKernel_spv_len, "myDenoisingKernel.cs", "main", ctx.myDenoisingKernel));
    }
    else
    {
        // DXBC binary blob
        // 
        //CHI_CHECK_RF(ctx.compute->createKernel((void*)myDenoisingKernel_cs, myDenoisingKernel_cs_len, "myDenoisingKernel.cs", "main", ctx.myDenoisingKernel));
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
    auto& ctx = (*tmpl::getContext());

    // Here we need to release/destroy any resource we created
    CHI_VALIDATE(ctx.compute->destroyKernel(ctx.myDenoisingKernel));

    // If we used 'evaluate' mechanism reset the callbacks here
    //
    //! IMPORTANT: Add new enum in sl.h and match that id in JSON config for this plugin (see below)
    ctx.registerEvaluateCallbacks(/* Change to correct id and also update the JSON config below */ kFeatureTemplate, nullptr, nullptr);

    // Common shutdown
    plugin::onShutdown(api::getContext());
}

//! Example hook to handle SwapChain::Present calls
//! 
HRESULT slHookPresent(IDXGISwapChain* swapChain, UINT SyncInterval, UINT Flags, bool& Skip)
{
    // NOP present hook, we tell host NOT to skip the base implementation and return OK
    // 
    // This is just an example, if your plugin just needs to do something in `evaluate`
    // then no hooks are necessary.
    //
    Skip = false;
    return S_OK;
}

//! Figure out if we are supported on the current hardware or not
//! 
void updateEmbeddedJSON(json& config)
{
    // Check if plugin is supported or not on this platform and set the flag accordingly
    common::SystemCaps* caps = {};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kSystemCaps, &caps);
    common::PFunUpdateCommonEmbeddedJSONConfig* updateCommonEmbeddedJSONConfig{};
    param::getPointerParam(api::getContext()->parameters, sl::param::common::kPFunUpdateCommonEmbeddedJSONConfig, &updateCommonEmbeddedJSONConfig);
    if (caps && updateCommonEmbeddedJSONConfig)
    {
        common::PluginInfo info{};
        // Specify minimum driver version we need
        info.minDriver = sl::Version(455, 0, 0);
        // SL does not work on Win7, only Win10+
        info.minOS = sl::Version(10, 0, 0);
        // Specify 0 if our plugin runs on any adapter otherwise specify enum value `NV_GPU_ARCHITECTURE_*` from NVAPI
        info.minGPUArchitecture = 0;
        updateCommonEmbeddedJSONConfig(&config, info);
    }
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

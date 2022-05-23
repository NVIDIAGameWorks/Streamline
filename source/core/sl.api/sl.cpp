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
#include <d3d12.h>
#include <versionhelpers.h>
#else
#include <unistd.h>
#endif
#include <chrono>

#include "include/sl.h"
#include "internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.interposer/hook.h"
#include "source/core/sl.plugin-manager/pluginManager.h"

using namespace sl;

//! API

#define SL_VALIDATE_STATE                                                                                                                   \
if (!interposer::getInterface()->isEnabled())                                                                                               \
{                                                                                                                                           \
    return false;                                                                                                                           \
}                                                                                                                                           \
                                                                                                                                            \
if (!plugin_manager::getInterface()->arePluginsLoaded())                                                                                    \
{                                                                                                                                           \
    SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");     \
    return false;                                                                                                                           \
}

bool slInit(const Preferences &pref, int applicationId)
{
    if (!applicationId)
    {
        // Use a generic Id for now
        applicationId = 100721531;
    }

#ifdef SL_PRODUCTION
    auto* pref1 = (Preferences1*)pref.ext;
    if (!pref1 || !pref1->featuresToEnable || pref1->numFeaturesToEnable == 0)
    {
        SL_LOG_WARN("All features will be DISABLED - the explicit list of features to enable must be specified in production builds");
    }
#endif

    // Setup logging first
    auto log = log::getInterface();
    log->enableConsole(pref.showConsole);
    log->setLogLevel(pref.logLevel);
    log->setLogPath(pref.pathToLogsAndData);
    log->setLogName(L"sl.log");
    log->setLogCallback((void*)pref.logMessageCallback);
    
    if (sl::interposer::getInterface()->isEnabled())
    {
#ifndef SL_PRODUCTION
        // Allow overrides via 'sl.interposer.json'
        {
            auto config = sl::interposer::getInterface()->getConfig();
            if (config.contains("showConsole"))
            {
                bool showConsole = false;
                config.at("showConsole").get_to(showConsole);
                log->enableConsole(showConsole);
                SL_LOG_HINT("Overriding show console to %u", showConsole);
            }
            if (config.contains("logPath"))
            {
                std::string path;
                config.at("logPath").get_to(path);
                log->setLogPath(extra::toWStr(path).c_str());
                SL_LOG_HINT("Overriding log path to %s", path.c_str());
            }
            if (config.contains("logLevel"))
            {
                uint32_t level;
                config.at("logLevel").get_to(level);
                level = std::clamp(level, 0U, 2U);
                log->setLogLevel((LogLevel)level);
                SL_LOG_HINT("Overriding log level to %u", level);
            }
        }
#endif

        auto manager = plugin_manager::getInterface();
        if (manager->isInitialized())
        {
            SL_LOG_ERROR("slInit must be called before any DXGI/D3D12/D3D11/Vulkan APIs are invoked");
            return true;
        }
        if (!manager->isSupportedOnThisMachine())
        {
            // Error reported by the above method
            return false;
        }

        manager->setPreferences(pref);
        manager->setApplicationId(applicationId);

        param::getInterface()->set(param::global::kPFunAllocateResource, pref.allocateCallback);
        param::getInterface()->set(param::global::kPFunReleaseResource, pref.releaseCallback);
        param::getInterface()->set(param::global::kLogInterface, log::getInterface());

        // Enumerate plugins and check if they are supported or not
        return manager->loadPlugins();
    }

    return true;
}

bool slShutdown()
{
    if (interposer::getInterface()->isEnabled())
    {
        auto manager = plugin_manager::getInterface();
        if (!manager->isInitialized())
        {
            SL_LOG_ERROR_ONCE("SL not initialized, no plugins found or shutdown called multiple times");
            return false;
        }
        manager->unloadPlugins();
    }
    
    plugin_manager::destroyInterface();
    param::destroyInterface();
    log::destroyInterface();
    interposer::destroyInterface();

    return true;
}

bool slIsFeatureSupported(Feature feature, uint32_t* adapterBitMask)
{
    SL_VALIDATE_STATE

    auto ctx = plugin_manager::getInterface()->getFeatureContext(feature);
    if (!ctx)
    {
        SL_LOG_ERROR_ONCE("Feature %u is not supported or missing", feature);
        return false;
    }

    if (adapterBitMask)
    {
        *adapterBitMask = ctx->supportedAdapters;
    }
    return ctx->supportedAdapters != 0;
}

bool slSetFeatureEnabled(sl::Feature feature, bool enabled)
{
    SL_VALIDATE_STATE

    return plugin_manager::getInterface()->setFeatureEnabled(feature, enabled);
}

bool slSetTag(const Resource *resource, BufferType tag, uint32_t id, const Extent* extent)
{
    SL_VALIDATE_STATE

    auto ctx = plugin_manager::getInterface()->getFeatureContext(eFeatureCommon);
    if (!ctx)
    {
        return false;
    }

    PFunSlSetTag* fun;
    if (!getPointerParam(param::getInterface(), param::global::kPFunSetTag, &fun))
    {
        SL_LOG_ERROR_ONCE("Unable to obtain 'setTag' callback, sl.common plugin missing or not initialized");
        return false;
    }
    return fun(resource, tag, id, extent);
}

bool slSetConstants(const Constants &values, uint32_t frameIndex, uint32_t id)
{   
    SL_VALIDATE_STATE

    auto ctx = plugin_manager::getInterface()->getFeatureContext(eFeatureCommon);
    if (!ctx)
    {
        return false;
    }
    
    PFunSlSetConstants*fun;
    if (!getPointerParam(param::getInterface(), param::global::kPFunSetConsts, &fun))
    {
        SL_LOG_WARN_ONCE("Unable to obtain set common constants callback, sl.common plugin is either missing, failed to initialize or not initialized yet.");
        return false;
    }
    return fun(values, frameIndex, id);
}

bool slSetFeatureConstants(Feature feature, const void *consts, uint32_t frameIndex, uint32_t id)
{
    SL_VALIDATE_STATE

    auto ctx = plugin_manager::getInterface()->getFeatureContext(feature);
    if (!ctx)
    {
        return false;
    }

    if (!ctx->setConstants)
    {
        SL_LOG_WARN_ONCE("Unable to obtain callback 'setConstants', feature might not have any constants, plugin could be missing, failed to initialize or not initialized yet.");
        return false;
    }
    return ctx->setConstants(consts, frameIndex, id);
}

bool slGetFeatureSettings(Feature feature, const void* consts, void* settings)
{
    SL_VALIDATE_STATE

    auto ctx = plugin_manager::getInterface()->getFeatureContext(feature);
    if (!ctx)
    {
        return false;
    }

    if (!ctx->getSettings)
    {
        // It is OK if there is no callback, some features don't have any settings
        SL_LOG_WARN_ONCE("Unable to obtain callback 'getSettings', feature does not have any settings, plugin could be missing, failed to initialize or not initialized yet.");
        return false;
    }
    return ctx->getSettings(consts, settings);
}

bool slAllocateResources(sl::CommandBuffer* cmdBuffer, sl::Feature feature, uint32_t id)
{
    SL_VALIDATE_STATE

    auto ctx = plugin_manager::getInterface()->getFeatureContext(feature);
    if (!ctx)
    {
        return false;
    }

    if (!ctx->allocResources)
    {
        SL_LOG_WARN_ONCE("Unable to obtain callback 'allocateResource', plugin does not support explicit resource allocation.");
        return false;
    }
    return ctx->allocResources(cmdBuffer, feature, id);
}

bool slFreeResources(sl::Feature feature, uint32_t id)
{
    SL_VALIDATE_STATE

    auto ctx = plugin_manager::getInterface()->getFeatureContext(feature);
    if (!ctx)
    {
        return false;
    }

    if (!ctx->freeResources)
    {
        SL_LOG_WARN_ONCE("Unable to obtain callback 'freeResources', plugin does not support explicit resource deallocation.");
        return false;
    }
    return ctx->freeResources(feature, id);
}

bool slEvaluateFeature(CommandBuffer* cmdBuffer, Feature feature, uint32_t frameIndex, uint32_t id)
{
    SL_VALIDATE_STATE

    auto ctx = plugin_manager::getInterface()->getFeatureContext(feature);
    if (!ctx)
    {
        return false;
    }

    PFunSlEvaluateFeature* fun = {};
    // Evaluate is always handled by the sl.common plugin first then distributed to the appropriate plugin
    if (!getPointerParam(param::getInterface(), param::common::kPFunEvaluateFeature, &fun))
    {
        SL_LOG_ERROR_ONCE("Unable to obtain callback '%s', please make sure sl.common.dll is present and loaded correctly", param::common::kPFunEvaluateFeature);
        return false;
    }
    // Callback will return false and log an error if feature is not supported, missing or something else goes wrong
    return fun(cmdBuffer, feature, frameIndex, id);
}


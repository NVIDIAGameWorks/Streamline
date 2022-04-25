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

bool slInit(const Preferences &pref, int applicationId)
{
    if (!applicationId)
    {
        // Use a generic Id for now
        applicationId = 100721531;
    }

    // Setup logging first
    auto log = log::getInterface();
    log->enableConsole(pref.showConsole);
    log->setLogLevel(pref.logLevel);
    log->setLogPath(pref.pathToLogsAndData);
    log->setLogName(L"sl.log");
    log->setLogCallback((void*)pref.logMessageCallback);
    
    if (sl::interposer::getInterface()->isEnabled())
    {
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
    if (!interposer::getInterface()->isEnabled())
    {
        return false;
    }

    if (!plugin_manager::getInterface()->arePluginsLoaded())
    {   
        SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");
        return false;
    }

    auto params = plugin_manager::getInterface()->getFeatureParameters(feature);
    if (!params)
    {
        SL_LOG_ERROR_ONCE("Feature %u is not supported, missing or disabled", feature);
        return false;
    }

    uint32_t supportedAdapters = 0;
    param::getInterface()->get(params->supportedAdapters.c_str(), &supportedAdapters);
    if (adapterBitMask)
    {
        *adapterBitMask = supportedAdapters;
    }
    return supportedAdapters != 0;
}

bool slSetFeatureEnabled(sl::Feature feature, bool enabled)
{
    if (!interposer::getInterface()->isEnabled())
    {
        return false;
    }

    if (!plugin_manager::getInterface()->arePluginsLoaded())
    {
        SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");
        return false;
    }

    return plugin_manager::getInterface()->setFeatureEnabled(feature, enabled);
}

bool slSetTag(const Resource *resource, BufferType tag, uint32_t id, const Extent* extent)
{
    if (!interposer::getInterface()->isEnabled())
    {
        return false;
    }

    if (!plugin_manager::getInterface()->arePluginsLoaded())
    {
        SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");
        return false;
    }

    using PFunTagResource = bool(const Resource *, BufferType, uint32_t, const Extent*);
    PFunTagResource* fun;
    if (!getPointerParam(param::getInterface(), param::global::kPFunSetTag, &fun))
    {
        SL_LOG_ERROR_ONCE("Unable to obtain 'setTag' callback, sl.common plugin missing or not initialized");
        return false;
    }
    return fun(resource, tag, id, extent);
}

bool slSetConstants(const Constants &values, uint32_t frameIndex, uint32_t id)
{   
    if (!interposer::getInterface()->isEnabled())
    {
        return false;
    }

    if (!plugin_manager::getInterface()->arePluginsLoaded())    
    {
        SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");
        return false;
    }

    using PFunSetConstants = bool(const Constants &, uint32_t, uint32_t);
    PFunSetConstants*fun;
    if (!getPointerParam(param::getInterface(), param::global::kPFunSetConsts, &fun))
    {
        SL_LOG_WARN_ONCE("Unable to obtain set common constants callback, sl.common plugin missing or not initialized");
        return false;
    }
    return fun(values, frameIndex, id);
}

bool slSetFeatureConstants(Feature feature, const void *consts, uint32_t frameIndex, uint32_t id)
{
    if (!interposer::getInterface()->isEnabled())
    {
        return false;
    }

    if (!plugin_manager::getInterface()->arePluginsLoaded())
    {
        SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");
        return false;
    }

    using PFunSetConstants = bool(const void*, uint32_t, uint32_t);
    PFunSetConstants* fun{};

    auto params = plugin_manager::getInterface()->getFeatureParameters(feature);
    if (!params)
    {
        SL_LOG_ERROR_ONCE("Feature %u is not supported, missing or disabled", feature);
        return false;
    }

    if (!getPointerParam(param::getInterface(), params->setConstants.c_str(), &fun))
    {
        SL_LOG_WARN_ONCE("Unable to obtain callback '%s', feature might not have any constants", params->setConstants.c_str());
        return false;
    }
    return fun(consts, frameIndex, id);
}

bool slGetFeatureSettings(Feature feature, const void* consts, void* settings)
{
    if (!interposer::getInterface()->isEnabled())
    {
        return false;
    }

    if (!plugin_manager::getInterface()->arePluginsLoaded())
    {
        SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");
        return false;
    }

    using PFuncGetSettings = bool(const void*,void*);
    PFuncGetSettings* fun{};

    auto params = plugin_manager::getInterface()->getFeatureParameters(feature);
    if (!params)
    {
        SL_LOG_ERROR_ONCE("Feature %u is not supported, missing or disabled", feature);
        return false;
    }

    if (!getPointerParam(param::getInterface(), params->getSettings.c_str(), &fun))
    {
        // It is OK if there is no callback, some features don't have any settings
        SL_LOG_WARN_ONCE("Unable to obtain callback '%s', feature does not have any settings", params->getSettings.c_str());
        return false;
    }
    return fun(consts, settings);
}

bool slEvaluateFeature(CommandBuffer* cmdBuffer, Feature feature, uint32_t frameIndex, uint32_t id)
{
    if (!interposer::getInterface()->isEnabled())
    {
        return false;
    }

    if (!plugin_manager::getInterface()->arePluginsLoaded())
    {
        SL_LOG_ERROR_ONCE("SL not initialized or no plugins found - please make sure to include all required plugins including sl.common");
        return false;
    }

    using PFuncEvaluateFeature = bool(CommandBuffer*, Feature, uint32_t, uint32_t);
    PFuncEvaluateFeature* fun = {};

    auto params = plugin_manager::getInterface()->getFeatureParameters(feature);
    if (!params)
    {
        SL_LOG_ERROR_ONCE("Feature %u is not supported, missing or disabled", feature);
        return false;
    }

    // Evaluate is always handled by the sl.common plugin first then distributed to target the plugin
    if (!getPointerParam(param::getInterface(), param::common::kPFunEvaluateFeature, &fun))
    {
        SL_LOG_ERROR_ONCE("Unable to obtain callback '%s', please make sure sl.common.dll is present and loaded correctly", param::common::kPFunEvaluateFeature);
        return false;
    }
    // Callback will return false and log an error if feature is not supported, missing or something else goes wrong
    return fun(cmdBuffer, feature, frameIndex, id);
}

//! IMPORTANT: LEGACY API TO BE REMOVED SOON (AND NOT TO BE INCLUDED IN THE PUBLIC SDK)
//! 
namespace sl
{

SL_API bool init(const Preferences& pref, int applicationId)
{
    SL_LOG_WARN_ONCE("Deprecated API sl::FUNCTION, please switch to using slFUNCTION API");
    return slInit(pref, applicationId);
}
SL_API bool shutdown()
{
    SL_LOG_WARN_ONCE("Deprecated API sl::FUNCTION, please switch to using slFUNCTION API");
    return slShutdown();
}
SL_API bool isFeatureSupported(Feature feature, uint32_t* adapterBitMask)
{
    SL_LOG_WARN_ONCE("Deprecated API sl::FUNCTION, please switch to using slFUNCTION API");
    return slIsFeatureSupported(feature, adapterBitMask);
}
SL_API bool setTag(const Resource* resource, BufferType tag, uint32_t id, const Extent* extent)
{
    SL_LOG_WARN_ONCE("Deprecated API sl::FUNCTION, please switch to using slFUNCTION API");
    return slSetTag(resource, tag, id, extent);
}
SL_API bool setConstants(const Constants& values, uint32_t frameIndex, uint32_t id)
{
    SL_LOG_WARN_ONCE("Deprecated API sl::FUNCTION, please switch to using slFUNCTION API");
    return slSetConstants(values, frameIndex, id);
}
SL_API bool setFeatureConstants(Feature feature, const void* consts, uint32_t frameIndex, uint32_t id)
{
    SL_LOG_WARN_ONCE("Deprecated API sl::FUNCTION, please switch to using slFUNCTION API");
    return slSetFeatureConstants(feature, consts, frameIndex, id);
}
SL_API bool getFeatureSettings(Feature feature, const void* consts, void* settings)
{
    SL_LOG_WARN_ONCE("Deprecated API sl::FUNCTION, please switch to using slFUNCTION API");
    return slGetFeatureSettings(feature, consts, settings);
}
SL_API bool evaluateFeature(CommandBuffer* cmdBuffer, Feature feature, uint32_t frameIndex, uint32_t id)
{
    SL_LOG_WARN_ONCE("Deprecated API sl::FUNCTION, please switch to using slFUNCTION API");
    return slEvaluateFeature(cmdBuffer, feature, frameIndex, id);
}

} // namespace sl
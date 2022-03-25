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

namespace sl
{

//! API

bool init(const Preferences &pref, int applicationId)
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
            SL_LOG_ERROR("sl::init must be called before any DXGI/D3D12/D3D11/Vulkan APIs are invoked");
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

bool shutdown()
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

bool isFeatureSupported(Feature feature, uint32_t* adapterBitMask)
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
    if (feature >= eFeatureCount)
    {
        SL_LOG_ERROR_ONCE("Feature %u is out of range", feature);
        return false;
    }

    static const char *params[] = {
        param::dlss::kSupportedAdapters,
        param::nrd::kSupportedAdapters
    };
    static_assert(countof(params) == eFeatureCount);
    uint32_t supportedAdapters = 0;
    param::getInterface()->get(params[feature], &supportedAdapters);
    if (adapterBitMask)
    {
        *adapterBitMask = supportedAdapters;
    }
    return supportedAdapters != 0;
}

bool setTag(const Resource *resource, BufferType tag, uint32_t id, const Extent* extent)
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

    if (tag >= eBufferTypeCount)
    {
        SL_LOG_ERROR("Trying to set invalid tag %u", tag);
        return false;
    }

    using PFunTagResource = bool(const Resource *, BufferType, uint32_t, const Extent*);
    PFunTagResource* fun;
    if (!getPointerParam(param::getInterface(), param::global::kPFunSetTag, &fun))
    {
        SL_LOG_WARN("Unable to obtain set common constants callback, sl.common plugin missing or not initialized");
        return false;
    }
    return fun(resource, tag, id, extent);
}

bool setConstants(const Constants &values, uint32_t frameIndex, uint32_t id)
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
        SL_LOG_WARN("Unable to obtain set common constants callback, sl.common plugin missing or not initialized");
        return false;
    }
    return fun(values, frameIndex, id);
}

bool setFeatureConstants(Feature feature, const void *consts, uint32_t frameIndex, uint32_t id)
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

    if (feature >= eFeatureCount)
    {
        SL_LOG_ERROR_ONCE("Feature %u is out of range", feature);
        return false;
    }

    static const char *params[] = {
        param::dlss::kSetConstsFunc,
        param::nrd::kSetConstsFunc
    };
    static_assert(countof(params) == eFeatureCount);

    using PFunSetConstants = bool(const void*, uint32_t, uint32_t);
    PFunSetConstants *fun;
    if (!getPointerParam(param::getInterface(), params[feature], &fun))
    {
        SL_LOG_WARN("Unable to obtain callback '%s'", params[feature]);
        return false;
    }
    return fun(consts, frameIndex, id);
}

bool getFeatureSettings(Feature feature, const void* consts, void* settings)
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

    if (feature >= eFeatureCount)
    {
        SL_LOG_ERROR_ONCE("Feature %u is out of range", feature);
        return false;
    }

    static const char *params[] = {
        param::dlss::kGetSettingsFunc,
        param::nrd::kGetSettingsFunc
    };
    static_assert(countof(params) == eFeatureCount);

    using funcGetSettings = bool(const void*,void*);
    funcGetSettings *fun;
    if (!getPointerParam(param::getInterface(), params[feature], &fun))
    {
        // It is ok if there is no callback, some features don't have any settings
        SL_LOG_WARN("Unable to obtain callback '%s', feature does not have any settings", params[feature]);
        return false;
    }
    return fun(consts, settings);
}

bool evaluateFeature(CommandBuffer* cmdBuffer, Feature feature, uint32_t frameIndex, uint32_t id)
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

    if (feature >= eFeatureCount)
    {
        SL_LOG_ERROR_ONCE("Feature %u is out of range", feature);
        return false;
    }

    using funcEvaluateFeature = bool(CommandBuffer*, Feature, uint32_t, uint32_t);
    funcEvaluateFeature* fun = {};
    if (!getPointerParam(param::getInterface(), param::common::kPFunEvaluateFeature, &fun))
    {
        SL_LOG_ERROR("Unable to obtain callback '%s', please make sure sl.common.dll is present and loaded correctly", param::common::kPFunEvaluateFeature);
        return false;
    }
    return fun(cmdBuffer, feature, frameIndex, id);
}

} // namespace sl
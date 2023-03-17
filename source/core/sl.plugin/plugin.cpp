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

#include <sstream>

#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.api/internal.h"
#include "include/sl.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

#if SL_ENABLE_OTA
#include "source/core/sl.ota/ota.h"
#endif

namespace sl
{

#ifndef SL_COMMON_PLUGIN
#if SL_ENABLE_OTA
namespace ota
{
IOTA * s_ota = {};
IOTA * getInterface()
{
    return s_ota;
}
}
 #endif

namespace extra
{
namespace keyboard
{
IKeyboard* s_keyboard = {};
IKeyboard* getInterface()
{
    return s_keyboard;
}
}
}
#endif

namespace log
{
ILog* s_log = {};
ILog* getInterface()
{
    return s_log;
}
}

namespace plugin
{

void onLoad(api::Context* ctx, const char* loaderJSON, const char* embeddedJSON)
{
    // Setup logging and callbacks so we can report any issues correctly
    param::getPointerParam(api::getContext()->parameters, param::global::kLogInterface, &log::s_log);
#ifndef SL_COMMON_PLUGIN
    param::getPointerParam(api::getContext()->parameters, param::common::kKeyboardAPI, &extra::keyboard::s_keyboard);
#if SL_ENABLE_OTA
    param::getPointerParam(api::getContext()->parameters, param::global::kOTAInterface, &ota::s_ota);
#endif
#endif

    // Now let's populate our JSON config with our version and API
    json& loader = *(json*)ctx->loaderConfig;
    json& config = *(json*)ctx->pluginConfig;
    try
    {
        {
            std::istringstream stream(loaderJSON);
            stream >> loader;
        }

        {
            std::istringstream stream(embeddedJSON);
            stream >> config;
        }

        auto pluginVersion = api::getContext()->pluginVersion;
        auto apiVersion = api::getContext()->apiVersion;

        config["version"]["major"] = pluginVersion.major;
        config["version"]["minor"] = pluginVersion.minor;
        config["version"]["build"] = pluginVersion.build;
        config["api"]["major"] = apiVersion.major;
        config["api"]["minor"] = apiVersion.minor;
        config["api"]["build"] = apiVersion.build;

#ifndef SL_PRODUCTION
        // Search for "sl.$(plugin_name).json" with extra settings
        {
            const wchar_t *pluginPath = {};
            param::getPointerParam(ctx->parameters, param::global::kPluginPath, &pluginPath);
            std::wstring extraJSONFile = (pluginPath + std::wstring(L"/") + extra::toWStr(ctx->pluginName) + L".json").c_str();
            if (file::exists(extraJSONFile.c_str()))
            {
                SL_LOG_INFO("Found extra JSON config %S", extraJSONFile.c_str());
                auto jsonText = file::read(extraJSONFile.c_str());
                if (!jsonText.empty())
                {
                    // safety null in case the JSON string is not null-terminated (found by AppVerif)
                    jsonText.push_back(0);
                    std::istringstream stream((const char*)jsonText.data());
                    json& extraConfig = *(json*)ctx->extConfig;
                    stream >> extraConfig;
                }
            }
        }
#endif
    }
    catch (std::exception &e)
    {
        SL_LOG_ERROR( "JSON exception %s", e.what());
    };
}

StartupResult onStartup(api::Context *ctx, const char* jsonConfig)
{
    try
    {
        // Get information provided by host (plugin manager or installed plugin)
        json& config = *(json*)ctx->loaderConfig;

        std::istringstream stream(jsonConfig);
        stream >> config;

#if SL_ENABLE_OTA
        bool ota = false;
        if (config.contains("ota"))
        {
            config.at("ota").get_to(ota);
        }

        // Check if we are an OTA module or not
        if (!ota)
        {
            // We are the original plugin loaded by the plugin manager so check if there is an OTA for our module
            if (!ota::getInterface())
            {
                SL_LOG_ERROR("Missing required interface - make sure sl.common.dll is present and loaded");
                return eStartupResultFail;
            }
            ctx->getPluginFunction = ota::getInterface()->getOTAPluginEntryPointIfNewerAndSupported((ctx->pluginName).c_str(), ctx->pluginVersion, ctx->apiVersion);
            if (ctx->getPluginFunction)
            {
                // Redirecting via OTA
                api::PFuncOnPluginStartup* otaOnPluginStartup = (api::PFuncOnPluginStartup*)api::getContext()->getPluginFunction("slOnPluginStartup");
                if (otaOnPluginStartup)
                {
                    // Inform other plugin that it is an OTA module so we don't recurse indefinitely
                    config["ota"] = true;
                    std::string otaConfig = config.dump();
                    if (otaOnPluginStartup(otaConfig.c_str(), ctx->device))
                    {
                        return eStartupResultOTA;
                    }
                    SL_LOG_WARN("OTA failed, check the logs");
                    // OTA failed to start for whatever reason, we continue 
                    ctx->getPluginFunction = nullptr;
                }
            }
        }
#endif
    }
    catch (std::exception &e)
    {
        SL_LOG_ERROR( "JSON exception %s", e.what());
        return eStartupResultFail;
    };
    
    return eStartupResultOK;
}

void onShutdown(api::Context *ctx)
{
    SL_LOG_INFO("Shutting down plugin %s", ctx->pluginName.c_str());
    delete ctx->pluginConfig;
    delete ctx->loaderConfig;
    delete ctx->extConfig;
    ctx->pluginConfig = {};
    ctx->loaderConfig = {};
    ctx->extConfig = {};
#if SL_ENABLE_OTA
    if (ctx->getPluginFunction)
    {
        api::PFuncOnPluginShutdown* onShutdown = (api::PFuncOnPluginShutdown*)ctx->getPluginFunction("slOnPluginShutdown");
        if (onShutdown)
        {
            onShutdown();
        }
        else
        {
            SL_LOG_ERROR("OTA plugin missing shutdown API");
        }
    }
#endif
}

}
}
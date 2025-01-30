/*
* Copyright (c) 2022-2023 NVIDIA CORPORATION. All rights reserved
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
#include <unordered_set>

using json = nlohmann::json;

namespace sl
{

#ifndef SL_COMMON_PLUGIN

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
extern bool g_slEnableLogPreMetaDataUniqueWAR = false;

ILog* s_log = {};
ILog* getInterface()
{
    return s_log;
}
}

#define ENABLE_DISALLOW_NEWER_PLUGINS_WAR 1

namespace plugin
{
static bool isLoadingAllowed(const json& loader)
{
#if ENABLE_DISALLOW_NEWER_PLUGINS_WAR
    // This WAR is used to disallow SL ota cached plugins for some game titles like COD black ops 6.
    // The reason why SL ota cached plugins don't work on such game titles:
    //    Plugins are loaded/unloaded a couple of times from ota cache and the game package until finding the correct plugins.
    //    To unload a plugin dll, plugin manager calls FreeLibrary, but without explicitly calling a release or a reset function.
    //    The problem is that FreeLibrary doesn't work on some game titles, maybe by a anti-cheat module. 
    //    If FreeLibrary doesn't work, plugins are not unloaded properly, so some variables are not released or reset, 
    //    especially sl::PLUGIN_NAMESPACE::s_init is not reset. 
    //    For the repro game titles, FreeLibrary doesn't work, so that the final loading of sl.common.dll skips plugin::onLoad due to s_init is still true,
    //    which results in incomplete initialization.
    //    See http://nvbugs/5011092 for details.
    // This WAR is to remove the chance that we need to call FreeLibrary by disallowing newer plugins.
    // The issue would be prevented by update on plugin manager. After plugin manager is updated and the newer interposer is deployed in the game package, we could remove this WAR.

    constexpr uint32_t APP_ID_CALL_OF_DUTY_BLACK_OPS_6              = 0x0623e7c8;
    constexpr uint32_t APP_ID_F1_24                                 = 0x0616fc0b;
    constexpr uint32_t APP_ID_CALL_OF_DUTY_MODERN_WARFARE_III_2023  = 0x061198bf;

    static std::unordered_set<uint32_t> disallow_newer_plugins_apps = {
        APP_ID_CALL_OF_DUTY_BLACK_OPS_6,
        APP_ID_F1_24,
        APP_ID_CALL_OF_DUTY_MODERN_WARFARE_III_2023
    };

    uint32_t appId = loader.at("appId");
    if (disallow_newer_plugins_apps.find(appId) != disallow_newer_plugins_apps.end())
    {
        // If host sdk's version doesn't match the plugin's version, don't allow loading.
        uint32_t host_ver_major = loader["host"]["version"]["major"];
        uint32_t host_ver_minor = loader["host"]["version"]["minor"];
        uint32_t host_ver_build = loader["host"]["version"]["build"];

        uint32_t plugin_ver_major = api::getContext()->pluginVersion.major;
        uint32_t plugin_ver_minor = api::getContext()->pluginVersion.minor;
        uint32_t plugin_ver_build = api::getContext()->pluginVersion.build;
        if (Version(host_ver_major, host_ver_minor, host_ver_build) < Version(plugin_ver_major, plugin_ver_minor, plugin_ver_build))
        {
            SL_LOG_WARN("appId=0x%x doesn't allow to load a newer plugin: plugin=%s version=%d.%d.%d, host sdk version=%d.%d.%d", 
                appId, api::getContext()->pluginName.c_str(), plugin_ver_major, plugin_ver_minor, plugin_ver_build, 
                host_ver_major, host_ver_minor, host_ver_build);
            return false;
        }
    }
#endif
    return true;
}

bool onLoad(api::Context* ctx, const char* loaderJSON, const char* embeddedJSON)
{
    // Setup logging and callbacks so we can report any issues correctly
    param::getPointerParam(api::getContext()->parameters, param::global::kLogInterface, &log::s_log);
#ifndef SL_COMMON_PLUGIN
    param::getPointerParam(api::getContext()->parameters, param::common::kKeyboardAPI, &extra::keyboard::s_keyboard);
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

        if (!isLoadingAllowed(loader))
        {
            return false;
        }

        config = json::parse(embeddedJSON, nullptr, /* allow exceptions: */ true, /* ignore comments: */ true);

        auto pluginVersion = api::getContext()->pluginVersion;
        auto apiVersion = api::getContext()->apiVersion;

        config["version"]["major"] = pluginVersion.major;
        config["version"]["minor"] = pluginVersion.minor;
        config["version"]["build"] = pluginVersion.build;
        config["api"]["major"] = apiVersion.major;
        config["api"]["minor"] = apiVersion.minor;
        config["api"]["build"] = apiVersion.build;

        /* If being loaded by an sl.interposer that is before version 2.3.0,
         * then we need to enable the ABI compatibility WAR. */
        if (Version(loader["version"]["major"],
                    loader["version"]["minor"],
                    loader["version"]["build"]) < Version(2, 3, 0))
        {
            sl::log::g_slEnableLogPreMetaDataUniqueWAR = true;
            SL_LOG_INFO("Enabling WAR for LogPreMetaDataUnique ABI Breakage");
        }

#ifndef SL_PRODUCTION
        // Search for "sl.$(plugin_name).json" with extra settings
        {
            const wchar_t *pluginPath = {};
            param::getPointerParam(ctx->parameters, param::global::kPluginPath, &pluginPath);
            std::wstring extraJSONFile = (pluginPath + std::wstring(L"/") + extra::toWStr(ctx->pluginName) + L".json").c_str();
            if (file::exists(extraJSONFile.c_str()))
            {
                SL_LOG_INFO("Found extra JSON config %S", extraJSONFile.c_str());
                std::vector<uint8_t> jsonText = file::read(extraJSONFile.c_str());
                if (!jsonText.empty())
                {
                    json& extraConfig = *(json*)ctx->extConfig;
                    extraConfig = json::parse(jsonText.begin(), jsonText.end(), nullptr, /* allow exceptions: */ true, /* ignore comments: */ true);
                }
            }
        }
#endif
    }
    catch (std::exception &e)
    {
        SL_LOG_ERROR( "JSON exception %s", e.what());
    };
    return true;
}

StartupResult onStartup(api::Context *ctx, const char* jsonConfig)
{
    try
    {
        // Get information provided by host (plugin manager or installed plugin)
        json& config = *(json*)ctx->loaderConfig;

        std::istringstream stream(jsonConfig);
        stream >> config;

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
}

}
}

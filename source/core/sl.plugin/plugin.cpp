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

#include "source/core/sl.ota/iota.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.api/internal.h"
#include "include/sl.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.param/parameters.h"
#include "source/shared/sharedVersions.h"
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
extern bool g_slLogABIWARChecked = false;

ILog* s_log = {};
ILog* getInterface()
{
    return s_log;
}
}

#if defined(SL_PRODUCTION)
#define ENABLE_DISALLOW_NEWER_PLUGINS_WAR 1
#define ENABLE_SL_OTA_DENYLIST 1
#elif defined(SL_DUMMY_PLUGIN)
// Enable denylist checking in dummy-plugin for unit testing
// Note: SL_UNITTEST_ONLY_CODE is defined in premake.lua for dummy-plugin
#define ENABLE_DISALLOW_NEWER_PLUGINS_WAR 0
#define ENABLE_SL_OTA_DENYLIST 1
#else
#define ENABLE_DISALLOW_NEWER_PLUGINS_WAR 0
#define ENABLE_SL_OTA_DENYLIST 0
#endif

namespace plugin
{

static bool isAppDenylisted(api::Context* ctx)
{
    json& loader = *(json*)ctx->loaderConfig;

    uint32_t appId = loader.at("appId");

    // Get the path of THIS plugin DLL using the existing file utility
    const std::filesystem::path pluginPath = file::getModulePath();

    SL_LOG_INFO("Plugin path: %s", pluginPath.u8string().c_str());

    std::filesystem::path otaCachePath;
    bool otaCacheFound = ota::getInterface()->getNGXPath(otaCachePath);

    // If OTA cache path is not found or empty, denylist doesn't apply
    if (!otaCacheFound || otaCachePath.empty())
    {
        SL_LOG_INFO("OTA cache path not found - denylist does not apply");
        return false;
    }

    auto relative = std::filesystem::relative(pluginPath, otaCachePath);
    if (relative.empty() ||
        (relative.u8string().length() > 1 && relative.u8string()[0] == '.' && relative.u8string()[1] == '.'))
    {
        SL_LOG_INFO("Plugin is not from OTA - denylist does not apply");
        return false;
    }

    // Get-or-compute: first OTA plugin to run this sets the result in shared parameters; later plugins reuse it.
    bool denied = false;
    if (ctx->parameters->get(sl::param::global::kOtaDenylistDenied, &denied)){
        SL_LOG_INFO("Plugin is already checked for OTA denylist - returning cached result: %d", denied);
        return denied;
    }

    // If NGX config is present with engine info and projectId, use those to resolve appId
    EngineType engineType{};
    std::string projectId{};
    std::string engineVersion{};
    if (loader.contains("ngx"))
    {
        loader.at("ngx").at("engineType").get_to(engineType);
        loader.at("ngx").at("projectId").get_to(projectId);
        loader.at("ngx").at("engineVersion").get_to(engineVersion);
    }

    // Resolve app ID from project ID and engine type if defined
    // Mirrors NGX app ID resolution in source/plugins/sl.common/commonEntry.cpp (see updateNGXFeature)
    if (!projectId.empty() && !engineVersion.empty())
    {
        if (ota::getInterface()->readServerMappings())
        {
            if (auto mappedAppId = ota::getInterface()->appIdForProjectId(engineType, engineVersion, projectId))
            {
                appId = *mappedAppId;
                SL_LOG_INFO("Resolved project ID %s (engine v%s) to app ID 0x%x", projectId.c_str(), engineVersion.c_str(), appId);
            }
            else
            {
                SL_LOG_VERBOSE("No mapping found for project ID %s (engine v%s)", projectId.c_str(), engineVersion.c_str());
            }
        }
        else
        {
            SL_LOG_WARN("Failed to read server mappings");
        }
    }

    SL_LOG_INFO("Checking SL OTA denylist for appid=0x%x", appId);

    ota::getInterface()->readServerDenylist();

    denied = ota::getInterface()->isAppIdDenied(appId);
    ctx->parameters->set(sl::param::global::kOtaDenylistDenied, denied);
    return denied;
}

static bool isLoadingAllowed(api::Context* ctx)
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
    constexpr uint32_t APP_ID_BF_6_OPEN_BETA                        = 0x06323d82;

    static std::unordered_set<uint32_t> disallow_newer_plugins_apps = {
        APP_ID_CALL_OF_DUTY_BLACK_OPS_6,
        APP_ID_F1_24,
        APP_ID_CALL_OF_DUTY_MODERN_WARFARE_III_2023,
        APP_ID_BF_6_OPEN_BETA
    };

    json& loader = *(json*)ctx->loaderConfig;
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
#if ENABLE_SL_OTA_DENYLIST
    if (isAppDenylisted(ctx))
    {
        json& loader = *(json*)ctx->loaderConfig;
        uint32_t deniedAppId = loader.at("appId");
        std::string projectId{};
        if (loader.contains("ngx") && loader.at("ngx").contains("projectId"))
        {
            loader.at("ngx").at("projectId").get_to(projectId);
        }
        SL_LOG_WARN("appId=0x%x projectId='%s' doesn't allow loading from OTA: plugin=%s", deniedAppId, projectId.c_str(), api::getContext()->pluginName.c_str());
        return false;
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

        // If being loaded by an sl.interposer that is before version 2.3.0,
        // then we need to enable the ABI compatibility WAR. This must be done
        // before any logging to avoid an ABI mismatch crash with old
        // interposers (bug 4654661).
        if (Version(loader["version"]["major"],
                    loader["version"]["minor"],
                    loader["version"]["build"]) < Version(2, 3, 0))
        {
            sl::log::g_slEnableLogPreMetaDataUniqueWAR = true;
        }
        sl::log::g_slLogABIWARChecked = true;

        if (sl::log::g_slEnableLogPreMetaDataUniqueWAR)
        {
            SL_LOG_INFO("Enabling WAR for LogPreMetaDataUnique ABI Breakage");
        }

#if defined(SL_UNITTEST_ONLY_CODE)
        // If the loader provides an OTA cache path override, set it on this plugin's OTA singleton
        // This ensures the plugin uses the same OTA path as the plugin manager
        // NOTE: Only used for unit tests - this field is not set in production builds
        if (loader.contains("ngx") && loader["ngx"].contains("otaCachePath"))
        {
            std::string otaPath = loader["ngx"]["otaCachePath"];
            if (!otaPath.empty())
            {
                ota::getInterface()->setNGXPathOverride(std::filesystem::path(otaPath));
            }
        }
#endif

        if (!isLoadingAllowed(ctx))
        {
            return false;
        }

        config = json::parse(embeddedJSON, nullptr, /* allow exceptions: */ true, /* ignore comments: */ true);

        auto pluginVersion = api::getContext()->pluginVersion;
        auto apiVersion = api::getContext()->apiVersion;

        config["version"]["major"] = pluginVersion.major;
        config["version"]["minor"] = pluginVersion.minor;
        config["version"]["build"] = pluginVersion.build;
        config["build_config"] = BUILD_CONFIG_INFO;
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

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
ILog* s_log = {};
ILog* getInterface()
{
    return s_log;
}
}

namespace plugin
{

void onGetConfig(api::Context *ctx, const char *pluginJSON)
{
    // Setup logging and callbacks so we can report any issues correctly
    param::getPointerParam(api::getContext()->parameters, param::global::kLogInterface, &log::s_log);
#ifndef SL_COMMON_PLUGIN
    param::getPointerParam(api::getContext()->parameters, param::common::kKeyboardAPI, &extra::keyboard::s_keyboard);
#endif

    // Now let's populate our JSON config with our version and API
    json& config = *(json*)ctx->pluginConfig;
    try
    {
        std::istringstream stream(pluginJSON);
        stream >> config;

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
                    std::istringstream stream((const char*)jsonText.data());
                    json& extraConfig = *(json*)ctx->extConfig;
                    stream >> extraConfig;
                    if (extraConfig.contains("showConsole"))
                    {
                        bool showConsole = false;
                        extraConfig.at("showConsole").get_to(showConsole);
                        log::getInterface()->enableConsole(showConsole);
                    }
                    if (extraConfig.contains("logPath"))
                    {
                        std::string path;
                        extraConfig.at("logPath").get_to(path);
                        log::getInterface()->setLogPath(extra::toWStr(path).c_str());
                    }
                    if (extraConfig.contains("logLevel"))
                    {
                        uint32_t level;
                        extraConfig.at("logLevel").get_to(level);
                        level = std::clamp(level, 0U, 2U);
                        log::getInterface()->setLogLevel((LogLevel)level);
                    }
                    if (extraConfig.contains("swapChainBufferCount"))
                    {
                        uint32_t bufferCount = 2;
                        extraConfig.at("swapChainBufferCount").get_to(bufferCount);
                        // Make sure parameter is withing valid range
                        bufferCount = std::clamp(bufferCount, 2U, 3U);
                        ctx->parameters->set(param::global::kSwapchainBufferCount, bufferCount);
                    }
                }
            }
        }
#endif
    }
    catch (std::exception &e)
    {
        SL_LOG_ERROR("JSON exception %s", e.what());
    };
}

StartupResult onStartup(api::Context *ctx, const char *loaderJSON)
{
    try
    {
        // Get information provided by host (plugin manager or installed plugin)
        json& config = *(json*)ctx->loaderConfig;
        std::istringstream stream(loaderJSON);
        stream >> config;
    }
    catch (std::exception &e)
    {
        SL_LOG_ERROR("JSON exception %s", e.what());
        return eStartupResultFail;
    };
    
    return eStartupResultOK;
}

void onShutdown(api::Context *ctx)
{
    delete ctx->pluginConfig;
    delete ctx->loaderConfig;
    delete ctx->extConfig;
    ctx->pluginConfig = {};
    ctx->loaderConfig = {};
    ctx->extConfig = {};
}

}
}
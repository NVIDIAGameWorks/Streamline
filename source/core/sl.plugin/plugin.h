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

#pragma once

#include "source/core/sl.api/internal.h"

namespace sl
{
namespace api
{
struct Context
{
    std::string pluginName;
    Version pluginVersion;
    Version apiVersion;
    void *device;
    sl::param::IParameters *parameters;
    PFuncGetPluginFunction *getPluginFunction = {};
    void* pluginConfig;
    void* loaderConfig;
    void* extConfig;
};

Context *getContext();

#define SL_EXPORT extern "C" __declspec(dllexport)

#define SL_PLUGIN_COMMON_STARTUP()                                         \
using namespace plugin;                                                    \
api::getContext()->parameters = parameters;                                \
api::getContext()->device = device;                                        \
StartupResult res = plugin::onStartup(api::getContext(), jsonConfig);      \
if (res == eStartupResultFail)                                             \
{                                                                          \
    return false;                                                          \
}                                                                          \
else if (res == eStartupResultOTA)                                         \
{                                                                          \
    return true;                                                           \
}

// Core definitions, each plugin must use this define and specify versions
#define SL_PLUGIN_DEFINE(N,V1,V2,JSON,GET_SUPPORTED_ADAPTER_MASK)                            \
namespace api                                                                                \
{                                                                                            \
    static Context s_ctx = {N, V1, V2, nullptr, nullptr, nullptr,                            \
                            new json(), new json(), new json()};                             \
    Context *getContext() { return &s_ctx; }                                                 \
}                                                                                            \
                                                                                             \
void slSetParameters(sl::param::IParameters *p) {api::s_ctx.parameters = p;}                 \
                                                                                             \
const char *slGetPluginJSONConfig()                                                          \
{                                                                                            \
    static std::string s_json;                                                               \
    static bool s_init = false;                                                              \
    if(!s_init)                                                                              \
    {                                                                                        \
        s_init = true;                                                                       \
        plugin::onGetConfig(api::getContext(), JSON);                                        \
        json& config = *(json*)api::getContext()->pluginConfig;                              \
        config["supportedAdapters"] = GET_SUPPORTED_ADAPTER_MASK;                            \
        s_json = config.dump();                                                              \
    }                                                                                        \
                                                                                             \
    return s_json.c_str();                                                                   \
}                                                                                            \
                                                                                             \
BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)                              \
{                                                                                            \
    return TRUE;                                                                             \
}                                                                                            
}
namespace param
{
struct IParameters;
}
namespace plugin
{

#define SL_EXPORT_FUNCTION(fun)\
if (!strcmp(functionName, #fun))\
{\
    return fun;\
}

#define SL_EXPORT_OTA                                          \
if (api::getContext()->getPluginFunction)                      \
{                                                              \
    return api::getContext()->getPluginFunction(functionName); \
}

enum StartupResult
{
    eStartupResultOK,
    eStartupResultFail,
    eStartupResultOTA,
};

//! Common plugin startup/shutdown code
void onGetConfig(api::Context *ctx, const char* pluginJSON);
StartupResult onStartup(api::Context *ctx, const char* loaderJSON);
void onShutdown(api::Context *ctx);

}
}
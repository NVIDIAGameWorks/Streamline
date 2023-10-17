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

#include "include/sl_version.h"
#include "source/core/sl.api/internal.h"

#define SL_EXPORT extern "C" __declspec(dllexport)
SL_EXPORT BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID);

namespace sl
{

namespace api
{

//! Plugin specific context, with default consturctor and destructor
//! 
//! NOTE: Instance of this context is valid for the entire life-cycle of a plugin since it cannot
//! be destroyed anywhere else other than in DLLMain when plugin is detached from the process.
//! 
#define SL_PLUGIN_CONTEXT_CREATE_DESTROY(NAME)                                  \
protected:                                                                      \
    NAME() {onCreateContext();};                                                \
    /* Called on exit from DLL */                                               \
    ~NAME() {onDestroyContext();};                                              \
    friend BOOL APIENTRY ::DllMain(HMODULE hModule, DWORD fdwReason, LPVOID);   \
public:                                                                         \
    NAME(const NAME& rhs) = delete;

//! Plugin specific context, same as above except constructor is not auto-defined
//! 
#define SL_PLUGIN_CONTEXT_DESTROY_ONLY(NAME)                                    \
protected:                                                                      \
    /* Called on exit from DLL */                                               \
    ~NAME() {onDestroyContext();};                                              \
    friend BOOL APIENTRY ::DllMain(HMODULE hModule, DWORD fdwReason, LPVOID);   \
public:                                                                         \
    NAME(const NAME& rhs) = delete;

//! Generic context, same across all plugins
//! 
//! Contains basic information like versions, name, JSON configurations etc.
//! 
class Context
{
    Context(
        const std::string& _pluginName,
        Version _pluginVersion,
        Version _apiVersion,
        void* _device,
        sl::param::IParameters* _parameters,
        PFuncGetPluginFunction* _getPluginFunction,
        void* _pluginConfig,
        void* _loaderConfig,
        void* _extConfig)
    {
        pluginName = _pluginName;
        pluginVersion = _pluginVersion;
        apiVersion = _apiVersion;
        device = _device;
        parameters = _parameters;
        getPluginFunction = _getPluginFunction;
        pluginConfig = _pluginConfig;
        loaderConfig = _loaderConfig;
        extConfig = _extConfig;
    };

    SL_PLUGIN_CONTEXT_CREATE_DESTROY(Context);

    void onCreateContext() {};

    void onDestroyContext()
    {
        delete pluginConfig;
        delete loaderConfig;
        delete extConfig;
        pluginConfig = {};
        loaderConfig = {};
        extConfig = {};
    }

    std::string pluginName{};
    std::string pluginConfigStr{};
    Version pluginVersion{};
    Version apiVersion{};
    void *device{};
    sl::param::IParameters *parameters{};
    PFuncGetPluginFunction *getPluginFunction{};
    void* pluginConfig{};
    void* loaderConfig{};
    void* extConfig{};
};

Context *getContext();

#define SL_PLUGIN_COMMON_STARTUP()                                         \
using namespace plugin;                                                    \
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

#define SL_PLUGIN_CONTEXT_DEFINE(PLUGIN_NAMESPACE, PLUGIN_CTX)                               \
namespace PLUGIN_NAMESPACE                                                                   \
{                                                                                            \
    /* Created on DLL attached and destroyed on DLL detach from process */                   \
    static PLUGIN_CTX* s_ctx{};                                                              \
    PLUGIN_CTX* getContext() { return s_ctx; }                                               \
}

//! Core definitions, each plugin must use this define and specify versions
//! 
//! NOTE: This macro must be placed within 'namespace sl'
//! 
#define SL_PLUGIN_DEFINE(N,V1,V2,JSON,UPDATE_JSON_CONFIG, PLUGIN_NAMESPACE, PLUGIN_CTX)                    \
namespace api                                                                                              \
{                                                                                                          \
    /* Created on DLL attached and destroyed on DLL detach from process */                                 \
    static Context* s_ctx{};                                                                               \
    Context *getContext() { return s_ctx; }                                                                \
}                                                                                                          \
                                                                                                           \
SL_PLUGIN_CONTEXT_DEFINE(PLUGIN_NAMESPACE, PLUGIN_CTX)                                                     \
                                                                                                           \
bool slOnPluginLoad(sl::param::IParameters *params, const char* loaderJSON, const char **pluginJSON)       \
{                                                                                                          \
    static bool s_init = false;                                                                            \
    if(!s_init)                                                                                            \
    {                                                                                                      \
        api::s_ctx->parameters = params;                                                                   \
        s_init = true;                                                                                     \
        plugin::onLoad(api::getContext(), loaderJSON, JSON);                                               \
        json& config = *(json*)api::getContext()->pluginConfig;                                            \
        UPDATE_JSON_CONFIG(config);                                                                        \
        api::s_ctx->pluginConfigStr = config.dump();                                                       \
    }                                                                                                      \
                                                                                                           \
    *pluginJSON = api::s_ctx->pluginConfigStr.c_str();                                                     \
    return true;                                                                                           \
}                                                                                                          \
                                                                                                           \
}  /* namespace sl */                                                                                      \
                                                                                                           \
/* Always in global namespace */                                                                           \
SL_EXPORT BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID)                                  \
{                                                                                                          \
    switch (fdwReason)                                                                                     \
    {                                                                                                      \
        case DLL_PROCESS_ATTACH:                                                                           \
            sl::api::s_ctx = new sl::api::Context(N, sl::V1, sl::V2, nullptr, nullptr, nullptr,            \
                                        new json, new json, new json);                                     \
            sl::PLUGIN_NAMESPACE::s_ctx = new sl::PLUGIN_NAMESPACE::PLUGIN_CTX();                          \
            break;                                                                                         \
        case DLL_THREAD_ATTACH:                                                                            \
            break;                                                                                         \
        case DLL_THREAD_DETACH:                                                                            \
            break;                                                                                         \
        case DLL_PROCESS_DETACH:                                                                           \
            delete sl::api::s_ctx;                                                                         \
            delete sl::PLUGIN_NAMESPACE::s_ctx;                                                            \
            sl::api::s_ctx = {};                                                                           \
            sl::PLUGIN_NAMESPACE::s_ctx = {};                                                              \
            break;                                                                                         \
    }                                                                                                      \
    return TRUE;                                                                                           \
}                                                                                                          \
namespace sl {

} // namespace api

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

 #define SL_EXPORT_OTA                                         \
if (api::getContext()->getPluginFunction)                      \
{                                                              \
    return api::getContext()->getPluginFunction(functionName); \
}

enum StartupResult
{
    eStartupResultOK,
    eStartupResultFail,
    eStartupResultOTA
};

//! Common plugin startup/shutdown code
void onLoad(api::Context *ctx, const char* loaderJSON, const char* embeddedJSON);
StartupResult onStartup(api::Context *ctx, const char* jsonConfig);
void onShutdown(api::Context *ctx);

}
}

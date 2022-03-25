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

#include <vector>

namespace sl
{

// Parameters used to communicate across plugin borders

namespace param
{
namespace global
{

constexpr const char* kColorBuffersHDR = "sl.param.global.colorBuffersHDR";
constexpr const char* kPFunSetConsts = "sl.param.global.setConstsFunc";
constexpr const char* kPFunGetConsts = "sl.param.global.getConstsFunc";
constexpr const char* kPFunAllocateResource = "sl.param.global.allocateResource";
constexpr const char* kPFunReleaseResource = "sl.param.global.releaseResource";
constexpr const char* kPluginPath = "sl.param.global.pluginPath";
constexpr const char* kLogInterface = "sl.param.global.logInterface";
constexpr const char* kPluginManagerInterface = "sl.param.global.pluginManagerInterface";
constexpr const char* kOTAInterface = "sl.param.global.otaInterface";
constexpr const char *kNeedNGX = "sl.param.global.needNGX";
constexpr const char *kNGXContext = "sl.param.global.ngxContext";
constexpr const char* kSwapchainBufferCount = "sl.param.global.swapchainbuffercount";
constexpr const char* kDebugMode = "sl.param.global.dbgMode";
constexpr const char* kPFunSetTag = "sl.param.global.setTag";
constexpr const char* kPFunGetTag = "sl.param.global.getTag";
constexpr const char* kVulkanTable = "sl.param.global.vulkanTable";
}

namespace common
{

constexpr const char *kGPUInfo = "sl.param.common.gpuInfo";
constexpr const char *kComputeAPI = "sl.param.common.computeAPI";
constexpr const char* kKeyboardAPI = "sl.param.common.keyboardAPI";
constexpr const char* kPFunEvaluateFeature = "sl.param.common.evaluateFeature";
constexpr const char* kPFunRegisterEvaluateCallbacks = "sl.param.common.registerEvaluateCallbacks";
constexpr const char* kStats = "sl.param.common.stats";

}

namespace template_plugin
{

constexpr const char* kSupportedAdapters = "sl.param.template_plugin.supportedAdapters";
constexpr const char* kSetConstsFunc = "sl.param.template_plugin.setConstsFunc";
constexpr const char* kGetSettingsFunc = "sl.param.template_plugin.getSettingsFunc";
constexpr const char* kStats = "sl.param.template_plugin.stats";
constexpr const char* kCurrentFrame = "sl.param.template_plugin.frame";

}

namespace dlss
{

constexpr const char* kSupportedAdapters = "sl.param.dlss.supportedAdapters";
constexpr const char* kSetConstsFunc = "sl.param.dlss.setConstsFunc";
constexpr const char* kGetSettingsFunc = "sl.param.dlss.getSettingsFunc";
constexpr const char* kStats = "sl.param.dlss.stats";
constexpr const char* kCurrentFrame = "sl.param.dlss.frame";
constexpr const char* kMVecBuffer = "sl.param.dlss.mvec";

}

namespace nrd
{

constexpr const char* kSupportedAdapters = "sl.param.nrd.supportedAdapters";
constexpr const char* kSetConstsFunc = "sl.param.nrd.setConstsFunc";
constexpr const char* kGetSettingsFunc = "sl.param.nrd.getSettingsFunc";
constexpr const char* kStats = "sl.param.nrd.stats";
constexpr const char* kCurrentFrame = "sl.param.nrd.frame";
constexpr const char* kMVecBuffer = "sl.param.nrd.mvec";
constexpr const char* kViewZBuffer = "sl.param.nrd.viewZ";

}

struct IParameters
{
    virtual void set(const char* key, bool value) = 0;
    virtual void set(const char* key, unsigned long long value) = 0;
    virtual void set(const char* key, float value) = 0;
    virtual void set(const char* key, double value) = 0;
    virtual void set(const char* key, unsigned int value) = 0;
    virtual void set(const char* key, int value) = 0;
    virtual void set(const char* key, void *value) = 0;

    virtual bool get(const char* key, bool *value) const = 0;
    virtual bool get(const char* key, unsigned long long *value) const = 0;
    virtual bool get(const char* key, float *value) const = 0;
    virtual bool get(const char* key, double *value) const = 0;
    virtual bool get(const char* key, unsigned int *value) const = 0;
    virtual bool get(const char* key, int *value) const = 0;
    virtual bool get(const char* key, void **value) const = 0;

    virtual std::vector<std::string> enumerate() const = 0;
};

// Helpers

template<typename T>
inline bool getPointerParam(IParameters* parameters, const char *key, T **res, bool optional = false, uint32_t id = 0)
{
    void *p = nullptr;
    if (!parameters->get(id ? (std::string(key) + "." + std::to_string(id)).c_str() : key, &p))
    {
        if (!optional)
        {
            return false;
        }
    }
    *res = (T*)p;
    return true;
};

template<typename T>
inline bool getParam(IParameters* parameters, const char *key, T *res, bool optional = false)
{
    if (!parameters->get(key, res))
    {
        if (!optional)
        {
            return false;
        }
        *res = {};
    }
    return true;
};

} // namespace param
} // namespace sl
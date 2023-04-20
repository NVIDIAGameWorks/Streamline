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
#include <string>

#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

namespace sl
{
using Feature = uint32_t;

namespace interposer
{

// 8 bytes for 64bit address and 8 bytes for code
constexpr uint32_t kCodePatchSize = 16;

#ifdef SL_WINDOWS

using VirtualAddress = void*;

struct ExportedFunction
{
    ExportedFunction() {};
    ExportedFunction(const char* n, VirtualAddress r = nullptr) { name = n; replacement = r; }
    ExportedFunction(const ExportedFunction& rhs) { operator=(rhs); }
    
    inline bool operator==(const ExportedFunction& rhs) const { return strcmp(name, rhs.name) == 0; }
    inline ExportedFunction& operator=(const ExportedFunction& rhs)
    {
        name = rhs.name;
        target = rhs.target;
        replacement = rhs.replacement;
        memcpy(originalCode, rhs.originalCode, kCodePatchSize);
        memcpy(currentCode, rhs.currentCode, kCodePatchSize);
        return *this; 
    }

    uint8_t originalCode[kCodePatchSize]{};
    uint8_t currentCode[kCodePatchSize]{};
    const char* name{};
    VirtualAddress target{};
    VirtualAddress replacement{};
};

using ExportedFunctionList = std::vector<ExportedFunction>;

struct InterposerConfig
{
    bool enableInterposer = true;
    bool loadAllFeatures = false;
    bool showConsole = false;
    bool vkValidation = false;
    bool useDXGIProxy = true; // Avoids DXGI factory v-table injection if set to true
    bool waitForDebugger = false;
    bool forceProxies = false;
    bool forceNonNVDA = false;
    bool trackEngineAllocations = false;
    float logMessageDelayMs = 5000.0f;
    uint32_t logLevel = 2;
    std::string logPath{};
    std::string pathToPlugins{};
    std::vector<Feature> loadSpecificFeatures{};
};

struct IHook
{
    virtual void setUseDXGIProxy(bool value) = 0;
    virtual void setEnabled(bool value) = 0;
    virtual bool isEnabled() const = 0;
    virtual const InterposerConfig& getConfig() const = 0;
    virtual const std::wstring& getConfigPath() const = 0;
    virtual bool enumerateModuleExports(const wchar_t* systemModule, ExportedFunctionList& exportedFunctions) = 0;
    virtual bool registerHookForClassInstance(IUnknown* instance, uint32_t virtualTableOffset, ExportedFunction& f) = 0;
    virtual bool restoreOriginalCode(ExportedFunction& f) = 0;
    virtual bool restoreCurrentCode(const ExportedFunction& f) = 0;
};

template <typename T>
inline T call(T replacement, ExportedFunction& f)
{
    return reinterpret_cast<T>(f.target);
}

#else

struct IHook
{
    virtual void setEnabled(bool value) = 0;
    virtual bool isEnabled() const = 0;
    virtual const json& getConfig() const = 0;
};

#endif

IHook* getInterface();
void destroyInterface();


}

}

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

#ifdef SL_WINDOWS
#include <Windows.h>
#endif
#include <filesystem>

#include "include/sl.h"
#include "source/core/sl.interposer/hook.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.param/parameters.h"

namespace sl
{
namespace interposer
{

struct Hook : public IHook
{    
    Hook()
    {
#ifndef SL_PRODUCTION

#define SL_EXTRACT_CONFIG_FLAG(a)                                                   \
if (config.contains(#a))                                                            \
{                                                                                   \
    config.at(#a).get_to(m_config.a);                                               \
    auto v = extra::format("{}:{}",#a, m_config.a);                                 \
    SL_LOG_HINT("Read '%s' from sl.interposer.json", v.c_str());                    \
}
        // Hook interface can be called before slInit so we cannot use plugin locations from sl::Preferences
        std::vector<std::wstring> jsonLocations = { file::getModulePath(), file::getExecutablePath(), file::getCurrentDirectoryPath() };
        for (auto& path : jsonLocations)
        {
            std::wstring interposerJSONFile = path + L"/sl.interposer.json";
            if (file::exists(interposerJSONFile.c_str())) try
            {
                // NOTE: Logging does not work here, not initialized yet since values from this JSON can change the way logging works
                m_configPath = path;
                auto jsonText = file::read(interposerJSONFile.c_str());
                if (!jsonText.empty())
                {
                    json config = json::parse(jsonText.begin(), jsonText.end(), nullptr, /* allow exceptions: */ true, /* ignore comments: */ true);
                    
                    SL_EXTRACT_CONFIG_FLAG(enableInterposer);
                    SL_EXTRACT_CONFIG_FLAG(useDXGIProxy);
                    SL_EXTRACT_CONFIG_FLAG(loadAllFeatures);
                    SL_EXTRACT_CONFIG_FLAG(showConsole);
                    SL_EXTRACT_CONFIG_FLAG(vkValidation);
                    SL_EXTRACT_CONFIG_FLAG(logPath);
                    SL_EXTRACT_CONFIG_FLAG(pathToPlugins);
                    SL_EXTRACT_CONFIG_FLAG(logLevel);
                    SL_EXTRACT_CONFIG_FLAG(logMessageDelayMs);
                    SL_EXTRACT_CONFIG_FLAG(waitForDebugger);
                    SL_EXTRACT_CONFIG_FLAG(forceProxies);
                    SL_EXTRACT_CONFIG_FLAG(forceNonNVDA);
                    SL_EXTRACT_CONFIG_FLAG(trackEngineAllocations);
                    SL_EXTRACT_CONFIG_FLAG(enableD3D12DebugLayer);

                    if (m_config.trackEngineAllocations)
                    {
                        m_config.forceProxies = true;
                    }

                    if (config.contains("loadSpecificFeatures"))
                    {
                        auto& list = config.at("loadSpecificFeatures");
                        for (auto& item : list)
                        {
                            uint32_t id;
                            item.get_to(id);
                            m_config.loadSpecificFeatures.push_back((Feature)id);
                        }
                    }
                }
                break;
            }
            catch (std::exception&)
            {
                // This will tell other modules that interposer config is invalid
                m_configPath.clear();
            }
        }
#endif
    };

    void setUseDXGIProxy(bool value) override final
    {
        m_config.useDXGIProxy = value;
    }

    void setEnabled(bool value) override final
    {
        m_config.enableInterposer = value;
    }

    bool isEnabled() const override final
    {
        return m_config.enableInterposer;
    }

    const InterposerConfig& getConfig() const override final
    {
        return m_config;
    }

    const std::wstring& getConfigPath() const override final
    {
        return m_configPath;
    }

#ifdef SL_WINDOWS

    bool enumerateModuleExports(const wchar_t* systemModule, ExportedFunctionList& list) override final
    {
        auto handle = LoadLibraryW(systemModule);
        if (!handle)
        {
            return false;
        }

        auto dllBase = reinterpret_cast<const BYTE*>(handle);
        auto dllHeader = reinterpret_cast<const IMAGE_NT_HEADERS*>(dllBase + reinterpret_cast<const IMAGE_DOS_HEADER*>(dllBase)->e_lfanew);

        if (dllHeader->Signature != IMAGE_NT_SIGNATURE || dllHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size == 0)
        {
            return false;
        }

        auto exportDir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(dllBase + dllHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
        auto exportBase = static_cast<WORD>(exportDir->Base);

        list.resize(exportDir->NumberOfNames);
        for (DWORD i = 0; i < exportDir->NumberOfNames; i++)
        {
            auto& f = list[i];
            f.name = reinterpret_cast<const char*>(dllBase + reinterpret_cast<const DWORD*>(dllBase + exportDir->AddressOfNames)[i]);
            auto ordinal = exportBase + reinterpret_cast<const  WORD*>(dllBase + exportDir->AddressOfNameOrdinals)[i];
            f.target = const_cast<void*>(reinterpret_cast<const void*>(dllBase + reinterpret_cast<const DWORD*>(dllBase + exportDir->AddressOfFunctions)[ordinal - exportBase]));
        }
        return true;
    }

    bool registerHookForClassInstance(IUnknown* instance, uint32_t virtualTableOffset, ExportedFunction& f) override final
    {
        // When another interposer is attached (e.g. APIC) we might get multiple 
        // calls to hook a class instance which we must ignore to avoid circular references.
        if (!f.target)
        {
            std::scoped_lock<std::mutex> lock(m_mutex);
            void** virtualTable = *reinterpret_cast<VirtualAddress**>(instance);
            auto address = &virtualTable[virtualTableOffset];
            if (virtualTable[virtualTableOffset] != f.replacement)
            {
                f.target = virtualTable[virtualTableOffset];
                DWORD prevProtection{};
                if (!VirtualProtect(address, kCodePatchSize, PAGE_READWRITE, &prevProtection))
                {
                    return false;
                }
                *reinterpret_cast<VirtualAddress*>(address) = f.replacement;
                VirtualProtect(address, kCodePatchSize, prevProtection, &prevProtection);

                // Cache the original code at target's address
                if (!VirtualProtect(f.target, kCodePatchSize, PAGE_READWRITE, &prevProtection))
                {
                    return false;
                }
                memcpy(f.originalCode, f.target, kCodePatchSize);
                if (!VirtualProtect(f.target, kCodePatchSize, prevProtection, &prevProtection))
                {
                    return false;
                }
            }
        }

        //! Always set function pointer since it could have been cleared on slShutdown
        auto parameters = sl::param::getInterface();
        parameters->set(f.name, f.target);
        return true;
    }

    bool restoreOriginalCode(ExportedFunction& f) override final
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        DWORD prevProtection{};
        if (!VirtualProtect(f.target, kCodePatchSize, PAGE_READWRITE, &prevProtection))
        {
            return false;
        }
        memcpy(f.currentCode, f.target, kCodePatchSize);
        memcpy(f.target, f.originalCode, kCodePatchSize);
        if (!VirtualProtect(f.target, kCodePatchSize, prevProtection, &prevProtection))
        {
            return false;
        }
        FlushInstructionCache(GetCurrentProcess(), f.target, kCodePatchSize);
        return true;
    }

    bool restoreCurrentCode(const ExportedFunction& f) override final
    {
        std::scoped_lock<std::mutex> lock(m_mutex);
        DWORD prevProtection{};
        if (!VirtualProtect(f.target, kCodePatchSize, PAGE_READWRITE, &prevProtection))
        {
            return false;
        }
        memcpy(f.target, f.currentCode, kCodePatchSize);
        if (!VirtualProtect(f.target, kCodePatchSize, prevProtection, &prevProtection))
        {
            return false;
        }
        FlushInstructionCache(GetCurrentProcess(), f.target, kCodePatchSize);
        return true;
    }

#endif

    std::wstring m_configPath{};
    InterposerConfig m_config{};
    bool m_enabled = true;
    std::mutex m_mutex;
    inline static Hook* s_hook = {};
};

bool hasInterface()
{
    return Hook::s_hook != nullptr;
}
IHook* getInterface()
{
    if (!Hook::s_hook)
    {
        Hook::s_hook = new Hook();
    }
    return Hook::s_hook;
}

void destroyInterface()
{
    delete Hook::s_hook;
    Hook::s_hook = {};
}

}
}

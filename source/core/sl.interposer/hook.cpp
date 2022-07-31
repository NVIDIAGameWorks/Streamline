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
#include <Windows.h>
#endif
#include <filesystem>

#include "source/core/sl.interposer/hook.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
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
        // Hook interface can be called before slInit so we cannot use plugin locations from sl::Preferences
        std::vector<std::wstring> jsonLocations = { file::getExecutablePath(), file::getCurrentDirectoryPath() };
        for (auto& path : jsonLocations)
        {
            std::wstring interposerJSONFile = path + L"/sl.interposer.json";
            if (file::exists(interposerJSONFile.c_str())) try
            {
                SL_LOG_HINT("Found %S", interposerJSONFile.c_str());
                auto jsonText = file::read(interposerJSONFile.c_str());
                if (!jsonText.empty())
                {
                    // safety null in case the JSON string is not null-terminated (found by AppVerif)
                    jsonText.push_back(0);
                    std::istringstream stream((const char*)jsonText.data());
                    stream >> m_config;
                    if (m_config.contains("enableInterposer"))
                    {
                        m_config.at("enableInterposer").get_to(m_enabled);
                        SL_LOG_HINT("Interposer enabled - %s", m_enabled ? "yes" : "no");
                    }
                }
            }
            catch (std::exception& e)
            {
                SL_LOG_ERROR("Failed to parse JSON file - %s", e.what());
            }
        }
#endif
    };

    void setEnabled(bool value) override final
    {
        m_enabled = value;
    }

    bool isEnabled() const override final
    {
        return m_enabled;
    }

    const json& getConfig() const override final
    {
        return m_config;
    }

#ifdef SL_WINDOWS

    bool enumerateModuleExports(const wchar_t* systemModule, ExportedFunctionList& list) override final
    {
        WCHAR buf[MAX_PATH] = L"";
        GetSystemDirectoryW(buf, sizeof(buf));
        auto handle = LoadLibraryW((std::wstring(buf) + L"/" + systemModule).c_str());
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

            auto parameters = sl::param::getInterface();
            parameters->set(f.name, f.target);
        }
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

    json m_config;
    bool m_enabled = true;
    std::mutex m_mutex;
    inline static Hook* s_hook = {};
};

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

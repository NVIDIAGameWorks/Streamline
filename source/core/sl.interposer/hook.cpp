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
#include "external/json/include/nlohmann/json.hpp"
using json = nlohmann::json;

namespace sl
{
namespace interposer
{

struct Hook : public IHook
{    
    Hook()
    {
    };

    void setEnabled(bool value) override final
    {
        enabled = value;
    }

    bool isEnabled() const override final
    {
        return enabled;
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
        void** virtualTable = *reinterpret_cast<VirtualAddress**>(instance);
        auto address = &virtualTable[virtualTableOffset];
        if (virtualTable[virtualTableOffset] != f.replacement)
        {            
            f.target = virtualTable[virtualTableOffset];
            DWORD protection = PAGE_READWRITE;
            if (!VirtualProtect(address, sizeof(VirtualAddress), protection, &protection))
            {
                return false;
            }
            *reinterpret_cast<VirtualAddress*>(address) = f.replacement;
            VirtualProtect(address, sizeof(VirtualAddress), protection, &protection);

            auto parameters = sl::param::getInterface();
            parameters->set(f.name, f.target);
        }
        return true;
    }

#endif

    bool enabled = true;
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

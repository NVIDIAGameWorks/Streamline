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

namespace sl
{
namespace interposer
{

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
        return *this; 
    }

    const char* name = {};
    VirtualAddress target = {};
    VirtualAddress replacement = {};
};

using ExportedFunctionList = std::vector<ExportedFunction>;

struct IHook
{
    virtual void setEnabled(bool value) = 0;
    virtual bool isEnabled() const = 0;
    virtual bool enumerateModuleExports(const wchar_t* systemModule, ExportedFunctionList& exportedFunctions) = 0;
    virtual bool registerHookForClassInstance(IUnknown* instance, uint32_t virtualTableOffset, ExportedFunction& f) = 0;
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
};

#endif

IHook* getInterface();
void destroyInterface();


}

}

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

#include <string>
#ifdef SL_WINDOWS
#include <windows.h>
#endif

// Forward defines so we can reduce the overall number of includes
struct NVSDK_NGX_Parameter;
struct VkPhysicalDevice_T;
struct VkDevice_T;
struct VkInstance_T;
using VkPhysicalDevice = VkPhysicalDevice_T*;
using VkDevice = VkDevice_T*;
using VkInstance = VkInstance_T*;

struct ID3D12Device;
struct ID3D12Resource;

#ifdef SL_LINUX
using HMODULE = void*;
#define GetProcAddress dlsym
#define FreeLibrary dlclose
#define LoadLibraryA(lib) dlopen(lib, RTLD_LAZY)
#define LoadLibraryW(lib) dlopen(sl::extra::toStr(lib).c_str(), RTLD_LAZY)
#else

//! Dummy interface allowing us to extract the underlying base interface
struct DECLSPEC_UUID("ADEC44E2-61F0-45C3-AD9F-1B37379284FF") StreamlineRetreiveBaseInterface : IUnknown
{

};

#endif

namespace sl
{

struct VkDevices
{
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physical;
};

namespace param
{
  struct IParameters;
  IParameters *getInterface();
  void destroyInterface();
}

struct Version
{
    Version() : major(0), minor(0), build(0) {};
    Version(int v1, int v2, int v3) : major(v1), minor(v2), build(v3) {};

    inline operator bool() const { return major != 0 || minor != 0 || build != 0;}

    inline bool fromStr(const std::string& str)
    {
#if SL_WINDOWS
        return sscanf_s(str.c_str(), "%d.%d.%d", &major, &minor, &build) == 3;
#else
        return sscanf(str.c_str(), "%d.%d.%d", &major, &minor, &build) == 3;
#endif
    }
    inline std::string toStr() const
    {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(build);
    }
    inline std::wstring toWStr() const
    {
        return std::to_wstring(major) + L"." + std::to_wstring(minor) + L"." + std::to_wstring(build);
    }
    inline std::wstring toWStrOTAId() const
    {
        return std::to_wstring((major << 16) | (minor << 8) | build);
    }
    inline bool operator==(const Version &rhs) const
    {
        return major == rhs.major && minor == rhs.minor && build == rhs.build;
    }
    inline bool operator>(const Version &rhs) const
    {
        if (major < rhs.major) return false;
        else if (major > rhs.major) return true;
        // major version the same
        if (minor < rhs.minor) return false;
        else if (minor > rhs.minor) return true;
        // minor version the same
        if (build < rhs.build) return false;
        else if (build > rhs.build) return true;
        // build version the same
        return false;
    };
    inline bool operator>=(const Version &rhs) const
    {
        return operator>(rhs) || operator==(rhs);
    };
    inline bool operator<(const Version &rhs) const
    {
        if (major > rhs.major) return false;
        else if (major < rhs.major) return true;
        // major version the same
        if (minor > rhs.minor) return false;
        else if (minor < rhs.minor) return true;
        // minor version the same
        if (build > rhs.build) return false;
        else if (build < rhs.build) return true;
        // build version the same
        return false;
    };
    inline bool operator<=(const Version &rhs) const
    {
        return operator<(rhs) || operator==(rhs);
    };

    int major;
    int minor;
    int build;
};

namespace api
{

// Core API, each plugin must implement these
using PFuncSetParameters = void(sl::param::IParameters*);
using PFuncGetPluginJSONConfig = const char *(void);
using PFuncOnPluginStartup = bool(const char *jsonConfig, void* device, sl::param::IParameters *);
using PFuncOnPluginShutdown = void(void);
using PFuncGetPluginFunction = void*(const char*name);

} // namespace api
} // namespace sl
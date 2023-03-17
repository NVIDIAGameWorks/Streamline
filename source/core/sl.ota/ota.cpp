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

#include "source/core/sl.api/internal.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.ota/ota.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "source/plugins/sl.common/versions.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "external/json/include/nlohmann/json.hpp"

using json = nlohmann::json;

#ifdef SL_WINDOWS
#include <ShlObj.h>
#include <wininet.h>
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Wininet.lib")
#endif

namespace sl
{
namespace ota
{

struct OTA : IOTA
{
    bool m_enable = false;

    std::string exec(const std::wstring& command) 
    {
        std::string output;
#ifdef SL_WINDOWS
        HANDLE readPipe, writePipe;
        SECURITY_ATTRIBUTES security;
        STARTUPINFOW        start;
        PROCESS_INFORMATION processInfo;

        security.nLength = sizeof(SECURITY_ATTRIBUTES);
        security.bInheritHandle = true;
        security.lpSecurityDescriptor = NULL;

        if (CreatePipe(
            &readPipe,  // address of variable for read handle
            &writePipe, // address of variable for write handle
            &security,  // pointer to security attributes
            0           // number of bytes reserved for pipe
        )) {


            GetStartupInfoW(&start);
            start.hStdOutput = writePipe;
            start.hStdError = writePipe;
            start.hStdInput = readPipe;
            start.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            start.wShowWindow = SW_HIDE;

            if (CreateProcessW(NULL,     // pointer to name of executable module
                (wchar_t*)command.c_str(),         // pointer to command line string
                &security,               // pointer to process security attributes
                &security,               // pointer to thread security attributes
                TRUE,                    // handle inheritance flag
                NORMAL_PRIORITY_CLASS,   // creation flags
                NULL,                    // pointer to new environment block
                NULL,                    // pointer to current directory name
                &start,                  // pointer to STARTUPINFO
                &processInfo             // pointer to PROCESS_INFORMATION
            )) {

                // wait for the child process to start
                for (UINT state = WAIT_TIMEOUT; state == WAIT_TIMEOUT; state = WaitForSingleObject(processInfo.hProcess, 100));

                DWORD bytesRead = 0, count = 0;
                const int BUFF_SIZE = 1024;
                char* buffer = new char[BUFF_SIZE];
                output = "";
                do {
                    DWORD dwAvail = 0;
                    if (!PeekNamedPipe(readPipe, NULL, 0, NULL, &dwAvail, NULL)) {
                        // error, the child process might have ended
                        break;
                    }
                    if (!dwAvail) {
                        // no data available in the pipe
                        break;
                    }
                    ReadFile(readPipe, buffer, BUFF_SIZE, &bytesRead, NULL);
                    buffer[bytesRead] = '\0';
                    output += buffer;
                    count += bytesRead;
                } while (bytesRead >= BUFF_SIZE);
                delete buffer;
            }
        }

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        CloseHandle(writePipe);
        CloseHandle(readPipe);
#endif
        return output;
    }

    std::wstring getNGXPath() const
    {
        std::wstring path = L"";
        PWSTR programDataPath = NULL;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &programDataPath);
        if (SUCCEEDED(hr))
        {
            path = programDataPath + std::wstring(L"/NVIDIA/NGX/models/");
        }
        return path;
    }

    std::wstring getDriverPath() const
    {
        auto getStringRegKey = [&](HKEY InKey, const WCHAR *InValueName, WCHAR *OutValue, DWORD dwBufferSize)->LONG
        {
            ULONG nError = RegQueryValueExW(InKey, InValueName, 0, NULL, (LPBYTE)OutValue, &dwBufferSize);
            return nError;
        };

        auto getPathFromRegistry = [&](const WCHAR *InRegKeyHive, const WCHAR *InRegKeyName, WCHAR *OutPath)->LONG
        {
            HKEY Key;
            LONG Res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, InRegKeyHive, 0, KEY_READ, &Key);
            if (Res == ERROR_SUCCESS)
            {
                Res = getStringRegKey(Key, InRegKeyName, OutPath, MAX_PATH);            
                RegCloseKey(Key);
            }
            return Res;
        };

        WCHAR pathAbsW[MAX_PATH] = {};
        // DCH driver
        if(getPathFromRegistry(L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore", L"NGXPath", pathAbsW))    
        {
            // Finally, fall back to legacy location (all nonDCH drivers should have this regkey present)
            if (getPathFromRegistry(L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"FullPath", pathAbsW))
            {
                SL_LOG_ERROR("unable to find driver path");
            }
        }
        return pathAbsW;    
    }

    std::map<std::string, Version> m_versions;

    bool readServerManifest() override
    {
        std::wstring ngxPath = getNGXPath();
        auto manifest = file::open((ngxPath + L"nvngx_config.txt").c_str(), L"rt");
        if (!manifest)
        {
            return false;
        }
        
        char line[256];
        while (file::readLine(manifest, line, 256))
        {
            std::string tmp(line);
            auto i = tmp.find("[sl_");
            if (i != std::string::npos)
            {
                std::string feature = tmp.substr(4,tmp.size() - 5);
                file::readLine(manifest, line, 256);
                Version otaVersion;
                if (sscanf_s(line, "app_E658703 = %d.%d.%d", &otaVersion.major, &otaVersion.minor, &otaVersion.build) == 3)
                {
                    m_versions[feature] = otaVersion;
                    SL_LOG_VERBOSE("OTA feature %s version %s", feature.c_str(), otaVersion.toStr().c_str());
                }
            }
        }
        file::close(manifest);
        return true;
    }

    uint32_t getNVDAArchitecture(common::SystemCaps* caps)
    {
        uint32_t gpuArch = 0;
        for (uint32_t i = 0; i < common::kMaxNumSupportedGPUs; i++)
        {
            if (caps->adapters[i].vendor == chi::VendorId::eNVDA)
            {
                gpuArch = std::max(gpuArch, caps->adapters[i].architecture);
            }
        }
        return gpuArch;
    }

    bool checkForOTA() override
    {
        try
        {
            auto ctx = api::getContext();
            // Let's grab GPU info
            common::SystemCaps* caps = {};
            if(!param::getPointerParam(api::getContext()->parameters, sl::param::common::kSystemCaps, &caps))
            {
                return false;
            }

#ifndef SL_PRODUCTION
            std::string testServer;
            json& extraConfig = *(json*)api::getContext()->extConfig;
            if (extraConfig.contains("testServer"))
            {
                extraConfig.at("testServer").get_to(testServer);
            }
            if (extraConfig.contains("enableOTA"))
            {
                extraConfig.at("enableOTA").get_to(m_enable);
            }
#endif

            if (!m_enable)
            {
                SL_LOG_VERBOSE("OTA disabled");
                return false;
            }

            auto ngxPath = getNGXPath();
            auto driverPath = getDriverPath();
            auto keys = ctx->parameters->enumerate();
            for (auto &key : keys)
            {
                auto i = key.find(".supported");
                if (i != std::string::npos)
                {
                    // We found a supported feature!
                    auto j = key.rfind('.', i - 1);
                    auto name = key.substr(j + 1, i - j - 1);

                    if (m_versions.find(name) == m_versions.end())
                    {
                        // Bootstrap the feature first since it is not in the OTA manifest
                        std::string tmp = "\\nvngx_update.exe cmsid 0 feature sl_" + name + " api bootstrap";
                        auto cmd = driverPath + extra::toWStr(tmp);
                        SL_LOG_VERBOSE("Running %S", cmd.c_str());
                        auto res = exec(cmd);
                        SL_LOG_VERBOSE("%s", res.c_str());
                    }

                    auto gpuArch = getNVDAArchitecture(caps);

                    // Now let's check for updates
                    {
                        std::string tmp = "\\nvngx_update.exe cmsid 0 feature sl_" + name + " api update type dll gpuarch 0x" + extra::toHexStr<uint32_t>(gpuArch, 3);
#ifndef SL_PRODUCTION
                        if (!testServer.empty())
                        {
                            tmp += " test testroot " + testServer;
                        }
#endif
                        auto cmd = driverPath + extra::toWStr(tmp);
                        SL_LOG_VERBOSE("Running %S", cmd.c_str());
                        auto res = exec(cmd);
                        SL_LOG_VERBOSE("%s", res.c_str());
                    }
                }
            }
            return false;
        }
        catch (std::exception& e)
        {
            SL_LOG_ERROR("Exception: %s", e.what());
        }
        return true;
    }

    api::PFuncGetPluginFunction *getOTAPluginEntryPointIfNewerAndSupported(const char *pluginName, const Version &pluginVersion, const Version &apiVersion) override
    {   
        // Now let's check for the OTA version (if any)
        Version otaVersion = {};
        std::string name = pluginName;
        name = name.substr(name.find(".") + 1);
        auto it = m_versions.find(name);
        if (it == m_versions.end())
        {
            return false;
        }
        otaVersion = (*it).second;

        if (otaVersion <= pluginVersion)
        {
            return false;
        }

        // At this point we know there is an OTA for our plugin so let's load it!

        // Let's grab GPU info
        common::SystemCaps* caps = {};
        if (!param::getPointerParam(api::getContext()->parameters, sl::param::common::kSystemCaps, &caps))
        {
            return false;
        }

        if (!m_enable)
        {
            SL_LOG_VERBOSE("OTA disabled");
            return false;
        }

        auto gpuArch = getNVDAArchitecture(caps);

        std::wstring ngxPath = getNGXPath();
        auto arch = extra::toHexStr<uint32_t>(gpuArch, 3);
        std::wstring pluginPath = ngxPath + L"/sl_" + extra::toWStr(name) + L"/versions/" + otaVersion.toWStrOTAId() + L"/files/" + extra::toWStr(arch) + L"_E658703.dll";
        api::PFuncGetPluginFunction* otaGetPluginFunction = nullptr;
        HMODULE mod = security::loadLibrary(pluginPath.c_str());
        if (mod)
        {
            otaGetPluginFunction = reinterpret_cast<api::PFuncGetPluginFunction*>(GetProcAddress(mod, "slGetPluginFunction"));
            if (otaGetPluginFunction)
            {
                auto otaOnLoad = reinterpret_cast<api::PFuncOnPluginLoad*>(otaGetPluginFunction("slOnPluginLoad"));
                if (otaOnLoad) try
                {
                    const char* pluginJSONText{};
                    json& loaderJSON = *(json*)api::getContext()->loaderConfig;
                    auto loaderJSONStr = loaderJSON.dump();
                    if (!otaOnLoad(api::getContext()->parameters, loaderJSONStr.c_str(), &pluginJSONText))
                    {
                        return false;
                    }

                    json config;
                    std::istringstream stream(pluginJSONText);
                    stream >> config;

                    uint32_t supportedAdapters = 0;
                    Version otaVersion, otaApi;
                    config.at("version").at("major").get_to(otaVersion.major);
                    config.at("version").at("minor").get_to(otaVersion.minor);
                    config.at("version").at("build").get_to(otaVersion.build);
                    config.at("api").at("major").get_to(otaApi.major);
                    config.at("api").at("minor").get_to(otaApi.minor);
                    config.at("api").at("build").get_to(otaApi.build);
                    config.at("supportedAdapters").get_to(supportedAdapters);

                    if (supportedAdapters != 0 && otaVersion >= pluginVersion && otaApi >= apiVersion)
                    {
                        // Newer plugin with the compatibe API, use it
                        SL_LOG_HINT("Found OTA for plugin %s API %s version upgrade %s > %s", pluginName, otaApi.toStr().c_str(), pluginVersion.toStr().c_str(), otaVersion.toStr().c_str());
                    }
                    else
                    {
                        FreeLibrary(mod);
                        otaGetPluginFunction = nullptr;
                    }
                }
                catch (std::exception &e)
                {
                    SL_LOG_ERROR("JSON exception %s", e.what());
                    return nullptr;
                };
            }
            else
            {
                SL_LOG_ERROR("This should never happen unless somebody messed with the DLL(s) failed to fetch 'slGetPluginFunction' API from an OTA plugin '%S'", pluginPath.c_str());
            }
        }
        return otaGetPluginFunction;
    }
};

OTA s_ota = {};

IOTA* getInterface()
{
    return &s_ota;
}
}
}
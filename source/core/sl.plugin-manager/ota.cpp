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

#include <thread>

#include "source/core/sl.api/internal.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.plugin-manager/ota.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "source/core/sl.interposer/versions.h"
#include "source/core/sl.interposer/hook.h"
#include "source/plugins/sl.common/versions.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "external/json/include/nlohmann/json.hpp"

#include "external/nvapi/nvapi.h"

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

    void execThreadProc(const std::wstring command)
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
                DWORD bytesRead = 0, count = 0;
                // 4K buffers to fit nicely on a page :)
                const int BUFF_SIZE = 0x1000;
                char* buffer = new char[BUFF_SIZE];
                output = "";

                // Loop until process is complete, buffering out 4K pages of
                // stderr/stdout data to our output string
                do {
                    DWORD dwAvail = 0;
                    if (PeekNamedPipe(readPipe, NULL, 0, NULL, &dwAvail, NULL)) {
                        if (dwAvail) {
                            if (!ReadFile(readPipe, buffer, BUFF_SIZE - 1, &bytesRead, NULL))
                            {
                                // failed to read
                                SL_LOG_ERROR("Failed ReadFile with error 0x%x", GetLastError());
                                break;
                            }
                            buffer[bytesRead] = '\0';
                            output += buffer;
                            count += bytesRead;
                        }
                        else
                        {
                            // no data available in the pipe
                        }
                    }
                    else
                    {
                        // error, the child process might have ended
                    }
                } while (WaitForSingleObject(processInfo.hProcess, 100) == WAIT_TIMEOUT);

                delete buffer;
                CloseHandle(processInfo.hThread);
                CloseHandle(processInfo.hProcess);
            }
            else
            {
                SL_LOG_ERROR("Failed to create process %ls", command.c_str());
            }

            CloseHandle(writePipe);
            CloseHandle(readPipe);
        }
        else
        {
            SL_LOG_ERROR("Failed to create pipe");
        }
#endif
        SL_LOG_VERBOSE("execThreadProc: %ls", command.c_str());

        // Append a '\n' here so that SL uses "unformatted" logs. The output
        // from the NGX updater is formatted already with timestamps, so we
        // want to remove them before adding our own.
        // Safety note: Passing this directly to the `fmt` parameter of `logva`
        // is safe because the '\n' at the end skips formatting. Using "%s\n"
        // would make the logger skip formatting and print "%s" instead of the
        // intended message.
        if (!output.empty())
        {
            output += '\n';
            SL_LOG_VERBOSE(output.c_str());
        }
    }

struct OTA : IOTA
{
    bool m_enable = true;

    // The hash ID for NGX OTA CMS id zero
#define NGX_OTA_CMS_ID_0_HASH "_E658703"
#define L_NGX_OTA_CMS_ID_0_HASH L"_E658703"

    void exec(const std::wstring& command)
    {
        std::thread execThread(execThreadProc, command);
        execThread.detach();
    }

    bool getNGXPath(std::wstring &ngxPath) const
    {
        auto getDwordRegKey = [&](HKEY InKey, const WCHAR *InValueName, DWORD *OutValue)->LONG
        {
            DWORD dwordSize = sizeof(DWORD);
            ULONG nError = RegGetValueW(InKey, NULL, InValueName, RRF_RT_REG_DWORD, NULL, (LPBYTE)OutValue, &dwordSize);
            return nError;
        };

        auto getDwordFromRegistry = [&](const WCHAR *InRegKeyHive, const WCHAR *InRegKeyName, DWORD *OutValue)->bool
        {
            HKEY Key;
            LONG Res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, InRegKeyHive, 0, KEY_READ, &Key);
            if (Res == ERROR_SUCCESS)
            {
                Res = getDwordRegKey(Key, InRegKeyName, OutValue);
                RegCloseKey(Key);
                if (Res == ERROR_SUCCESS)
                {
                    return true;
                }
            }
            return false;
        };

        std::wstring path = L"";
        PWSTR programDataPath = NULL;
        HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramData, 0, NULL, &programDataPath);
        if (!SUCCEEDED(hr))
        {
            SL_LOG_VERBOSE("Failed to get path to PROGRAMDATA for NGX Cache");
            CoTaskMemFree(programDataPath);
            return false;
        }

        bool useStagingCDN = false;
        DWORD CDNServerType;
        if (getDwordFromRegistry(L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"CDNServerType", &CDNServerType))
        {
            SL_LOG_INFO("Read CDNServerType: %d from registry", CDNServerType);

            // CDNServerType
            //  0 - production
            //  1 - staging
            useStagingCDN = (CDNServerType == 1);
        }

        if (useStagingCDN)
        {
            ngxPath = programDataPath + std::wstring(L"/NVIDIA/NGX/Staging/models/");
        }
        else
        {
            ngxPath = programDataPath + std::wstring(L"/NVIDIA/NGX/models/");
        }

        CoTaskMemFree(programDataPath);
        return true;
    }

    bool getDriverPath(std::wstring &driverPath) const
    {
        auto getStringRegKey = [&](HKEY InKey, const WCHAR *InValueName, WCHAR *OutValue, DWORD dwBufferSize)->LONG
        {
            ULONG nError = RegGetValueW(InKey, NULL, InValueName, RRF_RT_REG_SZ, NULL, (LPBYTE)OutValue, &dwBufferSize);
            return nError;
        };

        auto getPathFromRegistry = [&](const WCHAR *InRegKeyHive, const WCHAR *InRegKeyName, WCHAR *OutPath)->LONG
        {
            HKEY Key;
            LONG Res = RegOpenKeyExW(HKEY_LOCAL_MACHINE, InRegKeyHive, 0, KEY_READ, &Key);
            if (Res == ERROR_SUCCESS)
            {
                Res = getStringRegKey(Key, InRegKeyName, OutPath, sizeof(WCHAR) * MAX_PATH);
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

        driverPath = std::wstring(pathAbsW);
        return true;
    }

    // Map of plugin name+apiVersion to pluginVersion
    // For the longest time the apiVersion has been frozen at 0.0.1 so we aren't
    // making use of that quite yet, but to design for the future we need to
    // account for ABI incompatibility and as such track that field as well. For
    // now this can be tracked in the string side of the map, but in the future
    // it may be best to use a three-dimensional map with a custom comparator
    // for handling ABI compatibility.
    //
    // Example entries:
    // sl_dlss_0 => 3.1.11
    // sl_dlss_g_0 => 3.2.0
    std::map<std::string, Version> m_versions;

    bool readServerManifest() override
    {
        std::wstring ngxPath;
        if (!getNGXPath(ngxPath))
        {
            SL_LOG_ERROR("Failed to read server manifest, couldn't get NGX Cache Path");
            return false;
        }
        auto manifest = file::open((ngxPath + L"nvngx_config.txt").c_str(), L"rt");
        if (!manifest)
        {
            SL_LOG_ERROR("Failed to open manifest file at: %lsnvngx_config.txt", ngxPath.c_str());
            return false;
        }

        // Read in lines 256 bytes at at time. The NGX manifest file is quite
        // narrow and due to the feature name maximum length 256 is sufficient.
        const size_t BUF_SIZE = 256;
        char line[BUF_SIZE];
        while (file::readLine(manifest, line, BUF_SIZE))
        {
            // Search for the sl feature sections, there is only one appid for
            // SL, so parse out the first line available in the section to get
            // the version for the given SL feature
            std::string tmp(line);
            auto i = tmp.find("[sl_");
            if (i != std::string::npos)
            {
                std::string feature = tmp.substr(4, tmp.size() - 5);
                file::readLine(manifest, line, BUF_SIZE);
                Version otaVersion;
                if (sscanf_s(line, "app" NGX_OTA_CMS_ID_0_HASH " = %d.%d.%d", &otaVersion.major, &otaVersion.minor, &otaVersion.build) == 3)
                {
                    m_versions[feature] = otaVersion;
                    SL_LOG_VERBOSE("OTA feature %s version %s", feature.c_str(), otaVersion.toStr().c_str());
                }
                else
                {
                    SL_LOG_ERROR("Unexpected line in manifest file: %s", line);
                }
            }
        }

        file::close(manifest);
        return true;
    }

    uint32_t getNVDAVersion()
    {
        NvU32 DriverVersion;
        NvAPI_ShortString DriverName;
        NvAPI_Status nvStatus = NvAPI_SYS_GetDriverAndBranchVersion(&DriverVersion, DriverName);
        if (nvStatus != NVAPI_OK)
        {
            SL_LOG_ERROR("Failed to get driver version from NvAPI!");
            return 0;
        }
        return DriverVersion;
    }

    uint32_t getNVDAArchitecture()
    {
        // loop over all NvAPI exposed GPUs and return highest architecture
        // present
        NvU32 nvGpuCount = 0;
        uint32_t gpuArch = 0;
        NvPhysicalGpuHandle nvapiGpuHandles[NVAPI_MAX_PHYSICAL_GPUS];

        if (NvAPI_EnumPhysicalGPUs(nvapiGpuHandles, &nvGpuCount) == NVAPI_OK)
        {
            SL_LOG_VERBOSE("Found NVIDIA GPUs, [%p]: %d", nvapiGpuHandles, nvGpuCount);
            for (uint32_t i = 0; i < nvGpuCount; i++)
            {
                NV_GPU_ARCH_INFO archInfo{};
                archInfo.version = NV_GPU_ARCH_INFO_VER;
                NVAPI_VALIDATE_RF(NvAPI_GPU_GetArchInfo(nvapiGpuHandles[i], &archInfo));
                SL_LOG_VERBOSE("Found GPU %d, arch=0x%x", i, archInfo.architecture);

                if (archInfo.architecture > gpuArch)
                {
                    gpuArch = archInfo.architecture;
                }
            }
        }
        return gpuArch;
    }

    bool checkForOTA(Feature featureID, const Version &apiVersion, bool requestOptionalUpdates) override
    {
        uint32_t gpuArch = getNVDAArchitecture();

        if (!gpuArch)
        {
            SL_LOG_VERBOSE("OTA only enabled with NVIDIA GPUs in the system");
            return false;
        }

        // check for null? log+ return false
        std::wstring driverPath;
        if (!getDriverPath(driverPath))
        {
            SL_LOG_VERBOSE("Failed to get path to driver files");
            return false;
        }

        std::string name_version = getFeatureFilenameAsStrNoSL(featureID);
        name_version += extra::format("_{}", apiVersion.major);

        if (m_versions.find(name_version) == m_versions.end())
        {
            // Bootstrap the feature first since it is not in the OTA manifest
            std::string tmp = "\\nvngx_update.exe -cmsid 0 -feature sl_" + name_version + " -api bootstrap";
            std::wstring cmd = driverPath + extra::toWStr(tmp);
            SL_LOG_VERBOSE("Running %S", cmd.c_str());
            exec(cmd);
        }

        // Now let's check for updates
        {
            std::string tmp = "\\nvngx_update.exe -cmsid 0 -feature sl_" + name_version + " -api update -type dll -gpuarch 0x" + extra::toHexStr<uint32_t>(gpuArch, 3);

            if (requestOptionalUpdates)
            {
                uint32_t driverVersion = getNVDAVersion();
                // The NGX Updater is pendatic about its command-line input and will
                // fail to run anything if it encounters an unexpected command-line
                // flag. Because of this we need to determine if the NGX Updater
                // we're going to use supports the -optional flag. This can be
                // quickly (and pretty roughly) done with a driver version check,
                // which isn't the best, but it's much faster than running `strings`
                // on the binary :)
                //
                // For now enable updates on versions 535.85 and later, we may
                // lower this requirement in the future depending on where the
                // NGX Updater -optional flag support is integrated.
                if (driverVersion >= 53585)
                {
                    tmp += " -optional";
                    SL_LOG_INFO("Requesting optional updates!");
                }
                else
                {
                    uint32_t verMaj = driverVersion / 100;
                    uint32_t verMin = driverVersion % 100;
                    SL_LOG_WARN("Optional updates requested but your driver version %d.%d is too old!", verMaj, verMin);
                }
            }

            std::wstring cmd = driverPath + extra::toWStr(tmp);
            SL_LOG_VERBOSE("Running %S", cmd.c_str());
            exec(cmd);
        }
        return true;
    }

    bool getOTAPluginForFeature(Feature featureID, const Version &apiVersion, std::wstring &filePath) override
    {
        // First get GPU Architecture, needed to download appropriate OTA
        // snippet
        uint32_t gpuArch = getNVDAArchitecture();
        if (!gpuArch)
        {
            SL_LOG_VERBOSE("OTA only enabled with NVIDIA GPUs in the system");
            return false;
        }

        std::wstring ngxPath;
        if (!getNGXPath(ngxPath))
        {
            SL_LOG_ERROR("Failed to read server manifest, couldn't get NGX Cache Path");
            return false;
        }

        // Construct the name_version pair for this feature
        Version otaVersion = {};
        std::string name_version = getFeatureFilenameAsStrNoSL(featureID);
        name_version += extra::format("_{}", apiVersion.major);

        // Find the corresponding section in versions
        auto it = m_versions.find(name_version);
        if (it == m_versions.end())
        {
            SL_LOG_WARN("Could not find version matching for plugin: %s", name_version.c_str());
            return false;
        }
        else
        {
            otaVersion = it->second;
        }

        // Any real Plugin will have a non-zero version, if we hit the
        // zero-version that means that we just found the bootstrapped value and
        // not an actual downloaded version
        if (otaVersion == Version(0, 0, 0))
        {
            SL_LOG_WARN("No updated version found for plugin: %s", name_version.c_str());
            return false;
        }

        // Convert the version to the integer-string used in the NGX Cache
        std::wstring otaVersionString = otaVersion.toWStrOTAId();

        // SL Plugins will be subdirectories of this
        // like
        // models
        //  - dlss
        //  - dlslowmo
        //  - sl_dlss_0
        //  - sl_reflex_0
        //  - sl_dlss_g_0
        //  - sl_nis_0
        //
        // This is handled by pluginDirName so that's easy for us

        // Then inside of those it goes
        // sl_dlss_0
        // - versions
        //   - NUMBER
        //     - files
        //       - *.dll

        // XXX[ljm] there is probably a nicer sugary way to construct this oh
        // well, this at least matches the tiering of the comment above
        std::wstring pluginPath = ngxPath + \
                                  L"sl_" + extra::toWStr(name_version) + L"/" + \
                                  L"versions/" + \
                                  otaVersionString + L"/" + \
                                  L"files/" + \
                                  extra::toWStr(extra::toHexStr<uint32_t>(gpuArch, 3)) + L_NGX_OTA_CMS_ID_0_HASH + L".dll";

        // Check if exists
        if (!fs::exists(pluginPath))
        {
            SL_LOG_ERROR("Found non-zero plugin \"%s\" in NGX Cache but missing file: %ls", name_version.c_str(), pluginPath.c_str());
            return false;
        }

        filePath = pluginPath;
        return true;
    }

};

OTA s_ota = {};

IOTA* getInterface()
{
    return &s_ota;
}
}
}

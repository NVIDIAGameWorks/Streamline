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

#include "source/core/sl.ota/ota.h"

#include <ios>
#include <fstream>
#include <regex>
#include <sstream>
#include <thread>

#include "source/core/sl.api/internal.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.plugin/plugin.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.extra/extra.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "source/core/sl.interposer/versions.h"
#include "source/core/sl.interposer/hook.h"
#include "source/plugins/sl.common/versions.h"
#include "source/platforms/sl.chi/compute.h"
#include "source/plugins/sl.common/commonInterface.h"
#include "external/json/include/nlohmann/json.hpp"

#include "nvapi.h"

#ifdef SL_WINDOWS
#include <ShlObj.h>
#include <wininet.h>
#pragma comment(lib,"shlwapi.lib")
#pragma comment(lib, "Urlmon.lib")
#pragma comment(lib, "Wininet.lib")
#endif

namespace
{

sl::ota::OTA s_ota = {};

void execThreadProc(const std::wstring& command)
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

// extra::toHexStr zero-pads to a fixed width, but NGX CMS IDs in manifests
// and filenames use unpadded uppercase hex (e.g. "E658703" not "0E658703").
std::string cmsIdToHexStr(uint32_t hashedCmsId)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%X", hashedCmsId);
    return buf;
}

// When OTA override is active, feature directories and sections use
// "_override" before the API version suffix, e.g. "nis_0" -> "nis_override_0".
std::string toOverrideFeatureName(const std::string &nameVersion)
{
    auto pos = nameVersion.rfind('_');
    if (pos != std::string::npos)
    {
        return nameVersion.substr(0, pos) + "_override" + nameVersion.substr(pos);
    }
    return nameVersion + "_override";
}

}  // namespace

namespace sl::ota
{

using json = nlohmann::json;


IOTA* getInterface()
{
    return &s_ota;
}

bool OTA::findNGXUpdater()
{
    if (!m_foundUpdater.has_value())
    {
        m_foundUpdater = false;

        std::filesystem::path driverPath;
        const bool haveDriverPath = getDriverPath(driverPath);

        std::filesystem::path interposerPath;
        const bool haveInterposerPath = getSlInterposerPath(interposerPath);

        const std::wstring ngxUpdaterExeName = L"nvngx_update.exe";

        if (haveDriverPath && std::filesystem::exists(driverPath / ngxUpdaterExeName))
        {
            m_updaterExe = driverPath / ngxUpdaterExeName;
            m_foundUpdater = true;
        }

        // If the NGX Updater exists in the same location as the sl interposer, then
        // use that one instead.
        if (haveInterposerPath && std::filesystem::exists(interposerPath / ngxUpdaterExeName))
        {
            m_updaterExe = interposerPath / ngxUpdaterExeName;
            m_foundUpdater = true;
            SL_LOG_VERBOSE("Found NGX Updater in sl.interposer.dll location: %ls", m_updaterExe.c_str());
        }

        if (!m_foundUpdater)
        {
            SL_LOG_ERROR("Unable to determine NGX Updater location.");
        }
    }

    return m_foundUpdater.value();
}

bool OTA::invokeNGXUpdater(const std::wstring& args)
{
    if (!findNGXUpdater())
    {
        SL_LOG_ERROR("NGX Updater not available. OTA downloads are disabled.");
        return false;
    }

    std::wstring command = m_updaterExe.wstring() + L" " + args;

    SL_LOG_VERBOSE("Invoking NGX Updater: %ls", command.c_str());
    std::thread execThread(execThreadProc, command);
    execThread.detach();

    return true;
}

bool OTA::getNGXPath(std::filesystem::path& ngxPath) const
{
    if (m_ngxPathOverride.has_value())
    {
        ngxPath = m_ngxPathOverride.value();
        return true;
    }

    WCHAR registryPath[MAX_PATH] = {};
    if (extra::getRegistryString(L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"OTACachePath", registryPath, MAX_PATH) && registryPath[0] != L'\0')
    {
        ngxPath = registryPath;
        return true;
    }

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
    if (extra::getRegistryDword(L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"CDNServerType", &CDNServerType))
    {
        SL_LOG_INFO("Read CDNServerType: %d from registry", CDNServerType);

        // CDNServerType
        //  0 - production
        //  1 - staging
        useStagingCDN = (CDNServerType == 1);
    }

    if (useStagingCDN)
    {
        ngxPath = std::filesystem::path(programDataPath) / L"NVIDIA/NGX/Staging/models/";
    }
    else
    {
        ngxPath = std::filesystem::path(programDataPath) / L"NVIDIA/NGX/models/";
    }

    CoTaskMemFree(programDataPath);
    return true;
}

void OTA::setNGXPathOverride(const std::optional<std::filesystem::path> ngxPath)
{
    m_ngxPathOverride = ngxPath;
}

void OTA::setOTAOverrideEnabled(std::optional<bool> enabled)
{
    m_otaOverrideEnabled = enabled;
}

bool OTA::getDriverPath(std::filesystem::path& driverPath) const
{
    WCHAR pathAbsW[MAX_PATH] = {};
    // DCH driver + Parameters subkey
    if(!extra::getRegistryString(L"System\\CurrentControlSet\\Services\\nvlddmkm\\Parameters\\NGXCore", L"NGXPath", pathAbsW, MAX_PATH) || !fs::exists(pathAbsW))
    {
        // DCH driver
        if(!extra::getRegistryString(L"System\\CurrentControlSet\\Services\\nvlddmkm\\NGXCore", L"NGXPath", pathAbsW, MAX_PATH) || !fs::exists(pathAbsW))
        {
            // Finally, fall back to legacy location (all nonDCH drivers should have this regkey present)
            if (!extra::getRegistryString(L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore", L"FullPath", pathAbsW, MAX_PATH) || !fs::exists(pathAbsW))
            {
                SL_LOG_ERROR("unable to find driver path");
                return false;
            }
        }
    }

    driverPath = pathAbsW;
    return true;
}

bool OTA::getSlInterposerPath(std::filesystem::path& slPath) const
{
    HMODULE hModule = GetModuleHandleW(L"sl.interposer.dll");
    if (hModule)
    {
        std::array<wchar_t, MAX_PATH> pathString;
        GetModuleFileNameW(hModule, pathString.data(), (DWORD)pathString.size());
        slPath = std::filesystem::path(pathString.data()).remove_filename();
        return true;
    }

    SL_LOG_WARN("Unable to determine SL Interposer DLL path");
    return false;
}

bool OTA::parseServerManifest(std::ifstream &manifest, std::map<std::string, Version> &versionMap, std::vector<std::string> &optionalDownloadPresent, bool useOverride)
{
    uint32_t cmsid = useOverride ? kOverrideCmsId : kDefaultCmsId;
    std::string hashedCmsStr = cmsIdToHexStr(hashCmsId(cmsid));
    std::string versionFmt = "app_" + hashedCmsStr + " = %d.%d.%d";
    std::string optionalFmt = "app_" + hashedCmsStr + "_sl_%50s = 1";

    std::string currentFeature;
    bool inOptionalSection = false;

    std::string line;
    while (std::getline(manifest, line))
    {
        // Search for the sl feature sections
        if (line.find("[sl_") == 0)
        {
            constexpr const char* kOverrideSuffix = "_override";

            std::string sectionName = line.substr(4, line.size() - 5);
            const bool isOverrideSection = sectionName.find(kOverrideSuffix) != std::string::npos;

            if (useOverride && !isOverrideSection)
            {
                currentFeature.clear();
                inOptionalSection = false;
                continue;
            }

            if (!useOverride && isOverrideSection)
            {
                currentFeature.clear();
                inOptionalSection = false;
                continue;
            }

            currentFeature = sectionName;
            inOptionalSection = false;
            continue;
        }

        if (line.find("[optional_update_present]") == 0)
        {
            currentFeature.clear();
            inOptionalSection = true;
            continue;
        }

        if (line.empty() || line[0] == '[')
        {
            currentFeature.clear();
            inOptionalSection = false;
            continue;
        }

        if (!currentFeature.empty())
        {
            Version otaVersion;
            if (sscanf_s(line.c_str(), versionFmt.c_str(), &otaVersion.major, &otaVersion.minor, &otaVersion.build) == 3)
            {
                versionMap[currentFeature] = otaVersion;
                SL_LOG_VERBOSE("OTA feature %s version %s", currentFeature.c_str(), otaVersion.toStr().c_str());
            }
        }

        // Search for the [optional_update_present] section, there can be
        // multiple entries under this section, and other sections can trail
        // it.
        if (inOptionalSection)
        {
            // NGX defines NV_OTA_MAX_FTR_LEN as 50, so allocate space for
            // that and a NUL-byte.
            char featureString[51];
            if (sscanf_s(line.c_str(), optionalFmt.c_str(), &featureString, (uint32_t)sizeof(featureString)) == 1)
            {
                SL_LOG_VERBOSE("OTA feature %s is an optional download", featureString);
                optionalDownloadPresent.emplace_back(featureString);
            }
        }
    }
    return true;
}
// Returns ull cms id from std::string hex format.
uint64_t ExtractCMSId(const std::string& cmsID)
{
    return std::stoull(cmsID, nullptr, 16);
}

/*
    Data format in mapping file (nvngx_mapping.json):
    [{
        "engine-type": "ue4",
        "list-items": [{
            "engine-version": "4.25",
            "generic-cmsID": "B9D8C54",
            "mappings": [
                { "projectID": "{09481010-0AE3-4697-92E9-7BA7E9EE7EAD}", "cmsID": "B9D2F5C" },
                { "projectID": "{C37C0D85-01CC-4B46-832F-19B32A8A3F55}", "cmsID": "B9DAE68" }
            ]
        }]
    }]
*/
// Helper to convert engine-type string from mapping file to EngineType enum
static EngineType engineTypeFromString(const std::string& engineTypeStr)
{
    std::string upper = extra::toUpper(engineTypeStr);
    if (upper == "UE4" || upper == "UE5" || upper == "UNREAL")
    {
        return EngineType::eUnreal;
    }
    else if (upper == "UNITY")
    {
        return EngineType::eUnity;
    }
    return EngineType::eCustom;
}

// Parses the mapping file and populates the m_projectToAppIds structure (per-engine list of version patterns + projectId -> appId).
// Return true if successful, false if file is not valid JSON.
bool OTA::parseMappingFile(std::ifstream& file)
{
    m_projectToAppIds.clear();

    try
    {
        json jsonDoc = json::parse(file);

        if (!jsonDoc.is_array())
        {
            SL_LOG_VERBOSE("JSON document is not array, invalid data");
            return false;
        }

        for (const auto& engineEntry : jsonDoc)
        {
            // Skip entries without required fields
            if (!engineEntry.contains("engine-type") || !engineEntry.contains("list-items"))
            {
                SL_LOG_VERBOSE("Skipping engine entry without required fields");
                continue;
            }

            std::string engineTypeStr = engineEntry["engine-type"].get<std::string>();
            EngineType engineType = engineTypeFromString(engineTypeStr);

            const auto& listItems = engineEntry["list-items"];
            if (!listItems.is_array())
            {
                SL_LOG_VERBOSE("Skipping list-items entry for engine %s", engineTypeStr.c_str());
                continue;
            }

            for (const auto& versionEntry : listItems)
            {
                // Skip entries without engine-version or mappings
                if (!versionEntry.contains("engine-version") || !versionEntry.contains("mappings"))
                {
                    SL_LOG_VERBOSE("Skipping version entry for engine %s: missing engine-version or mappings", engineTypeStr.c_str());
                    continue;
                }

                std::string engineVersion = versionEntry["engine-version"].get<std::string>();

                std::regex versionPattern;
                try
                {
                    versionPattern = std::regex(engineVersion);
                }
                catch (const std::regex_error&)
                {
                    SL_LOG_WARN("Invalid engine-version pattern for engine %s, version %s, skipping entry", engineTypeStr.c_str(), engineVersion.c_str());
                    continue;
                }

                const auto& mappings = versionEntry["mappings"];
                if (!mappings.is_array())
                {
                    SL_LOG_VERBOSE("Skipping mappings entry for engine %s, version %s", engineTypeStr.c_str(), engineVersion.c_str());
                    continue;
                }

                VersionMappingEntry entry;
                entry.versionPattern = std::move(versionPattern);

                // Process all project ID mappings for this version
                for (const auto& mapping : mappings)
                {
                    if (!mapping.contains("projectID") || !mapping.contains("cmsID"))
                    {
                        SL_LOG_WARN("Bad mapping entry found for engine %s, version %s: mapping %s", engineTypeStr.c_str(), engineVersion.c_str(), mapping.dump().c_str());
                        continue;
                    }

                    std::string projectID = mapping["projectID"].get<std::string>();
                    // Remove all whitespace and curly braces
                    projectID.erase(std::remove_if(projectID.begin(),
                                                   projectID.end(),
                                                   [](unsigned char c)
                                                   { return std::isspace(c) || c == '{' || c == '}'; }),
                                    projectID.end());

                    projectID = extra::toUpper(projectID);

                    // Extract the specific CMS ID for this project mapping
                    uint64_t cmsId = ExtractCMSId(mapping["cmsID"].get<std::string>());
                    uint32_t appId = hashCmsId(static_cast<uint32_t>(cmsId));

                    if (entry.projectToAppId.find(projectID) != entry.projectToAppId.end())
                    {
                        SL_LOG_WARN("Duplicate project ID %s for engine %s version %s found, using first mapping",
                            projectID.c_str(), engineTypeStr.c_str(), engineVersion.c_str());
                        continue;
                    }
                    entry.projectToAppId[projectID] = appId;

                    SL_LOG_VERBOSE("Mapped Engine %s v%s Project ID %s to App ID 0x%X (from CMS ID 0x%llX)",
                                   engineTypeStr.c_str(),
                                   engineVersion.c_str(),
                                   projectID.c_str(),
                                   appId,
                                   cmsId);
                }

                m_projectToAppIds[engineType].push_back(std::move(entry));
            }
        }
    }
    catch (const json::exception& e)
    {
        SL_LOG_ERROR("Failed to parse JSON mapping file: %s", e.what());
        return false;
    }

    return true;
}

std::optional<uint32_t> OTA::appIdForProjectId(EngineType engineType, const std::string& engineVersion, const std::string& projectId) const
{
    std::string projectIdUpper = extra::toUpper(projectId);

    auto engineIt = m_projectToAppIds.find(engineType);
    if (engineIt == m_projectToAppIds.end())
    {
        return std::nullopt;
    }

    for (const auto& entry : engineIt->second)
    {
        if (!std::regex_match(engineVersion, entry.versionPattern))
            continue;

        auto projectIt = entry.projectToAppId.find(projectIdUpper);
        if (projectIt != entry.projectToAppId.end())
            return projectIt->second;
        // Pattern matched but projectId not in this entry's map; continue to next entry (no generic fallback)
    }

    return std::nullopt;
}

bool OTA::parseServerDenylist(std::ifstream& denylist)
{
    DWORD denylistDisabled = 0;
    if (extra::getRegistryDword(L"SOFTWARE\\NVIDIA Corporation\\Global\\Streamline", L"DisableOTADenylist", &denylistDisabled))
    {
        SL_LOG_INFO("Read DisableOTADenylist: %d from registry", denylistDisabled);
        if (denylistDisabled == 1)
        {
            SL_LOG_INFO("OTA Denylist disabled");
            return true;
        }
    }

    m_deniedAppIds.clear();

    std::string line;
    while (std::getline(denylist, line))
    {
        // Search for the streamline section
        auto i = line.find("[streamline-ota]");
        if (i != std::string::npos)
        {
            uint32_t appId;
            int denyState;
            std::getline(denylist, line);
            while (!line.empty() && line[0] != '[' && line[line.length() - 1] != ']')
            {
                if (sscanf_s(line.c_str(), "app_%x = %d", &appId, &denyState) == 2 && denyState == 1)
                {
                    SL_LOG_VERBOSE("Hashed CMSID %lX is on the Streamline OTA denylist", appId);
                    m_deniedAppIds.insert(appId);
                }
                line.clear();
                std::getline(denylist, line);
            }
        }
    }
    return true;
}

bool OTA::isAppIdDenied(const uint32_t appId) const
{
    return m_deniedAppIds.find(hashCmsId(appId)) != m_deniedAppIds.end();
}

uint32_t OTA::hashCmsId(const uint32_t cmsid) const
{
    return cmsid ^ 0xE658703LLU;
}

bool OTA::readOTAOverrideDRS()
{
    // TODO: refactor DRS code to deduplicate the code currently in drs.cpp in sl.common and use
    // that here.
    NvDRSSessionHandle drsSession = 0;
    NvDRSProfileHandle baseProfile = 0;
    NvDRSProfileHandle appProfile = 0;

    if (NvAPI_DRS_CreateSession(&drsSession) != NVAPI_OK)
    {
        return false;
    }

    if (NvAPI_DRS_LoadSettings(drsSession) != NVAPI_OK)
    {
        NvAPI_DRS_DestroySession(drsSession);
        return false;
    }

    if (NvAPI_DRS_GetBaseProfile(drsSession, &baseProfile) != NVAPI_OK || !baseProfile)
    {
        NvAPI_DRS_DestroySession(drsSession);
        return false;
    }

    std::wstring wAppName = sl::file::getFullPathOfExecutable();
    std::vector<NvU16> appName{ wAppName.begin(), wAppName.end() };
    appName.push_back(0); // add null terminator
    NVDRS_APPLICATION application;
    application.version = NVDRS_APPLICATION_VER;

    if (NvAPI_DRS_FindApplicationByName(drsSession, appName.data(), &appProfile, &application) == NVAPI_OK)
    {
        NVDRS_SETTING setting = {};
        setting.version = NVDRS_SETTING_VER;
        bool result = false;

        // Only return a result if this is defined in the application profile. If it is not, then we should
        // still check the base profile and return that if it's set there.
        if (NvAPI_DRS_GetSetting(drsSession, appProfile, SL_DLSS_OVERRIDE_ID, &setting) == NVAPI_OK)
        {
            result = (setting.u32CurrentValue == SL_DLSS_OVERRIDE_ON);
            SL_LOG_INFO("Read SL_DLSS_OVERRIDE DRS key (app profile): %u", setting.u32CurrentValue);
            NvAPI_DRS_DestroySession(drsSession);
            return result;
        }
    }

    NVDRS_SETTING setting = {};
    setting.version = NVDRS_SETTING_VER;
    bool result = false;
    if (NvAPI_DRS_GetSetting(drsSession, baseProfile, SL_DLSS_OVERRIDE_ID, &setting) == NVAPI_OK)
    {
        result = (setting.u32CurrentValue == SL_DLSS_OVERRIDE_ON);
        SL_LOG_INFO("Read SL_DLSS_OVERRIDE DRS key (global profile): %u", setting.u32CurrentValue);
    }

    NvAPI_DRS_DestroySession(drsSession);
    return result;
}

bool OTA::isOTAOverrideEnabled()
{
    if (!m_otaOverrideEnabled.has_value())
    {
        m_otaOverrideEnabled = readOTAOverrideDRS();
        if (m_otaOverrideEnabled.value())
        {
            SL_LOG_INFO("OTA override enabled via DRS key, will use CMSID 0x%X", hashCmsId(kOverrideCmsId));
        }
    }
    return m_otaOverrideEnabled.value();
}

bool OTA::readServerManifest()
{
    bool succeeded = false;
    std::filesystem::path ngxPath;
    if (!getNGXPath(ngxPath))
    {
        SL_LOG_ERROR("Failed to read server manifest, couldn't get NGX Cache Path");
        return succeeded;
    }
    std::ifstream manifestFile((ngxPath / L"nvngx_config.txt"));
    if (!manifestFile.is_open() || (manifestFile.rdstate() != std::ios_base::goodbit))
    {
        SL_LOG_WARN("Failed to open manifest file at: %lsnvngx_config.txt", ngxPath.c_str());
        return succeeded;
    }
    bool ok = parseServerManifest(manifestFile, m_versions, m_optionalDownloadPresent, false);

    if (isOTAOverrideEnabled())
    {
        manifestFile.clear();
        manifestFile.seekg(0);
        ok = parseServerManifest(manifestFile, m_overrideVersions, m_overrideOptionalDownloadPresent, true) && ok;
    }

    return ok;
}

bool OTA::readServerDenylist()
{
    bool succeeded = false;
    std::filesystem::path ngxPath;
    if (!getNGXPath(ngxPath))
    {
        SL_LOG_ERROR("Failed to read server denylist, couldn't get NGX Cache Path");
        return succeeded;
    }

    std::filesystem::path denylistFilePath = ngxPath / L"config" / L"versions" / L"1" / L"files" / L"nvngx_deny_list.txt";
    std::ifstream denylistFile(denylistFilePath);
    if (!denylistFile.is_open() || (denylistFile.rdstate() != std::ios_base::goodbit))
    {
        SL_LOG_WARN("Failed to open denylist file at: %ls", denylistFilePath.c_str());
        return succeeded;
    }
    else
    {
        SL_LOG_INFO("Reading SL denylist file at: %ls", denylistFilePath.c_str());
    }

    return parseServerDenylist(denylistFile);
}

bool OTA::readServerMappings()
{
    bool succeeded = false;
    std::filesystem::path ngxPath;
    if (!getNGXPath(ngxPath))
    {
        SL_LOG_ERROR("Failed to read server mapping, couldn't get NGX Cache Path");
        return succeeded;
    }

    std::filesystem::path mappingFilePath =
        ngxPath / L"config" / L"versions" / L"1" / L"files" / L"nvngx_mapping.json";
    std::ifstream mappingFile(mappingFilePath);
    if (!mappingFile.is_open() || (mappingFile.rdstate() != std::ios_base::goodbit))
    {
        SL_LOG_WARN("Failed to open mapping file at: %ls", mappingFilePath.c_str());
        return succeeded;
    }
    else
    {
        SL_LOG_INFO("Reading SL mapping file at: %ls", mappingFilePath.c_str());
    }

    return parseMappingFile(mappingFile);
}

uint32_t OTA::getNVDAVersion()
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

uint32_t OTA::getNVDAArchitecture()
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

bool OTA::checkForOTA(const Version &apiVersion, bool requestOptionalUpdates)
{
    uint32_t gpuArch = getNVDAArchitecture();

    if (!gpuArch)
    {
        SL_LOG_VERBOSE("OTA only enabled with NVIDIA GPUs in the system");
        return false;
    }

    bool overrideEnabled = isOTAOverrideEnabled();
    uint32_t cmsid = overrideEnabled ? kOverrideCmsId : kDefaultCmsId;
    std::string name_version  = overrideEnabled ? extra::format("sdk_override_{}", apiVersion.major) : extra::format("sdk_{}", apiVersion.major);
    std::string cmsidArg = std::to_string(cmsid);

    uint32_t driverVersion = getNVDAVersion();

    // Starting with the 565 driver series, the NGX Updater supports the
    // -bootstrap flag on the -update call, allowing us to combine bootstrap
    // and update into a single invocation.
    constexpr uint32_t kMinDriverVersionForBootstrapFlag = 56500;
    bool canUseBootstrapFlag = (driverVersion >= kMinDriverVersionForBootstrapFlag);

    if (!canUseBootstrapFlag && m_versions.find(name_version) == m_versions.end())
    {
        // Bootstrap the SDK bundle first since it is not in the OTA manifest
        // (legacy path for drivers older than the 565 series)
        std::string args = "-cmsid " + cmsidArg + " -feature sl_" + name_version + " -api bootstrap";
        if (!invokeNGXUpdater(extra::toWStr(args)))
        {
            return false;
        }
    }

    // Now let's check for updates
    {
        std::string args = "-cmsid " + cmsidArg + " -feature sl_" + name_version + " -api update -type zip -gpuarch 0x" + extra::toHexStr<uint32_t>(gpuArch, 3);

        if (canUseBootstrapFlag)
        {
            args += " -bootstrap";
        }

        if (requestOptionalUpdates)
        {
            // The NGX Updater is pedantic about its command-line input and will
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
                args += " -optional";
                SL_LOG_INFO("Requesting optional updates!");
            }
            else
            {
                uint32_t verMaj = driverVersion / 100;
                uint32_t verMin = driverVersion % 100;
                SL_LOG_WARN("Optional updates requested but your driver version %d.%d is too old!", verMaj, verMin);
            }
        }

        if (!invokeNGXUpdater(extra::toWStr(args)))
        {
            return false;
        }
    }
    return true;
}

bool OTA::getOTAPluginForFeature(Feature featureID, const Version &apiVersion, std::filesystem::path &filePath, bool loadOptionalUpdates, bool useOverride)
{
    // First get GPU Architecture, needed to download appropriate OTA
    // snippet
    uint32_t gpuArch = getNVDAArchitecture();
    if (!gpuArch)
    {
        SL_LOG_VERBOSE("OTA only enabled with NVIDIA GPUs in the system");
        return false;
    }

    std::filesystem::path ngxPath;
    if (!getNGXPath(ngxPath))
    {
        SL_LOG_ERROR("Failed to read server manifest, couldn't get NGX Cache Path");
        return false;
    }

    // Construct the name_version pair for this feature
    Version otaVersion = {};
    std::string name_version = getFeatureFilenameAsStrNoSL(featureID);
    if (useOverride)
    {
        name_version += extra::format("_override_{}", apiVersion.major);
        SL_LOG_INFO("OTA override is enabled, using override feature name: %s", name_version.c_str());
    }
    else
    {
        name_version += extra::format("_{}", apiVersion.major);
    }

    // Select the version map and optional list based on override mode
    auto& versions = useOverride ? m_overrideVersions : m_versions;
    auto& optionals = useOverride ? m_overrideOptionalDownloadPresent : m_optionalDownloadPresent;

    // Find the corresponding section in versions
    auto it = versions.find(name_version);
    if (it == versions.end())
    {
        SL_LOG_WARN("Could not find version matching for plugin: %s", name_version.c_str());
        return false;
    }
    else
    {
        otaVersion = it->second;
    }

    // If optional updates are not allowed, check if this feature is
    // optional and skip it
    if (!loadOptionalUpdates)
    {
        uint32_t driverVersion = getNVDAVersion();

        // Support for optional update tracking was added in R580 and later.
        // If we are on a driver prior to that we cannot infer whether a
        // downloaded feature was optional or mandatory and thus need to
        // assume it was optional and not load it.
        if (driverVersion < 58000)
        {
            SL_LOG_INFO("eLoadDownloadedPlugins flag not passed to preferences, unable to infer if %s plugin is optional due to driver version. Skipping!", name_version);
            return false;
        }
        for (const auto &entry: optionals)
        {
            if (entry == name_version)
            {
                // Version is marked as optional
                SL_LOG_INFO("eLoadDownloadedPlugins flag not passed to preferences, optional %s plugin will not be loaded!", name_version);
                return false;
            }
        }
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
    uint32_t pluginCmsid = useOverride ? kOverrideCmsId : kDefaultCmsId;
    std::wstring hashedCmsid = extra::toWStr("_" + cmsIdToHexStr(hashCmsId(pluginCmsid)));
    std::filesystem::path pluginPath = ngxPath /
                                (L"sl_" + extra::toWStr(name_version)) /
                                L"versions" /
                                otaVersionString /
                                L"files" /
                                (extra::toWStr(extra::toHexStr<uint32_t>(gpuArch, 3)) + hashedCmsid + L".dll");

    // Check if exists
    if (!fs::exists(pluginPath))
    {
        SL_LOG_ERROR("Found non-zero plugin \"%s\" in NGX Cache but missing file: %ls", name_version.c_str(), pluginPath.c_str());
        return false;
    }

    filePath = pluginPath;
    return true;
}

}  // namespace sl::ota

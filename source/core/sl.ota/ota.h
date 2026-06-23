/*
 * Copyright (c) 2022-2025 NVIDIA CORPORATION. All rights reserved
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

#include "source/core/sl.ota/iota.h"

#include <filesystem>
#include <optional>
#include <regex>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace sl::ota
{

// One version-pattern entry per engine: versionPattern is std::regex (ECMAScript grammar).
// In the pattern, '.' matches any character; '*' and '?' are regex quantifiers (not glob wildcards).
// List order matches JSON list-items order; first matching pattern wins at lookup.
struct VersionMappingEntry
{
    std::regex versionPattern;
    std::unordered_map<std::string, uint32_t> projectToAppId;
};

class OTA : public IOTA
{
  public:
    bool readServerManifest() override;
    bool checkForOTA(const Version& apiVersion, bool requestOptionalUpdates) override;
    bool getOTAPluginForFeature(Feature featureID,
                                const Version& apiVersion,
                                std::filesystem::path& filePath,
                                bool loadOptionalUpdates,
                                bool useOverride) override;
    bool isOTAOverrideEnabled() override;
    bool getNGXPath(std::filesystem::path& ngxPath) const override;
    bool readServerDenylist() override;
    bool readServerMappings() override;
    bool isAppIdDenied(const uint32_t appId) const override;
    std::optional<uint32_t> appIdForProjectId(EngineType engineType, const std::string& engineVersion, const std::string& projectId) const override;

  protected:
    void setNGXPathOverride(const std::optional<std::filesystem::path> ngxPath) override;
    void setOTAOverrideEnabled(std::optional<bool> enabled) override;
    bool parseServerManifest(std::ifstream& manifest,
                             std::map<std::string, Version>& versionMap,
                             std::vector<std::string>& optionalDownloadPresent,
                             bool useOverride) override;
    bool parseServerDenylist(std::ifstream& denylist) override;
    bool parseMappingFile(std::ifstream& file) override;

  private:
    static constexpr uint32_t SL_DLSS_OVERRIDE_ID = 0x10e41e06;
    static constexpr uint32_t SL_DLSS_OVERRIDE_OFF = 0;
    static constexpr uint32_t SL_DLSS_OVERRIDE_ON = 1;

    static constexpr uint32_t kDefaultCmsId = 0;
    static constexpr uint32_t kOverrideCmsId = 3;

    bool invokeNGXUpdater(const std::wstring& args);
    bool getDriverPath(std::filesystem::path& driverPath) const;
    bool getSlInterposerPath(std::filesystem::path& slPath) const;
    bool findNGXUpdater();
    uint32_t hashCmsId(const uint32_t cmsid) const;

    uint32_t getNVDAVersion();
    uint32_t getNVDAArchitecture();

    bool readOTAOverrideDRS();

    bool m_enable = true;
    std::optional<bool> m_foundUpdater = std::nullopt;
    std::optional<bool> m_otaOverrideEnabled = std::nullopt;

    std::filesystem::path m_updaterExe;

    std::optional<std::filesystem::path> m_ngxPathOverride;

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
    std::vector<std::string> m_optionalDownloadPresent;
    std::map<std::string, Version> m_overrideVersions;
    std::vector<std::string> m_overrideOptionalDownloadPresent;
    std::unordered_set<uint32_t> m_deniedAppIds;
    // Per engine type: ordered list of (version pattern, projectId -> appId). First regex match wins.
    std::map<EngineType, std::vector<VersionMappingEntry>> m_projectToAppIds;
};

}  // namespace sl::ota

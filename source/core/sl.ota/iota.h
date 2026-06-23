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

#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <vector>

#include "include/sl_version.h"
#include "include/sl_appidentity.h"

namespace sl
{

using Feature = uint32_t;

#if defined(SL_UNITTEST_ONLY_CODE)
// Forward declarations for friend access to protected setNGXPathOverride
namespace api { class Context; }
namespace plugin
{
bool onLoad(api::Context* ctx, const char* loaderJSON, const char* embeddedJSON);
}
void slSetOTAPath(const wchar_t* otaPath);

namespace test
{
class SlOtaParserUnitTest;
class SlPluginManagerUnitTest;
class SlPluginDenylistUnitTest;
class SlPluginManagerOTAOverrideTest;
}
#endif

namespace ota
{
class IOTA
{
  public:
    //! Reads manifest downloaded from the server
    //! and collects information about plugins
    //! which have an OTA available
    virtual bool readServerManifest() = 0;

    //! Reads denylist downloaded from the server
    virtual bool readServerDenylist() = 0;

    //! Reads Project ID to Application ID mappings from server
    virtual bool readServerMappings() = 0;

    //! Checks whether the current application ID is on the server
    //! denylist.
    virtual bool isAppIdDenied(const uint32_t appId) const = 0;

    //! Returns an Application ID for a given Engine Type, Engine Version, and Project ID
    //! from the server mapping list.
    //! Returns the app ID if found, std::nullopt if not found.
    virtual std::optional<uint32_t> appIdForProjectId(EngineType engineType, const std::string& engineVersion, const std::string& projectId) const = 0;

    //! Pings server and downloads OTA config file then
    //! compares it to the local version (if any) and downloads
    //! new plugins if there is an update on the server
    virtual bool checkForOTA(const Version& apiVersion, bool requestOptionalUpdates) = 0;

    //! Fetches the path to the latest plugin matching the feature ID + API
    //! Version combination.
    //!
    //! On success filePath will be populated with the path to the suitable
    //! plugin file.
    //!
    //! Return values:
    //!   TRUE - a suitable plugin was found
    //!   FALSE - otherwise
    virtual bool getOTAPluginForFeature(Feature featureID,
                                        const Version& apiVersion,
                                        std::filesystem::path& filePath,
                                        bool loadOptionalUpdates,
                                        bool useOverride) = 0;

    //! Returns true if the SL_OTA_OVERRIDE_ENABLE DRS key is set to TRUE,
    //! indicating that OTA plugin loading should be forced on regardless of
    //! whether eLoadDownloadedPlugins was passed in preferences.
    virtual bool isOTAOverrideEnabled() = 0;

    //! Fetches the path to the OTA cache on the local filesystem.
    //!
    //! If a cache path override is in place, then this returns the overridden path.
    virtual bool getNGXPath(std::filesystem::path& ngxPath) const = 0;

  protected:
    virtual void setNGXPathOverride(const std::optional<std::filesystem::path> ngxPath) = 0;
    virtual void setOTAOverrideEnabled(std::optional<bool> enabled) = 0;

    virtual bool parseServerManifest(std::ifstream& manifest,
                                     std::map<std::string, Version>& versionMap,
                                     std::vector<std::string>& optionalDownloadPresent,
                                     bool useOverride) = 0;
    virtual bool parseServerDenylist(std::ifstream& denylist) = 0;
    virtual bool parseMappingFile(std::ifstream& file) = 0;

#if defined(SL_UNITTEST_ONLY_CODE)
    friend bool sl::plugin::onLoad(api::Context* ctx, const char* loaderJSON, const char* embeddedJSON);
    friend void sl::slSetOTAPath(const wchar_t* otaPath);
    friend class sl::test::SlOtaParserUnitTest;
    friend class sl::test::SlPluginManagerOTAOverrideTest;
    friend class sl::test::SlPluginManagerUnitTest;
    friend class sl::test::SlPluginDenylistUnitTest;
#endif
};

IOTA* getInterface();

}  // namespace ota
}  // namespace sl

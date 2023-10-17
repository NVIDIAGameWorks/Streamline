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
#include <versionhelpers.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#endif

#include <sstream>
#include <random>

#include "include/sl_hooks.h"
#include "include/sl_version.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.param/parameters.h"
#include "source/core/sl.plugin-manager/ota.h"
#include "source/core/sl.plugin-manager/pluginManager.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "source/core/sl.interposer/versions.h"
#include "source/core/sl.interposer/hook.h"
#include "source/plugins/sl.imgui/imgui.h"
#include "_artifacts/gitVersion.h"
#include "include/sl_helpers.h"

namespace sl
{

namespace plugin_manager
{

enum class PluginManagerStatus
{
    eUnknown,
    ePluginsLoaded,
    ePluginsInitialized,
    ePluginsUnloaded
};

class PluginManager : public IPluginManager
{
public:

    PluginManager();
    
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    virtual Result loadPlugins() override final;
    virtual void unloadPlugins() override final;
    virtual Result initializePlugins() override final;

    virtual const HookList& getBeforeHooks(FunctionHookID functionHookID) override final;
    virtual const HookList& getAfterHooks(FunctionHookID functionHookID) override final;
    virtual const HookList& getBeforeHooksWithoutLazyInit(FunctionHookID functionHookID) override final;
    virtual const HookList& getAfterHooksWithoutLazyInit(FunctionHookID functionHookID) override final;
    
    virtual Result setHostSDKVersion(uint64_t sdkVersion) override final
    {
        // SL version is 64bit split in four 16bit values
        // 
        // major | minor | patch | magic
        //
        if ((sdkVersion & kSDKVersionMagic) == kSDKVersionMagic)
        {
            m_hostSDKVersion = { (sdkVersion >> 48) & 0xffff, (sdkVersion >> 32) & 0xffff, (sdkVersion >> 16) & 0xffff };
        }
        else
        {
            // Legacy titles use redirection which reports SL 1.5.0, this must be a genuine integration bug
            m_hostSDKVersion = { 2, 0, 0 };
            SL_LOG_ERROR("Invalid host SDK version detected - did you forget to pass in 'kSDKVersion' on slInit?");
            return Result::eErrorInvalidParameter;
        }

        SL_LOG_INFO("Streamline v%u.%u.%u.%s - built on %s - host SDK v%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, GIT_LAST_COMMIT_SHORT, __TIMESTAMP__, m_hostSDKVersion.toStr().c_str());
        return Result::eOk;
    }

    virtual const Version& getHostSDKVersion() override final
    {
        return m_hostSDKVersion;
    }

    virtual const Preferences& getPreferences() const override final { return m_pref; }

    virtual void setPreferences(const Preferences& pref) override final
    {
        m_pref = pref;
        param::getInterface()->set(param::global::kPreferenceFlags, (uint64_t)m_pref.flags);

        // Keep a copy since we need it later so host does not have keep these allocations around
        m_pathsToPlugins.resize(pref.numPathsToPlugins);
        for (uint32_t i = 0; i < pref.numPathsToPlugins; i++)
        {
            m_pathsToPlugins[i] = m_pref.pathsToPlugins[i];
        }
        
        // Allow override for features to load
#ifndef SL_PRODUCTION
        auto interposerConfig = sl::interposer::getInterface()->getConfig();

        // Build inclusion list based on preferences
        std::vector<Feature> features =
        {
            kFeatureDLSS,
            kFeatureNRD,
            kFeatureNIS,
            kFeatureReflex,
            kFeatureDLSS_G,
            kFeatureImGUI,
            kFeatureDLSS_RR,
        };

        // Allow override via JSON config file
        if (interposerConfig.loadAllFeatures)
        {
            SL_LOG_HINT("Loading all features");
            m_featuresToLoad = features;
        }
        if (!interposerConfig.loadSpecificFeatures.empty() && m_featuresToLoad.empty())
        {
            for (auto& id : interposerConfig.loadSpecificFeatures) try
            {
                if (std::find(features.begin(), features.end(), id) == features.end())
                {
                    SL_LOG_WARN("Feature '%s' in 'loadSpecificFeatures' list is invalid - ignoring", getFeatureAsStr((Feature)id));
                }
                else
                {
                    m_featuresToLoad.push_back((Feature)id);
                }
            }
            catch (std::exception& e)
            {
                SL_LOG_ERROR( "Failed to parse JSON file - %s", e.what());
            }
        }
#endif

        // This could be already populated from JSON config in development builds
        if (m_featuresToLoad.empty())
        {
            m_featuresToLoad.resize(pref.numFeaturesToLoad);
            for (uint32_t i = 0; i < pref.numFeaturesToLoad; i++)
            {
                m_featuresToLoad[i] = pref.featuresToLoad[i];
            }
        }

        if (m_featuresToLoad.empty())
        {
            SL_LOG_WARN("No features will be loaded - the explicit list of features to load must be specified in sl::Preferences or provided with 'sl.interposer.json' in development builds");
        }

        // sl.common is always enabled
        m_featuresToLoad.push_back(kFeatureCommon);

        // These are not safe to touch after we exit here
        m_pref.pathsToPlugins = {};
        m_pref.pathToLogsAndData = {};

        m_appId = pref.applicationId ? pref.applicationId : kTemporaryAppId;
        m_engine = pref.engine;
        m_engineVersion = pref.engineVersion ? pref.engineVersion : "";
        m_projectId = pref.projectId ? pref.projectId : "";
    }

    virtual void setApplicationId(int appId)  override final { m_appId = appId; }
    virtual void setD3D12Device(ID3D12Device* device)  override final { m_d3d12Device = device; }
    virtual void setD3D11Device(ID3D11Device* device)  override final { m_d3d11Device = device; }
    virtual void setVulkanDevice(VkPhysicalDevice physicalDevice, VkDevice device, VkInstance instance) override final
    {
        m_vkPhysicalDevice = physicalDevice;
        m_vkDevice = device;
        m_vkInstance = instance;
    }
    
    virtual Result setFeatureEnabled(Feature feature, bool value) override final;

    virtual ID3D12Device* getD3D12Device() const override final { return m_d3d12Device; }
    virtual ID3D11Device* getD3D11Device() const override final { return m_d3d11Device; }
    virtual VkDevice getVulkanDevice() const override final { return m_vkDevice; }
    virtual bool isProxyNeeded(const char* className) override final;
    virtual bool isInitialized() const  override final { return s_status == PluginManagerStatus::ePluginsInitialized; }
    virtual bool arePluginsLoaded() const  override final { return s_status == PluginManagerStatus::ePluginsInitialized || s_status == PluginManagerStatus::ePluginsLoaded; }

    virtual const FeatureContext* getFeatureContext(Feature feature) override final
    {
        auto it = m_featurePluginsMap.find(feature);
        if (it != m_featurePluginsMap.end())
        {
            return &(*it).second->context;
        }
        return nullptr;
    }

    virtual bool getExternalFeatureConfig(Feature feature, const char** configAsText) override final
    {
        std::scoped_lock lock(m_mtxPluginConfig);

        *configAsText = nullptr;
        auto it = m_featureExternalConfigMap.find(feature);
        if (it != m_featureExternalConfigMap.end())
        {
            auto config = (*it).second;
            if (m_externalJSONConfigs.find(feature) == m_externalJSONConfigs.end())
            {
                m_externalJSONConfigs[feature] = new std::string;
            }
            std::string* str = m_externalJSONConfigs[feature];
            *str = config.dump();
            *configAsText = str->c_str();
            return true;
        }
        return false;
    }

    virtual bool getLoadedFeatureConfigs(std::vector<json>& configList) const override final
    {
        for (auto plugin : m_plugins)
        {
            configList.push_back(plugin->config);
        }
        return !configList.empty();
    }

    virtual bool getLoadedFeatures(std::vector<Feature>& featureList) const override final
    {
        for (auto plugin : m_plugins)
        {
            featureList.push_back(plugin->id);
        }
        return !featureList.empty();
    }

    void populateLoaderJSON(uint32_t deviceType, json& config);

    std::mutex m_mtxPluginConfig;

    inline static PluginManager* s_manager = {};

private:

    Result mapPlugins(std::vector<std::wstring>& files);
    Result findPlugins(const std::wstring& path, std::vector<std::wstring>& files);
    

    struct Plugin
    {
        Plugin() {}
        Plugin(const Plugin& rhs) = delete;
        Plugin& operator=(const Plugin& rhs) = delete;

        Feature id{};
        std::string sha{};
        int priority{};
        Version version{};
        Version api{};
        HMODULE lib{};
        json config{};
        std::string name{};
        fs::path filename{};
        fs::path fullpath{};
        std::string paramNamespace{};
        api::PFuncOnPluginStartup* onStartup{};
        api::PFuncOnPluginShutdown* onShutdown{};
        api::PFuncGetPluginFunction* getFunction{};
        api::PFuncOnPluginLoad* onLoad{};
        std::vector<std::string> requiredPlugins;
        std::vector<std::string> exclusiveHooks;
        std::vector<std::string> incompatiblePlugins;
        FeatureContext context{};
    };

    bool loadPlugin(const fs::path path, Plugin **ppPlugin);
    void processPluginHooks(const Plugin* plugin);
    void mapPluginCallbacks(Plugin* plugin);
    uint32_t getFunctionHookID(const std::string& name);

    Plugin* isPluginLoaded(const std::string& name) const;
    Plugin* isExclusiveHookUsed(const Plugin* exclusivePlugin, const std::string& exclusiveHook) const;

    Version m_hostSDKVersion{};

    Version m_version = { 0,0,1 };
    Version m_api = { 0,0,1 };

    HookList m_beforeHooks[(uint32_t)FunctionHookID::eMaxNum];
    HookList m_afterHooks[(uint32_t)FunctionHookID::eMaxNum];

    ID3D12Device* m_d3d12Device = {};
    ID3D11Device* m_d3d11Device = {};
    VkPhysicalDevice m_vkPhysicalDevice = {};
    VkDevice m_vkDevice = {};
    VkInstance m_vkInstance = {};

    std::unordered_map<std::string, FunctionHookID> m_functionHookIDMap;

    using PluginList = std::vector<Plugin*>;
    using PluginMap = std::map<Feature,Plugin*>;
    using ConfigMap = std::map<Feature, json>;

    PluginList m_plugins;
    PluginMap m_featurePluginsMap;
    ConfigMap m_featureExternalConfigMap;

    int m_appId = 0;
    inline static PluginManagerStatus s_status = PluginManagerStatus::eUnknown;
    
    EngineType m_engine = EngineType::eCustom;
    std::string m_engineVersion{};
    std::string m_projectId{};

    std::wstring m_pluginPath{};
    std::vector<std::wstring> m_pathsToPlugins{};
    std::vector<Feature> m_featuresToLoad{};
    std::map<Feature, std::string*> m_externalJSONConfigs{};

    Preferences m_pref{};

    sl::ota::IOTA* m_ota{};
};

IPluginManager* getInterface()
{
    if (!PluginManager::s_manager)
    {
        PluginManager::s_manager = new PluginManager();
    }
    return PluginManager::s_manager;
}

void destroyInterface()
{
    delete PluginManager::s_manager;
    PluginManager::s_manager = {};
}

PluginManager::Plugin* PluginManager::isPluginLoaded(const std::string& name) const
{
    for (auto& plugin : m_plugins)
    {
        if (plugin->name == name)
        {
            return plugin;
        }
    }
    return nullptr;
}

PluginManager::Plugin* PluginManager::isExclusiveHookUsed(const Plugin* exclusivePlugin, const std::string& exclusiveHook) const
{
    for (auto& plugin : m_plugins)
    {
        if (plugin == exclusivePlugin) continue;

        auto hooks = plugin->config.at("hooks");
        for (auto hook : hooks)
        {
            std::string cls, target;
            hook.at("class").get_to(cls);
            hook.at("target").get_to(target);
            auto fullName = cls + "_" + target;
            if (fullName == exclusiveHook) return plugin;
        }
    }
    return nullptr;
}

bool PluginManager::isProxyNeeded(const char* className)
{
    for (auto plugin : m_plugins)
    {
        auto hooks = plugin->config.at("hooks");
        for (auto hook : hooks)
        {
            std::string cls;
            hook.at("class").get_to(cls);
            if (cls == className) return true;
        }
    }
    return false;
}

Result PluginManager::setFeatureEnabled(Feature feature, bool value)
{
    // This is clearly not thread safe so the assumption is that
    // host will not invoke any hooks while we are running as it is
    // documented in the programming guide.

    auto it = m_featurePluginsMap.find(feature);
    if (it == m_featurePluginsMap.end())
    {
        SL_LOG_WARN("Feature '%s' not loaded", getFeatureAsStr(feature));
        return Result::eErrorFeatureFailedToLoad;
    }
    auto& ctx = (*it).second->context;
    if (!ctx.supportedAdapters)
    {
        SL_LOG_WARN("Feature '%s' not supported on any available adapter", getFeatureAsStr(feature));
        return Result::eErrorNoSupportedAdapterFound;
    }
    if (ctx.enabled == value)
    {
        SL_LOG_VERBOSE("Feature '%s' is already in the requested 'loaded' state", getFeatureAsStr(feature));
        return Result::eOk;
    }
    ctx.enabled = value;
    SL_LOG_INFO("Feature '%s' %s", getFeatureAsStr(feature), value ? "loaded" : "unloaded");
    auto hooks = (*it).second->config.at("hooks");
    if (!hooks.empty())
    {
        // Plugin has registered hooks, need to redo our prioritized hook lists
        // 
        // We do this to minimize the CPU overhead when accessing hooks i.e. 
        // we could leave the lists intact and check for each hook if plugin 
        // is enabled or not but that is very expensive when hooks are accessed 
        // hundreds of times per frame.

        for (auto& hooks : m_afterHooks)
        {
            hooks.clear();
        }
        for (auto& hooks : m_beforeHooks)
        {
            hooks.clear();
        }

        // Sorted by priority so processing hooks by priority
        for (auto plugin : m_plugins)
        {
            processPluginHooks(plugin);
        }
    }
    return Result::eOk;
}

Result PluginManager::findPlugins(const std::wstring& directory, std::vector<std::wstring>& files)
{
#ifdef SL_WINDOWS
    std::wstring dynamicLibraryExt = L".dll";
#else
    std::wstring dynamicLibraryExt = L".so";
#endif

    SL_LOG_INFO("Looking for plugins in %S ...", directory.c_str());
    try
    {
        for (auto const& entry : fs::directory_iterator{ directory })
        {
            auto ext = entry.path().extension().wstring();
            auto name = entry.path().filename().wstring();
            name = name.erase(name.find(ext));
            // Make sure this is a dynamic library and it starts with "sl." but ignore "sl.interposer.dll"
            if (ext == dynamicLibraryExt && name.find(L"sl.") == 0 && name.find(L"sl.interposer") == std::string::npos)
            {
                files.push_back(directory + L"/" + name + ext);
            }
        }
    }
    catch (std::exception& e)
    {
        SL_LOG_ERROR( "Failed while looking for plugins - error %s", e.what());
    }
    return files.empty() ? Result::eErrorNoPlugins : Result::eOk;
}

bool PluginManager::loadPlugin(const fs::path pluginFullPath, Plugin **ppPlugin)
{
    auto freePlugin = [](Plugin** plugin)->void
    {
        FreeLibrary((*plugin)->lib);
        delete* plugin;
        *plugin = nullptr;
    };

    HMODULE mod = security::loadLibrary(pluginFullPath.c_str());
    if (!mod)
    {
        return false;
    }

    Plugin *plugin = new Plugin();
    plugin->fullpath = pluginFullPath;
    plugin->filename = pluginFullPath.stem();
    plugin->lib = mod;
    plugin->getFunction = reinterpret_cast<api::PFuncGetPluginFunction*>(GetProcAddress(mod, "slGetPluginFunction"));
    if (plugin->getFunction)
    {
        plugin->onLoad = reinterpret_cast<api::PFuncOnPluginLoad*>(plugin->getFunction("slOnPluginLoad"));
    }
    if (!plugin->getFunction || !plugin->onLoad)
    {
        SL_LOG_ERROR( "Ignoring '%ls' since it does not contain proper API", plugin->filename.wstring().c_str());
        freePlugin(&plugin);
        return false;
    }

    plugin->context.getFunction = plugin->getFunction;
    plugin->context.isSupported = reinterpret_cast<PFun_slIsSupported*>(plugin->getFunction("slIsSupported"));

    param::IParameters* parameters = param::getInterface();
    try
    {
        // Let's get JSON config from our plugin

        json loaderJSON;
        // Here we do not know device type yet so just pass invalid id
        populateLoaderJSON((uint32_t)m_pref.renderAPI, loaderJSON);
        auto loaderJSONStr = loaderJSON.dump();
        const char* pluginJSONText{};
        if (!plugin->onLoad(parameters, loaderJSONStr.c_str(), &pluginJSONText))
        {
            SL_LOG_ERROR( "Ignoring '%ls' since core API 'onPluginLoad' failed", plugin->filename.wstring().c_str());
            freePlugin(&plugin);
            return false;
        }

        // pluginJSONText allocation freed by plugin
        std::istringstream stream(pluginJSONText);
        stream >> plugin->config;

        // Check if plugin id is unique
        plugin->config.at("id").get_to(plugin->id);

        // Store external config so we can share it with host at any point in time (even if plugin gets unloaded)
        m_featureExternalConfigMap[plugin->id] = plugin->config.at("external");
        auto& extCfg = m_featureExternalConfigMap[plugin->id];

        // Now we check if plugin is supported on this system
        plugin->config.at("supportedAdapters").get_to(plugin->context.supportedAdapters);
        plugin->config.at("sha").get_to(plugin->sha);
        plugin->config.at("name").get_to(plugin->name);
        plugin->config.at("namespace").get_to(plugin->paramNamespace);
        plugin->config.at("priority").get_to(plugin->priority);
        plugin->config.at("version").at("major").get_to(plugin->version.major);
        plugin->config.at("version").at("minor").get_to(plugin->version.minor);
        plugin->config.at("version").at("build").get_to(plugin->version.build);
        plugin->config.at("api").at("major").get_to(plugin->api.major);
        plugin->config.at("api").at("minor").get_to(plugin->api.minor);
        plugin->config.at("api").at("build").get_to(plugin->api.build);

        // Let the host know about API, priority etc. 
        // Plugin has already populated OS, driver and other custom requirements.
        extCfg["feature"]["lastError"] = "ok";
        extCfg["feature"]["rhi"] = plugin->config.at("rhi");
        extCfg["feature"]["supported"] = plugin->context.supportedAdapters != 0;
        extCfg["feature"]["unloaded"] = false;
        extCfg["feature"]["api"]["detected"] = plugin->api.toStr();
        extCfg["feature"]["api"]["requested"] = m_api.toStr();
        extCfg["feature"]["api"]["supported"] = true;
        extCfg["feature"]["priority"]["detected"] = plugin->priority;
        extCfg["feature"]["priority"]["supported"] = true;
    }
    catch (std::exception& e)
    {
        SL_LOG_ERROR( "JSON exception %s in plugin %s", e.what(), plugin->name.c_str());
        freePlugin(&plugin);
        return false;
    };

    // Finally on success make ppPlugin point to our new plugin
    *ppPlugin = plugin;
    return true;
}

Result PluginManager::mapPlugins(std::vector<std::wstring>& files)
{
    using namespace sl::api;

    auto freePlugin = [](Plugin** plugin)->void
    {
        FreeLibrary((*plugin)->lib);
        delete* plugin;
        *plugin = nullptr;
    };

    for (auto fileName : files)
    {
        // From this point any error is fatal since user requested specific set of features
        Plugin *plugin = nullptr;
        fs::path pluginFullPath(fileName);
        if (loadPlugin(pluginFullPath, &plugin))
        {
            auto& extCfg = m_featureExternalConfigMap[plugin->id];
            Plugin *duplicatedPluginById = nullptr;
            for (auto p : m_plugins)
            {
                if (p->id == plugin->id)
                {
                    SL_LOG_INFO("Detected two plugins with the same id %ls - %ls", p->filename.wstring().c_str(), plugin->filename.wstring().c_str());
                    duplicatedPluginById = p;
                }
            }

            // Check if plugin's id (SL feature) is requested by the host
            bool requested = false;
            for (auto f : m_featuresToLoad)
            {
                if (f == plugin->id)
                {
                    requested = true;
                    break;
                }
            }
            extCfg["feature"]["requested"] = requested;

            bool pluginNeedsInterposer = false;

            bool newerVersion = false;
            if (duplicatedPluginById)
            {
                // Sanity check we're looking at a compatible plugin,
                // this is done later on as well, but we musn't try to
                // load an incompatible plugin and remove a compatible
                // one in the meantime.
                if (plugin->api.major == m_api.major)
                {
                    // Compare the versions of these two plugins, if
                    // pluginIsNewer then we should load it instead of p.
                    if (plugin->version > duplicatedPluginById->version)
                    {
                        SL_LOG_INFO("Plugin %s is newer (%s) will choose that", plugin->name.c_str(), plugin->version.toStr().c_str());
                        newerVersion = true;
                    }
                }
                else
                {
                    SL_LOG_INFO("Plugin %s has a newer apiVersion (%s) than sl.interposer (%s)",
                                plugin->name.c_str(), plugin->api.toStr().c_str(), m_api.toStr().c_str());
                }
            }

            if (!requested)
            {
                SL_LOG_WARN("Ignoring plugin '%s' since it is was not requested by the host", plugin->name.c_str());
                freePlugin(&plugin);
            }
            else if (duplicatedPluginById && !newerVersion)
            {
                SL_LOG_WARN("Ignoring plugin '%s' since it has duplicated unique id", plugin->name.c_str());
                freePlugin(&plugin);

                // XXX[ljm] Plugins can inject global state in their 'onLoad'
                // functions. We need to ensure that this global state is set in
                // accordance with the plugin we actually have loaded rather
                // than whatever plugin we *attempted* to load most recently.
                // In order to do this (without refactoring plugins to not
                // mutate global state 'onLoad'), we need to reload the desired
                // plugin from scratch so thaat it's 'onLoad' can execute and
                // write to the global state.
                //
                // Loop over our plugin list to find the duplicated plugin that
                // we want to reload
                for (auto it = m_plugins.begin(); it != m_plugins.end(); it++)
                {
                    if (*it == duplicatedPluginById)
                    {
                        // Reload the plugin by it's full-path. Ensure that 
                        fs::path fullPath = duplicatedPluginById->fullpath;
                        freePlugin(&duplicatedPluginById);
                        if (!loadPlugin(fullPath, &duplicatedPluginById))
                        {
                            SL_LOG_ERROR("Failed to reload plugin file: %ls it loaded before, so what happened!?", fullPath.wstring().c_str());
                            m_plugins.erase(it);
                            continue;
                        }
                        *it = duplicatedPluginById;
                        continue;
                    }
                }
            }
            else
            {
                // Next step, check if plugin's API is compatible

                // Manager needs to be aware of the API otherwise if plugin is newer we just skip it
                if (plugin->api > m_api)
                {
                    SL_LOG_ERROR( "Detected plugin %s with newer API version %s - host should ship with proper DLLs", plugin->name.c_str(), plugin->api.toStr().c_str());
                    extCfg["feature"]["api"]["supported"] = false;
                    extCfg["feature"]["unloaded"] = true;
                    extCfg["feature"]["lastError"] = "Error: feature has newer API than the plugin manager";
                    freePlugin(&plugin);
                }

                // Make sure that common plugin always runs first
                if (plugin->priority <= 0 && plugin->name != "sl.common")
                {
                    SL_LOG_ERROR( "Detected plugin '%s' with priority <= 0 which is not allowed", plugin->name.c_str());
                    extCfg["feature"]["priority"]["supported"] = false;
                    extCfg["feature"]["unloaded"] = true;
                    extCfg["feature"]["lastError"] = "Error: feature has invalid priority";
                    freePlugin(&plugin);
                }

                // Now let's check for special requirements, dependencies to other plugins, exclusive hooks etc.
                auto extractItems = [plugin](const char* key, std::vector<std::string>& stringList)->void
                {
                    if (plugin->config.contains(key))
                    {
                        auto& items = plugin->config.at(key);
                        for (auto& item : items)
                        {
                            std::string name;
                            item.get_to(name);
                            stringList.push_back(name);
                        }
                    }
                };
                
                extractItems("required_plugins", plugin->requiredPlugins);
                extractItems("exclusive_hooks", plugin->exclusiveHooks);
                extractItems("incompatible_plugins", plugin->incompatiblePlugins);
            }

            // We have loaded a newer version of a plugin that has already
            // been loaded (from a secondary source, likely OTA). We need to
            // unload the old plugin and remove it from the list.
            if (newerVersion)
            {
                SL_LOG_INFO("A duplicate was found, but a newer plugin version was available");
                for (auto it = m_plugins.begin(); it != m_plugins.end(); it++)
                {
                    if (*it == duplicatedPluginById)
                    {
                        // Remove the plugin from the list and free it
                        SL_LOG_INFO("Removing plugin with name: %s superseded by plugin %s", duplicatedPluginById->name.c_str(), plugin->name.c_str());
                        m_plugins.erase(it);
                        freePlugin(&duplicatedPluginById);
                        break;
                    }
                }
            }

            if (plugin)
            {
                SL_LOG_INFO("Loaded plugin '%s' - version %u.%u.%u.%s - id %u - priority %u - adapter mask 0x%x - interposer '%s'", plugin->name.c_str(), plugin->version.major, plugin->version.minor, plugin->version.build, 
                    plugin->sha.c_str(), plugin->id, plugin->priority, plugin->context.supportedAdapters, pluginNeedsInterposer ? "yes" : "no");
            }

            if (plugin)
            {
                m_plugins.push_back(plugin);
            }
        }
        else
        {
            SL_LOG_WARN("Failed to load plugin '%ls' - last error %s", pluginFullPath.wstring().c_str(), std::system_category().message(GetLastError()).c_str());
        }
    };

    return m_plugins.empty() ? Result::eErrorNoPlugins : Result::eOk;
}

Result PluginManager::loadPlugins()
{
    using namespace sl::api;

    std::scoped_lock lock(m_mtxPluginConfig);

    if (s_status == PluginManagerStatus::ePluginsLoaded)
    {
        // Nothing to do, already loaded everything
        return Result::eOk;
    }
    else if (s_status == PluginManagerStatus::ePluginsInitialized)
    {
        SL_LOG_ERROR( "Trying to load plugins while in invalid state");
        return Result::eErrorInvalidState;
    }

    // Here we know that we are either in eUnknown or ePluginsUnloaded state which are valid to restart

#ifndef SL_PRODUCTION
    auto interposerConfig = sl::interposer::getInterface()->getConfig();
    if (!interposerConfig.pathToPlugins.empty())
    {
        m_pathsToPlugins.clear();
        auto path = extra::utf8ToUtf16(interposerConfig.pathToPlugins.c_str());
        if (!file::isRelativePath(path))
        {
            // Ignore relative paths, only used when redirecting SDKs
            m_pathsToPlugins.push_back(path);
        }
    }
#endif

    s_status = PluginManagerStatus::ePluginsLoaded;

    // Kickoff OTA update, this function internally will check OTA preferences
    m_ota->readServerManifest();
    bool requestOptionalUpdates = (m_pref.flags & PreferenceFlags::eAllowOTA);
    for (Feature f : m_featuresToLoad)
    {
        m_ota->checkForOTA(f, m_api, requestOptionalUpdates);
    }

    //! Now let's enumerate SL plugins!
    //!
    //! Two options - look next to the sl.interposer or in the specified paths
    m_pluginPath = file::getModulePath();
    std::vector<std::wstring> pluginList;
    if (m_pathsToPlugins.empty())
    {
        SL_CHECK(findPlugins(m_pluginPath, pluginList));
    }
    else
    {
        for (auto& path : m_pathsToPlugins)
        {
            m_pluginPath = path;
            if(findPlugins(m_pluginPath, pluginList) == Result::eOk)
            {
                break;
            }
        }
    }

    if (m_pref.flags & PreferenceFlags::eLoadDownloadedPlugins)
    {
        SL_LOG_INFO("Searching for OTA'd plugins...");
        for (Feature f : m_featuresToLoad)
        {
            std::wstring pluginPath;
            if (m_ota->getOTAPluginForFeature(f, m_api, pluginPath))
            {
                SL_LOG_INFO("Found plugin: %ls", pluginPath.c_str());
                if (f == kFeatureCommon)
                {
                    // Push kFeatureCommon OTA to front of list so sl.common is
                    // loaded first+foremost
                    pluginList.insert(pluginList.begin(), pluginPath);
                }
                else
                {
                    pluginList.push_back(pluginPath);
                }
            }
        }
    }
    else
    {
        SL_LOG_INFO("eLoadDownloadedPlugins flag not passed to preferences, OTA'd plugins will not be loaded!");
    }

    if (pluginList.empty())
    {
        SL_LOG_WARN("No plugins found - last searched path %S", m_pluginPath.c_str());
        return Result::eErrorNoPlugins;
    }

    param::getInterface()->set(param::global::kPluginPath, (void*)m_pluginPath.c_str());

    SL_CHECK(mapPlugins(pluginList));
    
    // Sort by priority so we can execute hooks in the specific order and check dependencies and other requirements in the correct order
    std::sort(m_plugins.begin(), m_plugins.end(),
        [](const Plugin* a, const Plugin* b) -> bool
        {
            return a->priority < b->priority;
        });

    // Before we can proceed we need to check for plugin dependencies and other special requirements
    {
        std::vector<Plugin*> pluginsToUnload;
        for (auto it = m_plugins.begin(); it != m_plugins.end(); it++)
        {
            // This is our current plugin
            auto plugin = *it;
            
            // If we are not supported then we just unload ourselves
            if (!plugin->context.supportedAdapters)
            {
                SL_LOG_WARN("Ignoring plugin '%s' since it is not supported on this platform", plugin->name.c_str());
                pluginsToUnload.push_back(plugin);
                continue;
            }

            // Check if it was scheduled to be unloaded by a higher-priority plugin
            auto isAboutToBeUnloaded = [&pluginsToUnload](const std::string& pluginName)->bool
            {
                for (auto p : pluginsToUnload)
                {
                    if (pluginName == p->name)
                    {
                        return true;
                    }
                }
                return false;
            };

            // Nothing to do if plugin if about to be unloaded anyway
            if(isAboutToBeUnloaded(plugin->name)) continue;

            // Provide info to host, default to all OK but this can change in code below
            auto& extCfg = m_featureExternalConfigMap[plugin->id];
            extCfg["feature"]["dependency"] = "none";
            extCfg["feature"]["incompatible"] = "none";

            // First we check if current plugin requires any other plugin(s)
            for (auto& required : plugin->requiredPlugins)
            {
                // If required plugin was not loaded or it is just about to be unloaded we cannot use this plugin
                if (!isPluginLoaded(required) || isAboutToBeUnloaded(required))
                {
                    SL_LOG_ERROR( "Plugin '%s' will be unloaded since it requires plugin '%s' which is NOT loaded or about to be unloaded.", plugin->name.c_str(), required.c_str());
                    pluginsToUnload.push_back(plugin);
                    plugin = nullptr;
                    extCfg["feature"]["unloaded"] = true;
                    extCfg["feature"]["dependency"] = required;
                    extCfg["feature"]["lastError"] = extra::format("Error: feature depends on {} which is missing", required);
                    break;
                }
            }
            if (plugin)
            {
                // At this point we know that current plugin is not missing any dependencies
                
                // Check for incompatible plugins and unload them
                for (auto& incompatible : plugin->incompatiblePlugins)
                {
                    if (Plugin* incompatiblePlugin = isPluginLoaded(incompatible))
                    {
                        SL_LOG_WARN("Plugin '%s' is incompatible with plugin '%s' and will be unloaded.", incompatible.c_str(), plugin->name.c_str());
                        pluginsToUnload.push_back(incompatiblePlugin);
                        auto& extCfg1 = m_featureExternalConfigMap[incompatiblePlugin->id];
                        extCfg1["feature"]["unloaded"] = true;
                        extCfg1["feature"]["incompatible"] = plugin->name;
                        extCfg1["feature"]["lastError"] = extra::format("Error: feature is incompatible with {}", plugin->name);
                    }
                }

                // Check if current plugin has any exclusive hooks we don't want others to use
                for (auto& hook : plugin->exclusiveHooks)
                {
                    auto collidingPlugin = isExclusiveHookUsed(plugin, hook);
                    if (collidingPlugin)
                    {
                        SL_LOG_WARN("Plugin '%s' is using an exclusive hook '%s' required by plugin '%s' so it will be unloaded.", collidingPlugin->name.c_str(), hook.c_str(), plugin->name.c_str());
                        pluginsToUnload.push_back(collidingPlugin);
                        auto& extCfg1 = m_featureExternalConfigMap[collidingPlugin->id];
                        extCfg1["feature"]["unloaded"] = true;
                        extCfg1["feature"]["incompatible"] = plugin->name;
                        extCfg1["feature"]["lastError"] = extra::format("Error: feature is incompatible with {} due to an exclusive hook {}", plugin->name, hook);
                    }
                }
            }
        }

        // Now we go and unload plugins which are either missing dependencies, incompatible or using hooks which are exclusive
        for (auto plugin : pluginsToUnload)
        {
            for (auto it = m_plugins.begin(); it != m_plugins.end();)
            {
                if (*it == plugin)
                {
                    it = m_plugins.erase(it);
                    FreeLibrary(plugin->lib);
                    delete plugin;
                }
                else
                {
                    it++;
                }
            }
        }
    }

    if (m_plugins.empty())
    {
        SL_LOG_WARN("Failed to find any plugins!");
    }
    else
    {
        SL_LOG_INFO("Plugin execution order based on priority:");
        for (auto plugin : m_plugins)
        {
            SL_LOG_INFO("P%u - %s", plugin->priority, plugin->name.c_str());
            m_featurePluginsMap[plugin->id] = plugin;
        }
    }
    return m_plugins.empty() ? Result::eErrorNoPlugins : Result::eOk;
}

void PluginManager::unloadPlugins()
{
    SL_LOG_INFO("Unloading all plugins ...");

    // IMPORTANT: Shut down in the opposite order lower priority to higher
    for (auto plugin = m_plugins.rbegin(); plugin != m_plugins.rend(); plugin++)
    {
        if ((*plugin)->onShutdown)
        {
            (*plugin)->onShutdown();
        }
        FreeLibrary((*plugin)->lib);
        delete (*plugin);
    }
    m_plugins.clear();
    m_featurePluginsMap.clear();
    m_featureExternalConfigMap.clear();
    for (auto& hooks : m_afterHooks)
    {
        hooks.clear();
    }
    for (auto& hooks : m_beforeHooks)
    {
        hooks.clear();
    }
    for (auto& [feature, str] : m_externalJSONConfigs)
    {
        delete str;
    }
    m_externalJSONConfigs.clear();
    // After shutdown any hook triggers will be ignored
    s_status = PluginManagerStatus::ePluginsUnloaded;
}

void PluginManager::processPluginHooks(const Plugin* plugin)
{
    // Make sure that whatever is requested by a plugin is actually supported by our interposer
    auto isHookSupported = [this](const std::string& requestedClass, const std::string& requestedTarget)->bool
    {
        return m_functionHookIDMap.find(requestedClass + "_" + requestedTarget) != m_functionHookIDMap.end();
    };

    if (!plugin->context.enabled)
    {
        SL_LOG_INFO("Plugin '%s' is disabled, not mapping any hooks for it", plugin->name.c_str());
        return;
    }

    auto hooks = plugin->config.at("hooks");
    if (hooks.empty())
    {
        SL_LOG_INFO("Plugin '%s' has no registered hooks", plugin->name.c_str());
    }

    for (auto hook : hooks)
    {
        std::string cls, target, replacement, base;
        hook.at("class").get_to(cls);
        hook.at("target").get_to(target);
        hook.at("replacement").get_to(replacement);
        hook.at("base").get_to(base);

        // Skip hooks for unused APIs
        bool clsVulkan = cls == "Vulkan";
        if ((getVulkanDevice() && !clsVulkan) || (!getVulkanDevice() && clsVulkan))
        {
            SL_LOG_INFO("Hook %s:%s:%s - skipped", plugin->name.c_str(), replacement.c_str(), base.c_str());
            continue;
        }

        if (!isHookSupported(cls, target))
        {
            SL_LOG_WARN( "Hook %s:%s:%s is NOT supported, plugin will not function properly", plugin->name.c_str(), cls.c_str(), target.c_str());
            continue;
        }

        void* address = plugin->getFunction(replacement.c_str());
        if (!address)
        {
            SL_LOG_ERROR( "Failed to obtain replacement address for %s in module %s", replacement.c_str(), plugin->name.c_str());
            continue;
        }

        // Two options here, hook before or after the base call.
        auto key = getFunctionHookID(cls + "_" + target);
        auto& list = base == "after" ? m_afterHooks[key] : m_beforeHooks[key];
        std::pair pair = { address, (Feature)plugin->id };
        if (std::find(list.begin(), list.end(), pair) == list.end())
        {
            list.push_back(pair);
            SL_LOG_INFO("Hook %s:%s:%s - OK", plugin->name.c_str(), replacement.c_str(), base.c_str());
        }
        else
        {
            SL_LOG_WARN("Hook %s:%s:%s - DUPLICATED", plugin->name.c_str(), replacement.c_str(), base.c_str());
        }
    }
}

void PluginManager::populateLoaderJSON(uint32_t deviceType, json& config)
{
    try
    {
        // Inform plugins about our version and other properties via JSON config

        config["host"]["version"]["major"] = m_hostSDKVersion.major;
        config["host"]["version"]["minor"] = m_hostSDKVersion.minor;
        config["host"]["version"]["build"] = m_hostSDKVersion.build;

        config["version"]["major"] = m_version.major;
        config["version"]["minor"] = m_version.minor;
        config["version"]["build"] = m_version.build;

        config["api"]["major"] = m_api.major;
        config["api"]["minor"] = m_api.minor;
        config["api"]["build"] = m_api.build;

        config["appId"] = m_appId;
        config["deviceType"] = deviceType;
        auto& paths = config["paths"] = json::array();
        for (auto& path : m_pathsToPlugins)
        {
            paths.push_back(extra::utf16ToUtf8(path.c_str()));
        }
        config["ngx"]["engineType"] = m_engine;
        config["ngx"]["engineVersion"] = m_engineVersion;
        config["ngx"]["projectId"] = m_projectId;

        config["preferences"]["flags"] = m_pref.flags;
        config["interposerEnabled"] = sl::interposer::getInterface()->isEnabled();
        config["forceNonNVDA"] = sl::interposer::getInterface()->getConfig().forceNonNVDA;
    }
    catch (std::exception& e)
    {
        // This should really never happen
        SL_LOG_ERROR( "JSON exception %s", e.what());
    };
}

Result PluginManager::initializePlugins()
{
    if (s_status == PluginManagerStatus::ePluginsLoaded)
    {
        std::scoped_lock lock(m_mtxPluginConfig);

        if (!m_d3d12Device && !m_vkDevice && !m_d3d11Device)
        {
            SL_LOG_ERROR("D3D or VK API hook is activated without device being created, did you forget to call `slSetD3DDevice` or `slSetVulkanInfo` or trying to use another SL API before setting the device?");
            return Result::eErrorDeviceNotCreated;
        }

        if (m_plugins.empty())
        {
            SL_LOG_ERROR_ONCE( "Trying to initialize but no plugins are found, please make sure to place plugins in the correct location.");
            return Result::eErrorNoPlugins;
        }

        // Default to VK
        uint32_t deviceType = (uint32_t)RenderAPI::eVulkan;
        VkDevices vk = { m_vkInstance, m_vkDevice, m_vkPhysicalDevice };
        void* device = &vk;

        if (m_d3d12Device)
        {
            device = m_d3d12Device;
            deviceType = (uint32_t)RenderAPI::eD3D12;
        }
        else if (m_d3d11Device)
        {
            device = m_d3d11Device;
            deviceType = (uint32_t)RenderAPI::eD3D11;
        }

        // We have correct device type so generate new config
        json config;
        populateLoaderJSON(deviceType, config);
        const auto configStr = config.dump(); // serialize to std::string

        SL_LOG_INFO("Initializing plugins - api %u.%u.%u - application ID %u", m_api.major, m_api.minor, m_api.build, m_appId);

        param::IParameters* parameters = param::getInterface();

        auto plugins = m_plugins;
        for (auto plugin : plugins)
        {
            auto& extCfg = m_featureExternalConfigMap[plugin->id];

            plugin->onStartup = reinterpret_cast<api::PFuncOnPluginStartup*>(plugin->getFunction("slOnPluginStartup"));
            plugin->onShutdown = reinterpret_cast<api::PFuncOnPluginShutdown*>(plugin->getFunction("slOnPluginShutdown"));
            bool unload = false;
            if (!plugin->onStartup || !plugin->onShutdown)
            {
                unload = true;
                SL_LOG_ERROR( "onStartup/onShutdown missing for plugin %s", plugin->name.c_str());
                extCfg["feature"]["lastError"] = "Error: core API not found in the plugin";
            }
            else if (!plugin->onStartup(configStr.c_str(), device))
            {
                unload = true;
                extCfg["feature"]["lastError"] = "Error: onStartup failed";
            }
            if (unload)
            {
                extCfg["feature"]["unloaded"] = true;
                extCfg["feature"]["supported"] = false;
                m_featurePluginsMap.erase(plugin->id);
                FreeLibrary(plugin->lib);
                m_plugins.erase(std::remove(m_plugins.begin(), m_plugins.end(), plugin), m_plugins.end());
                delete plugin;
                
                continue;
            }
            else
            {
                // Plugin initialized correctly, let's map callbacks for the core API
                mapPluginCallbacks(plugin);
                // Let other plugins know that this plugin is loaded and supported and on which adapters
                auto supportedAdaptersParam = "sl.param." + plugin->paramNamespace + ".supportedAdapters";
                parameters->set(supportedAdaptersParam.c_str(), plugin->context.supportedAdapters);
            }
            processPluginHooks(plugin);
        }

        // Check for UI and register our callback
        imgui::ImGUI* ui{};
        param::getPointerParam(parameters, param::imgui::kInterface, &ui);
        if (ui)
        {
            auto renderUI = [this](imgui::ImGUI* ui, bool finalFrame)->void
            {
                if (ui->collapsingHeader(extra::format("sl.interposer v{}", (m_version.toStr() + "." + GIT_LAST_COMMIT_SHORT)).c_str(), imgui::kTreeNodeFlagDefaultOpen))
                {
                    ui->text("Built on %s ", __TIMESTAMP__);
                    ui->text("Host SDK v%s ", m_hostSDKVersion.toStr().c_str());
                }
            };
            ui->registerRenderCallbacks(renderUI, nullptr);
        }

        s_status = PluginManagerStatus::ePluginsInitialized;
    }
    else if (s_status == PluginManagerStatus::ePluginsInitialized)
    {
        SL_LOG_ERROR_ONCE("Plugins already initialized but could be using the wrong device, please call slSetD3DDevice immediately after creating desired device");
        return Result::eErrorInvalidIntegration;
    }
    else
    {
        SL_LOG_ERROR_ONCE( "Please call slInit before any other SL/DirectX/DXGI/Vulkan API");
        return Result::eErrorInvalidIntegration;
    }

    return Result::eOk;
}

void PluginManager::mapPluginCallbacks(Plugin* plugin)
{
    plugin->context.initialized = true;
    plugin->context.setData = (PFun_slSetDataInternal*)plugin->getFunction("slSetData");
    plugin->context.getData = (PFun_slGetDataInternal*)plugin->getFunction("slGetData");
    plugin->context.allocResources = (PFun_slAllocateResources*)plugin->getFunction("slAllocateResources");
    plugin->context.freeResources = (PFun_slFreeResources*)plugin->getFunction("slFreeResources");
    plugin->context.evaluate = (PFun_slEvaluateFeature*)plugin->getFunction("slEvaluateFeature");
    plugin->context.setTag = (PFun_slSetTag*)plugin->getFunction("slSetTag");
    plugin->context.setConstants = (PFun_slSetConstants*)plugin->getFunction("slSetConstants");

    SL_LOG_INFO("Callback %s:slSetData:0x%llx", plugin->name.c_str(), plugin->context.setData);
    SL_LOG_INFO("Callback %s:slGetData:0x%llx", plugin->name.c_str(), plugin->context.getData);
    SL_LOG_INFO("Callback %s:slAllocateResources:0x%llx", plugin->name.c_str(), plugin->context.allocResources);
    SL_LOG_INFO("Callback %s:slFreeResources:0x%llx", plugin->name.c_str(), plugin->context.freeResources);
    SL_LOG_INFO("Callback %s:slEvaluateFeature:0x%llx", plugin->name.c_str(), plugin->context.evaluate);
    SL_LOG_INFO("Callback %s:slSetTag:0x%llx", plugin->name.c_str(), plugin->context.setTag);
    SL_LOG_INFO("Callback %s:slSetConsts:0x%llx", plugin->name.c_str(), plugin->context.setConstants);
}

const HookList& PluginManager::getBeforeHooks(FunctionHookID functionHookID)
{
    // Lazy plugin initialization because of the late device initialization
    if (s_status == PluginManagerStatus::ePluginsLoaded)
    {
        initializePlugins();
    }
    else if(s_status == PluginManagerStatus::eUnknown)
    {
        SL_LOG_ERROR( "Please make sure to call slInit before calling DXGI/D3D/Vulkan API");
    }
    return m_beforeHooks[(uint32_t)functionHookID];
}

const HookList& PluginManager::getAfterHooks(FunctionHookID functionHookID)
{
    // Lazy plugin initialization because of the late device initialization
    if (s_status == PluginManagerStatus::ePluginsLoaded)
    {
        initializePlugins();
    }
    else if (s_status == PluginManagerStatus::eUnknown)
    {
        SL_LOG_ERROR( "Please make sure to call slInit before calling DXGI/D3D/Vulkan API");
    }
    return m_afterHooks[(uint32_t)functionHookID];
}

const HookList& PluginManager::getBeforeHooksWithoutLazyInit(FunctionHookID functionHookID)
{
    return m_beforeHooks[(uint32_t)functionHookID];
}

const HookList& PluginManager::getAfterHooksWithoutLazyInit(FunctionHookID functionHookID)
{
    return m_afterHooks[(uint32_t)functionHookID];
}

PluginManager::PluginManager()
{
    m_version.major = VERSION_MAJOR;
    m_version.minor = VERSION_MINOR;
    m_version.build = VERSION_PATCH;

#define FUNCTION_HOOK_ID_MAP_ENTRY(id) (m_functionHookIDMap[#id] = FunctionHookID::e##id)
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGIFactory_CreateSwapChain);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGIFactory_CreateSwapChainForHwnd);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGIFactory_CreateSwapChainForCoreWindow);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_Destroyed);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_Present);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_Present1);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_GetBuffer);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_ResizeBuffers);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_ResizeBuffers1);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_GetCurrentBackBufferIndex);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_SetFullscreenState);
    FUNCTION_HOOK_ID_MAP_ENTRY(ID3D12Device_CreateCommandQueue);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_Present);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_CreateSwapchainKHR);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_DestroySwapchainKHR);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_GetSwapchainImagesKHR);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_AcquireNextImageKHR);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_DeviceWaitIdle);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_CreateWin32SurfaceKHR);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_DestroySurfaceKHR);

    assert((size_t)FunctionHookID::eMaxNum == m_functionHookIDMap.size());

    m_ota = ota::getInterface();
}

uint32_t PluginManager::getFunctionHookID(const std::string& name)
{
    return (uint32_t)m_functionHookIDMap.find(name)->second;
}

}
}

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
#include <random>

#include "include/sl.h"
#include "source/core/sl.api/internal.h"
#include "source/core/sl.log/log.h"
#include "source/core/sl.file/file.h"
#include "source/core/sl.param/parameters.h"
#include "pluginManager.h"
#include "source/core/sl.security/secureLoadLibrary.h"
#include "source/core/sl.interposer/versions.h"
#include "source/core/sl.interposer/hook.h"
#include "_artifacts/gitVersion.h"
#include "include/sl_helpers.h"

#ifdef SL_WINDOWS
#include <versionhelpers.h>
#else
#include <dlfcn.h>
#include <dirent.h>
#include <unistd.h>
#endif

namespace sl
{

namespace plugin_manager
{

class PluginManager : public IPluginManager
{
public:

    PluginManager();
    
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    virtual bool loadPlugins() override final;
    virtual void unloadPlugins() override final;
    virtual bool initializePlugins() override final;

    virtual const HookList& getBeforeHooks(FunctionHookID functionHookID) override final;
    virtual const HookList& getAfterHooks(FunctionHookID functionHookID) override final;
    virtual const HookList& getBeforeHooksWithoutLazyInit(FunctionHookID functionHookID) override final;
    virtual const HookList& getAfterHooksWithoutLazyInit(FunctionHookID functionHookID) override final;

    virtual const Preferences& getPreferences() const override final { return m_pref; }

    virtual void setPreferences(const Preferences& pref) override final
    {
        m_pref = pref;
        param::getInterface()->set(param::global::kPreferenceFlags, m_pref.flags);

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
            Feature::eFeatureDLSS,
            Feature::eFeatureNRD,
            Feature::eFeatureNIS,
            Feature::eFeatureReflex
        };

        // Allow override via JSON config file
        if (interposerConfig.contains("loadAllFeatures"))
        {
            bool value = false;
            interposerConfig.at("loadAllFeatures").get_to(value);
            if (value)
            {
                SL_LOG_HINT("Loading all features");
                m_featuresToLoad = features;
            }
        }
        if (interposerConfig.contains("loadSpecificFeatures") && m_featuresToLoad.empty())
        {
            auto& list = interposerConfig.at("loadSpecificFeatures");
            for (auto& item : list) try
            {
                uint32_t id;
                item.get_to(id);
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
                SL_LOG_ERROR("Failed to parse JSON file - %s", e.what());
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

        // These are not safe to touch after we exit here
        m_pref.pathsToPlugins = {};
        m_pref.pathToLogsAndData = {};
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
    
    virtual bool setFeatureEnabled(Feature feature, bool value) override final;

    virtual bool isSupportedOnThisMachine() const override final;
    virtual bool isRunningD3D12() const  override final { return m_d3d12Device != nullptr; };
    virtual bool isRunningD3D11() const  override final { return m_d3d11Device != nullptr; };
    virtual bool isRunningVulkan() const  override final { return m_vkDevice != nullptr; };
    virtual bool isInitialized() const  override final { return m_initialized; }
    virtual bool arePluginsLoaded() const  override final { return !m_plugins.empty(); }

    virtual const FeatureContext* getFeatureContext(Feature feature) override final
    {
        if (!m_initialized)
        {
            initializePlugins();
        }
        auto it = m_featurePluginsMap.find(feature);
        if (it != m_featurePluginsMap.end())
            return &(*it).second->context;

        return nullptr;
    }

    inline static PluginManager* s_manager = {};

private:

    bool mapPlugins(const std::wstring& path, std::vector<std::wstring>& files);
    bool findPlugins(const std::wstring& path, std::vector<std::wstring>& files);
    

    struct Plugin
    {
        Plugin() {}
        Plugin(const Plugin& rhs) = delete;
        Plugin& operator=(const Plugin& rhs) = delete;

        uint32_t id{};
        int priority{};
        Version version{};
        Version api{};
        HMODULE lib{};
        json config{};
        std::string name{};
        std::string paramNamespace{};
        api::PFuncGetPluginJSONConfig* getConfig{};
        api::PFuncOnPluginStartup* onStartup{};
        api::PFuncOnPluginShutdown* onShutdown{};
        api::PFuncGetPluginFunction* getFunction{};
        api::PFuncSetParameters* setParameters{};
        std::vector<std::string> requiredPlugins;
        std::vector<std::string> exclusiveHooks;
        std::vector<std::string> incompatiblePlugins;
        FeatureContext context{};
    };

    void processPluginHooks(const Plugin* plugin);
    void mapPluginCallbacks(Plugin* plugin);
    uint32_t getFunctionHookID(const std::string& name);

    Plugin* isPluginLoaded(const std::string& name) const;
    Plugin* isExclusiveHookUsed(const Plugin* exclusivePlugin, const std::string& exclusiveHook) const;

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
    using PluginMap = std::map<uint32_t,Plugin*>;
    
    PluginList m_plugins;
    PluginMap m_featurePluginsMap;

    int m_appId = 0;
    Boolean m_initialized = Boolean::eFalse;
    bool m_triedToLoadPlugins = false;

    std::wstring m_pluginPath = {};
    std::vector<std::wstring> m_pathsToPlugins;
    std::vector<Feature> m_featuresToLoad;
 
    Preferences m_pref{};
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

bool PluginManager::setFeatureEnabled(Feature feature, bool value)
{
    // This is clearly not thread safe so the assumption is that
    // host will not invoke any hooks while we are running as it is
    // documented in the programming guide.

    auto it = m_featurePluginsMap.find(feature);
    if (it == m_featurePluginsMap.end() || (*it).second->context.enabled == value)
    {
        // Feature either not loaded or enabled flag does not change, nothing to do
        SL_LOG_WARN("Feature either not present or already in the requested 'enabled' state");
        return false;
    }
    auto featurePlugin = (*it).second;
    featurePlugin->context.enabled = value;
    SL_LOG_INFO("Feature with id:%u %s", feature, value ? "enabled" : "disabled");
    auto hooks = featurePlugin->config.at("hooks");
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
    return true;
}

bool PluginManager::findPlugins(const std::wstring& directory, std::vector<std::wstring>& files)
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
                files.push_back(name + ext);
            }
        }
    }
    catch (std::exception& e)
    {
        SL_LOG_ERROR("Failed while looking for plugins - error %s", e.what());
    }
    return !files.empty();
}

bool PluginManager::mapPlugins(const std::wstring& path, std::vector<std::wstring>& files)
{
    using namespace sl::api;

    param::IParameters* parameters = param::getInterface();

    // sl.common is always enabled
    m_featuresToLoad.push_back(Feature::eFeatureCommon);

    auto freePlugin = [](Plugin** plugin)->void
    {
        FreeLibrary((*plugin)->lib);
        delete* plugin;
        *plugin = nullptr;
    };

    for (auto fileName : files)
    {

        // From this point any error is fatal since user requested specific set of features
        auto pluginFullPath = path + L"/" + fileName;
        HMODULE mod = security::loadLibrary(pluginFullPath.c_str());
        if (mod)
        {
            Plugin* plugin = new Plugin();
            plugin->name = file::removeExtension(extra::toStr(fileName));
            plugin->lib = mod;
            plugin->getFunction = reinterpret_cast<api::PFuncGetPluginFunction*>(GetProcAddress(mod, "slGetPluginFunction"));
            if (plugin->getFunction)
            {
                plugin->setParameters = reinterpret_cast<api::PFuncSetParameters*>(plugin->getFunction("slSetParameters"));
                plugin->getConfig = reinterpret_cast<api::PFuncGetPluginJSONConfig*>(plugin->getFunction("slGetPluginJSONConfig"));
            }
            if (!plugin->getFunction || !plugin->getConfig || !plugin->setParameters)
            {
                SL_LOG_ERROR("Ignoring '%s' since it does not contain proper API", plugin->name.c_str());
                freePlugin(&plugin);
                continue;
            }

            // First let's pass parameters
            plugin->setParameters(parameters);

            try
            {
                // Let's get JSON config from our plugin
                std::istringstream stream(plugin->getConfig());
                stream >> plugin->config;

                // Check if plugin's id (SL feature) is requested by the host
                plugin->config.at("id").get_to(plugin->id);
                bool requested = false;
                for (auto f : m_featuresToLoad)
                {
                    if (f == plugin->id)
                    {
                        requested = true;
                        break;
                    }
                }

                // Now we check if plugin is supported on this system
                plugin->config.at("supportedAdapters").get_to(plugin->context.supportedAdapters);
                if (!requested || !plugin->context.supportedAdapters)
                {
                    if (!requested)
                    {
                        SL_LOG_WARN("Ignoring plugin '%s' since it is was not requested by the host", plugin->name.c_str());
                    }
                    else if (!plugin->context.supportedAdapters)
                    {
                        SL_LOG_ERROR("Ignoring plugin '%s' since it is not supported on this platform", plugin->name.c_str());
                    }
                    freePlugin(&plugin);
                }
                else
                {
                    // Next step, check if plugin's API is compatible
                    plugin->config.at("id").get_to(plugin->id);
                    plugin->config.at("namespace").get_to(plugin->paramNamespace);
                    plugin->config.at("priority").get_to(plugin->priority);
                    plugin->config.at("version").at("major").get_to(plugin->version.major);
                    plugin->config.at("version").at("minor").get_to(plugin->version.minor);
                    plugin->config.at("version").at("build").get_to(plugin->version.build);
                    plugin->config.at("api").at("major").get_to(plugin->api.major);
                    plugin->config.at("api").at("minor").get_to(plugin->api.minor);
                    plugin->config.at("api").at("build").get_to(plugin->api.build);

                    // Manager needs to be aware of the API otherwise if plugin is newer we just skip it
                    if (plugin->api > m_api)
                    {
                        SL_LOG_ERROR("Detected plugin %s with newer API version %s - host should ship with proper DLLs", plugin->name.c_str(), plugin->api.toStr().c_str());
                        freePlugin(&plugin);
                    }

                    // Make sure that common plugin always runs first
                    if (plugin->priority <= 0 && plugin->name != "sl.common")
                    {
                        SL_LOG_ERROR("Detected plugin '%s' with priority <= 0 which is not allowed", plugin->name.c_str());
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

                if (plugin)
                {
                    SL_LOG_INFO("Loaded plugin '%s' - version %u.%u.%u - id %u - priority %u - adapter mask 0x%x - namespace '%s'", plugin->name.c_str(), plugin->version.major, plugin->version.minor, plugin->version.build, plugin->id, plugin->priority, plugin->context.supportedAdapters, plugin->paramNamespace.c_str());
                }
            }
            catch (std::exception& e)
            {
                SL_LOG_ERROR("JSON exception %s", e.what());
                SL_LOG_ERROR("Plugin JSON file %s", plugin->getConfig());
                freePlugin(&plugin);
            };

            if (plugin)
            {
                m_plugins.push_back(plugin);
            }
        }
        else
        {
            SL_LOG_WARN("Failed to load plugin %S", pluginFullPath.c_str());
        }
    };
    return !m_plugins.empty();
}

bool PluginManager::loadPlugins()
{
    using namespace sl::api;

    if (m_triedToLoadPlugins)
    {
        // Nothing to do, already loaded everything
        return true;
    }

    if (!isSupportedOnThisMachine())
    {
        // Running on unsupported HW, also could be old OS
        return false;
    }

#ifndef SL_PRODUCTION
    auto interposerConfig = sl::interposer::getInterface()->getConfig();
    if (interposerConfig.contains("pathToPlugins"))
    {
        std::string path;
        interposerConfig.at("pathToPlugins").get_to(path);
        SL_LOG_HINT("Overriding pathsToPlugins to %s", path.c_str());
        m_pathsToPlugins.clear();
        m_pathsToPlugins.push_back(extra::toWStr(path));
    }
#endif

    m_triedToLoadPlugins = true;

    // Now let's enumerate SL plugins!
    std::wstring exePath = file::getExecutablePath();
    m_pluginPath = exePath;
    // Two options - look next to the executable or in the specified paths
    std::vector<std::wstring> pluginList;
    if (m_pathsToPlugins.empty())
    {
        findPlugins(m_pluginPath, pluginList);
    }
    else
    {
        for (auto& path : m_pathsToPlugins)
        {
            m_pluginPath = path;
            if(findPlugins(m_pluginPath, pluginList))
            {
                break;
            }
        }
    }
    
    if (pluginList.empty())
    {
        SL_LOG_WARN("No plugins found - last searched path %S", m_pluginPath.c_str());
        return false;
    }

    param::getInterface()->set(param::global::kPluginPath, (void*)m_pluginPath.c_str());

    if (!mapPlugins(m_pluginPath, pluginList))
    {
        return false;
    }

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

            // Check if it was scheduled to be unloaded by a higher-priority plugin
            for (auto p : pluginsToUnload)
            {
                if (plugin == p)
                {
                    plugin = nullptr;
                    break;
                }
            }

            // Nothing to do if plugin if about to be unloaded anyway
            if (!plugin) continue;

            // First we check if current plugin requires any other plugin(s)
            for (auto& required : plugin->requiredPlugins)
            {
                if (!isPluginLoaded(required))
                {
                    SL_LOG_ERROR("Plugin '%s' will be unloaded since it requires plugin '%s' which is NOT loaded.", plugin->name.c_str(), required.c_str());
                    pluginsToUnload.push_back(plugin);
                    plugin = nullptr;
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
        SL_LOG_ERROR("Failed to find any plugins!");
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
    return !m_plugins.empty();
}

void PluginManager::unloadPlugins()
{
    SL_LOG_INFO("Unloading all plugins ...");

    // IMPORTANT: Shut down in the opposite order lower priority to higher
    for (auto plugin = m_plugins.rbegin(); plugin != m_plugins.rend(); plugin++)
    {
        (*plugin)->onShutdown();
        FreeLibrary((*plugin)->lib);
        delete (*plugin);
    }
    m_plugins.clear();
    m_featurePluginsMap.clear();
    for (auto& hooks : m_afterHooks)
    {
        hooks.clear();
    }
    for (auto& hooks : m_beforeHooks)
    {
        hooks.clear();
    }
    m_initialized = Boolean::eInvalid;
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

        if (!isHookSupported(cls, target))
        {
            SL_LOG_ERROR("Hook %s:%s:%s is NOT supported, plugin will not function properly", plugin->name.c_str(), cls.c_str(), target.c_str());
            continue;
        }

        // Skip hooks for unused APIs
        bool clsVulkan = cls == "Vulkan";
        if ((isRunningVulkan() && !clsVulkan) || (!isRunningVulkan() && clsVulkan))
        {
            SL_LOG_INFO("Hook %s:%s:%s - skipped", plugin->name.c_str(), replacement.c_str(), base.c_str());
            continue;
        }

        void* address = plugin->getFunction(replacement.c_str());
        if (!address)
        {
            SL_LOG_ERROR("Failed to obtain replacement address for %s in module %s", replacement.c_str(), plugin->name.c_str());
            continue;
        }

        SL_LOG_INFO("Hook %s:%s:%s - OK", plugin->name.c_str(), replacement.c_str(), base.c_str());

        if (base == "after")
        {
            // After base call
            m_afterHooks[getFunctionHookID(cls + "_" + target)].push_back(address);
        }
        else
        {
            // Before base call with an option to skip the base call
            m_beforeHooks[getFunctionHookID(cls + "_" + target)].push_back(address);
        }
    }
}

bool PluginManager::initializePlugins()
{
    if (!m_triedToLoadPlugins)
    {
        SL_LOG_ERROR_ONCE("Please call slInit before any other SL/DirectX/DXGI/Vulkan API");
        return false;
    }

    if (!m_initialized)
    {
        if (!m_d3d12Device && !m_vkDevice && !m_d3d11Device)
        {
            SL_LOG_WARN_ONCE("D3D or VK API hook is activated without device being created - ignoring");
            return false;
        }

        if (m_plugins.empty())
        {
            SL_LOG_ERROR_ONCE("Trying to initialize but no plugins are found, please make sure to place plugins in the correct location.");
            return false;
        }

        // Default to VK
        uint32_t deviceType = 2;
        VkDevices vk = { m_vkInstance, m_vkDevice, m_vkPhysicalDevice };
        void* device = &vk;

        if (m_d3d12Device)
        {
            device = m_d3d12Device;
            deviceType = 1;
        }
        else if (m_d3d11Device)
        {
            device = m_d3d11Device;
            deviceType = 0;
        }

        json config;

        try
        {
            // Inform plugins about our version and other properties via JSON config

            config["version"]["major"] = m_version.major = VERSION_MAJOR;
            config["version"]["minor"] = m_version.minor = VERSION_MINOR;
            config["version"]["build"] = m_version.build = VERSION_PATCH;

            config["api"]["major"] = m_api.major;
            config["api"]["minor"] = m_api.minor;
            config["api"]["build"] = m_api.build;

            config["appId"] = m_appId;
            config["deviceType"] = deviceType;
            auto& paths = config["paths"] = json::array();
            for (auto &path : m_pathsToPlugins)
            {
                paths.push_back(extra::utf16ToUtf8(path.c_str()));
            }
            SL_LOG_INFO("Initializing plugins - api %u.%u.%u - application ID %u", m_api.major, m_api.minor, m_api.build, m_appId);
        }
        catch (std::exception& e)
        {
            // This should really never happen
            SL_LOG_ERROR("JSON exception %s", e.what());
            return false;
        };
        const auto configStr = config.dump(); // serialize to std::string

        param::IParameters* parameters = param::getInterface();

        auto plugins = m_plugins;
        for (auto plugin : plugins)
        {
            plugin->onStartup = reinterpret_cast<api::PFuncOnPluginStartup*>(plugin->getFunction("slOnPluginStartup"));
            plugin->onShutdown = reinterpret_cast<api::PFuncOnPluginShutdown*>(plugin->getFunction("slOnPluginShutdown"));
            bool unload = false;
            if (!plugin->onStartup || !plugin->onShutdown)
            {
                unload = true;
                SL_LOG_ERROR("onStartup/onShutdown missing for plugin %s", plugin->name.c_str());
            }
            else if (!plugin->onStartup(configStr.c_str(), device, parameters))
            {
                unload = true;
            }
            if (unload)
            {
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

        m_initialized = Boolean::eTrue;
    }
    return true;
}

void PluginManager::mapPluginCallbacks(Plugin* plugin)
{
    if (plugin->name == "sl.common")
    {
        // Nothing to do for common plugin
        return;
    }
    plugin->context.setConstants = (PFunSlSetConstantsInternal*)plugin->getFunction("slSetConstants");
    plugin->context.getSettings = (PFunSlGetSettingsInternal*)plugin->getFunction("slGetSettings");
    plugin->context.allocResources = (PFunSlAllocateResources*)plugin->getFunction("slAllocateResources");
    plugin->context.freeResources = (PFunSlFreeResources*)plugin->getFunction("slFreeResources");

    SL_LOG_INFO("Callback %s:slSetConstants:0x%llx", plugin->name.c_str(), plugin->context.setConstants);
    SL_LOG_INFO("Callback %s:slGetSettings:0x%llx", plugin->name.c_str(), plugin->context.getSettings);
    SL_LOG_INFO("Callback %s:slAllocateResources:0x%llx", plugin->name.c_str(), plugin->context.allocResources);
    SL_LOG_INFO("Callback %s:slFreeResources:0x%llx", plugin->name.c_str(), plugin->context.freeResources);
}

const HookList& PluginManager::getBeforeHooks(FunctionHookID functionHookID)
{
    // Lazy plugin initialization because of the late device initialization
    if (m_initialized == Boolean::eFalse)
    {
        initializePlugins();
    }

    return m_beforeHooks[(uint32_t)functionHookID];
}

const HookList& PluginManager::getAfterHooks(FunctionHookID functionHookID)
{
    // Lazy plugin initialization because of the late device initialization
    if (m_initialized == Boolean::eFalse)
    {
        initializePlugins();
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

bool PluginManager::isSupportedOnThisMachine() const
{
    return true; // Always supported, plugins decide individually if they are or not
}

PluginManager::PluginManager()
{
    SL_LOG_INFO("Streamline v%u.%u.%u - built on %s - %s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, __TIMESTAMP__, GIT_LAST_COMMIT);

#define FUNCTION_HOOK_ID_MAP_ENTRY(id) (m_functionHookIDMap[#id] = FunctionHookID::e##id)
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGIFactory_CreateSwapChain);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGIFactory2_CreateSwapChainForHwnd);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGIFactory2_CreateSwapChainForCoreWindow);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_Destroyed);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_Present);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_Present1);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_GetBuffer);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_ResizeBuffers);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_GetCurrentBackBufferIndex);
    FUNCTION_HOOK_ID_MAP_ENTRY(IDXGISwapChain_SetFullscreenState);
    FUNCTION_HOOK_ID_MAP_ENTRY(ID3D12Device_CreateCommittedResource);
    FUNCTION_HOOK_ID_MAP_ENTRY(ID3D12Device_CreatePlacedResource);
    FUNCTION_HOOK_ID_MAP_ENTRY(ID3D12Device_CreateReservedResource);
    FUNCTION_HOOK_ID_MAP_ENTRY(ID3D12GraphicsCommandList_ResourceBarrier);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_BeginCommandBuffer);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_CmdBindPipeline);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_Present);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_CmdPipelineBarrier);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_CmdBindDescriptorSets);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_CreateSwapchainKHR);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_GetSwapchainImagesKHR);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_AcquireNextImageKHR);
    FUNCTION_HOOK_ID_MAP_ENTRY(Vulkan_CreateImage);
}

uint32_t PluginManager::getFunctionHookID(const std::string& name)
{
    return (uint32_t)m_functionHookIDMap.find(name)->second;
}

}
}

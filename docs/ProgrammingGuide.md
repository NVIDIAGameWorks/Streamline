

Streamline - SL
=======================

Version 1.0.4
------

1 SETTING UP
-------------

### 1.1 MACHINE CONFIGURATION
The following is needed to use SL features:

* Windows PC with Windows 10 RS3 (64-bit) or newer
* GPU capable of running DirectX 11/12

> **NOTE:**
> At the moment, some features only work on specific NVIDIA hardware. Please see below on how to check if a feature is supported or not. Also please note that **Vulkan is NOT supported in this release**

2 INTEGRATING SL WITH YOUR APPLICATION
---------------------------------

### 2.1 ADDING SL TO YOUR APPLICATION

The SL SDK comes with several header files, `sl.h` and `sl_*.h`, which are located in the `./include` folder. Two main header files are `sl.h`and `sl_consts.h` and they should be included always. Depending on which SL features are used by your application, additional header(s) should be included (see below for more details). Your project should link against the export library `sl.interposer.lib` (which is provided in the ./lib/x64 folder) and if using Vulkan, distribute the included `VkLayer_streamline.json` with your application. Since SL is an interposer, there are several ways it can be integrated into your application:

* If you are statically linking `d3d11.lib`, `d3d12.lib` and/or `dxgi.lib`, there are two options:
    * remove standard libraries from the linking stage and and link `sl.interposer.lib` instead; SL will automatically intercept API it requires to function correctly
    * keep linking the standard libraries, load `sl.interposer.dll` dynamically and redirect DXGI/D3D API calls as required (code sample can be found below)
* If you are dynamically loading `d3d11.dll`, `d3d12.dll` and `dxgi.dll`, dynamically load `sl.interposer.dll` instead 
* If you are using Vulkan, enable `VK_LAYER_Streamline` and include `VkLayer_streamline.json` with your distribution

> **IMPORTANT:**
> Vulkan support is under development and does not function correctly in the current release.

If you want full control, simply incorporate SL source code into your engine and instead of creating standard DirectX/DXGI interfaces, use proxies provided by the SL SDK. Please note that in this scenario, you also need to ensure that the SL plugin manager is initialized properly and SL plugins are loaded and mapped correctly, otherwise SL features will not work.

> **IMPORTANT:**
> When running on hardware which does not support SL, `sl.interposer.dll` serves as a simple pass-through layer with negligible impact on performance. 

Here is how SL library can be loaded dynamically and used instead of standard DXGI/D3D API while continuing to link standard `d3d11.lib`, `d3d12.lib` and `dxgi.lib` libraries (if needed or desired to simplify build process):

```cpp

// IMPORTANT: Always securely load SL library, see source/core/sl.security/secureLoadLibrary for more details
auto mod = sl::security::loadLibrary(PATH_TO_SL_IN_YOUR_BUILD + "/sl.interposer.dll");
if(!mod)
{
    // Disable SL, handle error
}

// These are the exports from SL library
typedef HRESULT(WINAPI* PFunCreateDXGIFactory)(REFIID, void**);
typedef HRESULT(WINAPI* PFunCreateDXGIFactory1)(REFIID, void**);
typedef HRESULT(WINAPI* PFunCreateDXGIFactory2)(UINT, REFIID, void**);
typedef HRESULT(WINAPI* PFunDXGIGetDebugInterface1)(UINT, REFIID, void**);
typedef HRESULT(WINAPI* PFunD3D12CreateDevice)(IUnknown* , D3D_FEATURE_LEVEL, REFIID , void**);

// Map functions from SL and use them instead of standard DXGI/D3D12 API
auto slCreateDXGIFactory = reinterpret_cast<PFunCreateDXGIFactory>(GetProcAddress(mod, "CreateDXGIFactory"));
auto slCreateDXGIFactory1 = reinterpret_cast<PFunCreateDXGIFactory1>(GetProcAddress(mod, "CreateDXGIFactory1"));
auto slCreateDXGIFactory2 = reinterpret_cast<PFunCreateDXGIFactory2>(GetProcAddress(mod, "CreateDXGIFactory2"));
auto slDXGIGetDebugInterface1 = reinterpret_cast<PFunDXGIGetDebugInterface1>(GetProcAddress(mod, "DXGIGetDebugInterface1"));
auto slD3D12CreateDevice = reinterpret_cast<PFunD3D12CreateDevice>(GetProcAddress(mod, "D3D12CreateDevice"));

// For example to create DXGI factory and D3D12 device we could do something like this:

IDXGIFactory1* DXGIFactory{};
if(s_useStreamline)
{
    // Interposed factory
    slCreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory));
}    
else    
{
    // Regular factory
    CreateDXGIFactory1(IID_PPV_ARGS(&DXGIFactory));
}

ID3D12Device* device{};
if(s_useStreamline)
{
    // Interposed device
    slD3D12CreateDevice(targetAdapter, deviceParams.featureLevel,IID_PPV_ARGS(&device));
}
else 
{
    // Regular device
    D3D12CreateDevice(targetAdapter, deviceParams.featureLevel,IID_PPV_ARGS(&device));
}

// IMPORTANT: When SL is enabled from this point onwards any new swap-chains or command lists will be managed by SL automatically

```
#### 2.1.1 SECURITY

All modules provided in the `./bin/x64` SDK folder are digitally signed by NVIDIA. There are two digital signatures on each SL module, one is a standard Windows store certificate and can be validated using `WinVerifyTrust` while the other is a custom NVIDIA certificate used to handle scenarios where the OS is compromised and Windows store certificates cannot be trusted. To secure your application from potentially malicious replacement modules, please do the following:

* Validate the digital signature on ***all Streamline modules*** using the `WinVerifyTrust` Win32 API when starting your application.
* Validate the public key for the NVIDIA custom digital certificate on `sl.interposer.dll` if using the binary provided by NVIDIA (see `source/core/sl.security/secureLoadLibrary` for more details)

***It is strongly recommended*** to use the provided `sl.interposer.dll` binary and follow the above guidelines. The prebuilt binary automatically performs the above steps when loading SL plugins to ensure maximum security. If you decide to build your own `sl.interposer.dll`, make sure to enforce your own strict security policies.

### 2.2 INITIALIZING SL 

#### 2.2.1 SL FEATURE LIFECYCLE
Here is the typical lifecycle for SL features:
* Requested feature DLLs are loaded during `slInit` call.
    * This is (and must be) done before any DX/VK APIs are invoked (which is why the app must call slInit very early in its initialization)
    * The feature "request" process is detailed in [featuresToLoad in preferences](#222-preferences)
* At this point, required and supported features are loaded but are NOT yet initialized
    * As a result, methods like `slSet/Get/Eval` CANNOT BE USED immediately after slInit
* The app can and should call `slIsFeatureSupported` at this point to check which specific feature(s) are supported and on which adapters
* Later in the application's initialization sequence, the app creates a rendering API device
    *  Specifically, a device on the adapter where SL features are supported
* This device creation indirectly triggers SL feature initialization since the device is now available
    * In practice, this means that each enabled and loaded feature's slOnPluginStartup is called
    * If any feature's slOnPluginStartup returns failure, that plugin will be unloaded
* Note that features can fail to initialize (and thus be unloaded) even if they are declared as supported
    * For example, the app may create a D3D11 device for a feature that is supported on the device's adapter, but only when using a D3D12 device
* After device creation, any calls to `slIsFeatureSupported` will succeed only if the feature is FULLY functional on that device
    * This means that the set of features that return true from slIsFeatureSupported AFTER device creation can be smaller (but not larger) than the set that returned true from slIsFeatureSupported after slInit, but before device creation
    * And thus, at this point, any SL method can be used safely since all required and supported features are initialized
    * In addition, at this point it is possible to [explicitly allocate/free resources used by feature(s)](#2224-resource-allocation-and-de-allocation)

#### 2.2.2 PREFERENCES

To control the behavior of the SL SDK, some preferences can be specified using the following data structure:

```cpp
//! Optional flags
enum PreferenceFlags : uint64_t
{
    //! IMPORTANT: If this flag is set then the host application is responsible for restoring CL state correctly
    ePreferenceFlagDisableCLStateTracking = 1 << 0,
    //! Disables debug text on screen in development builds
    ePreferenceFlagDisableDebugText = 1 << 1,
};

//! Optional preferences
struct Preferences
{
    //! Optional - In non-production builds it is useful to enable debugging console window
    bool showConsole = false;
    //! Optional - Various logging levels
    LogLevel logLevel = eLogLevelDefault;
    //! Optional - Absolute paths to locations where to look for plugins, first path in the list has the highest priority
    const wchar_t** pathsToPlugins = {};
    //! Optional - Number of paths to search
    uint32_t numPathsToPlugins = 0;
    //! Optional - Absolute path to location where logs and other data should be stored
    //! NOTE: Set this to nullptr in order to disable logging to a file
    const wchar_t* pathToLogsAndData = {};
    //! Optional - Allows resource allocation tracking on the host side
    pfunResourceAllocateCallback* allocateCallback = {};
    //! Optional - Allows resource deallocation tracking on the host side
    pfunResourceReleaseCallback* releaseCallback = {};
    //! Optional - Allows log message tracking including critical errors if they occur
    pfunLogMessageCallback* logMessageCallback = {};
    //! Optional – Flags used to enable or disable advanced options
    PreferenceFlags flags{};
    //! Required - Features to load (assuming appropriate plugins are found), if not specified NO features will be loaded by default
    const Feature* featuresToLoad = {};
    //! Required - Number of features to load, only used when list is not a null pointer
    uint32_t numFeaturesToLoad = 0;
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

```

##### 2.2.2.1 MANAGING FEATURES

SL will only load features specified via the `Preferences::featuresToLoad` (assuming they are supported on the user's system). ***If this value is empty, no features will be loaded.*** For example, to load only DLSS, one can do the following:

```cpp
Preferences pref;
Feature myFeatures[] = { eFeatureDLSS };
pref.featuresToLoad = myFeatures;
pref.numFeaturesToLoad = _countof(myFeatures);
// Set other preferences here ...
```

> **IMPORTANT:**
> This is a change in behavior from previous versions of the Streamline SDK.  Previously, all available features were loaded by default if no list was provided.

> **IMPORTANT:**
> If SL plugins are not located next to the host executable, then the absolute paths to locations where to look for them must be specified by setting the `pathsToPlugins` field in the `Preferences` structure. Plugins will be loaded from the first path where they can be found.

##### 2.2.2.2 LOGGING AND DEBUG CONSOLE WINDOW

SL provides different levels of logging and can also show a debug console window if requested via the `Preferences` structure. The host can also specify the
location where logs should be saved and track all messages via special `logMessagesCallback` including any errors or warnings.

```cpp
//! Different levels for logging
enum LogLevel
{
    //! No logging
    eLogLevelOff,
    //! Default logging
    eLogLevelDefault,
    //! Verbose logging
    eLogLevelVerbose,
    //! Total count
    eLogLevelCount
};

//! Log type
enum LogType
{
    //! Controlled by LogLevel, SL can show more information in eLogLevelVerbose mode
    eLogTypeInfo,
    //! Always shown regardless of LogLevel
    eLogTypeWarn,
    eLogTypeError,
    //! Total count
    eLogTypeCount
};

//! Logging callback
//!
//! Use these callbacks to track messages posted in the log.
//! If any of the SL methods returns false, check for 
//! eLogTypeError or eLogTypeWarn messages to track down what went wrong and why.
using pfunLogMessageCallback = void(LogType type, const char *msg);
```

> **NOTE:**
> If a logging callback is specified then SL will not use `OutputDebugString` debug API.

##### 2.2.2.3 MEMORY MANAGEMENT

SL can hand over the control over resource allocation and de-allocation, if requested. When specified, the following callbacks on the host
side will be **fully responsible for the resource allocation and destruction**.

```cpp
//! Resource description
struct ResourceDesc
{
    //! Indicates the type of resource
    ResourceType type = eResourceTypeTex2d;
    //! D3D12_RESOURCE_DESC/VkImageCreateInfo/VkBufferCreateInfo
    void* desc{};
    //! Initial state as D3D12_RESOURCE_STATES or VkMemoryPropertyFlags
    uint32_t state = 0;
    //! CD3DX12_HEAP_PROPERTIES or nullptr
    void* heap{};
    //! Reserved for future expansion, must be set to null
    void* ext{};
};

//! Native resource
struct Resource
{
    //! Indicates the type of resource
    ResourceType type = eResourceTypeTex2d;
    //! ID3D11Resource/ID3D12Resource/VkBuffer/VkImage
    void* native{};
    //! vkDeviceMemory or nullptr
    void* memory{};
    //! VkImageView/VkBufferView or nullptr
    void* view{};
    //! State as D3D12_RESOURCE_STATES or VkImageLayout
    //! 
    //! IMPORTANT: State needs to be correct when tagged resources are actually used.
    //! 
    uint32_t state{};
    //! Reserved for future expansion, must be set to null
    void* ext{};
};

//! Resource allocation/de-allocation callbacks
//!
//! Use these callbacks to gain full control over 
//! resource life cycle and memory allocation tracking.
//!
//! IMPORTANT: All resources must be able to be bound as UAV input/output.
//! In addition, all textures are also sampled as NON pixel shader resources.
using pfunResourceAllocateCallback = Resource(const ResourceDesc *desc, void* device);
using pfunResourceReleaseCallback = void(Resource *resource, void* device);
```

> **NOTE:**
> Memory management done by the host is an optional feature and it is NOT required for the SL to work properly. If used, proper care needs to be taken to avoid releasing resources which are still used by the GPU.

##### 2.2.2.4 RESOURCE ALLOCATION AND DE-ALLOCATION

By default SL performs the so called `lazy` initialization and destruction of all resources - in other words resources are created only when used and destroyed when plugins are unloaded. If required an explicit allocation or de-allocation of internal SL resources is also possible using the following methods:

```cpp
//! Allocates resources for the specified feature.
//!
//! Call this method to explicitly allocate resources
//! for an instance of the specified feature.
//! 
//! @param cmdBuffer Command buffer to use (must be created on device where feature is supported but can be null if not needed)
//! @param feature Feature we are working with
//! @param id Unique id (instance handle)
//! @return false if resources cannot be allocated true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool slAllocateResources(sl::CommandBuffer* cmdBuffer, sl::Feature feature, uint32_t id);

//! Frees resources for the specified feature.
//!
//! Call this method to explicitly free resources
//! for an instance of the specified feature.
//! 
//! @param feature Feature we are working with
//! @param id Unique id (instance handle)
//! @return false if resources cannot be freed true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool slFreeResources(sl::Feature feature, uint32_t id);
```

For example, let's assume we have two viewports using `sl::eFeatureDLSS` and we want to manage SL resource allocation explicitly - here is some sample code:

```cpp
// Viewport1
{
    // We need to setup our constants first so sl.dlss plugin has enough information
    sl::DLSSConstants dlssConsts = {};
    dlssConsts.mode = viewport1->getDLSSMode(); // e.g. sl::eDLSSModeBalanced;
    dlssConsts.outputWidth = viewport1->getOutputWidth();    // e.g 1920;
    dlssConsts.outputHeight = viewport1->getOutputHeight(); // e.g. 1080;
    // Note that we are passing viewport id
    slSetFeatureConstants(sl::eFeatureDLSS, &dlssConsts, 0, viewport1->id);
    
    // Set our tags, note that we are passing viewport id
    sl::Resource colorIn = {sl::ResourceType::eResourceTypeTex2d, viwport1->lowResInput};    
    setTag(&colorIn, sl::BufferType::eBufferTypeScalingInputColor, viewport1->id);
    // and so on ...

    // Now we can allocate our feature explicitly, again passing viewport id
    slAllocateResources(sl::eFeatureDLSS, viewport1->id);

    // Evaluate DLSS on viewport1, again passing viewport id so we can map tags, constants correctly
    //
    // NOTE: If slAllocateResources is not called DLSS resources would be initialized at this point
    slEvaluateFeature(myCmdList, sl::Feature::eFeatureDLSS, myFrameIndex, viewport1->id)

    // When we no longer need this viewport
    slFreeResources(sl::eFeatureDLSS, viewport1->id);
}

// Viewport2
{
    // We need to setup our constants first so sl.dlss plugin has enough information
    sl::DLSSConstants dlssConsts = {};
    dlssConsts.mode = viewport2->getDLSSMode(); // e.g. sl::eDLSSModeBalanced;
    dlssConsts.outputWidth = viewport2->getOutputWidth();    // e.g 1920;
    dlssConsts.outputHeight = viewport2->getOutputHeight(); // e.g. 1080;
    slSetFeatureConstants(sl::eFeatureDLSS, &dlssConsts, 0, viewport2->id);

    // Set our tags
    sl::Resource colorIn = {sl::ResourceType::eResourceTypeTex2d, viwport2->lowResInput};
    // Note that we are passing viewport id
    setTag(&colorIn, sl::BufferType::eBufferTypeScalingInputColor, viewport2->id);
    // and so on ...

    // Now we can allocate our feature explicitly, again passing viewport id
    slAllocateResources(sl::eFeatureDLSS, viewport2->id);
    
    // Evaluate DLSS on viewport2, again passing viewport id so we can map tags, constants correctly
    //
    // NOTE: If slAllocateResources is not called DLSS resources would be initialized at this point
    slEvaluateFeature(myCmdList, sl::Feature::eFeatureDLSS, myFrameIndex, viewport2->id)

    // When we no longer need this viewport
    slFreeResources(sl::eFeatureDLSS, viewport2->id);
}

```

#### 2.2.3 INITIALIZATION

To initialize an SDK instance, simply call the following method:

```cpp

//! Unique application ID
//!
//! Each application is assigned a unique ID.
//! If you do not have one please leave this value as 0.
constexpr int kUniqueApplicationId = 0;

//! Initializes the SL module
//!
//! Call this method when the game is initializing. 
//!
//! @param pref Specifies preferred behavior for the SL library.
//! @param applicationId Unique id provided by NVIDIA, can NOT be 0 in production builds.
//! @return false if SL is not supported on the system; true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool slInit(const Preferences &pref, int applicationId = kUniqueApplicationId);
```

> **IMPORTANT:**
> Please make sure to call `slInit` very early, before any DirectX/DXGI/Vulkan API calls are made and check verbose logging for any warnings or errors.

#### 2.2.4 ERROR HANDLING

Since SL is an interposer **it is not always possible to immediately report potential errors or warnings after a specific SL API is invoked**. This is because some functionality can be triggered with some delay or asynchronously when DirectX/Vulkan APIs are called by the host application.
Therefore, the SL SDK does not use error codes but instead provides `Preferences::logMessagesCallback` so that any errors or warnings can be tracked by placing breakpoints when `eLogTypeError` and/or `eLogTypeWarn` messages are received. Another useful feature is the debug console window, which should be
enabled in development by setting `Preferences::showConsole` to true. In the debug console window, each error will be highlighted in red while warnings are highlighted in yellow to make it easier to notice them.

### 2.3 CHECKING IF A FEATURE IS SUPPORTED

SL supports the following features:

```cpp
//! Features supported with this SDK
enum Feature
{
    //! Deep Learning Super Sampling
    eFeatureDLSS = 0,
    //! Real-Time Denoiser
    eFeatureNRD = 1,
    //! NVIDIA Image Scaling
    eFeatureNIS = 2,
    //! Low-Latency
    eFeatureLatency = 3,
    //! Common feature, NOT intended to be used directly
    eFeatureCommon = UINT_MAX
};
```

To check if a specific feature is available on the specific display adapter(s), you can call:

```cpp
//! Checks if a specific feature is supported or not.
//!
//! Call this method to check if a certain eFeature* (see above) is available.
//!
//! @param feature Specifies which feature to check
//! @param adapterBitMask Optional bit-mask specifying which adapter supports the give feature
//! @return false if feature is not supported on the system; true otherwise.
//!
//! NOTE: You can provide the adapter bit mask to ensure that feature is available on the adapter
//! for which you are planning to create a device. For the adapter at index N you can check the bit 1 << N.
//!
//! This method is NOT thread safe.
SL_API bool slIsFeatureSupported(sl::Feature feature, uint32_t* adapterBitMask = nullptr);
```

Here is some sample code showing how to use the adapter bit-mask to determine on which adapter a device should be created in order for the DLSS feature to work correctly:

```cpp
uint32_t adapterBitMask = 0;
if(!slIsFeatureSupported(sl::Feature::eFeatureDLSS, &adapterBitMask))
{
    // DLSS is not supported on the system, fallback to the default up-scaling method
}
// Now check your adapter
if((adapterBitMask & (1 << myAdapterIndex)) != 0)
{
    // It is OK to create a device on this adapter since feature we need is supported
}
```
> **NOTE:**  
> If `slIsFeatureSupported` returns false, you can enable the console window or use `logMessagesCallback` to find out why the specific feature is not supported.  
> If `slIsFeatureSupported` returns true, it means that the feature is supported by some adapter in the system.  In a multi-adapter system, you MUST check the bitmask to determine if the feature is supported on the desired adapter!

### 2.4 ENABLING OR DISABLING FEATURES

All loaded features are enabled by default. To explicitly enable or disable a specific feature use the following method:

```cpp
//! Sets the specified feature to either enabled or disabled state.
//!
//! Call this method to enable or disable certain eFeature*. 
//! All supported features are enabled by default and have to be disabled explicitly if needed.
//!
//! @param feature Specifies which feature to check
//! @param enabled Value specifying if feature should be enabled or disabled.
//! @return false if feature is not supported on the system or if device has not beeing created yet, true otherwise.
//!
//! NOTE: When this method is called no other DXGI/D3D/Vulkan APIs should be invoked in parallel so
//! make sure to flush your pipeline before calling this method.
//!
//! This method is NOT thread safe and requires DX/VK device to be created before calling it.
SL_API bool slSetFeatureEnabled(sl::Feature feature, bool enabled);
```
> **NOTE:**
> Please make sure to flush your pipeline and do not invoke DXGI/D3D/VULKAN API while this method is running.

You may also query whether a particular feature is currently enabled or not with the following method:

```cpp
//! Checks if specified feature is enabled or not.
//!
//! Call this method to check if feature is enabled.
//! All supported features are enabled by default and have to be disabled explicitly if needed.
//!
//! @param feature Specifies which feature to check
//! @return false if feature is disabled, not supported on the system or if device has not been created yet, true otherwise.
//!
//! This method is NOT thread safe and requires DX/VK device to be created before calling it.
SL_API bool slIsFeatureEnabled(sl::Feature feature);
```

### 2.5 TAGGING RESOURCES

The appropriate D3D12/VK resources should be tagged using the `slSetTag` API and the corresponding `BufferType` enum. 

```cpp
//! Buffer types used for tagging
enum BufferType : uint32_t
{
    //! Depth buffer - IMPORTANT - Must be suitable to use with clipToPrevClip transformation (see Constants below)
    eBufferTypeDepth,
    //! Object and optional camera motion vectors (see Constants below)
    eBufferTypeMVec,
    //! Color buffer with all post-processing effects applied but without any UI/HUD elements
    eBufferTypeHUDLessColor,
    //! Color buffer containing jittered input data for the image scaling pass
    eBufferTypeScalingInputColor,
    //! Color buffer containing results from the image scaling pass
    eBufferTypeScalingOutputColor,
    //! Normals
    eBufferTypeNormals,
    //! Roughness
    eBufferTypeRoughness,
    //! Albedo
    eBufferTypeAlbedo,
    //! Specular Albedo
    eBufferTypeSpecularAlbedo,
    //! Indirect Albedo
    eBufferTypeIndirectAlbedo,
    //! Specular Mvec
    eBufferTypeSpecularMVec,
    //! Disocclusion Mask
    eBufferTypeDisocclusionMask,
    //! Emissive
    eBufferTypeEmissive,
    //! Exposure
    eBufferTypeExposure,
    //! Buffer with normal and roughness in alpha channel
    eBufferTypeNormalRoughness,
    //! Diffuse and camera ray length
    eBufferTypeDiffuseHitNoisy,
    //! Diffuse denoised
    eBufferTypeDiffuseHitDenoised,
    //! Specular and reflected ray length
    eBufferTypeSpecularHitNoisy,
    //! Specular denoised
    eBufferTypeSpecularHitDenoised,
    //! Shadow noisy
    eBufferTypeShadowNoisy,
    //! Shadow denoised
    eBufferTypeShadowDenoised,
    //! AO noisy
    eBufferTypeAmbientOcclusionNoisy,
    //! AO denoised
    eBufferTypeAmbientOcclusionDenoised,

    //! Optional - UI/HUD pixels hint (set to 1 if a pixel belongs to the UI/HUD elements, 0 otherwise)
    eBufferTypeUIHint,
    //! Optional - Shadow pixels hint (set to 1 if a pixel belongs to the shadow area, 0 otherwise)
    eBufferTypeShadowHint,
    //! Optional - Reflection pixels hint (set to 1 if a pixel belongs to the reflection area, 0 otherwise)
    eBufferTypeReflectionHint,
    //! Optional - Particle pixels hint (set to 1 if a pixel represents a particle, 0 otherwise)
    eBufferTypeParticleHint,
    //! Optional - Transparency pixels hint (set to 1 if a pixel belongs to the transparent area, 0 otherwise)
    eBufferTypeTransparencyHint,
    //! Optional - Animated texture pixels hint (set to 1 if a pixel belongs to the animated texture area, 0 otherwise)
    eBufferTypeAnimatedTextureHint,
    //! Optional - Bias for current color vs history hint - lerp(history, current, bias) (set to 1 to completely reject history)
    eBufferTypeBiasCurrentColorHint,
    //! Optional - Ray-tracing distance (camera ray length)
    eBufferTypeRaytracingDistance,
    //! Optional - Motion vectors for reflections
    eBufferTypeReflectionMotionVectors,
    //! Optional - Position, in same space as eBufferTypeNormals
    eBufferTypePosition,
};

//! Tags resource
//!
//! Call this method to tag the appropriate buffers.
//!
//! @param resource Pointer to resource to tag, set to null to remove the specified tag
//! @param tag Specific tag for the resource
//! @param id Unique id (can be viewport id | instance id etc.)
//! @param extent The area of the tagged resource to use (if using the entire resource leave as null)
//! @return false if resource cannot be tagged or if device has not beeing created yet, true otherwise.
//!
//! This method is thread safe and requires DX/VK device to be created before calling it.
SL_API bool slSetTag(const sl::Resource *resource, sl::BufferType tag, uint32_t id = 0, const sl::Extent* extent = nullptr);
```

Resource state can be provided by the host or, if not, SL will use internal tracking. Here is an example:

```cpp
// Host providing native D3D12 resource state
//
// IMPORTANT: State needs to be correct when tagged resource is used by SL and not necessarily at this point when it is tagged.
//
sl::Resource mvec = { sl::ResourceType::eResourceTypeTex2d, mvecResource, nullptr, nullptr, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr };
// Host NOT providing native resource state
sl::Resource depth = { sl::ResourceType::eResourceTypeTex2d, depthResource};
slSetTag(&mvec, sl::eBufferTypeMVec);
slSetTag(&depth, sl::eBufferTypeDepth);
```

> **NOTE:**
> There is no need to transition tagged resources to any specific state. When used by the host application resources will always be in the state they where left before invoking any SL APIs.

All SL plugins require access to the following resources:

* `eBufferTypeDepth` (must contain values which can be used in clip space to transform 4D points e.g. clipToPrevClip * float4(x,y,depth,1))
* `eBufferTypeMVec` (object motion with or without camera motion - see `Constants::cameraMotionIncluded` flag)

The following resources are required by NRD:

* `eBufferTypeNormalRoughness`(packed normal + roughness in RGBA format)
* `eBufferTypeAmbientOcclusionNoisy`/`eBufferTypeAmbientOcclusionDenoised` if AO denoising is needed
* `eBufferTypeSpecularHitNoisy`/`eBufferTypeSpecularHitDenoised` if specular denoising is needed
* `eBufferTypeDiffuseHitNoisy`/`eBufferTypeDiffuseHitDenoised` if diffuse denoising is needed
* `eBufferTypeShadowNoisy`/`eBufferTypeShadowDenoised` if shadow denoising is needed

The following resources are required by DLSS:

* `eBufferTypeTAAUInputColor` (render resolution jittered color as input)
* `eBufferTypeTAAUOutputColor` (final/post-process resolution render target where the upscaled AA result is stored)

The following resources are required by DLSS next:

* `eBufferTypeAlbedo`
* `eBufferTypeSpecularAlbedo`
* `eBufferTypeIndirectAlbedo`
* `eBufferTypeNormals`
* `eBufferTypeRoughness`
* `eBufferTypeEmissive`
* `eBufferTypeDisocclusionMask`
* `eBufferTypeSpecularMVec`

The following resources are required by NIS:

* `eBufferTypeScalingInputColor`
* `eBufferTypeScalingOutputColor`

The following hints are optional but should be provided if they are easy to obtain:

* `eBufferTypeExposure` (1x1 buffer containing exposure of the current frame)
* `eBufferTypeUIHint` - boolean mask indicating which pixels represent the HUD/UI
* `eBufferTypeShadowHint` - boolean mask indicating which pixels represent the shadow area
* `eBufferTypeReflectionHint` - boolean mask indicating which pixels represent the transparency area
* `eBufferTypeParticleHint` - boolean mask indicating which pixels represent the reflections area
* `eBufferTypeTransparencyHint` - boolean mask indicating which pixels represent the particles

> **NOTE:**
> Tagged buffers should not be used for other purposes within a frame execution because SL plugins might need them at different stages. If that cannot be guaranteed, please make a copy and tag it instead of the original buffer.

### 2.6 PROVIDING ADDITIONAL INFORMATION

#### 2.6.1 COMMON CONSTANTS

Some additional information should be provided so that SL features can operate correctly. Please use `slSetConstants` to provide the required data ***as early in the frame as possible*** and make sure to set values for all fields in the following structure:

```cpp
struct Constants
{
    //! IMPORTANT: All matrices are row major (see float4x4 definition) and
    //! must NOT contain temporal AA jitter offset (if any). Clip space jitter offset
    //! should be provided as the additional parameter Constants::jitterOffset (see below)
            
    //! Specifies matrix transformation from the camera view to the clip space.
    float4x4 cameraViewToClip;
    //! Specifies matrix transformation from the clip space to the camera view space.
    float4x4 clipToCameraView;
    //! Optional - Specifies matrix transformation describing lens distortion in clip space.
    float4x4 clipToLensClip;
    //! Specifies matrix transformation from the current clip to the previous clip space.
    //! clipToPrevClip = clipToView * viewToWorld * worldToViewPrev * viewToClipPrev
    float4x4 clipToPrevClip;
    //! Specifies matrix transformation from the previous clip to the current clip space.
    //! prevClipToClip = clipToPrevClip.inverse()
    float4x4 prevClipToClip;
        
    //! Specifies clip space jitter offset
    float2 jitterOffset;
    //! Specifies scale factors used to normalize motion vectors (so the values are in [-1,1] range)
    float2 mvecScale;
    //! Optional - Specifies camera pinhole offset if used.
    float2 cameraPinholeOffset;
    //! Specifies camera position in world space.
    float3 cameraPos;
    //! Specifies camera up vector in world space.
    float3 cameraUp;
    //! Specifies camera right vector in world space.
    float3 cameraRight;
    //! Specifies camera forward vector in world space.
    float3 cameraFwd;
        
    //! Specifies camera near view plane distance.
    float cameraNear = INVALID_FLOAT;
    //! Specifies camera far view plane distance.
    float cameraFar = INVALID_FLOAT;
    //! Specifies camera field of view in radians.
    float cameraFOV = INVALID_FLOAT;
    //! Specifies camera aspect ratio defined as view space width divided by height.
    float cameraAspectRatio = INVALID_FLOAT;
    //! Specifies which value represents an invalid (un-initialized) value in the motion vectors buffer
    //! NOTE: This is only required if `cameraMotionIncluded` is set to false and SL needs to compute it.
    float motionVectorsInvalidValue = INVALID_FLOAT;

    //! Specifies if depth values are inverted (value closer to the camera is higher) or not.
    Boolean depthInverted = Boolean::eInvalid;
    //! Specifies if camera motion is included in the MVec buffer.
    Boolean cameraMotionIncluded = Boolean::eInvalid;
    //! Specifies if motion vectors are 3D or not.
    Boolean motionVectors3D = Boolean::eInvalid;
    //! Specifies if previous frame has no connection to the current one (i.e. motion vectors are invalid)
    Boolean reset = Boolean::eInvalid;
    //! Specifies if application is not currently rendering game frames (paused in menu, playing video cut-scenes)
    Boolean notRenderingGameFrames = Boolean::eInvalid;
    //! Specifies if orthographic projection is used or not.
    Boolean orthographicProjection = Boolean::eFalse;
    //! Specifies if motion vectors are already dilated or not.
    Boolean motionVectorsDilated = Boolean::eFalse;
    //! Specifies if motion vectors are jittered or not.
    Boolean motionVectorsJittered = Boolean::eFalse;

    //! Reserved for future expansion, must be set to null
    void* ext = {};
};


//! Sets common constants.
//!
//! Call this method to provide the required data (SL will keep a copy).
//!
//! @param values Common constants required by SL plugins (SL will keep a copy)
//! @param frameIndex Index of the current frame
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if constants cannot be set or if device has not beeing created yet, true otherwise.
//! 
//! This method is thread safe and requires DX/VK device to be created before calling it.
SL_API bool slSetConstants(const sl::Constants& values, uint32_t frameIndex, uint32_t id = 0);
```
> **NOTE:**
> Provided projection related matrices `should not` contain any clip space jitter offset. Jitter offset (if any) should be specified as a separate `float2` constant.

#### 2.6.2 FEATURE SPECIFIC CONSTANTS

Each feature requires specific data which is defined in a corresponding  `sl_<feature_name>.h` header file (e.g. `sl_dlss.h`, `sl_nrd.h`, etc.). To provide per feature specific data, use the following method:

```cpp
//! Sets feature specific constants.
//!
//! Call this method to provide the required data
//! for the specified feature (SL will keep a copy).
//!
//! @param feature Feature we are working with
//! @param consts Pointer to the feature specific constants (SL will keep a copy)
//! @param frameIndex Index of the current frame
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if constants cannot be set or if device has not beeing created yet, true otherwise.
//!
//! This method is thread safe and requires DX/VK device to be created before calling it.
SL_API bool slSetFeatureConstants(sl::Feature feature, const void *consts, uint32_t frameIndex, uint32_t id = 0);
```

> **NOTE:**
> To disable any given feature, simply set its mode to the `off` state. This will also unload any resources used by this feature (if it was ever invoked before). For example, to disable DLSS one would set `DLSSConstants::mode = eDLSSModeOff`.

### 2.7 FEATURE SETTINGS

Some features provide feedback to the host application specifying which rendering settings are optimal or preferred. To check if a certain feature has specific settings, you can call:

```cpp
//! Gets feature specific settings.
//!
//! Call this method to obtain settings for the specified feature.
//!
//! @param feature Feature we are working with
//! @param consts Pointer to the feature specific constants
//! @param settings Pointer to the returned feature specific settings
//! @return false if feature does not have settings or if device has not beeing created yet, true otherwise.
//!
//! This method is NOT thread safe and requires DX/VK device to be created before calling it.
SL_API bool slGetFeatureSettings(sl::Feature feature, const void* consts, void* settings);
```

For example, when using DLSS, it is mandatory to call `getFeatureSettings` to find out at which resolution we should render your game:

```cpp
sl::DLSSSettings dlssSettings;
sl::DLSSConstants dlssConsts;
// These are populated based on user selection in the UI
dlssConsts.mode = myUI->getDLSSMode(); // e.g. sl::eDLSSModeBalanced;
dlssConsts.outputWidth = myUI->getOutputWidth();    // e.g 1920;
dlssConsts.outputHeight = myUI->getOutputHeight(); // e.g. 1080;
// Now let's check what should our rendering resolution be
if(!slGetFeatureSettings(sl::eFeatureDLSS, &dlssConsts, &dlssSettings))
{
    // Handle error here
}
// Setup rendering based on the provided values in the sl::DLSSSettings structure
myViewport->setSize(dlssSettings.renderWidth, dlssSettings.renderHeight);
```

> **NOTE:**
> Not all features provide specific/optimal settings. Please refer to the appropriate, feature specific header for more details.

### 2.8 MARKING EVENTS IN THE PIPELINE

By design, SL SDK enables `host assisted replacement or injection of specific rendering features`. In order for SL features to work, specific sections in the rendering pipeline need to be marked with the following method:

```cpp
//! Evaluates feature
//! 
//! Use this method to mark the section in your rendering pipeline 
//! where specific feature should be injected.
//!
//! @param cmdBuffer Command buffer to use (must be created on device where feature is supported but can be null if not needed)
//! @param feature Feature we are working with
//! @param frameIndex Current frame index (can be 0 if not needed)
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if feature event cannot be injected in the command buffer or if device has not beeing created yet, true otherwise.
//! 
//! IMPORTANT: frameIndex and id must match whatever is used to set common and or feature constants (if any)
//!
//! This method is NOT thread safe and requires DX/VK device to be created before calling it.
SL_API bool slEvaluateFeature(sl::CommandBuffer* cmdBuffer, sl::Feature feature, uint32_t frameIndex, uint32_t id = 0);
```

Here is some pseudo code showing how the host application can replace the default up-scaling method with DLSS on hardware which supports it:

```cpp
// Make sure DLSS is available and user selected this option in the UI
bool useDLSS = slIsFeatureSupported(sl::eFeatureDLSS) && userSelectedDLSSInUI;
if(useDLSS) 
{
    // For example, here we assume user selected DLSS balanced mode and 4K resolution with no sharpening
    DLSSConstants consts = {eDLSSModeBalanced, 3840, 2160, 0.0}; 
    slSetFeatureConstants(sl::Feature::eFeatureDLSS, &consts, myFrameIndex);    
    // Inform SL that DLSS should be injected at this point
    if(!slEvaluateFeature(myCmdList, sl::Feature::eFeatureDLSS, myFrameIndex)) 
    {
        // Handle error
    }
}
else
{
    // Default up-scaling pass like, for example, TAAU goes here
}
```
### 2.9 SHUTDOWN

To release the SDK instance and all resources allocated with it, use the following method:

```cpp
//! Shuts down the SL module
//!
//! Call this method when the game is shutting down. 
//!
//! @return false if SL did not shutdown correctly; true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool slShutdown();
```
> **NOTE:**
> If shutdown is called too early, any SL features which are enabled and running will stop functioning and the host application will fallback to the
default implementation. For example, if DLSS is enabled and running and shutdown is called, the `sl.dlss` plugin will be unloaded, hence any `evaluateFeature` or `slIsFeatureSupported` calls will return an error and the host application should fallback to the default implementation (for example TAAU)

3 VALIDATING SL INTEGRATION WHEN REPLACING PLATFORM LIBRARIES
-----------------------------

If you are integrating Streamline by replacing the standard libraries with sl.interposer.lib, ensure that `sl.interposer.dll` is the only dependency in your application and no dependency on `dxgi.dll`, `d3d11.dll`, `d3d12.dll` or `vulkan-1.dll` remains.  To do so, please do the following:

* Open a Developer Command Prompt for Visual Studio
* Run "dumpbin /dependents `my engine exe/dll that links sl.interposer.dll`"
* Look for `dxgi.dll`, `d3d11.dll`, `d3d12.dll` or `vulkan-1.dll` in the list of dependents. If you see either of those or if `sl.interposer.dll` is missing in the list then **SL was NOT integrated correctly** in your application.

Note that if you are using a different method of integration where you continue to link to the standard platform libraries, this method does not apply.

4 DISTRIBUTING SL WITH YOUR APPLICATION
-----------------------------

SL SDK comes with several binaries which need to be distributed with your application. Here are the mandatory modules:

* sl.interposer.dll
* sl.common.dll

The remaining modules are optional depending on which features are enabled in your application. For example if DLSS/NRD are used, you need to include:

* sl.dlss.dll, nvngx_dlss.dll
* sl.nrd.dll, nrd.dll

> **NOTE:**
> If SL binaries are not installed next to the host executable, please make sure to specify the absolute paths to look for them using the `Preferences`

5 COMMON ISSUES AND HOW TO RESOLVE THEM
------------------------------------------------

* If you get a crash in Swapchain::Present or some similar unexpected behavior please double check that you are NOT linking dxgi.lib/d3d12.lib together with the sl.interposer.dll. See [section 3](#3-validating-sl-integration) in this guide.
* If SL does not work correctly make sure that some other library which includes `dxgi` or `d3d11/12` is not linked in your application (like for example `WindowsApp.lib`)
* Make sure that all matrices are multiplied in the correct order and provided in row-major order **without any jitter**
* Motion vector scaling factors in `sl::Constants` transform values from `eBufferTypeMvec` to the {-1,1} range
* Jitter offset should be provided in pixel space
* If state tracking in `D3D12GraphicsCommandList` causes a problem:
    * It can be disabled by providing `ePreferenceFlagDisableCLStateTracking` flag in `sl::Preferences`
    * IMPORTANT: In this scenario CL state must be restored by the host application after each call to `slEvaluateFeature`

6 SUPPORT
------------------------------------------------
For any SL related questions, please email StreamlineSupport@nvidia.com. 
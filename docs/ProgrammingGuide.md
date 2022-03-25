

Streamline - SL
=======================

Version 1.0.1
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

The SL SDK comes with two header files, `sl.h` and `sl_consts.h`, which are located in the ./include folder. In addition to including SL header files, your project should link against the export library `sl.interposer.lib` (which is provided in the ./lib/x64 folder) and if using Vulkan distribute the included `VkLayer_streamline.json` with your application. Since SL is an interposer, there are several ways it can be integrated into your application:

* If you are statically linking `d3d11.lib`, `d3d12.lib` and `dxgi.lib` **make sure to remove them** and link `sl.interposer.lib` instead
* If you are dynamically loading `d3d11.dll`, `d3d12.dll` and `dxgi.dll` dynamically load `sl.interposer.dll` instead 
* If you are using Vulkan simply enable `VK_LAYER_Streamline` and include `VkLayer_streamline.json` with your distribution

> **IMPORTANT:**
> Vulkan support is under development and does not function correctly in the current release.

If you want full control, simply incorporate SL source code into your engine and instead of creating standard DirectX/DXGI interfaces, use proxies provided by the SL SDK. Please note that in this scenario you also need to ensure that SL plugin manager is initialized properly and SL plugins are loaded and mapped correctly, otherwise SL features will not work.

> **IMPORTANT:**
> When running on hardware which does not support SL, `sl.interposer.dll` serves as a simple pass-through layer with negligible impact on performance. 

#### 2.1.1 SECURITY

All modules provided in the `./bin/x64` SDK folder are digitally signed by NVIDIA. There are two digital signatures on each SL module, one is standard Windows store certificate and can be validated using `WinVerifyTrust` while the other is custom NVIDIA certificate used to handle scenarios where OS is compromised and Windows store certificate cannot be trusted. To secure your application from potentially malicious replacement modules please do the following:

* Validate the digital signature on ***all Streamline modules*** using the `WinVerifyTrust` Win32 API when starting your application.
* Validate the public key for the NVIDIA custom digital certificate on `sl.interposer.dll` if using binary provided by NVIDIA (see `source/core/sl.security/secureLoadLibrary` for more details)

***It is strongly recommended*** to use the provided `sl.interposer.dll` binary and follow the above guidelines. Prebuilt binary automatically performs the above steps when loading SL plugins to ensure maximum security. If you decide to build your own `sl.interposer.dll` make sure to enforce your own strict security policies.

### 2.2 INITIALIZING SL 

#### 2.2.1 PREFERENCES

To control the behavior of the SL SDK, some preferences can be specified using the following data structure:

```cpp
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
    unsigned int numPathsToPlugins = 0;
    //! Optional - Absolute path to location where logs and other data should be stored
    //! NOTE: Set this to nullptr in order to disable logging to a file
    const wchar_t *pathToLogsAndData = {};
    //! Optional - Allows resource allocation tracking on the host side
    pfunResourceAllocateCallback* allocateCallback = {};
    //! Optional - Allows resource deallocation tracking on the host side
    pfunResourceReleaseCallback* releaseCallback = {};
    //! Optional - Allows log message tracking including critical errors if they occur
    pfunLogMessageCallback* logMessageCallback = {};
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};
```

> **IMPORTANT:**
> If SL plugins are not located next to the host executable then the absolute paths to locations where to look for them must be specified by setting the `pathsToPlugins` field in the `Preferences` structure. Plugins will be loaded from the first path where they can be found.

##### 2.2.1.1 LOGGING AND DEBUG CONSOLE WINDOW

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
> If logging callback is specified then SL will not use `OutputDebugString` debug API.

##### 2.2.1.2 MEMORY MANAGEMENT

SL can hand over the control over resource allocation and de-allocation, if requested. When specified, the following callbacks on the host
side will be **fully responsible for the resource allocation and destruction**.

```cpp
//! Resource description
struct ResourceDesc
{
    //! Indicates the type of resource
    ResourceType type;
    //! Indicates if resource is a buffer or not
    bool buffer;
    //! D3D12_RESOURCE_DESC/VkImageCreateInfo/VkBufferCreateInfo
    void *desc;
    //! D3D12_RESOURCE_STATES or VkMemoryPropertyFlags
    unsigned int state;
    //! CD3DX12_HEAP_PROPERTIES or nullptr
    void *heap;
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

//! Native resource
struct Resource
{
    //! Indicates the type of resource
    ResourceType type;
    //! Indicates if resource is a buffer or not
    bool buffer;
    //! ID3D12Resource/VkBuffer/VkImage
    void *native;
    //! vkDeviceMemory or nullptr
    void *memory;
    //! VkImageView or nullptr
    void *view;    
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

//! Resource allocation/de-allocation callbacks
//!
//! Use these callbacks to gain full control over 
//! resource life cycle and memory allocation tracking.
//!
//! IMPORTANT: All resources must be able to be bound as UAV input/output.
//! In addition, all textures are also sampled as NON pixel shader resources.
using pfunResourceAllocateCallback = Resource(const ResourceDesc *desc);
using pfunResourceReleaseCallback = void(Resource *resource);
```

> **NOTE:**
> Memory management done by the host is an optional feature and it is NOT required for the SL to work properly. If used, proper care needs to be taken to avoid releasing resources which are still used by the GPU.

#### 2.2.2 INITIALIZATION

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
SL_API bool init(const Preferences &pref, int applicationId = kUniqueApplicationId);
```

> **IMPORTANT:**
> Please make sure to call `sl::init` very early, before any DirectX/DXGI/Vulkan API calls are made and check verbose logging for any warnings or errors.

#### 2.2.3 ERROR HANDLING

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
    eFeatureDLSS,
    //! Real-Time Denoiser
    eFeatureNRD,
    //! Total count
    eFeatureCount
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
SL_API bool isFeatureSupported(Feature feature, uint32_t* adapterBitMask = nullptr);
```

Here is some sample showing how to use the adapter bit-mask to determine on which adapter a device should be created in order for DLSS feature to work correctly:

```cpp
uint32_t adapterBitMask = 0;
if(!isFeatureSupported(sl::Feature::eFeatureDLSS, &adapterBitMask))
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
> If `isFeatureSupported` returns false, you can enable the console window or use `logMessagesCallback` to find out why the specific feature is not supported.

### 2.4 TAGGING RESOURCES

The appropriate D3D12/VK resources should be tagged using the `sl::setTag` API and the corresponding `BufferType` enum. 

```cpp
//! Buffer types used for tagging
enum BufferType
{
    //! Depth buffer - IMPORTANT - Must be suitable to use with clipToPrevClip transformation (see Constants below)
    eBufferTypeDepth,
    //! Object and optional camera motion vectors (see Constants below)
    eBufferTypeMVec,
    //! Color buffer with all post-processing effects applied but without any UI/HUD elements
    eBufferTypeHUDLessColor,
    //! Color buffer containing jittered input data for DLSS pass (same as input for the default TAAU pass)
    eBufferTypeDLSSInputColor,
    //! Color buffer containing results from the DLSS pass (same as the output for the default TAAU pass)
    eBufferTypeDLSSOutputColor,
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
    //! Diffuse signal and camera ray length
    eBufferTypeDiffuseHitNoisy,
    //! Diffuse denoised
    eBufferTypeDiffuseHitDenoised,
    //! Specular signal and reflected ray length
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

    //! Total count
    eBufferTypeCount
};

//! Tags resource
//!
//! Call this method to tag the appropriate buffers.
//!
//! @param resource Pointer to resource to tag, set to null to remove the specified tag
//! @param tag Specific tag for the resource
//! @param id Unique id (can be viewport id | instance id etc.)
//! @param extent The area of the tagged resource to use (if using the entire resource leave as null)
//! @return false if resource cannot be tagged true otherwise.
//!
//! This method is thread safe.
SL_API bool setTag(Resource* resource, BufferType tag, uint32_t id = 0, const Extent* extent = nullptr);
```

> **NOTE:**
> SL manages resource states so there is no need to transition tagged resources. When used by your application resources will always be in the state you left them before invoking any SL APIs.

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

* `eBufferTypeDLSSInputColor` (render resolution jittered color as input)
* `eBufferTypeDLSSOutputColor` (final/post-process resolution render target where the upscaled AA result is stored)

The following resources are required by DLSS next:

* `eBufferTypeAlbedo`
* `eBufferTypeSpecularAlbedo`
* `eBufferTypeIndirectAlbedo`
* `eBufferTypeNormals`
* `eBufferTypeRoughness`
* `eBufferTypeEmissive`
* `eBufferTypeDisocclusionMask`
* `eBufferTypeSpecularMVec`

The following hints are optional but should be provided if they are easy to obtain:

* `eBufferTypeExposure` (1x1 buffer containing exposure of the current frame)
* `eBufferTypeUIHint` - boolean mask indicating which pixels represent the HUD/UI
* `eBufferTypeShadowHint` - boolean mask indicating which pixels represent the shadow area
* `eBufferTypeReflectionHint` - boolean mask indicating which pixels represent the transparency area
* `eBufferTypeParticleHint` - boolean mask indicating which pixels represent the reflections area
* `eBufferTypeTransparencyHint` - boolean mask indicating which pixels represent the particles

> **NOTE:**
> Tagged buffers should not be used for other purposes within a frame execution because SL plugins might need them at different stages. If that cannot be guaranteed, please make a copy and tag it instead of the original buffer.

### 2.5 PROVIDING ADDITIONAL INFORMATION

#### 2.5.1 COMMON CONSTANTS

Some additional information should be provided so that SL features can operate correctly. Please use `sl::setConstants` to provide the required data ***as early in the frame as possible*** and make sure to set values for all fields in the following structure:

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
    float motionVectorsInvalidValue = INVALID_FLOAT;

    //! Specifies if tagged color buffers are full HDR (rendering to an HDR monitor) or not 
    Boolean colorBuffersHDR = Boolean::eInvalid;
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
    Boolean orthographicProjection = Boolean::eInvalid;
    //! Specifies if motion vectors are already dilated or not.
    Boolean motionVectorsDilated = Boolean::eInvalid;
    //! Specifies if motion vectors are jittered or not.
    Boolean motionVectorsJittered = Boolean::eFalse;

    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

//! Sets common constants.
//!
//! Call this method to provide the required data.
//!
//! @param values Common constants required by SL plugins
//! @param frameIndex Index of the current frame
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if constants cannot be set; true otherwise.
//! 
//! This method is NOT thread safe.
SL_API bool setConstants(const Constants& values, uint32_t frameIndex, uint32_t id = 0);
```
> **NOTE:**
> Provided projection related matrices `should not` contain any clip space jitter offset. Jitter offset (if any) should be specified as a separate `float2` constant.

#### 2.5.2 FEATURE SPECIFIC CONSTANTS

Each feature requires specific data which is defined in the `sl_consts.h` header file. To provide per feature specific data, use the following method:

```cpp
//! Sets feature specific constants.
//!
//! Call this method to provide the required data
//! for the specified feature.
//!
//! @param feature Feature we are working with
//! @param consts Pointer to the feature specific constants
//! @param frameIndex Index of the current frame
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if constants cannot be set; true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool setFeatureConstants(Feature feature, const void *consts, uint32_t frameIndex, uint32_t id = 0);
```

> **NOTE:**
> To disable any given feature, simply set its mode to the `off` state. This will also unload any resources used by this feature (if it was ever invoked before). For example, to disable DLSS one would set `DLSSConstants::mode = eDLSSModeOff`.

### 2.6 FEATURE SETTINGS

Some features provide feedback to the host application specifying which rendering settings are optimal or preferred. To check if a certain feature has specific settings, you can call:

```cpp
//! Gets feature specific settings.
//!
//! Call this method to obtain settings for the specified feature.
//!
//! @param feature Feature we are working with
//! @param consts Pointer to the feature specific constants
//! @param settings Pointer to a feature specific settings
//! @return false if constants cannot be set; true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool getFeatureSettings(Feature feature, const void* consts, void* settings);
```

For example, when using DLSS, it is mandatory to call `getFeatureSettings` to find out at which resolution we should render our game:

```cpp
sl::DLSSSettings dlssSettings;
sl::DLSSConstants dlssConsts;
// These are populated based on user selection in the UI
dlssConsts.mode = myUI->getDLSSMode(); // e.g. sl::eDLSSModeBalanced;
dlssConsts.outputWidth = myUI->getOutputWidth();    // e.g 1920;
dlssConsts.outputHeight = myUI->getOutputHeight(); // e.g. 1080;
// Now let's check what should our rendering resolution be
if(!sl::getFeatureSettings(sl::eFeatureDLSS, &dlssConsts, &dlssSettings))
{
    // Handle error here
}
// Setup rendering based on the provided values in the sl::DLSSSettings structure
myViewport->setSize(dlssSettings.renderWidth, dlssSettings.renderHeight);
```

> **NOTE:**
> Not all features provide specific/optimal settings. Please refer to the `sl_consts.h` header for more details.

### 2.7 MARKING EVENTS IN THE PIPELINE

By design, SL SDK enables `host assisted replacement or injection of specific rendering features`. In order for SL features to work, specific sections in the rendering pipeline need to be marked with the following method:

```cpp
//! Evaluates feature
//! 
//! Use this method to mark the section
//! in your rendering pipeline where a specific feature
//! should be injected.
//!
//! @param cmdBuffer Command buffer to use - must be created on device where feature is supported
//! @param feature Feature we are working with
//! @param frameIndex Current frame index (must match the corresponding value in the sl::Constants)
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if feature event cannot be injected in the command buffer; true otherwise.
//!
//! IMPORTANT: Unique id must match whatever is used to set constants
//!
//! This method is NOT thread safe.
SL_API bool evaluateFeature(CommandBuffer* cmdBuffer, Feature feature, uint32_t frameIndex, uint32_t id = 0);
```

Here is some pseudo code showing how the host application can replace the default up-scaling method with DLSS on hardware which supports it:

```cpp
// Make sure DLSS is available and user selected this option in the UI
bool useDLSS = sl::isFeatureSupported(sl::eFeatureDLSS) && userSelectedDLSSInUI;
if(useDLSS) 
{
    // For example, here we assume user selected DLSS balanced mode and 4K resolution with no sharpening
    DLSSConstants consts = {eDLSSModeBalanced, 3840, 2160, 0.0}; 
    sl::setFeatureConstants(sl::Feature::eFeatureDLSS, &consts, myFrameIndex);    
    // Inform SL that DLSS should be injected at this point
    if(!sl::evaluateFeature(myCmdList, sl::Feature::eFeatureDLSS, myFrameIndex)) 
    {
        // Handle error
    }
}
else
{
    // Default up-scaling pass like, for example, TAAU goes here
}
```
### 2.8 SHUTDOWN

To release the SDK instance and all resources allocated with it, use the following method:
```cpp
//! Shuts down the SL module
//!
//! Call this method when the game is shutting down. 
//!
//! @return false if SL did not shutdown correctly; true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool shutdown();
```
> **NOTE:**
> If shutdown is called too early, any SL features which are enabled and running will stop functioning and the host application will fallback to the
default implementation. For example, if DLSS is enabled and running and shutdown is called, the `sl.dlss` plugin will be unloaded, hence any `evaluateFeature` or `isFeatureSupported` calls will return an error and the host application should fallback to the default implementation (for example TAAU)

3 VALIDATING SL INTEGRATION
-----------------------------

To ensure that `sl.interposer.dll` is the only dependency in your application and you did not accidentally link `dxgi.dll`, `d3d12.dll` or `vulkan-1.dll` please do the following:

* Open a Developer Command Prompt for Visual Studio
* Run "dumpbin /dependents `my engine exe/dll that links sl.interposer.dll`"
* Look for `dxgi.dll`, `d3d12.dll` or `vulkan-1.dll` in the list of dependents. If you see either of those or if `sl.interposer.dll` is missing in the list then **SL was NOT integrated correctly** in your application.

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

* If you get a crash in Swapchain::Present or some similar unexpected behavior please double check that you are NOT linking dxgi.lib/d3d12.lib together with the sl.interposer.dll
* Make sure that all matrices are multiplied in the correct order and provided in row-major order **without any jitter**
* Motion vector scaling factors in `sl::Constants` transform values from `eBufferTypeMvec` to the {-1,1} range
* Jitter offset should be provided in pixel space

6 SUPPORT
------------------------------------------------
For any SL related questions, please email StreamlineSupport@nvidia.com. 
Streamline - NRD (Beta)
=======================
>This feature is currently in Beta stage. Your experience with this feature may vary.

>The focus of this guide is on using Streamline to integrate NVIDIA Real-Time Denoisers (NRD) into an application.  For more information about NRD itself, please visit the [NVIDIA Developer NRD Page](https://developer.nvidia.com/rtx/ray-tracing/rt-denoisers)

Version 2.2.1
=======

### 1.0 INITIALIZE AND SHUTDOWN

Call `slInit` as early as possible (before any d3d11/d3d12/vk APIs are invoked)

```cpp
#include <sl.h>
#include <sl_consts.h>
#include <sl_nrd.h>

sl::Preferences pref;
pref.showConsole = true;                        // for debugging, set to false in production
pref.logLevel = sl::eLogLevelDefault;
pref.pathsToPlugins = {}; // change this if Streamline plugins are not located next to the executable
pref.numPathsToPlugins = 0; // change this if Streamline plugins are not located next to the executable
pref.pathToLogsAndData = {};                    // change this to enable logging to a file
pref.logMessageCallback = myLogMessageCallback; // highly recommended to track warning/error messages in your callback
if(!slInit(pref, myApplicationId))
{
    // Handle error, check the logs
}
```

For more details please see [preferences](ProgrammingGuide.md#221-preferences)

Call `slShutdown()` before destroying dxgi/d3d11/d3d12/vk instances, devices and other components in your engine.

```cpp
if(!slShutdown())
{
    // Handle error, check the logs
}
```

### 2.0 CHECK IF NRD IS SUPPORTED

As soon as SL is initialized, you can check if NRD is available for the specific adapter you want to use:

```cpp
Microsoft::WRL::ComPtr<IDXGIFactory> factory;
if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory)))
{
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter{};
    uint32_t i = 0;
    while (factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc{};
        if (SUCCEEDED(adapter->GetDesc(&desc)))
        {
            sl::AdapterInfo adapterInfo{};
            adapterInfo.deviceLUID = (uint8_t*)&desc.AdapterLuid;
            adapterInfo.deviceLUIDSizeInBytes = sizeof(LUID);
            if (SL_FAILED(result, slIsFeatureSupported(sl::kFeatureNRD, adapterInfo)))
            {
                // Requested feature is not supported on the system, fallback to the default method
                switch (result)
                {
                    case sl::Result::eErrorOSOutOfDate:         // inform user to update OS
                    case sl::Result::eErrorDriverOutOfDate:     // inform user to update driver
                    case sl::Result::eErrorNoSupportedAdapter:  // cannot use this adapter (older or non-NVDA GPU etc)
                    // and so on ...
                };
            }
            else
            {
                // Feature is supported on this adapter!
            }
        }
        i++;
    }
}
```

### 3.0 PREPARE RESOURCES FOR DENOISING

NRD expects input and output buffers to be provided in specific formats. For example

* `eNormalRoughness`       (RGB normal, A roughness)
* `eSpecularHitNoisy`      (RGB color,  A reflection ray hitT)
* `eDiffuseHitNoisy`       (RGB color,  A primary ray hitT)
* `eAmbientOcclusionNoisy` (R ambient)

Also, NRD expects input buffers to be encoded using encoders provided in NRD SDK. These shader encoders are not provided as a part of Streamline SDK. Encoders may be run at the end of a pass rendering the data into the buffers. Optionally, they can be run in a separate pass. The following code snippet illustrates encoding buffers by these encoders:

```cpp
// Convert for NRD
float4 outDiff = 0.0;
float4 outSpec = 0.0;
float4 outDiffSh = 0.0;
float4 outSpecSh = 0.0;

// depending on the type of denoiser used, different encoders are applicable
if( gDenoiserType == RELAX )
{
// similarly different denoising modes require different encoders
#if( NRD_MODE == SH )
    outDiff = RELAX_FrontEnd_PackSh( diffRadiance, diffHitDist, diffDirection, outDiffSh, USE_SANITIZATION );
    outSpec = RELAX_FrontEnd_PackSh( specRadiance, specHitDist, specDirection, outSpecSh, USE_SANITIZATION );
#else
    outDiff = RELAX_FrontEnd_PackRadianceAndHitDist( diffRadiance, diffHitDist, USE_SANITIZATION );
    outSpec = RELAX_FrontEnd_PackRadianceAndHitDist( specRadiance, specHitDist, USE_SANITIZATION );
#endif
}
else
{
#if( NRD_MODE == OCCLUSION )
    outDiff = diffHitDist;
    outSpec = specHitDist;
#elif( NRD_MODE == SH )
    outDiff = REBLUR_FrontEnd_PackSh( diffRadiance, diffHitDist, diffDirection, outDiffSh, USE_SANITIZATION );
    outSpec = REBLUR_FrontEnd_PackSh( specRadiance, specHitDist, specDirection, outSpecSh, USE_SANITIZATION );
#elif( NRD_MODE == DIRECTIONAL_OCCLUSION )
    outDiff = REBLUR_FrontEnd_PackDirectionalOcclusion( diffDirection, diffHitDist, USE_SANITIZATION );
#else
    outDiff = REBLUR_FrontEnd_PackRadianceAndNormHitDist( diffRadiance, diffHitDist, USE_SANITIZATION );
    outSpec = REBLUR_FrontEnd_PackRadianceAndNormHitDist( specRadiance, specHitDist, USE_SANITIZATION );
#endif
}

// with required values already computed, write the results to respective buffers
WriteResult( checkerboard, outPixelPos, outDiff, outSpec, outDiffSh, outSpecSh );
```

Similarly outputs are expected  returned from the plugin need to be decoded using decoders provided in NRD SDK. These , so make sure to run a pre-processing pass (simple compute shader) to prepare them:

### 4.0 PROVIDE COMMON CONSTANTS

Various per frame camera related constants are required by all Streamline features and must be provided ***if any SL feature is active and as early in the frame as possible***. Please keep in mind the following: 

* All SL matrices are row-major and should not contain any jitter offsets
* If motion vector values in your buffer are in {-1,1} range then motion vector scale factor in common constants should be {1,1}
* If motion vector values in your buffer are NOT in {-1,1} range then motion vector scale factor in common constants must be adjusted so that values end up in {-1,1} range

```cpp
sl::Constants consts = {};
// Set motion vector scaling based on your setup
consts.mvecScale = {1,1}; // Values in eMotionVectors are in [-1,1] range
consts.mvecScale = {1.0f / renderWidth,1.0f / renderHeight}; // Values in eMotionVectors are in pixel space
consts.mvecScale = myCustomScaling; // Custom scaling to ensure values end up in [-1,1] range
// Set all other constants here
if(!setConstants(consts, myFrameIndex)) // constants are changing per frame so frame index is required
{
    // Handle error, check logs
}
```
For more details please see [common constants](ProgrammingGuide.md#251-common-constants)


### 5.0 TAG ALL REQUIRED RESOURCES

Buffer tags provided by Streamline are either general or feature specific. General buffer tags are defined in sl.h header file, for example

```cpp
//! Object and optional camera motion vectors (see Constants below)
constexpr BufferType kBufferTypeMotionVectors = 1;
```

NRD feature specific buffer tags are defined in sl_nrd.h header file, for example
```cpp
constexpr BufferType kBufferTypeInDiffuseRadianceHitDist =      FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 0);
constexpr BufferType kBufferTypeInSpecularRadianceHitDist =     FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 1);
constexpr BufferType kBufferTypeInDiffuseHitDist =              FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 2);
```

Each denoiser provided by the plugin has its own set of required buffers. Required buffers must be tag appropriately with generic and feature specific buffer tags in order for the denoiser to work. All of the denoisers require tagging three buffers, these are motion vectors buffer, normal roughness buffer and depth (called also view-z) buffer. These three should be tagged using general buffer tags `kBufferTypeMotionVectors`, `kBufferTypeNormalRoughness` and `kBufferTypeDepth`. Remaining buffers must be tagged depending on the denoiser, for example Reblur Specular-Diffuse denoiser requires, other than the aforementioned three buffers, the following buffers: input diffuse radiance with hit distance, input specular radiance with hit distance, diffuse confidence (optionally), specular confidence (optionally), output diffuse radiance with hit distance, output specular radiance with hit distance.

The following code snippet illustrates resource tagging:

```cpp
// create and populate sl::Resource structure
sl::Resource resource = {};
resource.type = sl::ResourceType::eTex2d;
resource.memory = nullptr;
resource.view = nullptr;

// obtain a native pointer to the resource
resource.native = GetNative(info.texture);
resource.state = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

// obtain a native format representation (i.e. DXGI_FORMAT_R16_UNORM)
resource.nativeFormat = GetNative(info.format);

// create and populate sl::ResourceTag structure
sl::ResourceTag resTag = {};
resTag.resource = &resource;
resTag.type = sl::kBufferTypeInDiffuseRadianceHitDist;

// currently nrd plugin accepts only resources with eValidUntilEvaluate lifecycle
resTag.lifecycle = sl::ResourceLifecycle::eValidUntilEvaluate;

// create a viewport handle
sl::ViewportHandle viewportHandle{ m_id };

// call slSetTag method
slSetTag(viewportHandle, &resTag, 1, commandBuffer);
```
> **NOTE:**
> If dynamic resolution is used then please specify the extent for each tagged resource. Please note that SL **manages resource states so there is no need to transition tagged resources**.

### 6.0 PROVIDE NRD CONSTANTS

Additional to the Streamline common constants, NRD uses a plugin-specific set of constants represented by structure sl::NRDConstants. These must be provided in order for NRD to work. To provide NRD constants to the plugin, please call plugin specific `slNRDSetConstants` method. Please note that the `viewportHandle` provided along the constants must be the same viewport handle which will later be provided in an `slEvaluateFeature` call. 

```cpp
// prepare a viewport handle to accompany the constants
// the constants are set per viewport, so a handle created with this 
// m_id value must be later provided in slEvaluateFeature call
sl::ViewportHandle viewportHandle{ m_id };

// call slNRDSetConstants with populated NRDConstants object
slNRDSetConstants(viewportHandle, nrdConstants);
```

The following code snippet illustrates how to populate NRDConstants structure:

```cpp
sl::NRDConstants nrdConsts = {};
// Depending on what type of denoising we are doing, set the method mask
if (enableAo)
{
    nrdConsts.methodMask |= (1 << sl::NRDMethods::eNRDMethodReblurDiffuseOcclusion);
}
else if (enableDiffuse && enableSpecular)
{
    nrdConsts.methodMask |= (1 << sl::NRDMethods::eNRDMethodReblurDiffuseSpecular);
}
else if (enableDiffuse)
{
    nrdConsts.methodMask |= (1 << sl::NRDMethods::eNRDMethodReblurDiffuse);
}
else if (enableSpecular)
{
    nrdConsts.methodMask |= (1 << sl::NRDMethods::eNRDMethodReblurSpecular);
}

// Various camera matrices must be provided
nrdConsts.clipToWorld = myMatrix;
nrdConsts.clipToWorldPrev = myMatrix; // clipToWorld from the previous frame
nrdConsts.common = {};
nrdConsts.common.worldToViewMatrix = myworldToViewMatrix;
nrdConsts.common.motionVectorScale[0] = myMVecScaleX; // Normally 1
nrdConsts.common.motionVectorScale[1] = myMVecScaleY; // Normally 1
nrdConsts.common.resolutionScale[0] = myScaleX; // Are we rendering within a larger render target? If so set scale, otherwise set to 1.0f
nrdConsts.common.resolutionScale[1] = myScaleY; // Are we rendering within a larger render target? If so set scale, otherwise set to 1.0f
nrdConsts.common.denoisingRange = myDenoisingRange;
nrdConsts.common.splitScreen = mySplitScreen;
nrdConsts.common.accumulationMode = myAccumMode; // Reset or keep history etc

// Now set the specific settings for the method(s) chosen above (defaults are OK for 99% of the applications)
sl::NRDReblurSettings& reblurSettings = nrdConsts.reblurSettings;
reblurSettings = {};
```

### 7.0 ADD NRD TO THE RENDERING PIPELINE

On your rendering thread, call `slEvaluateFeature` at the appropriate location where denoising should happen. Please note that `viewportHandle` used in `slEvaluateFeature` must match the one used when settings constants.

```cpp
// Make sure NRD is available and user selected this option in the UI
bool useNRD = slIsFeatureSupported(sl::kFeatureNRD) && userSelectedNRDInUI;
if(useNRD) 
{
    // obtain a new frame token from SL
    sl::FrameToken* frameToken;
    slGetNewFrameToken(frameToken, &frameIndex);
    
    // prepare data structures you want to send along with your call
    // currently only viewport handle is required
    // m_id - id of the current viewport
    sl::ViewportHandle viewportHandle{ m_id };
    
    // Inform SL that NRD should be injected at this point.
    // Note that we are passing the specific frame token which needs to match the frame token used when setting constants.
    // The second and third parameter are data inputs and their count
    // currently NRD uses only viewportHandle
    if(!slEvaluateFeature(sl::kFeatureNRD, *frameToken, &viewportHandle, 1, commandList))) 
    {
        // Handle error
    }
    else
    {
        // IMPORTANT: Host is responsible for restoring state on the command list used
        restoreState(myCmdList);
    }
}
else
{
    // Default denoising method
}
```

> **IMPORTANT:**
> Plase note that **host is responsible for restoring the command buffer(list) state** after calling `slEvaluateFeature`. For more details on which states are affected please see [restore pipeline section](./ProgrammingGuideManualHooking.md#80-restoring-command-listbuffer-state)

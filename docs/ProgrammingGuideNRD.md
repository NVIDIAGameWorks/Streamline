

Streamline - NRD
=======================

>The focus of this guide is on using Streamline to integrate NVIDIA Real-Time Denoisers (NRD) into an application.  For more information about NRD itself, please visit the [NVIDIA Developer NRD Page](https://developer.nvidia.com/rtx/ray-tracing/rt-denoisers)

Version 1.0.3
------
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
uint32_t adapterBitMask = 0;
if(!slIsFeatureSupported(sl::Feature::eFeatureNRD, &adapterBitMask))
{
    // NRD is not supported on the system, fallback to the default upscaling method
}
// Now check your adapter
if((adapterBitMask & (1 << myAdapterIndex)) != 0)
{
    // It is ok to create a device on this adapter since feature we need is supported
}
```

### 3.0 PREPARE RESOURCES FOR DENOISING

NRD expects inputs in a specific format, so make sure to run a pre-processing pass (simple compute shader) to prepare them:

* `eBufferTypeNormalRoughness`       (RGB normal, A roughness)
* `eBufferTypeSpecularHitNoisy`      (RGB color,  A reflection ray hitT)
* `eBufferTypeDiffuseHitNoisy`       (RGB color,  A primary ray hitT)
* `eBufferTypeAmbientOcclusionNoisy` (R ambient)
### 4.0 PROVIDE NRD CONSTANTS

NRD constants must be set so that the NRD plugin can track any changes made by the user:

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
nrdConsts.common.denoisingRange = myDenoisingRange;
nrdConsts.common.worldToViewMatrix = myworldToViewMatrix;
nrdConsts.common.worldToViewMatrixPrev = myworldToViewMatrixPrev;
nrdConsts.common.viewToClipMatrix = myviewToClipMatrix;
nrdConsts.common.viewToClipMatrixPrev = myviewToClipMatrixPrev;
nrdConsts.common.cameraJitter[0] = myJitterOffsetX;
nrdConsts.common.cameraJitter[1] = myJitterOffsetY;
nrdConsts.common.frameIndex = myFrameIndex;
nrdConsts.common.isMotionVectorInWorldSpace = myMVecInWorldSpace; // Set to true if Mvec is in RGB(A) format and B component has depth based motion
nrdConsts.common.motionVectorScale[0] = myMVecScaleX; // Normally 1
nrdConsts.common.motionVectorScale[1] = myMVecScaleY; // Normally 1
nrdConsts.common.splitScreen = mySplitScreen;
nrdConsts.common.accumulationMode = myAccumMode; // Reset or keep history etc
nrdConsts.common.resolutionScale[0] = myScaleX; // Are we rendering within a larger render target? If so set scale, otherwise set to 1.0f
nrdConsts.common.resolutionScale[1] = myScaleY; // Are we rendering within a larger render target? If so set scale, otherwise set to 1.0f

// Now set the specific settings for the method(s) chosen above (defaults are OK for 99% of the applications)
sl::NRDReblurSpecularSettings& specularSettings = nrdConsts.reblurSpecular;
specularSettings = {};
sl::NRDReblurDiffuseSettings& diffuseSettings = nrdConsts.reblurDiffuse;
diffuseSettings = {};

// Note that we use method mask as a unique id
// This allows us to evaluate different NRD denoisers in the same or different viewports but at the different stages with the same frame
if(!slSetFeatureConstants(sl::eFeatureNRD, &nrdConsts, myFrameIndex, nrdConsts.methodMask))
{
    // Handle error here, check the logs
}
```

> **NOTE:**
> To disable NRD, set `sl::NRDConstants.methodMask` to `sl::NRDMode::eNRDMethodOff` or simply stop calling `slEvaluateFeature`

### 5.0 TAG ALL REQUIRED RESOURCES

NRD requires depth, motion vectors, noisy inputs and outputs depending on which type of denoising is selected:

```cpp
// These are required for any denoising method

// Prepare resources (assuming d3d11/d3d12 integration so leaving Vulkan view and device memory as null pointers)
sl::Resource depth = {sl::ResourceType::eResourceTypeTex2d, myDepthBuffer, nullptr, nullptr, nullptr};
sl::Resource mvec = {sl::ResourceType::eResourceTypeTex2d, myMotionVectorsBuffer, nullptr, nullptr, nullptr};
// Note that you can also pass unique id (if using multiple viewports) and the extent of the resource if dynamic resolution is active
setTag(&depth, sl::BufferType::eBufferTypeDepth);
setTag(&mvec, sl::BufferType::eBufferTypeMVec);

// Depending on what type of denoising is done tag the remaining resources.
// Note that we are passing the method mask to map each resource to the appropriate NRD instance
sl::Resource nroughness = {sl::ResourceType::eResourceTypeTex2d, myNormalRoughness, nullptr, nullptr, nullptr};
sl::Resource specularIn = {sl::ResourceType::eResourceTypeTex2d, mySpecularInput, nullptr, nullptr, nullptr};
sl::Resource specularOut = {sl::ResourceType::eResourceTypeTex2d, mySpecularOutput, nullptr, nullptr, nullptr};
setTag(&nroughness, sl::BufferType::eBufferTypeNormalRoughness, nrdConsts.methodMask);
setTag(&specularIn, sl::BufferType::eBufferTypeSpecularHitNoisy, nrdConsts.methodMask);
setTag(&specularOut, sl::BufferType::eBufferTypeSpecularHitDenoised, nrdConsts.methodMask);
// and so on ...
```
> **NOTE:**
> If dynamic resolution is used then please specify the extent for each tagged resource. Please note that SL **manages resource states so there is no need to transition tagged resources**.

### 6.0 PROVIDE COMMON CONSTANTS

Various per frame camera related constants are required by all Streamline features and must be provided ***if any SL feature is active as early in the frame as possible***. Please keep in mind the following: 

* All SL matrices are row-major and should not contain any jitter offsets
* If motion vector values in your buffer are in {-1,1} range then motion vector scale factor in common constants should be {1,1}
* If motion vector values in your buffer are NOT in {-1,1} range then motion vector scale factor in common constants must be adjusted so that values end up in {-1,1} range

```cpp
sl::Constants consts = {};
// Set motion vector scaling based on your setup
consts.mvecScale = {1,1}; // Values in eBufferTypeMVec are in [-1,1] range
consts.mvecScale = {1.0f / renderWidth,1.0f / renderHeight}; // Values in eBufferTypeMVec are in pixel space
consts.mvecScale = myCustomScaling; // Custom scaling to ensure values end up in [-1,1] range
sl::Constants consts = {};
// Set all constants here
if(!setConstants(consts, myFrameIndex)) // constants are changing per frame so frame index is required
{
    // Handle error, check logs
}
```
For more details please see [common constants](ProgrammingGuide.md#251-common-constants)

### 7.0 ADD NRD TO THE RENDERING PIPELINE

On your rendering thread, call `evaluateFeature` at the appropriate location where denoising should happen. Please note that `myFrameIndex` and `myId` used in `evaluateFeature` must match the one used when settings constants.

```cpp
// Make sure NRD is available and user selected this option in the UI
bool useNRD = slIsFeatureSupported(sl::eFeatureNRD) && userSelectedNRDInUI;
if(useNRD) 
{
    // Inform SL that NRD should be injected at this point.
    // Note that we are passing the specific id which needs to match the id used when setting constants.
    // This can be method mask or something different (like for example viewport id | method mask etc.)
    auto myId = nrdConsts.methodMask;
    if(!slEvaluateFeature(myCmdList, sl::Feature::eFeatureNRD, myFrameIndex, myId)) 
    {
        // Handle error
    }
}
else
{
    // Default denoising method
}
```
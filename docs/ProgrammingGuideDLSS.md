

Streamline - DLSS
=======================

>The focus of this guide is on using Streamline to integrate DLSS into an application.  For more information about DLSS itself, please visit the [NVIDIA Developer DLSS Page](https://developer.nvidia.com/rtx/dlss)

Version 1.0.3
------

### 1.0 INITIALIZE AND SHUTDOWN

Call `slInit` as early as possible (before any dxgi/d3d11/d3d12 APIs are invoked)

```cpp
#include <sl.h>
#include <sl_consts.h>
#include <sl_dlss.h>

sl::Preferences pref{};
pref.showConsole = true; // for debugging, set to false in production
pref.logLevel = sl::eLogLevelDefault;
pref.pathsToPlugins = {}; // change this if Streamline plugins are not located next to the executable
pref.numPathsToPlugins = 0; // change this if Streamline plugins are not located next to the executable
pref.pathToLogsAndData = {}; // change this to enable logging to a file
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

### 2.0 CHECK IF DLSS IS SUPPORTED

As soon as SL is initialized, you can check if DLSS is available for the specific adapter you want to use:

```cpp
uint32_t adapterBitMask = 0;
if(!slIsFeatureSupported(sl::Feature::eFeatureDLSS, &adapterBitMask))
{
    // DLSS is not supported on the system, fallback to the default upscaling method
}
// Now check your adapter
if((adapterBitMask & (1 << myAdapterIndex)) != 0)
{
    // It is ok to create a device on this adapter since feature we need is supported
}
```

### 3.0 CHECK DLSS SETTINGS AND SETUP VIEWPORT RENDERING SIZE

Next, we need to find out the rendering resolution and the optimal sharpness level based on DLSS settings:

```cpp
sl::DLSSSettings dlssSettings{};
sl::DLSSConstants dlssConsts{};
// These are populated based on user selection in the UI
dlssConsts.mode = myUI->getDLSSMode(); // e.g. sl::eDLSSModeBalanced;
dlssConsts.outputWidth = myUI->getOutputWidth();    // e.g 1920;
dlssConsts.outputHeight = myUI->getOutputHeight(); // e.g. 1080;
// Now let's check what should our rendering resolution be
if(!slGetFeatureSettings(sl::eFeatureDLSS, &dlssConsts, &dlssSettings))
{
    // Handle error here, check the logs
}
// Setup rendering based on the provided values in the sl::DLSSSettings structure
myViewport->setSize(dlssSettings.optimalRenderWidth, dlssSettings.optimalRenderHeight);
```

### 4.0 TAG ALL REQUIRED RESOURCES

DLSS requires depth, motion vectors, render-res input color and final-res output color buffers.

```cpp
// Prepare resources (assuming d3d11/d3d12 integration so leaving Vulkan view and device memory as null pointers)
sl::Resource colorIn = {sl::ResourceType::eResourceTypeTex2d, myTAAUInput, nullptr, nullptr, nullptr};
sl::Resource colorOut = {sl::ResourceType::eResourceTypeTex2d, myTAAUOutput, nullptr, nullptr, nullptr};
sl::Resource depth = {sl::ResourceType::eResourceTypeTex2d, myDepthBuffer, nullptr, nullptr, nullptr};
sl::Resource mvec = {sl::ResourceType::eResourceTypeTex2d, myMotionVectorsBuffer, nullptr, nullptr, nullptr};
// Note that you can also pass unique id (if using multiple viewports) and the extent of the resource if dynamic resolution is active
setTag(&colorIn, sl::BufferType::eBufferTypeScalingInputColor);
setTag(&colorOut, sl::BufferType::eBufferTypeScalingOutputColor);
setTag(&depth, sl::BufferType::eBufferTypeDepth);
setTag(&mvec, sl::BufferType::eBufferTypeMVec);
```

> **NOTE:**
> If dynamic resolution is used then please specify the extent for each tagged resource. Please note that SL **manages resource states so there is no need to transition tagged resources**.

DLSS next requires additional buffers so ***please tag all buffers that are available in your engine***

* `eBufferTypeExposure` (1x1 buffer containing exposure of the current frame)
* `eBufferTypeAlbedo`
* `eBufferTypeSpecularAlbedo`
* `eBufferTypeIndirectAlbedo`
* `eBufferTypeNormals`
* `eBufferTypeRoughness`
* `eBufferTypeEmissive`
* `eBufferTypeDisocclusionMask`
* `eBufferTypeSpecularMVec`

### 5.0 PROVIDE DLSS CONSTANTS

DLSS constants must be set so that the DLSS plugin can track any changes made by the user:

```cpp
sl::DLSSConstants dlssConsts = {};
// These are populated based on user selection in the UI
dlssConsts.mode = myUI->getDLSSMode(); // e.g. sl::eDLSSModeBalanced;
dlssConsts.outputWidth = myUI->getOutputWidth();    // e.g 1920;
dlssConsts.outputHeight = myUI->getOutputHeight(); // e.g. 1080;
dlssConsts.sharpness = dlssSettings.sharpness; // optimal sharpness
dlssConsts.colorBuffersHDR = sl::Boolean::eTrue; // assuming HDR pipeline
if(!slSetFeatureConstants(sl::eFeatureDLSS, &dlssConsts))
{
    // Handle error here, check the logs
}
```

> **NOTE:**
> To disable DLSS set `sl::DLSSConstants.mode` to `sl::DLSSMode::eDLSSModeOff`or simply stop calling `slEvaluateFeature`

### 6.0 PROVIDE COMMON CONSTANTS

Various per frame camera related constants are required by all Streamline features and must be provided ***if any SL feature is active and as early in the frame as possible***. Please keep in mind the following: 

* All SL matrices are row-major and should not contain any jitter offsets
* If motion vector values in your buffer are in {-1,1} range then motion vector scale factor in common constants should be {1,1}
* If motion vector values in your buffer are NOT in {-1,1} range then motion vector scale factor in common constants must be adjusted so that values end up in {-1,1} range

```cpp
sl::Constants consts = {};
// Set motion vector scaling based on your setup
consts.mvecScale = {1,1}; // Values in eBufferTypeMVec are in [-1,1] range
consts.mvecScale = {1.0f / renderWidth,1.0f / renderHeight}; // Values in eBufferTypeMVec are in pixel space
consts.mvecScale = myCustomScaling; // Custom scaling to ensure values end up in [-1,1] range
// Set all other constants here
if(!setConstants(consts, myFrameIndex)) // constants are changing per frame so frame index is required
{
    // Handle error, check logs
}
```
For more details please see [common constants](ProgrammingGuide.md#251-common-constants)

### 7.0 ADD DLSS TO THE RENDERING PIPELINE

On your rendering thread, call `slEvaluateFeature` at the appropriate location where up-scaling is happening. Please note that `myFrameIndex` used in `slEvaluateFeature` must match the one used when setting constants.

```cpp
// Make sure DLSS is available and user selected this option in the UI
bool useDLSS = slIsFeatureSupported(sl::eFeatureDLSS) && userSelectedDLSSInUI;
if(useDLSS) 
{
    // Inform SL that DLSS should be injected at this point
    if(!slEvaluateFeature(myCmdList, sl::Feature::eFeatureDLSS, myFrameIndex)) 
    {
        // Handle error
    }
}
else
{
    // Default up-scaling pass like for example TAAU goes here
}
```
### 8.0 MULTIPLE VIEWPORTS

Here is a code snippet showing one way of handling two viewports with explicit resource allocation and de-allocation:

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

### 9.0 TROUBLESHOOTING

If the DLSS output does not look right please check the following:

* If your motion vectors are in pixel space then scaling factors `sl::Constants::mvecScale` should be {1 / render width, 1 / render height}
* If your motion vectors are in normalized -1,1 space then scaling factors `sl::Constants::mvecScale` should be {1, 1}
* Make sure that jitter offset values are in pixel space
* `NVSDK_NGX_Parameter_FreeMemOnReleaseFeature` is replaced with `slFreeResources`
* `NVSDK_NGX_DLSS_Feature_Flags_MVLowRes` is handled automatically based on tagged motion vector buffer's size and extent.
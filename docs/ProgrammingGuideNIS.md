

Streamline - NIS
=======================

>The focus of this guide is on using Streamline to integrate the NVIDIA Image Scaling (NIS) SDK into an application.  For more information about NIS itself, please visit the [NVIDIA Image Scaling SDK Github Page](https://github.com/NVIDIAGameWorks/NVIDIAImageScaling)  
>For information on user interface considerations when using the NIS plugin, please see the "RTX UI Developer Guidelines.pdf" document included in the NIS SDK.

Version 1.1.0
------

### Introduction

The NVIDIA Image Scaling SDK (NIS) provides a single spatial scaling and sharpening algorithm for cross-platform support. The scaling algorithm uses a 6-tap scaling filter combined with 4 directional scaling and adaptive sharpening filters, which creates nice smooth images and sharp edges. In addition, the SDK provides a state-of-the-art adaptive directional sharpening algorithm for use in applications where no scaling is required. By integrating both NVIDIA Image Scaling and NVIDIA DLSS, developers can get the best of both worlds: NVIDIA DLSS for the best image quality, and NVIDIA Image Scaling for cross-platform support.

The directional scaling and sharpening algorithm are combined together in NVScaler while NVSharpen only implements the adaptive-directional-sharpening algorithm. Both algorithms are provided as compute shaders and developers are free to integrate them in their applications. Note that if you integrate NVScaler, you should NOT also integrate NVSharpen, as NVScaler already includes a sharpening pass.

For more information on the NVIDIA Image scaling SDK visit https://github.com/NVIDIAGameWorks/NVIDIAImageScaling

### 1.0 INITIALIZE AND SHUTDOWN

Call `slInit` as early as possible (before any dxgi/d3d11/d3d12 APIs are invoked)

```cpp
#include <sl.h>
#include <sl_consts.h>
#include <sl_nis.h>

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

### 2.0 CHECK IF NIS IS SUPPORTED

As soon as SL is initialized, you can check if NIS is available for the specific adapter you want to use:

```cpp
uint32_t adapterBitMask = 0;
if(!slIsFeatureSupported(sl::Feature::eFeatureNIS, &adapterBitMask))
{
    // NIS is not supported on the system, fallback to the default upscaling or sharpening method
}
// Now check your adapter
if((adapterBitMask & (1 << myAdapterIndex)) != 0)
{
    // It is ok to create a device on this adapter since feature we need is supported
}
```

### 3.0 TAG ALL REQUIRED RESOURCES

NIS requires render-res input color after TAA and final-res output color buffers.

```cpp
// Prepare resources (assuming d3d11/d3d12 integration so leaving Vulkan view and device memory as null pointers)
sl::Resource colorIn = {sl::ResourceType::eResourceTypeTex2d, myLowResInput, nullptr, nullptr, nullptr};
sl::Resource colorOut = {sl::ResourceType::eResourceTypeTex2d, myUpscaledOutput, nullptr, nullptr, nullptr};
// Note that you can also pass unique id (if using multiple viewports) and the extent of the resource if dynamic resolution is active
setTag(&colorIn, sl::BufferType::eBufferTypeScalingInputColor);
setTag(&colorOut, sl::BufferType::eBufferTypeScalingOutputColor);
```

### 4.0 PROVIDE NIS CONSTANTS

NIS constants must be set so that the NIS plugin can track any changes made by the user:

```cpp
sl::NISConstants nisConsts{};
nisConsts.mode = NISMode::eNISModeScaler; // use upscaling algorithm or use eNISModeSharpen for sharpening only
nisConsts.hdrMode = NISHDR::eNISHDRNone; // No HDR mode;
// These can be populated based on user selection in the UI
nisConsts.sharpness = myUI->getSharpness();
if(!slSetFeatureConstants(sl::Feature::eFeatureNIS, &nisConsts))
{
    // Handle error here, check the logs
}
```
> **NOTE:**
> To use NIS sharpening only mode (with no upscaling) set `sl::NISConstants.mode` to `sl::NISMode::eNISModeSharpen`

> **NOTE:**
> To disable NIS set `sl::NISConstants.mode` to `sl::NISMode::eNISModeOff`or simply stop calling `slEvaluateFeature`


### 5.0 ADD NIS TO THE RENDERING PIPELINE

On your rendering thread, call `slEvaluateFeature` at the appropriate location where up-scaling is happening. Please note that `myFrameIndex` used in `slEvaluateFeature` must match the one used when setting constants.

```cpp
// Make sure NIS is available and user selected this option in the UI
bool useNIS = slIsFeatureSupported(sl::Feature::eFeatureNIS) && userSelectedNISInUI;
if(useNIS) 
{
    // Inform SL that NIS should be injected at this point
    if(!slEvaluateFeature(myCmdList, sl::Feature::eFeatureNIS, myFrameIndex)) 
    {
        // Handle error
    }
}
else
{
    // Default up-scaling pass like for example TAAU goes here
}
```

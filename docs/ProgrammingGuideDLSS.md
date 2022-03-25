

Streamline - DLSS
=======================

Version 1.0.1
------

### 1.0 INITIALIZE AND SHUTDOWN

Call `sl::init` as early as possible (before any dxgi/d3d11/d3d12 APIs are invoked)

```cpp
sl::Preferences pref{};
pref.showConsole = true; // for debugging, set to false in production
pref.logLevel = sl::eLogLevelDefault;
pref.pathsToPlugins = {}; // change this if Streamline plugins are not located next to the executable
pref.numPathsToPlugins = 0; // change this if Streamline plugins are not located next to the executable
pref.pathToLogsAndData = {}; // change this to enable logging to a file
pref.logMessageCallback = myLogMessageCallback; // highly recommended to track warning/error messages in your callback
if(!sl::init(pred, myApplicationId)) // !!! Make sure to obtain your app Id from NVIDIA !!!
{
    // Handle error, check the logs
}
```

For more details please see [preferences](ProgrammingGuide.md#221-preferences)

Call `sl::shutdown()` before destroying dxgi/d3d11/d3d12/vk instances, devices and other components in your engine.

```cpp
if(!sl::shutdown())
{
    // Handle error, check the logs
}
```

### 2.0 CHECK IF DLSS IS SUPPORTED

As soon as SL is initialized, you can check if DLSS is available for the specific adapter you want to use:

```cpp
uint32_t adapterBitMask = 0;
if(!isFeatureSupported(sl::Feature::eFeatureDLSS, &adapterBitMask))
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
if(!sl::getFeatureSettings(sl::eFeatureDLSS, &dlssConsts, &dlssSettings))
{
    // Handle error here, check the logs
}
// Setup rendering based on the provided values in the sl::DLSSSettings structure
myViewport->setSize(dlssSettings.renderWidth, dlssSettings.renderHeight);
// Use recommended sharpness
dlssConsts.sharpness = dlssSettings.optimalSharpness;
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
setTag(&colorIn, sl::BufferType::eBufferTypeDLSSInputColor);
setTag(&colorOut, sl::BufferType::eBufferTypeDLSSOutputColor);
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
if(!sl::setFeatureSettings(sl::eFeatureDLSS, &dlssConsts))
{
    // Handle error here, check the logs
}
```

> **NOTE:**
> To disable DLSS set `sl::DLSSConstants.mode` to `sl::DLSSMode::eDLSSModeOff`or simply stop calling `sl::evaluateFeature`

### 6.0 PROVIDE COMMON CONSTANTS

Various per frame camera related constants are required by all Streamline features and must be provided ***as early in the frame as possible***. Please keep in mind the following: 

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

On your rendering thread, call `evaluateFeature` at the appropriate location where up-scaling is happening. Please note that `myFrameIndex` used in `evaluateFeature` must match the one used when setting constants.

```cpp
// Make sure DLSS is available and user selected this option in the UI
bool useDLSS = sl::isFeatureSupported(sl::eFeatureDLSS) && userSelectedDLSSInUI;
if(useDLSS) 
{
    // Inform SL that DLSS should be injected at this point
    if(!sl::evaluateFeature(myCmdList, sl::Feature::eFeatureDLSS, myFrameIndex)) 
    {
        // Handle error
    }
}
else
{
    // Default up-scaling pass like for example TAAU goes here
}
```
### 8.0 TROUBLESHOOTING

If the DLSS output does not look right please check the following:

* If your motion vectors are in pixel space then scaling factors `sl::Constants::mvecScale` should be {1 / render width, 1 / render height}
* If your motion vectors are in normalized -1,1 space then scaling factors `sl::Constants::mvecScale` should be {1, 1}
* Make sure that jitter offset values are in pixel space
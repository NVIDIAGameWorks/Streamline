

Streamline - Latency
=======================

>The focus of this guide is on using Streamline to integrate Reflex into an application.  For more information about Reflex itself, please visit the [NVIDIA Developer Reflex Page](https://developer.nvidia.com/performance-rendering-tools/reflex)  
>For information on user interface considerations when using the Latency plugin, please see the "Reflex SDK UI Integration Guidelines.pdf" document included in the Reflex SDK.

Version 1.0.4
------

### 1.0 INITIALIZE AND SHUTDOWN

Call `slInit` as early as possible (before any dxgi/d3d11/d3d12 APIs are invoked)

```cpp

#include <sl.h>
#include <sl_consts.h>
#include <sl_latency.h>

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

### 2.0 CHECK IF LATENCY IS SUPPORTED

As soon as SL is initialized, you can check if Latency is available for the specific adapter you want to use:

```cpp
uint32_t adapterBitMask = 0;
if(!slIsFeatureSupported(sl::Feature::eFeatureLatency, &adapterBitMask))
{
    // Latency is not supported on the system
}
// Now check your adapter
if((adapterBitMask & (1 << myAdapterIndex)) != 0)
{
    // It is OK to create a device on this adapter since feature we need is supported
}
```

### 3.0 CHECK LATENCY SETTINGS AND CAPABILITIES

To find out what latency modes are available, currently active or to obtain the latest latency stats you can do the following:

```cpp
sl::LatencySettings latencySettings{};
if(!slGetFeatureSettings(sl::eFeatureLatency, nullptr, &latencySettings))
{
    // Handle error here, check the logs
}
if(latencySettings.lowLatencyAvailable)
{
    // Low-latency is available, on NVDA hardware this would be done through Reflex
}
if(latencySettings.latencyStatsAvailable)
{
    // Latency statistics are available, on NVDA hardware this would be done through Reflex
}
```
> **NOTE:**
> Latency can be enabled using the driver control panel so when SL is initialized it could already be active.

### 4.0 SET LATENCY CONSTANTS

To configure Latency please do the following:

```cpp
sl::LatencyConstants latencyConsts = {};
latencyConsts.mode = eLatencyModeLowLatency; // to disable Latency simply set this to off
latencyConsts.frameLimitFPS = myFrameLimit;
latencyConsts.useMarkersToOptimize = myUseMarkers;
if(!slSetFeatureConstants(sl::eFeatureLatency, &latencyConsts))
{
    // Handle error here, check the logs
}
```

> **NOTE:**
> `slSetFeatureSettings` method does not need to be called every frame but rather only when Latency settings change. To disable Latency set `sl::LatencyConstants.mode` to `sl::LatencyMode::eLatencyModeOff`and stop calling `slEvaluateFeature`

### 5.0 ADD LATENCY TO THE RENDERING PIPELINE

Call `slEvaluateFeature` at the appropriate location where Latency markers need to be injected or where your application should sleep. Note that since there is only ever one instance of the Latency feature active, we use the `id` parameter of `slEvaluateFeature` to indicate which marker is to be injected, rather than its typical usage of indicating which instance of the feature is to be evaluated.

Here is some pseudo code:

```cpp
// Make sure Latency is available
bool useLatency = slIsFeatureSupported(sl::eFeatureLatency);
if(useLatency) 
{
    // Mark the section where specific activity is happening.
    //
    // Here for example we will make the simulation code.
    //
    // Note that command buffer can be null since it is not used
    //

    // Simulation start, command buffer is not needed hence null
    if(!slEvaluateFeature(nullptr, sl::Feature::eFeatureLatency, myFrameIndex, sl::LatencyMarker::eLatencyMarkerSimulationStart)) 
    {
        // Handle error
    }

    // Simulation code goes here

    // Simulation end, command buffer is not needed hence null
    if(!slEvaluateFeature(nullptr, sl::Feature::eFeatureLatency, myFrameIndex, sl::LatencyMarker::eLatencyMarkerSimulationEnd)) 
    {
        // Handle error
    }   
}

// When checking for custom low latency messages inside the Windows message loop
//
if(useLatency && latencySettings.statsWindowMessage == msgId) 
{
    // Latency ping based on custom message, command buffer is not needed hence null
    if(!slEvaluateFeature(nullptr, sl::Feature::eFeatureLatency, myFrameIndex, sl::LatencyMarker::eLatencyMarkerPCLatencyPing)) 
    {
        // Handle error
    }
}

// When your application should sleep to achieve optimal low-latency mode
//
if(useLatency) 
{
    // Sleep, command buffer is not needed hence null and also frame index is not needed so setting it to 0
    if(!slEvaluateFeature(nullptr, sl::Feature::eFeatureLatency, 0, sl::LatencyMarker::eLatencyMarkerSleep)) 
    {
        // Handle error
    }
}

```

### 6.0 HOW TO TRANSITION FROM REFLEX TO SL LATENCY

Existing Reflex integrations can be easily converted to use SL Latency by following these steps:

* Remove NVAPI from your application
* There is no longer any need to provide a native D3D/VK device when making Reflex calls - SL takes care of that, hence making the integrations easier
* `NvAPI_D3D_SetSleepMode` is replaced with [set latency constants](#40-set-latency-constants)
* `NvAPI_D3D_GetSleepStatus` is replaced with [get latency settings](#30-check-latency-settings-and-capabilities) - see `sl::LatencySettings::lowLatencyAvailable`
* `NvAPI_D3D_GetLatency` is replaced with [get latency settings](#30-check-latency-settings-and-capabilities) - see `sl::LatencyReport`
* `NvAPI_D3D_SetLatencyMarker` is replaced with `slEvaluateFeature(nullptr, sl::Feature::eFeatureLatency, myFrameIndex, sl::LatencyMarker::eLatencyMarker***)`
* `NvAPI_D3D_Sleep` is replaced with `slEvaluateFeature(nullptr, sl::Feature::eFeatureLatency, myFrameIndex, sl::LatencyMarker::eLatencyMarkerSleep)`
* `NVSTATS*` calls are handled automatically by SL latency plugin and are GPU agnostic
* `NVSTATS_IS_PING_MSG_ID` is replaced with [get latency settings](#30-check-latency-settings-and-capabilities) - see `sl::LatencySettings::statsWindowMessage`
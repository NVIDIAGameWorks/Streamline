# Improvements and API changes in SL 1.5 and SL 2.0

> **NOTE**:
> This document contains the high level breakdown of changes introduced in SL 1.5/2.0 and how they compare to previous SL versions. For more details please read the full [programming guide](./ProgrammingGuide.md)

## SL 1.5

### RESOURCE STATE TRACKING

* SL no longer tracks resource states, host is responsible for providing correct states when tagging resources.

### COMMAND LIST STATE TRACKING

* SL by default disables any command list (CL) tracking, host is responsible for restoring CL state after each `slEvaluateFeature` call.

### DISABLING OF THE INTERPOSER

* SL disables interposer (DXGI/D3D proxies) unless they are **explicitly requested by at least one supported plugin**

### VERIFYING SL DLLs

* SL now includes `sl_security.h` header as part of the main SDK (no longer needed to download the SL sample SDK)

### OS VERSION DETECTION

* Switched to obtaining the **correct** version from the `kernel32.dll` product description instead of using various MS APIs which return different/incorrect versions
* Flag `PreferenceFlags::eBypassOSVersionCheck` in `sl::Preferences` can be used to opt-out from the OS detection (off by default and **highly discouraged to use**)

### OTA OPT-IN

* Expanded `sl::Preferences` to include `PreferenceFlags::eAllowOTA` to automatically opt-in in the OTA program (SL and NGX). This flag is set by default.

### DLSS PRESETS

* Host can change DL networks (presets) for the DLSS by modifying `sl::DLSSOptions`.

## SL 2.0

Streamline 2.0 **includes all the above mentioned v1.5 changes with the following additions**:

### FEATURE ID

* No longer provided as enum but rather as constant expression unsigned integers.
* Core feature IDs declared and defined in `sl.h` while specific feature IDs are now located in their respective header files.

### ENUM NAMING

* All enums have been converted to `enum class Name::eValue` format.

### ERROR REPORTING

* All SL functions now return `sl::Result` (new header `sl_result.h`)
* Still required to monitor error logging callbacks to catch every single possible error at the right time but this improves handling of the most common errors.
* Introduced helper macro `SL_FAILED`.
* Helper method added to convert `sl::Result` to a string.

### VERSIONING AND OTA

* New API `slGetFeatureVersion` returns both SL and NGX (if any) versions.

### REQUIREMENTS REPORTING

* Removed `slGetFeatureConfiguration` which was returning JSON since it is not always convenient for eve
* New API `slGetFeatureRequirements` is added to provide info about OS, driver, HWS, rendering API, VK extensions and other requirements
* Added `getVkPhysicalDeviceVulkan12Features` and `getVkPhysicalDeviceVulkan13Features` helper functions in the new `sl_helpers_vk.h` header.

### CONSTANTS VS SETTINGS

* Generic `slSetFeatureConsts` and `slGetFeatureSettings` API have been removed and new API `slGetFeatureFunction` has been added.
* Each SL feature exports set of functions which are used to perform feature specific task.
* New helper macro `SL_FEATURE_FUN_IMPORT` and helper functions in the related `sl_$feature.h` headers.
* Helper functions added to each per-feature header to make importing easy.

### IS FEATURE SUPPORTED AND ADAPTER BIT-MASK

* `slIsFeatureSupported` is modified to use adapter LUID which is easily obtained from DXGIAdapterDesc or VK physical device.
* Engines are already enumerating adapters so this should fit in nicely with the existing code.
* When using VK host can provide `VkPhysicalDevice` instead of LUID if needed.

### ACTUAL FPS

* Removed `actualFrameTimeMs` and `timeBetweenPresentsMs` and replaced with an integer value `numFramesActuallyPresented`.

### IMPROVED VIEWPORT SUPPORT FOR DLSS-G

* Host can specify any viewport id when calling `slDLSSGSetOptions` rather than forcing viewport 0 all the time.

### "NOT RENDERING GAME FRAMES" FLAG

* Removed completely since it has become redundant.
* Host is now required to turn DLSS-G on/off using `slDLSSGSetOptions`

### BUFFER COPIES AND TAGGING

* `slSetTag` API has been expanded to include command list and resource life-cycle information.
* If needed resources are automatically copied internally by SL.
* `slEvaluateFeature` can be used to tag resources locally (tags only valid for the specific evaluate call, see programming guide for details)

### FRAME ID

* New API `slGetNewFrameToken` is added to allow host to obtain frame handle for the next frame.
* Same handle is then passed around to all SL calls.
* If host wants to fully control frame counting the frame index can be provided as input

### MULTIPLE DEVICES

* New API `slSetD3DDevice` and `slSetVulkanInfo` can be used to specify which device SL should be using.

### OBTAINING NATIVE INTERFACES

* When using 3rd party libraries (including NVAPI) it is not advisable to pass SL proxies as inputs.
* New API `slGetNativeInterface` added to allow access to native interfaces as needed.

### MANUAL HOOKING

* Removed `slGetHook*` API
* New API `slGetNativeInterface` in combination with `slUpgradeIntercace` is now used to significantly simplify manual hooking.

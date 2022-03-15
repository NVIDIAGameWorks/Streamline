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

#pragma once

#include "sl_consts.h"

#ifdef SL_SDK
#define SL_API __declspec(dllexport) 
#else
#define SL_API __declspec(dllimport)
#endif

namespace sl {

using CommandBuffer = void;

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
    
    //! Total count
    eBufferTypeCount
};

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

//! Resource types
enum ResourceType : char
{
    eResourceTypeTex2d,
    eResourceTypeBuffer
};

//! Resource description
struct ResourceDesc
{
    //! Indicates the type of resource
    ResourceType type;
    //! D3D12_RESOURCE_DESC/VkImageCreateInfo/VkBufferCreateInfo
    void *desc;
    //! Initial state as D3D12_RESOURCE_STATES or VkMemoryPropertyFlags
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
    //! ID3D11Resource/ID3D12Resource/VkBuffer/VkImage
    void *native;
    //! vkDeviceMemory or nullptr
    void *memory;
    //! VkImageView/VkBufferView or nullptr
    void *view;
    //! Reserved for future expansion, must be set to null
    void* ext = {};
};

//! Resource allocation/deallocation callbacks
//!
//! Use these callbacks to gain full control over 
//! resource life cycle and memory allocation tracking.
//!
//! IMPORTANT: Textures must have the pixel shader resource
//! and the unordered access view flags set
using pfunResourceAllocateCallback = Resource(const ResourceDesc *desc);
using pfunResourceReleaseCallback = void(Resource *resource);

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
//! If any of the SL methods returns false use eLogTypeError
//! type to track down what went wrong and why.
using pfunLogMessageCallback = void(LogType type, const char *msg);

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
    //! Reserved for future use, should be null
    void* ext = {};
};

//! Unique application ID
constexpr int kUniqueApplicationId = 0;

//! Initializes the SL module
//!
//! Call this method when the game is initializing. 
//!
//! @param pref Specifies preferred behavior for the SL library (SL will keep a copy)
//! @param applicationId Unique id for your application.
//! @return false if SL is not supported on the system true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool init(const Preferences &pref, int applicationId = kUniqueApplicationId);

//! Shuts down the SL module
//!
//! Call this method when the game is shutting down. 
//!
//! @return false if SL did not shutdown correctly true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool shutdown();

//! Checks is specific feature is supported or not.
//!
//! Call this method to check if certain eFeature* (see above) is available.
//! This method must be called after init otherwise it will always return false.
//!
//! @param feature Specifies which feature to check
//! @param adapterBitMask Optional bit-mask specifying which adapter supports the give feature
//! @return false if feature is not supported on the system true otherwise.
//!
//! NOTE: You can provide the adapter bit mask to ensure that feature is available on the adapter
//! for which you are planning to create a device. For the adapter at index N you can check the bit 1 << N.
//!
//! This method is NOT thread safe.
SL_API bool isFeatureSupported(Feature feature, uint32_t* adapterBitMask = nullptr);

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
SL_API bool setTag(const Resource *resource, BufferType tag, uint32_t id = 0, const Extent* extent = nullptr);

//! Sets common constants.
//!
//! Call this method to provide the required data (SL will keep a copy).
//!
//! @param values Common constants required by SL plugins (SL will keep a copy)
//! @param frameIndex Index of the current frame
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if constants cannot be set true otherwise.
//! 
//! This method is NOT thread safe.
SL_API bool setConstants(const Constants& values, uint32_t frameIndex, uint32_t id = 0);

//! Sets feature specific constants.
//!
//! Call this method to provide the required data
//! for the specified feature (SL will keep a copy).
//!
//! @param feature Feature we are working with
//! @param consts Pointer to the feature specific constants (SL will keep a copy)
//! @param frameIndex Index of the current frame
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if constants cannot be set true otherwise.
//!
//! This method is NOT thread safe.
SL_API bool setFeatureConstants(Feature feature, const void *consts, uint32_t frameIndex, uint32_t id = 0);

//! Gets feature specific settings.
//!
//! Call this method to obtain settings for the specified feature.
//!
//! @param feature Feature we are working with
//! @param consts Pointer to the feature specific constants
//! @param settings Pointer to the returned feature specific settings
//! @return false if feature does not have settings true otherwise.
//!
//! For example:
//!
//! DLSSSettings settings = {};
//! DLSSConstants consts = {eDLSSModeBalanced, 3840, 2160, 0.0}; // targeting 4K
//! if(getFeatureSettings(eFeatureDLSS, &consts, &settings))
//! {
//!    Setup application to use (settings.renderWidth, settings.renderHeight) render targets
//! }
//!
//! This method is NOT thread safe.
SL_API bool getFeatureSettings(Feature feature, const void* consts, void* settings);

//! Evaluates feature
//! 
//! Use this method to mark the section in your rendering pipeline 
//! where specific feature should be injected.
//!
//! @param cmdBuffer Command buffer to use - must be created on device where feature is supported
//! @param feature Feature we are working with
//! @param frameIndex Current frame index (must match the corresponding value in the sl::Constants)
//! @param id Unique id (can be viewport id | instance id etc.)
//! @return false if feature event cannot be injected in the command buffer true otherwise.
//! 
//! For example:
//!
//! bool useDLSS = isFeatureSupported(eFeatureDLSS) && userSelectedDLSS;
//! if(useDLSS) 
//! {
//!    DLSSSettings settings = {};
//!    DLSSConstants consts = {eDLSSModeBalanced, 3840, 2160, 0.0}; // targeting 4K
//!    sl::setFeatureConstants(sl::Feature::eFeatureDLSS, &dlss);
//!    sl::evaluateFeature(myCmdList, sl::Feature::eFeatureDLSS, myFrameIndex); 
//! }
//! else
//! {
//!    ... do TAAU pass with all draw calls included 
//! }
//!
//! IMPORTANT: Unique id must match whatever is used to set constants
//!
//! This method is NOT thread safe.
SL_API bool evaluateFeature(CommandBuffer* cmdBuffer, Feature feature, uint32_t frameIndex, uint32_t id = 0);

} // namespace sl
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

namespace sl
{

//! Real-Time Denoiser
constexpr Feature kFeatureNRD = 1;

enum class NRDMethods : uint32_t
{
    eOff,
    eReblurDiffuse,
    eReblurDiffuseOcclusion,
    eReblurSpecular,
    eReblurSpecularOcclusion,
    eReblurDiffuseSpecular,
    eReblurDiffuseSpecularOcclusion,
    eReblurDiffuseDirectionalOcclusion,
    eSigmaShadow,
    eSigmaShadowTranslucency,
    eRelaxDiffuse,
    eRelaxSpecular,
    eRelaxDiffuseSpecular,
    eCount
};

SL_ENUM_OPERATORS_32(NRDMethods);

// IMPORTANT: default values assume that "meter" is the primary measurement unit. If other units are used,
// values marked as "m" need to be adjusted. NRD inputs (viewZ, hit distance) can be scaled instead of input settings.

// Internally, NRD uses the following sequence based on "CommonSettings::frameIndex":
//     Even frame (0)  Odd frame (1)   ...
//         B W             W B
//         W B             B W
//     BLACK and WHITE modes define cells with VALID data
// Checkerboard can be only horizontal
// Notes:
//     - all inputs have the same resolution - logical FULL resolution
//     - noisy input signals (IN_DIFF_XXX / IN_SPEC_XXX) are tightly packed to the LEFT HALF of the texture (the input pixel = 2x1 screen pixel)
//     - for others the input pixel = 1x1 screen pixel
//     - upsampling will be handled internally in checkerboard mode
enum class NRDCheckerboardMode : uint8_t
{
    OFF,
    BLACK,
    WHITE,

    MAX_NUM
};

enum class NRDAccumulationMode : uint8_t
{
    // Common mode (accumulation continues normally)
    CONTINUE,

    // Discards history and resets accumulation
    RESTART,

    // Like RESTART, but additionally clears resources from potential garbage
    CLEAR_AND_RESTART,

    MAX_NUM
};

enum class NRDPrePassMode
{
    // Pre-pass is disabled
    OFF,

    // A not requiring additional inputs spatial reuse pass
    SIMPLE,

    // A requiring IN_DIFF_DIRECTION_PDF / IN_SPEC_DIRECTION_PDF spatial reuse pass
    ADVANCED
};

struct NRDCommonSettings
{
    // World-space to camera-space matrix
    float4x4 worldToViewMatrix = {};

    // If "isMotionVectorInWorldSpace = true" will be used as "MV * motionVectorScale.xyy"
    float motionVectorScale[2] = { 1.0f, 1.0f };

    // (0; 1] - dynamic resolution scaling
    float resolutionScale[2] = { 1.0f, 1.0f };

    // (ms) - user provided if > 0, otherwise - tracked internally
    float timeDeltaBetweenFrames = 0.0f;

    // (m) > 0 - use TLAS or tracing range
    float denoisingRange = 1e7f;

    // (normalized %)
    float disocclusionThreshold = 0.01f;

    // [0; 1] - enables "noisy input / denoised output" comparison
    float splitScreen = 0.0f;

    // To reset history set to RESTART / CLEAR_AND_RESTART for one frame
    NRDAccumulationMode accumulationMode = NRDAccumulationMode::CONTINUE;

    // If "true" IN_DIFF_CONFIDENCE and IN_SPEC_CONFIDENCE are provided
    bool isHistoryConfidenceInputsAvailable = false;
};

// "Normalized hit distance" = saturate( "hit distance" / f ), where:
// f = ( A + viewZ * B ) * lerp( 1.0, C, exp2( D * roughness ^ 2 ) ), see "NRD.hlsl/REBLUR_FrontEnd_GetNormHitDist"
struct NRDHitDistanceParameters
{
    // (m) - constant value
    float A = 3.0f;

    // (> 0) - viewZ based linear scale (1 m - 10 cm, 10 m - 1 m, 100 m - 10 m)
    float B = 0.1f;

    // (>= 1) - roughness based scale, use values > 1 to get bigger hit distance for low roughness
    float C = 10.0f;

    // (<= 0) - absolute value should be big enough to collapse "exp2( D * roughness ^ 2 )" to "~0" for roughness = 1
    float D = -25.0f;
};

// Optional specular lobe trimming = A * smoothstep( B, C, roughness )
// Recommended settings if lobe trimming is needed = { 0.85f, 0.04f, 0.11f }
struct NRDLobeTrimmingParameters
{
    // [0; 1] - main level  (0 - GGX dominant direction, 1 - full lobe)
    float A = 1.0f;

    // [0; 1] - max trimming if roughness is less than this threshold
    float B = 0.0f;

    // [0; 1] - main level if roughness is greater than this threshold
    float C = 0.0001f;
};

// Antilag logic:
//    delta = ( abs( old - new ) - localVariance * sigmaScale ) / ( max( old, new ) + localVariance * sigmaScale + sensitivityToDarkness )
//    delta = LinearStep( thresholdMax, thresholdMin, delta )
//        - 1 - keep accumulation
//        - 0 - history reset
struct NRDAntilagIntensitySettings
{
    // (normalized %) - must be big enough to almost ignore residual noise (boiling), default is tuned for 0.5rpp in general
    float thresholdMin = 0.04f;

    // (normalized %) - max > min, usually 3-5x times greater than min
    float thresholdMax = 0.20f;

    // (> 0) - real delta is reduced by local variance multiplied by this value
    float sigmaScale = 1.0f;

    // (intensity units * exposure) - the default is tuned for inputs multiplied by exposure without over-exposuring
    float sensitivityToDarkness = 0.75f;

    // Ideally, must be enabled, but since "sensitivityToDarkness" requires fine tuning from the app side it is disabled by default
    bool enable = false;
};

struct NRDAntilagHitDistanceSettings
{
    // (normalized %) - must almost ignore residual noise (boiling), default is tuned for 0.5rpp for the worst case
    float thresholdMin = 0.02f;

    // (normalized %) - max > min, usually 2-4x times greater than min
    float thresholdMax = 0.10f;

    // (> 0) - real delta is reduced by local variance multiplied by this value
    float sigmaScale = 1.0f;

    // (0; 1] - hit distances are normalized
    float sensitivityToDarkness = 0.5f;

    // Enabled by default
    bool enable = true;
};

// REBLUR_DIFFUSE and REBLUR_DIFFUSE_OCCLUSION

const uint32_t REBLUR_MAX_HISTORY_FRAME_NUM = 63;

struct NRDReblurSettings
{
    NRDLobeTrimmingParameters lobeTrimmingParameters = {};
    NRDHitDistanceParameters hitDistanceParameters = {};
    NRDAntilagIntensitySettings antilagIntensitySettings = {};
    NRDAntilagHitDistanceSettings antilagHitDistanceSettings = {};

    // [0; REBLUR_MAX_HISTORY_FRAME_NUM]
    uint32_t maxAccumulatedFrameNum = 31;

    // (pixels) - base (worst case) denoising radius
    float blurRadius = 30.0f;

    // (normalized %) - defines base blur radius shrinking when number of accumulated frames increases
    float minConvergedStateBaseRadiusScale = 0.25f;

    // [0; 10] - adaptive radius scale, comes into play if the algorithm detects boiling
    float maxAdaptiveRadiusScale = 5.0f;

    // (normalized %) - base fraction of diffuse or specular lobe angle used to drive normal based rejection
    float lobeAngleFraction = 0.1f;

    // (normalized %) - base fraction of center roughness used to drive roughness based rejection
    float roughnessFraction = 0.05f;

    // [0; 1] - if roughness < this, temporal accumulation becomes responsive and driven by roughness (useful for animated water)
    float responsiveAccumulationRoughnessThreshold = 0.0f;

    // (normalized %) - stabilizes output, more stabilization improves antilag (clean signals can use lower values)
    float stabilizationStrength = 1.0f;

    // (normalized %) - aggresiveness of history reconstruction in disoccluded regions (0 - no reconstruction)
    float historyFixStrength = 1.0f;

    // (normalized %) - represents maximum allowed deviation from local tangent plane
    float planeDistanceSensitivity = 0.005f;

    // (normalized %) - adds a portion of input to the output of spatial passes
    float inputMix = 0.0f;

    // [0.01; 0.1] - default is tuned for 0.5rpp for the worst case
    float residualNoiseLevel = 0.03f;

    // If checkerboarding is enabled, defines the orientation of even numbered frames
    NRDCheckerboardMode checkerboardMode = NRDCheckerboardMode::OFF;

    // Enables a spatial reuse pass before the accumulation pass
    NRDPrePassMode prePassMode = NRDPrePassMode::SIMPLE;

    // Adds bias in case of badly defined signals, but tries to fight with fireflies
    bool enableAntiFirefly = false;

    // Turns off spatial filtering, more aggressive accumulation
    bool enableReferenceAccumulation = false;

    // Boosts performance by sacrificing IQ
    bool enablePerformanceMode = false;

    // Spatial passes do optional material index comparison as: ( materialEnabled ? material[ center ] == material[ sample ] : 1 )
    bool enableMaterialTestForDiffuse = false;
    bool enableMaterialTestForSpecular = false;
};

// SIGMA_SHADOW and SIGMA_SHADOW_TRANSLUCENCY

struct NRDSigmaShadowSettings
{
    // (m) - viewZ 1m => only 2 mm deviations from surface plane are allowed
    float planeDistanceSensitivity = 0.002f;
    // [1; 3] - adds bias and stability if > 1
    float blurRadiusScale = 2.0f;
};

// RELAX_DIFFUSE_SPECULAR

const uint32_t RELAX_MAX_HISTORY_FRAME_NUM = 63;

struct NRDRelaxDiffuseSpecularSettings
{
    // [0; 100] - radius in pixels (0 disables prepass)
    float specularPrepassBlurRadius = 50.0f;
    // [0; 100] - radius in pixels (0 disables prepass)
    float diffusePrepassBlurRadius = 0.0f;
    // [0; RELAX_MAX_HISTORY_FRAME_NUM]
    uint32_t specularMaxAccumulatedFrameNum = 31;
    // [0; RELAX_MAX_HISTORY_FRAME_NUM]
    uint32_t specularMaxFastAccumulatedFrameNum = 8;
    // [0; RELAX_MAX_HISTORY_FRAME_NUM]
    uint32_t diffuseMaxAccumulatedFrameNum = 31;
    // [0; RELAX_MAX_HISTORY_FRAME_NUM]
    uint32_t diffuseMaxFastAccumulatedFrameNum = 8;
    // How much variance we inject to specular if reprojection confidence is low
    float specularVarianceBoost = 1.0f;
    // [0; 1], shorten diffuse history if dot (N, previousN) is less than  (1 - this value), this maintains sharpness
    float rejectDiffuseHistoryNormalThreshold = 0.0f;
    // Normal edge stopper for cross-bilateral sparse filter
    float disocclusionFixEdgeStoppingNormalPower = 8.0f;
    // Maximum radius for sparse bilateral filter, expressed in pixels
    float disocclusionFixMaxRadius = 14.0f;
    // Cross-bilateral sparse filter will be applied to frames with history length shorter than this value
    uint32_t disocclusionFixNumFramesToFix = 3;
    // [1; 3] - standard deviation scale of color box for clamping main "slow" history to responsive "fast" history
    float historyClampingColorBoxSigmaScale = 2.0f;
    // History length threshold below which spatial variance estimation will be executed
    uint32_t spatialVarianceEstimationHistoryThreshold = 3;
    // [2; 8] - number of iteration for A-Trous wavelet transform
    uint32_t atrousIterationNum = 5;
    // A-trous edge stopping Luminance sensitivity
    float specularPhiLuminance = 2.0f;
    // A-trous edge stopping Luminance sensitivity
    float diffusePhiLuminance = 2.0f;
    // [0; 1] - A-trous edge stopping Luminance weight minimum
    float minLuminanceWeight = 0.0f;
    // A-trous edge stopping normal sensitivity for diffuse, spatial variance estimation normal sensitivity
    float phiNormal = 64.0f;
    // A-trous edge stopping depth sensitivity
    float phiDepth = 0.05f;
    // Base fraction of the specular lobe angle used in normal based rejection of specular during A-Trous passes; 0.333 works well perceptually
    float specularLobeAngleFraction = 0.333f;
    // Slack (in degrees) for the specular lobe angle used in normal based rejection of specular during A-Trous passes
    float specularLobeAngleSlack = 0.3f;
    // How much we relax roughness based rejection in areas where specular reprojection is low
    float roughnessEdgeStoppingRelaxation = 0.3f;
    // How much we relax normal based rejection in areas where specular reprojection is low
    float normalEdgeStoppingRelaxation = 0.3f;
    // How much we relax luminance based rejection in areas where specular reprojection is low
    float luminanceEdgeStoppingRelaxation = 1.0f;
    // If not OFF, diffuse mode equals checkerboard mode set here, and specular mode opposite: WHITE if diffuse is BLACK and vice versa
    NRDCheckerboardMode checkerboardMode = NRDCheckerboardMode::OFF;
    // Skip reprojection test when there is no motion, might improve quality along the edges for static camera with a jitter
    bool enableSkipReprojectionTestWithoutMotion = false;
    // Clamp specular virtual history to the current frame neighborhood
    bool enableSpecularVirtualHistoryClamping = true;
    // Limit specular accumulation based on roughness
    bool enableRoughnessBasedSpecularAccumulation = true;
    // Roughness based rejection
    bool enableRoughnessEdgeStopping = true;
    // Firefly suppression
    bool enableAntiFirefly = false;
};

// RELAX_DIFFUSE

struct NRDRelaxDiffuseSettings
{
    // [0; 100] - radius in pixels (0 disables prepass)
    float prepassBlurRadius = 0.0f;
    uint32_t diffuseMaxAccumulatedFrameNum = 31;
    uint32_t diffuseMaxFastAccumulatedFrameNum = 8;
    // [0; 1], shorten diffuse history if dot (N, previousN) is less than  (1 - this value), this maintains sharpness
    float rejectDiffuseHistoryNormalThreshold = 0.0f;
    float disocclusionFixEdgeStoppingNormalPower = 8.0f;
    float disocclusionFixMaxRadius = 14.0f;
    uint32_t disocclusionFixNumFramesToFix = 3;
    float historyClampingColorBoxSigmaScale = 2.0f;
    uint32_t spatialVarianceEstimationHistoryThreshold = 3;
    uint32_t atrousIterationNum = 5;
    float diffusePhiLuminance = 2.0f;
    float minLuminanceWeight = 0.0f;
    float phiNormal = 64.0f;
    float phiDepth = 0.05f;
    NRDCheckerboardMode checkerboardMode = NRDCheckerboardMode::OFF;
    bool enableSkipReprojectionTestWithoutMotion = false;
    bool enableAntiFirefly = false;
};

// RELAX_SPECULAR

struct NRDRelaxSpecularSettings
{
    float prepassBlurRadius = 50.0f;
    uint32_t specularMaxAccumulatedFrameNum = 31;
    uint32_t specularMaxFastAccumulatedFrameNum = 8;
    float specularVarianceBoost = 1.0f;
    float disocclusionFixEdgeStoppingNormalPower = 8.0f;
    float disocclusionFixMaxRadius = 14.0f;
    uint32_t disocclusionFixNumFramesToFix = 3;
    float historyClampingColorBoxSigmaScale = 2.0f;
    uint32_t spatialVarianceEstimationHistoryThreshold = 3;
    uint32_t atrousIterationNum = 5;
    float specularPhiLuminance = 2.0f;
    float minLuminanceWeight = 0.0f;
    float phiNormal = 64.0f;
    float phiDepth = 0.05f;
    float specularLobeAngleFraction = 0.333f;
    float specularLobeAngleSlack = 0.3f;
    float roughnessEdgeStoppingRelaxation = 0.3f;
    float normalEdgeStoppingRelaxation = 0.3f;
    float luminanceEdgeStoppingRelaxation = 1.0f;
    NRDCheckerboardMode checkerboardMode = NRDCheckerboardMode::OFF;
    bool enableSkipReprojectionTestWithoutMotion = false;
    bool enableSpecularVirtualHistoryClamping = true;
    bool enableRoughnessBasedSpecularAccumulation = true;
    bool enableRoughnessEdgeStopping = true;
    bool enableAntiFirefly = false;
};

// {616B9345-F235-40F3-8EA7-BEE1E153F95A}
SL_STRUCT(NRDConstants, StructType({ 0x616b9345, 0xf235, 0x40f3, { 0x8e, 0xa7, 0xbe, 0xe1, 0xe1, 0x53, 0xf9, 0x5a } }), kStructVersion1)
    //! Specifies which methodsshould be used (1 << eNRDMethodXXX | 1 << eNRDMethodYYY etc)
    //! Note that this serves as a unique ID and must be provided in the evaluate call.
    uint32_t methodMask {};
    //! Clip to world space matrix
    float4x4 clipToWorld;
    //! Previous clip to world space matrix
    float4x4 clipToWorldPrev;
    //! Common tweaks
    NRDCommonSettings common;
    //! Reblur Settings
    NRDReblurSettings reblurSettings;
    //! Specular tweaks
    NRDRelaxSpecularSettings relaxSpecular;
    //! Diffuse tweaks
    NRDRelaxDiffuseSettings relaxDiffuse;
    //! Diffuse/Specular tweaks
    NRDRelaxDiffuseSpecularSettings relaxDiffuseSpecular;
    //! Shadow tweaks
    NRDSigmaShadowSettings sigmaShadow;
};

}

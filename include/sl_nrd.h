/*
* Copyright (c) 2022-2023 NVIDIA CORPORATION. All rights reserved
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

// inputs
constexpr BufferType kBufferTypeInDiffuseRadianceHitDist =      FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 0);
constexpr BufferType kBufferTypeInSpecularRadianceHitDist =     FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 1);
constexpr BufferType kBufferTypeInDiffuseHitDist =              FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 2);
constexpr BufferType kBufferTypeInSpecularHitDist =             FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 3);
constexpr BufferType kBufferTypeInDiffuseDirectionHitDist =     FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 4);
constexpr BufferType kBufferTypeInDiffuseSH0 =                  FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 5);
constexpr BufferType kBufferTypeInDiffuseSH1 =                  FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 6);
constexpr BufferType kBufferTypeInSpecularSH0 =                 FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 7);
constexpr BufferType kBufferTypeInSpecularSH1 =                 FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 8);
constexpr BufferType kBufferTypeInDiffuseConfidence =           FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 9);
constexpr BufferType kBufferTypeInSpecularConfidence =          FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 10);
constexpr BufferType kBufferTypeInDisocclusionThresholdMix =    FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 11);
constexpr BufferType kBufferTypeInBasecolorMetalness =          FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 12);
constexpr BufferType kBufferTypeInShadowData =                  FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 13);
constexpr BufferType kBufferTypeInShadowTransluscency =         FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 14);
constexpr BufferType kBufferTypeInRadiance =                    FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 15);
constexpr BufferType kBufferTypeInDeltaPrimaryPos =             FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 16);
constexpr BufferType kBufferTypeInDeltaSecondaryPos =           FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 17);

// ouputs
constexpr BufferType kBufferTypeOutDiffuseRadianceHitDist =     FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 18);
constexpr BufferType kBufferTypeOutSpecularRadianceHitDist =    FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 19);
constexpr BufferType kBufferTypeOutDiffuseSH0 =                 FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 20);
constexpr BufferType kBufferTypeOutDiffuseSH1 =                 FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 21);
constexpr BufferType kBufferTypeOutSpecularSH0 =                FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 22);
constexpr BufferType kBufferTypeOutSpecularSH1 =                FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 23);
constexpr BufferType kBufferTypeOutDiffuseHitDist =             FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 24);
constexpr BufferType kBufferTypeOutSpecularHitDist =            FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 25);
constexpr BufferType kBufferTypeOutDiffuseDirectionHitDist =    FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 26);
constexpr BufferType kBufferTypeOutShadowTransluscency =        FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 27);
constexpr BufferType kBufferTypeOutRadiance =                   FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 28);
constexpr BufferType kBufferTypeOutReflectionMv =               FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 29);
constexpr BufferType kBufferTypeOutDeltaMv =                    FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 30);
constexpr BufferType kBufferTypeOutValidation =                 FEATURE_SPECIFIC_BUFFER_TYPE_ID(kFeatureNRD, 31);

enum class NRDMethods : uint32_t
{
    eOff,
    eReblurDiffuse,
    eReblurDiffuseOcclusion,
    eReblurDiffuseSh,
    eReblurSpecular,
    eReblurSpecularOcclusion,
    eReblurSpecularSh,
    eReblurDiffuseSpecular,
    eReblurDiffuseSpecularOcclusion,
    eReblurDiffuseSpecularSh,
    eReblurDiffuseDirectionalOcclusion,
    eSigmaShadow,
    eSigmaShadowTranslucency,
    eRelaxDiffuse,
    eRelaxDiffuseSh,
    eRelaxSpecular,
    eRelaxSpecularSh,
    eRelaxDiffuseSpecular,
    eRelaxDiffuseSpecularSh,
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

enum class NRDHitDistanceReconstructionMode : uint8_t
{
    // Probabilistic split at primary hit is not used, hence hit distance is always valid (reconstruction is not needed)
    OFF,

    // If hit distance is invalid due to probabilistic sampling, reconstruct using 3x3 neighbors
    AREA_3X3,

    // If hit distance is invalid due to probabilistic sampling, reconstruct using 5x5 neighbors
    AREA_5X5,

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
    // Matrix requirements:
    //     - usage - vector is a column
    //     - layout - column-major
    //     - non jittered!
    // LH / RH projection matrix (INF far plane is supported) with non-swizzled rows, i.e. clip-space depth = z / w
    float viewToClipMatrix[16] = {};

    // Previous projection matrix
    float viewToClipMatrixPrev[16] = {};

    // World-space to camera-space matrix
    float worldToViewMatrix[16] = {};

    // If coordinate system moves with the camera, camera delta must be included to reflect camera motion
    float worldToViewMatrixPrev[16] = {};

    // (Optional) Previous world-space to current world-space matrix. It is for virtual normals, where a coordinate
    // system of the virtual space changes frame to frame, such as in a case of animated intermediary reflecting
    // surfaces when primary surface replacement is used for them.
    float worldPrevToWorldMatrix[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    // used as "IN_MV * motionVectorScale" (use .z = 0 for 2D screen-space motion)
    float motionVectorScale[3] = { 1.0f, 1.0f, 0.0f };

    // [-0.5; 0.5] - sampleUv = pixelUv + cameraJitter
    float cameraJitter[2] = {};
    float cameraJitterPrev[2] = {};

    // (0; 1] - dynamic resolution scaling
    float resolutionScale[2] = { 1.0f, 1.0f };
    float resolutionScalePrev[2] = { 1.0f, 1.0f };

    // (ms) - user provided if > 0, otherwise - tracked internally
    float timeDeltaBetweenFrames = 0.0f;

    // (units) > 0 - use TLAS or tracing range (max value = NRD_FP16_MAX / NRD_FP16_VIEWZ_SCALE - 1 = 524031)
    float denoisingRange = 500000.0f;

    // (normalized %) - if relative distance difference is greater than threshold, history gets reset (0.5-2.5% works well)
    float disocclusionThreshold = 0.01f;

    // (normalized %) - alternative disocclusion threshold, which is mixed to based on IN_DISOCCLUSION_THRESHOLD_MIX
    float disocclusionThresholdAlternate = 0.05f;

    // [0; 1] - enables "noisy input / denoised output" comparison
    float splitScreen = 0.0f;

    // For internal needs
    float debug = 0.0f;

    // (pixels) - data rectangle origin in ALL input textures
    uint32_t inputSubrectOrigin[2] = {};

    // A consecutive number
    uint32_t frameIndex = 0;

    // To reset history set to RESTART / CLEAR_AND_RESTART for one frame
    NRDAccumulationMode accumulationMode = NRDAccumulationMode::CONTINUE;

    // If "true" IN_MV is 3D motion in world-space (0 should be everywhere if the scene is static),
    // otherwise it's 2D (+ optional Z delta) screen-space motion (0 should be everywhere if the camera doesn't move) (recommended value = true)
    bool isMotionVectorInWorldSpace = false;

    // If "true" IN_DIFF_CONFIDENCE and IN_SPEC_CONFIDENCE are available
    bool isHistoryConfidenceAvailable = false;

    // If "true" IN_DISOCCLUSION_THRESHOLD_MIX is available
    bool isDisocclusionThresholdMixAvailable = false;

    // If "true" IN_BASECOLOR_METALNESS is available
    bool isBaseColorMetalnessAvailable = false;

    // Enables debug overlay in OUT_VALIDATION, requires "DenoiserCreationDesc::allowValidation = true"
    bool enableValidation = false;
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

struct NRDReblurAntilagSettings
{
    // [1; 3] - delta is reduced by local variance multiplied by this value
    float luminanceSigmaScale = 2.0f;
    float hitDistanceSigmaScale = 1.0f;

    // (0; 1] - antilag = pow( antilag, power )
    float luminanceAntilagPower = 0.5f;
    float hitDistanceAntilagPower = 1.0f;
};

// REBLUR_DIFFUSE and REBLUR_DIFFUSE_OCCLUSION

const uint32_t REBLUR_MAX_HISTORY_FRAME_NUM = 63;

struct NRDReblurSettings
{
    NRDHitDistanceParameters hitDistanceParameters = {};
    NRDReblurAntilagSettings antilagSettings = {};

    // [0; REBLUR_MAX_HISTORY_FRAME_NUM] - maximum number of linearly accumulated frames (= FPS * "time of accumulation")
    uint32_t maxAccumulatedFrameNum = 30;

    // [0; REBLUR_MAX_HISTORY_FRAME_NUM] - maximum number of linearly accumulated frames in fast history (less than "maxAccumulatedFrameNum")
    uint32_t maxFastAccumulatedFrameNum = 6;

    // [0; REBLUR_MAX_HISTORY_FRAME_NUM] - number of reconstructed frames after history reset (less than "maxFastAccumulatedFrameNum")
    uint32_t historyFixFrameNum = 3;

    // (pixels) - pre-accumulation spatial reuse pass blur radius (0 = disabled, must be used in case of probabilistic sampling)
    float diffusePrepassBlurRadius = 30.0f;
    float specularPrepassBlurRadius = 50.0f;

    // (pixels) - base denoising radius (30 is a baseline for 1440p)
    float blurRadius = 15.0f;

    // (pixels) - base stride between samples in history reconstruction pass
    float historyFixStrideBetweenSamples = 14.0f;

    // (normalized %) - base fraction of diffuse or specular lobe angle used to drive normal based rejection
    float lobeAngleFraction = 0.13f;

    // (normalized %) - base fraction of center roughness used to drive roughness based rejection
    float roughnessFraction = 0.15f;

    // [0; 1] - if roughness < this, temporal accumulation becomes responsive and driven by roughness (useful for animated water)
    float responsiveAccumulationRoughnessThreshold = 0.0f;

    // (normalized %) - stabilizes output, more stabilization improves antilag (clean signals can use lower values)
    float stabilizationStrength = 1.0f;

    // (normalized %) - represents maximum allowed deviation from local tangent plane
    float planeDistanceSensitivity = 0.005f;

    // IN_MV = lerp(IN_MV, specularMotion, smoothstep(specularProbabilityThresholdsForMvModification[0], specularProbabilityThresholdsForMvModification[1], specularProbability))
    float specularProbabilityThresholdsForMvModification[2] = { 0.5f, 0.9f };

    // If not OFF and used for DIFFUSE_SPECULAR, defines diffuse orientation, specular orientation is the opposite
    NRDCheckerboardMode checkerboardMode = NRDCheckerboardMode::OFF;

    // Must be used only in case of probabilistic sampling (not checkerboarding), when a pixel can be skipped and have "0" (invalid) hit distance
    NRDHitDistanceReconstructionMode hitDistanceReconstructionMode = NRDHitDistanceReconstructionMode::OFF;

    // Adds bias in case of badly defined signals, but tries to fight with fireflies
    bool enableAntiFirefly = false;

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

struct NRDRelaxAntilagSettings
{
    // IMPORTANT: History acceleration and reset amounts for specular are made 2x-3x weaker than values for diffuse below
    // due to specific specular logic that does additional history acceleration and reset

    // (>= 0) - amount of history acceleration if history clamping happened in pixel
    float accelerationAmount = 3.0f;

    // (> 0) - history is being reset if delta between history and raw input is larger than spatial sigma + temporal sigma
    float spatialSigmaScale = 4.5f;

    // (> 0) - history is being reset if delta between history and raw input is larger than spatial sigma + temporal sigma
    float temporalSigmaScale = 0.5f;

    // [0; 1] - amount of history reset, 0.0 - no reset, 1.0 - full reset
    float resetAmount = 0.5f;
};

struct NRDRelaxDiffuseSpecularSettings
{
    NRDRelaxAntilagSettings antilagSettings = {};

    // (pixels) - pre-accumulation spatial reuse pass blur radius (0 = disabled, must be used in case of probabilistic sampling)
    float diffusePrepassBlurRadius = 0.0f;
    float specularPrepassBlurRadius = 50.0f;

    // [0; RELAX_MAX_HISTORY_FRAME_NUM] - maximum number of linearly accumulated frames ( = FPS * "time of accumulation")
    uint32_t diffuseMaxAccumulatedFrameNum = 30;
    uint32_t specularMaxAccumulatedFrameNum = 30;

    // [0; RELAX_MAX_HISTORY_FRAME_NUM] - maximum number of linearly accumulated frames in fast history (less than "maxAccumulatedFrameNum")
    uint32_t diffuseMaxFastAccumulatedFrameNum = 6;
    uint32_t specularMaxFastAccumulatedFrameNum = 6;

    // [0; RELAX_MAX_HISTORY_FRAME_NUM] - number of reconstructed frames after history reset (less than "maxFastAccumulatedFrameNum")
    uint32_t historyFixFrameNum = 3;

    // A-trous edge stopping Luminance sensitivity
    float diffusePhiLuminance = 2.0f;
    float specularPhiLuminance = 1.0f;

    // (normalized %) - base fraction of diffuse or specular lobe angle used to drive normal based rejection
    float diffuseLobeAngleFraction = 0.5f;
    float specularLobeAngleFraction = 0.5f;

    // (normalized %) - base fraction of center roughness used to drive roughness based rejection
    float roughnessFraction = 0.15f;

    // (>= 0) - how much variance we inject to specular if reprojection confidence is low
    float specularVarianceBoost = 0.0f;

    // (degrees) - slack for the specular lobe angle used in normal based rejection of specular during A-Trous passes
    float specularLobeAngleSlack = 0.15f;

    // (pixels) - base stride between samples in history reconstruction pass
    float historyFixStrideBetweenSamples = 14.0f;

    // (> 0) - normal edge stopper for history reconstruction pass
    float historyFixEdgeStoppingNormalPower = 8.0f;

    // [1; 3] - standard deviation scale of color box for clamping main "slow" history to responsive "fast" history
    float historyClampingColorBoxSigmaScale = 2.0f;

    // (>= 0) - history length threshold below which spatial variance estimation will be executed
    uint32_t spatialVarianceEstimationHistoryThreshold = 3;

    // [2; 8] - number of iteration for A-Trous wavelet transform
    uint32_t atrousIterationNum = 5;

    // [0; 1] - A-trous edge stopping Luminance weight minimum
    float diffuseMinLuminanceWeight = 0.0f;
    float specularMinLuminanceWeight = 0.0f;

    // (normalized %) - Depth threshold for spatial passes
    float depthThreshold = 0.003f;

    // Confidence inputs can affect spatial blurs, relaxing some weights in areas with low confidence
    float confidenceDrivenRelaxationMultiplier = 0.0f;
    float confidenceDrivenLuminanceEdgeStoppingRelaxation = 0.0f;
    float confidenceDrivenNormalEdgeStoppingRelaxation = 0.0f;

    // How much we relax roughness based rejection for spatial filter in areas where specular reprojection is low
    float luminanceEdgeStoppingRelaxation = 0.5f;
    float normalEdgeStoppingRelaxation = 0.3f;

    // How much we relax rejection for spatial filter based on roughness and view vector
    float roughnessEdgeStoppingRelaxation = 1.0f;

    // If not OFF and used for DIFFUSE_SPECULAR, defines diffuse orientation, specular orientation is the opposite
    NRDCheckerboardMode checkerboardMode = NRDCheckerboardMode::OFF;

    // Must be used only in case of probabilistic sampling (not checkerboarding), when a pixel can be skipped and have "0" (invalid) hit distance
    NRDHitDistanceReconstructionMode hitDistanceReconstructionMode = NRDHitDistanceReconstructionMode::OFF;

    // Firefly suppression
    bool enableAntiFirefly = false;

    // Skip reprojection test when there is no motion, might improve quality along the edges for static camera with a jitter
    bool enableReprojectionTestSkippingWithoutMotion = false;

    // Roughness based rejection
    bool enableRoughnessEdgeStopping = true;

    // Spatial passes do optional material index comparison as: ( materialEnabled ? material[ center ] == material[ sample ] : 1 )
    bool enableMaterialTestForDiffuse = false;
    bool enableMaterialTestForSpecular = false;
};

// RELAX_DIFFUSE

struct NRDRelaxDiffuseSettings
{
    NRDRelaxAntilagSettings antilagSettings = {};

    float prepassBlurRadius = 0.0f;

    uint32_t diffuseMaxAccumulatedFrameNum = 30;
    uint32_t diffuseMaxFastAccumulatedFrameNum = 6;
    uint32_t historyFixFrameNum = 3;

    float diffusePhiLuminance = 2.0f;
    float diffuseLobeAngleFraction = 0.5f;

    float historyFixEdgeStoppingNormalPower = 8.0f;
    float historyFixStrideBetweenSamples = 14.0f;

    float historyClampingColorBoxSigmaScale = 2.0f;

    uint32_t spatialVarianceEstimationHistoryThreshold = 3;
    uint32_t atrousIterationNum = 5;
    float minLuminanceWeight = 0.0f;
    float depthThreshold = 0.01f;

    float confidenceDrivenRelaxationMultiplier = 0.0f;
    float confidenceDrivenLuminanceEdgeStoppingRelaxation = 0.0f;
    float confidenceDrivenNormalEdgeStoppingRelaxation = 0.0f;

    NRDCheckerboardMode checkerboardMode = NRDCheckerboardMode::OFF;
    NRDHitDistanceReconstructionMode hitDistanceReconstructionMode = NRDHitDistanceReconstructionMode::OFF;

    bool enableAntiFirefly = false;
    bool enableReprojectionTestSkippingWithoutMotion = false;
    bool enableMaterialTest = false;
};

// RELAX_SPECULAR

struct NRDRelaxSpecularSettings
{
    NRDRelaxAntilagSettings antilagSettings = {};

    float prepassBlurRadius = 50.0f;

    uint32_t specularMaxAccumulatedFrameNum = 30;
    uint32_t specularMaxFastAccumulatedFrameNum = 6;
    uint32_t historyFixFrameNum = 3;

    float specularPhiLuminance = 1.0f;
    float diffuseLobeAngleFraction = 0.5f;
    float specularLobeAngleFraction = 0.5f;
    float roughnessFraction = 0.15f;

    float specularVarianceBoost = 0.0f;
    float specularLobeAngleSlack = 0.15f;

    float historyFixEdgeStoppingNormalPower = 8.0f;
    float historyFixStrideBetweenSamples = 14.0f;

    float historyClampingColorBoxSigmaScale = 2.0f;

    uint32_t spatialVarianceEstimationHistoryThreshold = 3;
    uint32_t atrousIterationNum = 5;
    float minLuminanceWeight = 0.0f;
    float depthThreshold = 0.01f;

    float confidenceDrivenRelaxationMultiplier = 0.0f;
    float confidenceDrivenLuminanceEdgeStoppingRelaxation = 0.0f;
    float confidenceDrivenNormalEdgeStoppingRelaxation = 0.0f;

    float luminanceEdgeStoppingRelaxation = 0.5f;
    float normalEdgeStoppingRelaxation = 0.3f;
    float roughnessEdgeStoppingRelaxation = 1.0f;

    NRDCheckerboardMode checkerboardMode = NRDCheckerboardMode::OFF;
    NRDHitDistanceReconstructionMode hitDistanceReconstructionMode = NRDHitDistanceReconstructionMode::OFF;

    bool enableAntiFirefly = false;
    bool enableReprojectionTestSkippingWithoutMotion = false;
    bool enableRoughnessEdgeStopping = true;
    bool enableMaterialTest = false;
}; 

struct NRDReferenceSettings
{
    // (>= 0) - maximum number of linearly accumulated frames ( = FPS * "time of accumulation")
    uint32_t maxAccumulatedFrameNum = 1024;
};

struct NRDSpecularReflectionMvSettings
{
    float unused;
};

struct NRDSpecularDeltaMvSettings
{
    float unused;
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

//! Sets NRD options
//!
//! Call this method to provide constants for NRD, change mode etc.
//!
//! @param viewport Specified viewport we are working with
//! @param options Specifies NRD constants to use
//! @return sl::ResultCode::eOk if successful, error code otherwise (see sl_result.h for details)
//!
//! This method is NOT thread safe.
inline sl::Result slNRDSetConstants(const sl::ViewportHandle& viewport, const sl::NRDConstants& constants)
{
    using PFun_slSetFeatureSpecificInputs = sl::Result(const sl::FrameToken& frame, const sl::BaseStructure** inputs, uint32_t numInputs);
    struct : public sl::FrameToken { operator uint32_t() const override { return 0; } } fakeToken;
    sl::BaseStructure const* inputs[] = { &constants, &viewport };
    SL_FEATURE_FUN_IMPORT_STATIC(sl::kFeatureNRD, slSetFeatureSpecificInputs);
    return s_slSetFeatureSpecificInputs(fakeToken, inputs, sizeof(inputs) / sizeof(sl::BaseStructure const*));
}

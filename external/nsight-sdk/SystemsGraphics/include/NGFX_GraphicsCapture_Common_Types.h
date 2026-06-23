/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//--------------------------------------------------------------------------------------
// File: NGFX_GraphicsCapture_Common_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_GRAPHICS_CAPTURE_COMMON_TYPES_H
#define NGFX_GRAPHICS_CAPTURE_COMMON_TYPES_H

#include "NGFX_D3D12_Types.h"
#include "NGFX_Vulkan_Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief Enum representing the type of delimiter used for graphics capture
 */
typedef enum NGFX_GraphicsCapture_Delimiter
{
    NGFX_GraphicsCapture_Delimiter_Present,            /**< Use Present to delimit a frame. */
    NGFX_GraphicsCapture_Delimiter_FrameBoundary,      /**< Use NGFX FrameBoundary to delimit a frame. */
    NGFX_GraphicsCapture_Delimiter_VkFrameBoundaryEXT, /**< Use VK_Ext_frame_boundary to delimit a frame. */
} NGFX_GraphicsCapture_Delimiter;

typedef enum NGFX_GraphicsCapture_HudPosition
{
    NGFX_GraphicsCapture_HUD_POSITION_DEFAULT = 0,
    NGFX_GraphicsCapture_HUD_POSITION_TOP_LEFT = 1,
    NGFX_GraphicsCapture_HUD_POSITION_TOP_RIGHT = 2,
    NGFX_GraphicsCapture_HUD_POSITION_BOTTOM_LEFT = 3,
    NGFX_GraphicsCapture_HUD_POSITION_BOTTOM_RIGHT = 4,
    NGFX_GraphicsCapture_HUD_POSITION_TOP = 5,
    NGFX_GraphicsCapture_HUD_POSITION_LEFT = 6,
    NGFX_GraphicsCapture_HUD_POSITION_RIGHT = 7,
    NGFX_GraphicsCapture_HUD_POSITION_BOTTOM = 8,
    NGFX_GraphicsCapture_HUD_POSITION_COUNT = 9
} NGFX_GraphicsCapture_HudPosition;

// should match Pylon::CompressionMode
typedef enum NGFX_GraphicsCapture_Compression
{
    NGFX_GraphicsCapture_COMPRESSION_DEFAULT = 0,
    NGFX_GraphicsCapture_COMPRESSION_NO = 1,
    NGFX_GraphicsCapture_COMPRESSION_HIGH = 2,
    NGFX_GraphicsCapture_COMPRESSION_COUNT = 3
} NGFX_GraphicsCapture_Compression;

typedef enum NGFX_GraphicsCapture_HvvmMode
{
    NGFX_GraphicsCapture_HVVM_DEFAULT = 0,
    NGFX_GraphicsCapture_HVVM_DEMOTE = 1,
    NGFX_GraphicsCapture_HVVM_DISABLE = 2,
    NGFX_GraphicsCapture_HVVM_MANUAL_TRACKING = 3,
    NGFX_GraphicsCapture_HVVM_CPU_HASH = 4,
    NGFX_GraphicsCapture_HVVM_COUNT = 5
} NGFX_GraphicsCapture_HvvmMode;

/**
 * @brief struct containing GraphicsCapture injection settings
 *  Note: these options mirror those displayed by ngfx-capture.exe --help,
 *  which is a useful reference
 */
typedef struct NGFX_GraphicsCapture_InjectionSettings_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GraphicsCapture_InjectionSettings_VER */

    // Application Launch Options
    bool noHUD;                                   /**< Disable the heads-up display (HUD) during capture. */
    NGFX_GraphicsCapture_HudPosition hudPosition; /**< Position of the HUD on screen during capture. */
    bool newConsole;                              /**< Launch the application in a new console window. */
    bool terminateAfterCapture;                   /**< Automatically terminate the application after capture completes. */

    // Capture Output Options
    const char* outputFile;                       /**< Path to the output capture file. If NULL, a default name is used. */
    const char* outputDir;                        /**< Directory where capture files will be saved. If NULL, current directory is used. */
    uint32_t frameCount;                          /**< Maximum number of frames to capture. Default is 1 if not specified. */
    bool bundleReplayer;                          /**< Bundle the replayer with the capture file for easier distribution. */
    bool nonPortable;                             /**< Create a non-portable capture file (may be smaller but requires specific environment). */
    NGFX_GraphicsCapture_Compression compression; /**< Compression level for the capture file. */

    // Capture Triggers
    bool captureDefaultHotkey;      /**< Enable capture using the default hotkey (F11). */
    const char* captureHotkey;      /**< Custom hotkey string for triggering capture. */
    uint32_t captureFrame;          /**< Frame number at which to automatically start capture (1-based indexing). */
    uint32_t captureCountdownTimer; /**< Timer in seconds before automatically starting capture. */
    bool captureUntilHotkey;        /**< Capture until hotkey is pressed or after capturing capturing max number of frames. */

    // Host Visible Video Memory (HVVM) Mode
    NGFX_GraphicsCapture_HvvmMode hvvmMode; /**< Host Visible Video Memory mode for capture. */

    // Ray Tracing Options
    bool useRTASSerializeAPI; /**< Use RTAS serialize API for ray tracing acceleration structures. */
    uint64_t maxSBTSize;      /**< Max shader binding table deep copy size in bytes. 0 indicates unbounded size. */

    // D3D12 Options
    uint64_t d3d12IndirectSBTBufferSize; /**< Buffer size for D3D12 indirect shader binding tables. */

    // Troubleshooting Knobs
    bool ignoreIncompatible;            /**< Ignore incompatible features and continue capture. */
    bool noLazyDataCollection;          /**< Disable lazy data collection (may improve performance). */
    bool noBlockOnFirstIncompatibility; /**< Don't block capture on first incompatibility encountered. */
    bool noInternalPipelineCaches;      /**< Disable internal pipeline cache usage. */
    bool noUncachedMemoryDemotion;      /**< Disable uncached memory demotion. */
    bool noStreamlineCapture;           /**< Disable Streamline capture integration. */
    bool noVulkanWriteWatchMemory;      /**< Disable Vulkan write-watch memory tracking. */
    bool noVulkanCaptureReplayMemory;   /**< Disable Vulkan capture/replay memory optimization. */
    bool noVulkanPrivateDataLookups;    /**< Disable Vulkan private data lookups. */

    // Troubleshooting Knobs (Resource Data Options)
    bool captureFullGPUAllocations; /**< Capture full GPU allocations (may increase file size significantly). */
} NGFX_GraphicsCapture_InjectionSettings_V1;
typedef NGFX_GraphicsCapture_InjectionSettings_V1 NGFX_GraphicsCapture_InjectionSettings;
#define NGFX_GraphicsCapture_InjectionSettings_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GraphicsCapture_InjectionSettings_V1, 1)
#define NGFX_GraphicsCapture_InjectionSettings_VER NGFX_GraphicsCapture_InjectionSettings_V1_VER

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_GRAPHICS_CAPTURE_COMMON_TYPES_H

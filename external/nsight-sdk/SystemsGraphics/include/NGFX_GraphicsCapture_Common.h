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
// File: NGFX_GraphicsCapture_Common.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_GRAPHICS_CAPTURE_COMMON_H
#define NGFX_GRAPHICS_CAPTURE_COMMON_H

#include "Impl/NGFX_Core.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

#define NGFX_INVALID_U32 (uint32_t) - 1
#define NGFX_INVALID_U64 (uint64_t) - 1

/**
 * @brief Initialize NGFX_GraphicsCapture_InjectionSettings struct with default values.
 *
 * @param settings Struct to be populated
 */
NGFX_FUNCTION NGFX_Result NGFX_GraphicsCapture_InjectionSettings_SetDefaults(NGFX_GraphicsCapture_InjectionSettings* settings)
{
    if (!settings)
    {
        return NGFX_Result_InvalidParameter;
    }

    settings->version = NGFX_GraphicsCapture_InjectionSettings_VER;

    // Application Launch Options
    settings->noHUD = false;
    settings->hudPosition = NGFX_GraphicsCapture_HUD_POSITION_DEFAULT;
    settings->newConsole = false;
    settings->terminateAfterCapture = false;

    // Capture Output Options
    settings->outputFile = NULL;
    settings->outputDir = NULL;
    settings->frameCount = 1;
    settings->bundleReplayer = false;
    settings->nonPortable = false;
    settings->compression = NGFX_GraphicsCapture_COMPRESSION_DEFAULT;

    // Capture Triggers
    settings->captureDefaultHotkey = false;
    settings->captureHotkey = NULL;
    settings->captureFrame = NGFX_INVALID_U32;
    settings->captureCountdownTimer = NGFX_INVALID_U32;
    settings->captureUntilHotkey = false;

    // Host Visible Video Memory (HVVM) Mode
    settings->hvvmMode = NGFX_GraphicsCapture_HVVM_DEFAULT;

    // Ray Tracing Options
    settings->useRTASSerializeAPI = false;
    settings->maxSBTSize = 0;

    // D3D12 Options
    settings->d3d12IndirectSBTBufferSize = NGFX_INVALID_U64;

    // Troubleshooting Knobs
    settings->ignoreIncompatible = false;
    settings->noLazyDataCollection = false;
    settings->noBlockOnFirstIncompatibility = false;
    settings->noInternalPipelineCaches = false;
    settings->noUncachedMemoryDemotion = false;
    settings->noStreamlineCapture = false;
    settings->noVulkanWriteWatchMemory = false;
    settings->noVulkanCaptureReplayMemory = false;
    settings->noVulkanPrivateDataLookups = false;

    // Troubleshooting Knobs (Resource Data Options)
    settings->captureFullGPUAllocations = false;

    return NGFX_Result_Success;
}

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_GRAPHICS_CAPTURE_COMMON_H

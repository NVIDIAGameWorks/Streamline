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
// File: NGFX_GPUTrace_Common.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_GPUTRACE_COMMON_H
#define NGFX_GPUTRACE_COMMON_H

#include "Impl/NGFX_Core.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief Checks if the GPU Trace activity is currently injected.
 *
 * @param injected Pointer to a boolean that will be set to true if the activity is injected, false otherwise.
 * @return NGFX_Result_Success if the check is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_IsInjected(bool* injected)
{
    return NGFX_IsActivityInjected(NGFX_ActivityType_GPUTrace, injected);
}

/**
 * @brief Checks if the GPU Trace activity is currently initialized.
 *
 * @param initialized Pointer to a boolean that will be set to true if the activity is initialized, false otherwise.
 * @return NGFX_Result_Success if the check is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_IsInitialized(bool* initialized)
{
    return NGFX_IsActivityInitialized(NGFX_ActivityType_GPUTrace, initialized);
}

/**
 * @brief Retrieves the current status of the GPU Trace activity.
 *
 * @param params Pointer to a variable that will be set to the current GPU Trace status.
 * @return NGFX_Result_Success if the status is successfully retrieved, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_GetStatus(NGFX_GPUTrace_GetStatus_Params* params)
{
    return g_NGFX_Globals.GPUTrace.GetStatus(params);
}

/**
 * @brief Waits for the GPU Trace activity to reach a specific status.
 *
 * @param params Pointer to a structure containing the desired status and timeout.
 * @return NGFX_Result_Success if the reaches the desired status within the timeout, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_WaitForStatus(NGFX_GPUTrace_WaitForStatus_Params* params)
{
    return g_NGFX_Globals.GPUTrace.WaitForStatus(params);
}

/**
 * @brief Initialize Ngfx_GPUTrace_InjectionSettings struct with default values.
 *
 * @param settings Struct to be populated
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_InjectionSettings_SetDefaults(NGFX_GPUTrace_InjectionSettings* settings)
{
    if (!settings)
    {
        return NGFX_Result_InvalidParameter;
    }

    settings->version = NGFX_GPUTrace_InjectionSettings_V1_VER;

    // Trace Controls
    settings->startEvent = NGFX_GPUTrace_StartEvent_Manual;
    settings->stopEvent = NGFX_GPUTrace_StopEvent_Frame;
    settings->stopParams.frameCount = 1;
    settings->maxDurationMs = 1000;

    // Collection Controls
    settings->collectScreenshot = true;
    settings->timeEveryAction = false;
    settings->traceShaderBindings = true;
    settings->collectShaderInfo = true;
    settings->collectExternalShaderDebugInfo = true;
    settings->enableNVTXranges = true;

    // Allocation Controls
    settings->eventBufferSizeKB = 10000;
    settings->timestampCount = 100000;

    // API-specific Controls
    settings->recompileCachedVulkanShaderModules = true;
    settings->disableD3D12DebugLayer = true;
    settings->forceD3D12BackgroundOptimizations = false;
    settings->maxInternalVKSCCommandBuffersPerQueue = 24;
    settings->maxInternalVKSCCommandBuffersMemoryKB = 16;

    // Application Overrides
    settings->vsyncMode = NGFX_GPUTrace_VSyncMode_Off;
    settings->gpuClockMode = NGFX_GPUTrace_GPUClockMode_LockToBase;
    settings->forceRepaint = false;

    // HUD Position
    settings->hudPosition = NGFX_GPUTrace_HUDPosition_TopLeft;

    return NGFX_Result_Success;
}

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_GPUTRACE_COMMON_H

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
// File: NGFX_SystemProfiling_Vulkan.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_SYSTEM_PROFILING_VULKAN_H
#define NGFX_SYSTEM_PROFILING_VULKAN_H

#include "NGFX_SystemProfiling_Common.h"
#include "NGFX_Vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief Injects the SystemProfiling activity into a Vulkan application.
 *
 * @note Only supported on Windows.
 * @note SystemProfiling currently uses direct injection and does not support bootstrap loading.
 *       Bootstrap loading support will be added in the future.
 *
 * @param params injection parameters
 * @return NGFX_Result_Success if the injection is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_SystemProfiling_Inject_Vulkan(NGFX_SystemProfiling_Inject_Vulkan_Params* params)
{
    return NGFX_InjectActivity(NGFX_ActivityType_SystemProfiling, params->installationPath, params->settings);
}

/**
 * @brief Initialize the System profiling activity in an already injected Vulkan application
 *
 * @param params injection parameters
 * @return NGFX_Result_Success if the initialization is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_SystemProfiling_InitializeActivity_Vulkan(NGFX_SystemProfiling_InitializeActivity_Vulkan_Params* params)
{
    return NGFX_Do_InitializeActivity(NGFX_ActivityType_SystemProfiling, NGFX_ApiType_Vulkan, (void*)params);
}

/** @brief Starts system profiling of the application.
 *
 * This method must be paired with an associated NGFX_SystemProfiling_StopProfiling_Vulkan
 *
 * @param params Parameters for starting the profiling
 * @return NGFX_Result_Success if the profiling is successfully started, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_SystemProfiling_StartProfiling_Vulkan(NGFX_SystemProfiling_StartProfiling_Vulkan_Params* params)
{
    // System profiling is handled through injection, so this is a simple success for Vulkan
    (void)params; // Suppress unused parameter warning
    return NGFX_Result_Success;
}

/**
 * @brief Stops a previously started system profiling of the application.
 *
 * This method must have been preceded by NGFX_SystemProfiling_StartProfiling_Vulkan call.
 *
 * @param params Parameters for stopping the profiling
 * @return NGFX_Result_Success if the profiling is successfully stopped, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_SystemProfiling_StopProfiling_Vulkan(NGFX_SystemProfiling_StopProfiling_Vulkan_Params* params)
{
    // System profiling is handled through injection, so this is a simple success for Vulkan
    (void)params; // Suppress unused parameter warning
    return NGFX_Result_Success;
}

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_SYSTEM_PROFILING_VULKAN_H

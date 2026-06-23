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
// File: NGFX_GraphicsCapture_Vulkan.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_GRAPHICS_CAPTURE_VULKAN_H
#define NGFX_GRAPHICS_CAPTURE_VULKAN_H

#include "NGFX_GraphicsCapture_Common.h"
#include "NGFX_Vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief Injects the GraphicsCapture activity into a Vulkan application.
 *
 * @note Only supported on Windows.
 *
 * @param params injection parameters
 * @return NGFX_Result_Success if the injection is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GraphicsCapture_Inject_Vulkan(NGFX_GraphicsCapture_Inject_Vulkan_Params* params)
{
    return NGFX_InjectActivity(NGFX_ActivityType_GraphicsCapture, params->installationPath, params->settings);
}

/**
 * @brief Initialize the Graphics capture activity in an already injected Vulkan application
 *
 * @param params injection parameters
 * @return NGFX_Result_Success if the initialization is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GraphicsCapture_InitializeActivity_Vulkan(NGFX_GraphicsCapture_InitializeActivity_Vulkan_Params* params)
{
    return NGFX_Do_InitializeActivity(NGFX_ActivityType_GraphicsCapture, NGFX_ApiType_Vulkan, (void*)params);
}

/**
 * @brief Requests that a capture begin at the start of the next delimiter
 *
 * This method specifies the bounds of the capture and accordingly automatically starts and stops
 * the capture after the specified capture period
 *
 * @param params Parameters for beginning the capture
 * @return NGFX_Result_Success if the capture is successfully requested, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GraphicsCapture_RequestCapture_Vulkan(NGFX_GraphicsCapture_RequestCapture_Vulkan_Params* params)
{
    return g_NGFX_Globals.GraphicsCapture.RequestCaptureVulkan(params);
}

/** @brief Starts a Capture of the application.
 *
 * This method must be paired with an associated NGFX_GraphicsCapture_StopCapture_Vulkan
 *
 * @param params Parameters for starting the capture
 * @return NGFX_Result_Success if the capture is successfully started, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GraphicsCapture_StartCapture_Vulkan(NGFX_GraphicsCapture_StartCapture_Vulkan_Params* params)
{
    return g_NGFX_Globals.GraphicsCapture.StartCaptureVulkan(params);
}

/**
 * @brief Stops a previously started Capture of the application.
 *
 * This method must have been preceded by NGFX_GraphicsCapture_StartCapture_Vulkan call.
 *
 * @param params Parameters for stopping the capture
 * @return NGFX_Result_Success if the capture is successfully stopped, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GraphicsCapture_StopCapture_Vulkan(NGFX_GraphicsCapture_StopCapture_Vulkan_Params* params)
{
    return g_NGFX_Globals.GraphicsCapture.StopCaptureVulkan(params);
}

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_GRAPHICS_CAPTURE_VULKAN_H

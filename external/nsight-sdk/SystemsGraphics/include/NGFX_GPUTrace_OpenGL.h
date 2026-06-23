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
// File: NGFX_GPUTrace_OpenGL.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_GPUTRACE_OPENGL_H
#define NGFX_GPUTRACE_OPENGL_H

#include "Impl/NGFX_Core.h"

#include "NGFX_GPUTrace_Common.h"
#include "NGFX_OpenGL.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup NGFX_API_CORE
 * @{
 */

/**
 * @brief Injects the GPU Trace activity into a OpenGL application.
 *
 * @note Only supported on Windows.
 *
 * @param params injection parameters
 * @return NGFX_Result_Success if the injection is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_Inject_OpenGL(NGFX_GPUTrace_Inject_OpenGL_Params* params)
{
    return NGFX_InjectActivity(NGFX_ActivityType_GPUTrace, params->installationPath, params->settings);
}

/**
 * @brief Initialize the GPU Trace activity in an already injected OpenGL application.
 *
 * @param params injection parameters
 * @return NGFX_Result_Success if the initialization is successful, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_InitializeActivity_OpenGL(NGFX_GPUTrace_InitializeActivity_OpenGL_Params* params)
{
    return NGFX_Do_InitializeActivity(NGFX_ActivityType_GPUTrace, NGFX_ApiType_OpenGL, (void*)params);
}

/**
 * @brief Activates the GPU Trace activity.
 *
 * If GPU Trace is inactive, this call may be used to prepare trace resources and activate GPU Trace, which is required
 * before starting a trace. Alternatively, GPU Trace can be activated by submitting work on any queue.
 * This is a blocking call that may require communication with the host.
 * GPU Trace only needs to be activated once across the lifetime of the application, and it can be reused for multiple traces.
 * The calling thread is required to have a current OpenGL context.
 *
 * @param params Parameters for activating the trace
 * @return NGFX_Result_Success if the trace is successfully activated, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_ActivateTrace_OpenGL(NGFX_GPUTrace_ActivateTrace_OpenGL_Params* params)
{
    return g_NGFX_Globals.GPUTrace.ActivateTraceOpenGL(params);
}

/*
 * @brief Starts a Trace of the application.
 *
 * GPU Trace must be in an active state before starting a trace.
 *
 * @param params Parameters for starting the trace
 * @return NGFX_Result_Success if the trace is successfully started, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_StartTrace_OpenGL(NGFX_GPUTrace_StartTrace_OpenGL_Params* params)
{
    return g_NGFX_Globals.GPUTrace.StartTraceOpenGL(params);
}

/**
 * @brief Stops a previously started Trace.
 *
 * GPU Trace must be in a tracing state before stopping a trace.
 * The calling thread is required to have a current OpenGL context.
 *
 * @param params Pointer to a structure containing parameters for stopping the trace.
 * @return NGFX_Result_Success if the trace is successfully stopped, or an appropriate error code.
 */
NGFX_FUNCTION NGFX_Result NGFX_GPUTrace_StopTrace_OpenGL(NGFX_GPUTrace_StopTrace_OpenGL_Params* params)
{
    return g_NGFX_Globals.GPUTrace.StopTraceOpenGL(params);
}

/**
 * END addtogroup NGFX_API_CORE
 * @}
 */

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NGFX_GPUTRACE_OPENGL_H

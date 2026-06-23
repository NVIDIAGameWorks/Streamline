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
// File: NGFX_GPUTrace_CUDA_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_GPUTRACE_CUDA_TYPES_H
#define NGFX_GPUTRACE_CUDA_TYPES_H

#include "NGFX_CUDA_Types.h"
#include "NGFX_GPUTrace_Common_Types.h"

/**
 * @brief Parameters for injecting GPUTrace into an application that uses the CUDA driver API
 */
typedef struct NGFX_GPUTrace_Inject_CUDA_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GPUTrace_Inject_CUDA_Params_VER */

    const NGFX_PathChar* installationPath;     /**< The path to the installation directory of the GPU Trace library */
    NGFX_GPUTrace_InjectionSettings* settings; /**< Pointer to structure containing settings for injection. Expected to be non Null. */
} NGFX_GPUTrace_Inject_CUDA_Params_V1;
typedef NGFX_GPUTrace_Inject_CUDA_Params_V1 NGFX_GPUTrace_Inject_CUDA_Params;
#define NGFX_GPUTrace_Inject_CUDA_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GPUTrace_Inject_CUDA_Params_V1, 1)
#define NGFX_GPUTrace_Inject_CUDA_Params_VER NGFX_GPUTrace_Inject_CUDA_Params_V1_VER

/**
 * @brief Parameters for initializing a GPU Trace in an application that uses the CUDA driver API
 */
typedef struct NGFX_GPUTrace_InitializeActivity_CUDA_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GPUTrace_InitializeActivity_CUDA_Params_VER */
} NGFX_GPUTrace_InitializeActivity_CUDA_Params_V1;
typedef NGFX_GPUTrace_InitializeActivity_CUDA_Params_V1 NGFX_GPUTrace_InitializeActivity_CUDA_Params;
#define NGFX_GPUTrace_InitializeActivity_CUDA_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GPUTrace_InitializeActivity_CUDA_Params_V1, 1)
#define NGFX_GPUTrace_InitializeActivity_CUDA_Params_VER NGFX_GPUTrace_InitializeActivity_CUDA_Params_V1_VER

/**
 * @brief Parameters for activating GPU trace.
 */
typedef struct NGFX_GPUTrace_ActivateTrace_CUDA_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GPUTrace_ActivateTrace_CUDA_Params_VER */

    CUcontext ctx;               /**< The CUDA context that owns the associated stream, this is a required parameter. */
    CUstream hStream;            /**< A CUDA stream handle that will be used to generate required trace resources, this is a required parameter. */
    bool perThreadDefaultStream; /**< Indicates if the default stream is per-thread. */
} NGFX_GPUTrace_ActivateTrace_CUDA_Params_V1;
typedef NGFX_GPUTrace_ActivateTrace_CUDA_Params_V1 NGFX_GPUTrace_ActivateTrace_CUDA_Params;
#define NGFX_GPUTrace_ActivateTrace_CUDA_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GPUTrace_ActivateTrace_CUDA_Params_V1, 1)
#define NGFX_GPUTrace_ActivateTrace_CUDA_Params_VER NGFX_GPUTrace_ActivateTrace_CUDA_Params_V1_VER

/**
 * @brief Parameters for starting a GPU trace.
 */
typedef struct NGFX_GPUTrace_StartTrace_CUDA_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GPUTrace_StartTrace_CUDA_Params_VER */

} NGFX_GPUTrace_StartTrace_CUDA_Params_V1;
typedef NGFX_GPUTrace_StartTrace_CUDA_Params_V1 NGFX_GPUTrace_StartTrace_CUDA_Params;
#define NGFX_GPUTrace_StartTrace_CUDA_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GPUTrace_StartTrace_CUDA_Params_V1, 1)
#define NGFX_GPUTrace_StartTrace_CUDA_Params_VER NGFX_GPUTrace_StartTrace_CUDA_Params_V1_VER

/**
 * @brief Parameters for stopping a GPU trace.
 */
typedef struct NGFX_GPUTrace_StopTrace_CUDA_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_GPUTrace_StopTrace_CUDA_Params_VER */

    NGFX_GPUTrace_StopTraceFlags flags; /**< Flags controlling the stop trace behavior. */
    CUcontext ctx;                      /**< The CUDA context that owns the associated stream. */
    CUstream hStream;                   /**< A CUDA stream handle, when all scheduled work on this stream has completed; the trace is complete.
                                             If the stream handle is set to NGFX_CUDA_INVALID_STREAM_HANDLE, the trace may immediately stop collecting data.
                                             Ignored if NGFX_GPUTrace_StopTraceFlag_NextFrameBoundary is used. */
    bool perThreadDefaultStream;        /**< Indicates if the default stream is per-thread. */
} NGFX_GPUTrace_StopTrace_CUDA_Params_V1;
typedef NGFX_GPUTrace_StopTrace_CUDA_Params_V1 NGFX_GPUTrace_StopTrace_CUDA_Params;
#define NGFX_GPUTrace_StopTrace_CUDA_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_GPUTrace_StopTrace_CUDA_Params_V1, 1)
#define NGFX_GPUTrace_StopTrace_CUDA_Params_VER NGFX_GPUTrace_StopTrace_CUDA_Params_V1_VER

#endif // NGFX_GPUTRACE_CUDA_TYPES_H

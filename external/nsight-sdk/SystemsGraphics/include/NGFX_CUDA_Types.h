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
// File: NGFX_CUDA_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_CUDA_TYPES_H
#define NGFX_CUDA_TYPES_H

#include "NGFX_Types.h"

// Must match CUDA driver API handle definition
/**
 * @cond INTERNAL_IMPLEMENTATION_DETAILS
 */
#define NGFX_DEFINE_CUDA_HANDLE(struct_name, handle_name) typedef struct struct_name##_st* handle_name;
/**
 * @endcond
 */

// Forward declarations of CUDA driver API handles
NGFX_DEFINE_CUDA_HANDLE(CUctx, CUcontext);
NGFX_DEFINE_CUDA_HANDLE(CUstream, CUstream);

/**
 * @brief An invalid CUDA driver API stream handle. This can be used to indicate that a stream handle parameter should be ignored.
 */
#define NGFX_CUDA_INVALID_STREAM_HANDLE ((CUstream) - 1)

/**
 * @brief Parameters for delimiting a CUDA driver API frame.
 */
typedef struct NGFX_FrameBoundary_CUDA_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_FrameBoundary_CUDA_Params_VER */

    CUcontext ctx;               /**< The CUDA context that owns the associated stream. */
    CUstream hStream;            /**< The CUDA stream associated with the end of frame operation. */
    bool perThreadDefaultStream; /**< Indicates if the default stream is per-thread. */
} NGFX_FrameBoundary_CUDA_Params_V1;
typedef NGFX_FrameBoundary_CUDA_Params_V1 NGFX_FrameBoundary_CUDA_Params;
#define NGFX_FrameBoundary_CUDA_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_FrameBoundary_CUDA_Params_V1, 1)
#define NGFX_FrameBoundary_CUDA_Params_VER NGFX_FrameBoundary_CUDA_Params_V1_VER

#endif // NGFX_CUDA_TYPES_H

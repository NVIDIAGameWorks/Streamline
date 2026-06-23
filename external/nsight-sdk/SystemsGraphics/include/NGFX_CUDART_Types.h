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
// File: NGFX_CUDART_Types.h
//
// Distributed as part of the NVIDIA Nsight Graphics SDK.
//--------------------------------------------------------------------------------------

#ifndef NGFX_CUDART_TYPES_H
#define NGFX_CUDART_TYPES_H

#include "NGFX_Types.h"

// Must match CUDA runtime API handle definition
/**
 * @cond INTERNAL_IMPLEMENTATION_DETAILS
 */
#define NGFX_DEFINE_CUDART_HANDLE(struct_name, handle_name) typedef struct struct_name##_st* handle_name;
/**
 * @endcond
 */

// Forward declarations of CUDA runtime API handles
NGFX_DEFINE_CUDART_HANDLE(CUstream, cudaStream_t);

/**
 * @brief An invalid CUDA runtime API stream handle. This can be used to indicate that a stream handle parameter should be ignored.
 */
#define NGFX_CUDART_INVALID_STREAM_HANDLE ((cudaStream_t) - 1)

/**
 * @brief Parameters for delimiting a CUDA runtime API frame.
 */
typedef struct NGFX_FrameBoundary_CUDART_Params_V1
{
    uint32_t version; /**< Used for versioning; must be assigned a value of NGFX_FrameBoundary_CUDART_Params_VER */

    cudaStream_t stream;         /**< The CUDA stream associated with the end of frame operation, must be associated with the context that is current on the calling thread. */
    bool perThreadDefaultStream; /**< Indicates if the default stream is per-thread. */
} NGFX_FrameBoundary_CUDART_Params_V1;
typedef NGFX_FrameBoundary_CUDART_Params_V1 NGFX_FrameBoundary_CUDART_Params;
#define NGFX_FrameBoundary_CUDART_Params_V1_VER NGFX_MAKE_STRUCT_VERSION(NGFX_FrameBoundary_CUDART_Params_V1, 1)
#define NGFX_FrameBoundary_CUDART_Params_VER NGFX_FrameBoundary_CUDART_Params_V1_VER

#endif // NGFX_CUDART_TYPES_H
